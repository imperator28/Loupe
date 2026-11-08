#include "app/ApplicationController.h"
#include "app/platform/StartupTrace.h"
#include "app/platform/UpdateController.h"
#include "app/worker/WorkerLaunch.h"
#include "protocol/GeometryPayload.h"
#include "app/cache/CacheKey.h"
#include "app/cache/CacheStore.h"
#include "app/cache/SourceFingerprint.h"
#include "app/render/PartPalette.h"
#include "core/domain/AssemblyTypes.h"
#include "core/inspection/MaterialProperties.h"

#include <QStandardPaths>
#include <QCoreApplication>
#include <QBuffer>
#include <QColor>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QQuaternion>
#include <QFutureWatcher>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QProcessEnvironment>
#include <QGenericMatrix>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QDateTime>
#include <QtConcurrent/QtConcurrentRun>

#include <cmath>
#include <algorithm>

namespace {

// Bumped from step-importer-4: StepImporter now splits a multi-solid
// compound into per-solid Body children, changing the node tree shape for
// any cached file that has one -- old cached snapshots must not be reused.
// Bumped again (-6): those split bodies now also get their own mesh/edge
// geometry entry instead of none, changing the cached geometry payload too.
// Bumped again (-7): each split body now gets its own unique definitionId
// instead of reusing the container's, so quantity grouping and per-definition
// material assignment don't treat unrelated sibling bodies as one part.
// Bumped again (-8): an inch file is no longer misread as unresolved with a 1.0
// factor, so a cached snapshot carries a unit decision export would get wrong.
const auto kImporterVersion = QStringLiteral("step-importer-9");
const auto kMeshProfile = QStringLiteral("progressive-1");
// Mesh payloads now contain the composed occurrence transform.  Keep older
// local-coordinate payloads out of the replay cache.
// -4: an uncoloured face carries no colour instead of a teal placeholder.
// -5: file colours are sRGB hex, no longer OCCT's linear values written as hex.
const auto kGeometryCacheProfile = QStringLiteral("progressive-5-srgb-colours");
constexpr auto kGeometryCacheMagic = "LGC1";

[[nodiscard]] std::optional<loupe::app::render::PartPaletteStyle> partPaletteStyle(const QString& mode)
{
    return loupe::app::render::partPaletteStyleFromName(mode);
}
constexpr auto kWorkerConnectionAttempts = 4'800;

} // namespace

namespace loupe::app {

ApplicationController::ApplicationController(QObject* parent)
    : ApplicationController(worker::defaultWorkerExecutable(), cache::CacheStore::defaultRoot(), parent)
{
}

ApplicationController::ApplicationController(const QString& workerExecutable, QObject* parent)
    : ApplicationController(workerExecutable, cache::CacheStore::defaultRoot(), parent)
{
}

ApplicationController::ApplicationController(const QString& workerExecutable, const QString& cacheRoot, QObject* parent)
    : QObject(parent)
    , workerExecutable_(workerExecutable)
{
    try {
        cacheStore_ = std::make_unique<cache::CacheStore>(cacheRoot, 512LL * 1024LL * 1024LL);
        overrideStore_ = std::make_unique<cache::OverrideStore>(QDir(cacheRoot).filePath(QStringLiteral("unit-overrides.sqlite")));
    } catch (const std::exception&) {
    }
    // The importer launched for each file is set up the way a prelaunched one is: its own
    // environment, its output forwarded to Loupe's.
    auto workerProcess = std::make_unique<QProcess>();
    worker::configureWorkerProcess(*workerProcess);
    useWorkerProcess(std::move(workerProcess));
    // A layout set aside when an edit starts over from the assembly is kept, unless it is
    // one already saved, so starting over never loses a layout.
    // Volume and bounds with nothing selected are of the whole model, whose parts may have
    // finished arriving since the selection last changed: measure again on entering them.
    connect(&measurementController_, &tools::MeasurementController::modeChanged, this,
            [this] { applyActiveGeometryToMeasurement(); });
    connect(&explodeController_, &tools::ExplodeController::layoutSetAside, this,
            [this](const QVariantMap& snapshot, const QString& summary) {
        for (const auto& value : savedViews_.explodeLayouts()) {
            if (snapshot == savedViews_.data(value.toMap().value(QStringLiteral("id")).toString())) return;
        }
        savedViews_.add(QStringLiteral("layout"), {}, tr("Set aside · %1").arg(summary), snapshot);
    });
    connect(&workerClient_, &worker::WorkerClient::snapshotReady, this, [this](quint64 requestId, const QByteArray& snapshot) {
        if (requestId != activeRequestId_) return;
        platform::startupMark("snapshot received");
        if (cacheHit_ && !snapshotJson_.isEmpty()) {
            // The cache already established the identical tree and replayed its geometry. Keep it visible
            // while the worker retains the XCAF document and streams any settled replacements.
            return;
        }
        applySnapshotToTree(snapshot);
        snapshotJson_ = QString::fromUtf8(snapshot);
        exportWorkspaceController_.replaceSnapshot(snapshotJson_);
        drawingWorkspaceController_.replaceSnapshot(snapshotJson_);
        if (cacheStore_ && pendingSource_) {
            const auto document = QJsonDocument::fromJson(snapshot).object();
            const cache::UnitDecision unit{document.value(QStringLiteral("effectiveUnit")).toString(), document.value(QStringLiteral("sourceToMillimeters")).toDouble(1.0)};
            const auto key = cache::CacheKey::from(*pendingSource_, kImporterVersion, kMeshProfile, unit);
            static_cast<void>(cacheStore_->put(key, snapshot, {*pendingSource_, kImporterVersion, kMeshProfile, unit}));
        }
        documentState_ = DocumentState::TreeReady;
        emit snapshotChanged();
        emit documentStateChanged();
    });
    connect(&workerClient_, &worker::WorkerClient::meshReady, this, [this](quint64 requestId, const QString& nodeId, const QString& segmentKey, const QString& sourceColor, const QByteArray& meshJson) {
        if (requestId != activeRequestId_) return;
        if (cacheHit_ && cachedGeometryLoaded_) return;
        appendCachedMesh(nodeId, segmentKey, sourceColor, meshJson);
        if (!previewGeometryReceived_) {
            previewGeometryReceived_ = true;
            emit importProgressChanged();
        }
        markOpenMilestone(openViewMs_);
        noteImportedColor(nodeId, sourceColor);
        if (noteSegmentColor(nodeId, segmentKey, sourceColor, meshJson) && isNodeSelected(nodeId))
            emit componentPropertiesChanged();
        notePartBounds(nodeId, meshJson);
        meshRevisionBySegment_.insert(segmentKey, ++geometryRevisionCounter_);
        emit meshReady(nodeId, segmentKey, sourceColor, meshJson);
    });
    connect(&workerClient_, &worker::WorkerClient::componentMetadata, this,
            [this](quint64 requestId, const QString& nodeId, const double surfaceAreaMm2, const double volumeMm3,
                   const double widthMm, const double heightMm, const double depthMm, const double longestEdgeMm,
                   const double circularRadiusMm, const int planarFaceCount) {
        if (requestId != activeRequestId_) return;
        geometryByNode_.insert(nodeId, {surfaceAreaMm2, volumeMm3, QVector3D{static_cast<float>(widthMm), static_cast<float>(heightMm), static_cast<float>(depthMm)},
                                       longestEdgeMm, circularRadiusMm, planarFaceCount});
        const auto modelExtent = std::max({modelExtentMm_, widthMm, heightMm, depthMm});
        if (!qFuzzyCompare(modelExtentMm_ + 1.0, modelExtent + 1.0)) {
            modelExtentMm_ = modelExtent;
            emit modelExtentChanged();
        }
        if (isNodeSelected(nodeId)) {
            applyActiveGeometryToMeasurement();
            emit componentPropertiesChanged();
        }
    });
    connect(&workerClient_, &worker::WorkerClient::edgeReady, this, [this](quint64 requestId, const QString& nodeId, const QByteArray& edgeJson) {
        if (requestId != activeRequestId_) return;
        if (cacheHit_ && cachedGeometryLoaded_) return;
        appendCachedEdges(nodeId, edgeJson);
        edgeRevisionByNode_.insert(nodeId, ++geometryRevisionCounter_);
        emit edgeReady(nodeId, edgeJson);
    });
    connect(&workerClient_, &worker::WorkerClient::sharedMeshReady, this,
            [this](quint64 requestId, const QString& sourceKey, const QString& sourceColor, const QByteArray& meshJson) {
        if (requestId != activeRequestId_ || (cacheHit_ && cachedGeometryLoaded_)) return;
        handleSharedMesh(sourceKey, sourceColor, meshJson, false);
    });
    connect(&workerClient_, &worker::WorkerClient::sharedEdgesReady, this,
            [this](quint64 requestId, const QString& sourceKey, const QByteArray& edgeJson) {
        if (requestId != activeRequestId_ || (cacheHit_ && cachedGeometryLoaded_)) return;
        handleSharedEdges(sourceKey, edgeJson, false);
    });
    connect(&workerClient_, &worker::WorkerClient::meshInstanceReady, this,
            [this](quint64 requestId, const QString& nodeId, const QString& segmentKey, const QString& sourceKey,
                   const QString& sourceColor, const QVector<float>& placement) {
        if (requestId != activeRequestId_ || (cacheHit_ && cachedGeometryLoaded_)) return;
        handleMeshInstance(nodeId, segmentKey, sourceKey, sourceColor, placement, false);
    });
    connect(&workerClient_, &worker::WorkerClient::edgeInstanceReady, this,
            [this](quint64 requestId, const QString& nodeId, const QString& sourceKey, const QVector<float>& placement) {
        if (requestId != activeRequestId_ || (cacheHit_ && cachedGeometryLoaded_)) return;
        handleEdgeInstance(nodeId, sourceKey, placement, false);
    });
    connect(&workerClient_, &worker::WorkerClient::progress, this, [this](quint64 requestId, const QString& stage, const double fraction) {
        if (requestId != 0 && requestId == activeAssemblyExportRequestId_) {
            assemblyExportStageKey_ = stage;
            assemblyExportProgress_ = std::clamp(fraction, 0.0, 1.0);
            emit assemblyExportProgressChanged();
            return;
        }
        if (requestId != activeRequestId_) return;
        setImportProgress(stage, fraction);
        if (fraction >= 1.0) {
            platform::startupMark("import complete");
            exportWorkspaceController_.setDocumentReady(true);
            drawingWorkspaceController_.setDocumentReady(true);
            saveGeometryCache();
            markOpenMilestone(openFullMs_);
            // The importer's own timings follow the last progress; record once they are in.
            const auto request = activeRequestId_;
            QTimer::singleShot(1500, this, [this, request] { if (request == activeRequestId_) recordOpen(); });
        }
    });
    // The importer's timings run from when it started, after the app launched it: shifted
    // onto the app's clock by when it finished, they give when every part was first shown.
    connect(&workerClient_, &worker::WorkerClient::importMetrics, this,
            [this](const quint64 requestId, const QString&, const QString&, qint64, qint64, qint64, qint64,
                   qint64, const qint64 previewReadyMs, const qint64 finalReadyMs, qint64, qint64, int) {
        if (requestId != activeRequestId_ || !openClock_.isValid() || openPreviewMs_ >= 0 || previewReadyMs <= 0) return;
        const auto offset = std::max<qint64>(0, openClock_.elapsed() - finalReadyMs);
        openPreviewMs_ = previewReadyMs + offset;
        emit openTimingChanged();
    });
    connect(&workerClient_, &worker::WorkerClient::requestCanceled, this, [this](quint64 requestId) {
        if (requestId == activeDrawingRequestId_) {
            activeDrawingRequestId_ = 0;
            drawingWorkspaceController_.handleDrawingCanceled();
            return;
        }
        if (requestId == activeDrawingPreviewRequestId_) {
            // A superseded preview, not a user-visible failure.
            activeDrawingPreviewRequestId_ = 0;
            return;
        }
        if (requestId == activeExportRequestId_) {
            activeExportRequestId_ = 0;
            exportWorkspaceController_.handleExportCanceled();
            return;
        }
        if (requestId != activeRequestId_) return;
        activeRequestId_ = 0;
        importInProgress_ = false;
        importProgressTimer_.stop();
        importStage_ = preservePreviewOnCancel_ ? tr("Preview shown - refinement canceled") : tr("Import canceled");
        emit importProgressChanged();
        if (!preservePreviewOnCancel_) {
            documentState_ = DocumentState::Empty;
            emit documentStateChanged();
        }
        preservePreviewOnCancel_ = false;
    });
    connect(&workerClient_, &worker::WorkerClient::requestFailed, this, [this](quint64 requestId, const QString& code, const QString& message, bool) {
        if (requestId == activeDrawingRequestId_) {
            activeDrawingRequestId_ = 0;
            drawingWorkspaceController_.handleDrawingFailed(message.isEmpty() ? code : message);
            return;
        }
        if (requestId == activeDrawingPreviewRequestId_) {
            // A preview that could not be produced leaves the candidate's status alone; the
            // add action is already gated on the candidate, not on the preview.
            activeDrawingPreviewRequestId_ = 0;
            return;
        }
        if (requestId == activeExportRequestId_) {
            activeExportRequestId_ = 0;
            exportWorkspaceController_.handleExportFailed(message.isEmpty() ? code : message);
            return;
        }
        if (requestId == activeAssemblyExportRequestId_) {
            activeAssemblyExportRequestId_ = 0;
            assemblyExportSucceeded_ = false;
            assemblyExportMessage_ = (activeAssemblyExportExploded_ ? tr("The exploded assembly could not be exported: %1")
                                                                    : tr("The assembly could not be exported: %1"))
                                         .arg(message.isEmpty() ? code : message);
            emit assemblyExportChanged();
            return;
        }
        if (requestId != activeRequestId_) return;
        const auto detail = message.isEmpty() ? code : message;
        setErrorMessage(tr("The STEP importer could not open this file: %1").arg(detail));
        documentState_ = DocumentState::WorkerFailed;
        emit documentStateChanged();
    });
    connect(&workerClient_, &worker::WorkerClient::assemblyExported, this,
            [this](const quint64 requestId, const QString& path, const int partCount, const int removedCount) {
        if (requestId != activeAssemblyExportRequestId_) return;
        activeAssemblyExportRequestId_ = 0;
        assemblyExportSucceeded_ = true;
        // The title already says what was exported; this says where, and what was left out.
        const auto where = QDir::toNativeSeparators(path);
        assemblyExportMessage_ = removedCount == 0 ? tr("Saved to %1.").arg(where)
                               : removedCount == 1 ? tr("Saved to %1, with 1 hidden part left out.").arg(where)
                                                   : tr("Saved to %1, with %2 hidden parts left out.").arg(where).arg(removedCount);
        Q_UNUSED(partCount);
        emit assemblyExportChanged();
    });
    connect(&exportWorkspaceController_, &exporting::ExportWorkspaceController::executeRequested,
            this, [this](const QByteArray& planJson, const QString& fingerprint) {
        activeExportRequestId_ = workerClient_.executeExportPlan(planJson, fingerprint);
        exportWorkspaceController_.setExportRequestId(activeExportRequestId_);
    });
    connect(&exportWorkspaceController_, &exporting::ExportWorkspaceController::cancelRequested,
            this, [this](const quint64 requestId) { workerClient_.cancel(requestId); });
    connect(&workerClient_, &worker::WorkerClient::exportProgress, this,
            [this](const quint64 requestId, const int rowIndex, const int rowCount,
                   const QString& stage, const double fraction) {
        if (requestId == activeExportRequestId_) {
            exportWorkspaceController_.handleExportProgress(rowIndex, rowCount, stage, fraction);
        }
    });
    connect(&workerClient_, &worker::WorkerClient::exportRowResult, this,
            [this](const quint64 requestId, const int rowIndex, const QString& nodeId,
                   const QString& path, const bool passed, const QString& message) {
        if (requestId == activeExportRequestId_) {
            exportWorkspaceController_.handleExportRowResult(rowIndex, nodeId, path, passed, message);
        }
    });
    connect(&workerClient_, &worker::WorkerClient::exportCompleted, this,
            [this](const quint64 requestId, const int succeededCount, const int failedCount) {
        if (requestId != activeExportRequestId_) return;
        activeExportRequestId_ = 0;
        exportWorkspaceController_.handleExportCompleted(succeededCount, failedCount);
    });
    connect(&workerClient_, &worker::WorkerClient::protocolError, this, [this](const QString& message) {
        if (activeDrawingRequestId_ != 0) {
            activeDrawingRequestId_ = 0;
            drawingWorkspaceController_.handleDrawingFailed(message);
        }
        if (activeExportRequestId_ == 0) return;
        activeExportRequestId_ = 0;
        exportWorkspaceController_.handleExportFailed(message);
    });
    connect(&workerClient_, &worker::WorkerClient::connectionLost, this, [this] {
        if (activeDrawingRequestId_ != 0) {
            activeDrawingRequestId_ = 0;
            drawingWorkspaceController_.handleDrawingFailed(tr("Connection to the export worker was lost"));
        }
        if (activeExportRequestId_ == 0) return;
        activeExportRequestId_ = 0;
        exportWorkspaceController_.handleExportFailed(tr("Connection to the export worker was lost"));
    });
    connect(&drawingWorkspaceController_, &drawing::DrawingWorkspaceController::executeRequested,
            this, [this](const QByteArray& planJson, const QString& fingerprint) {
        activeDrawingRequestId_ = workerClient_.executeDrawingPlan(planJson, fingerprint);
        drawingWorkspaceController_.setExportRequestId(activeDrawingRequestId_);
    });
    connect(&drawingWorkspaceController_, &drawing::DrawingWorkspaceController::cancelRequested,
            this, [this](const quint64 requestId) { workerClient_.cancel(requestId); });
    connect(&drawingWorkspaceController_, &drawing::DrawingWorkspaceController::previewRequested,
            this, [this](const QByteArray& requestJson, const int revision) {
        activeDrawingPreviewRevision_ = revision;
        activeDrawingPreviewRequestId_ = workerClient_.requestDrawingPreview(requestJson, revision);
    });
    connect(&drawingWorkspaceController_, &drawing::DrawingWorkspaceController::previewCanceled,
            this, [this](const int revision) {
        // Only cancel the in-flight request if it is the one being superseded; a newer
        // request may already have replaced it.
        if (revision != activeDrawingPreviewRevision_ || activeDrawingPreviewRequestId_ == 0) return;
        workerClient_.cancel(activeDrawingPreviewRequestId_);
        activeDrawingPreviewRequestId_ = 0;
    });
    connect(&workerClient_, &worker::WorkerClient::drawingProgress, this,
            [this](const quint64 requestId, const int rowIndex, const int rowCount,
                   const QString& stage, const double fraction) {
        if (requestId == activeDrawingRequestId_) {
            drawingWorkspaceController_.handleDrawingProgress(rowIndex, rowCount, stage, fraction);
        }
    });
    connect(&workerClient_, &worker::WorkerClient::drawingRowResult, this,
            [this](const quint64 requestId, const int rowIndex, const QString& drawingId,
                   const QString& path, const bool passed, const QString& message) {
        if (requestId == activeDrawingRequestId_) {
            drawingWorkspaceController_.handleDrawingRowResult(rowIndex, drawingId, path, passed, message);
        }
    });
    connect(&workerClient_, &worker::WorkerClient::drawingCompleted, this,
            [this](const quint64 requestId, const int succeededCount, const int failedCount) {
        if (requestId != activeDrawingRequestId_) return;
        activeDrawingRequestId_ = 0;
        drawingWorkspaceController_.handleDrawingCompleted(succeededCount, failedCount);
    });
    connect(&workerClient_, &worker::WorkerClient::drawingPreviewReady, this,
            [this](const quint64 requestId, const int revision, const QByteArray& previewJson,
                   const bool approximate) {
        // Two gates rather than one: the request ID rules out a reply for a different
        // request, the revision rules out a reply that raced a newer candidate.
        if (requestId != activeDrawingPreviewRequestId_ || revision != activeDrawingPreviewRevision_) return;
        activeDrawingPreviewRequestId_ = 0;
        emit drawingPreviewReady(QString::fromUtf8(previewJson), revision, approximate);
    });
    connect(&visibilityModel_, &models::VisibilityModel::presentationChanged, this, [this] {
        ++visibilityRevision_;
        emit visibilityChanged();
        emit viewerPresentationChanged();
    });
    connect(&materialLibrary_, &models::MaterialLibraryModel::materialsChanged, this, [this] {
        bool changed = false;
        for (auto it = materialByNode_.begin(); it != materialByNode_.end();) {
            if (materialLibrary_.find(it.value())) { ++it; continue; }
            it = materialByNode_.erase(it);
            changed = true;
        }
        if (changed) emit componentPropertiesChanged();
    });
    importProgressTimer_.setInterval(1'000);
    connect(&importProgressTimer_, &QTimer::timeout, this, [this] {
        if (!importInProgress_) return;
        importElapsedSeconds_ = importElapsed_.elapsed() / 1'000;
        emit importProgressChanged();
    });
    geometryReplayTimer_.setSingleShot(true);
    geometryReplayTimer_.setInterval(0);
    connect(&geometryReplayTimer_, &QTimer::timeout, this, &ApplicationController::replayGeometryChunk);
}

void ApplicationController::useWorkerProcess(std::unique_ptr<QProcess> process)
{
    workerProcess_ = std::move(process);
    workerProcess_->setParent(nullptr);
    connect(workerProcess_.get(), &QProcess::started, this, [this] {
        platform::startupMark("worker started signal");
        setImportProgress(tr("Starting STEP importer"), 0.005);
        connectWorker();
    });
    connect(workerProcess_.get(), &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (stoppingWorker_) {
            return;
        }
        setErrorMessage(tr("Could not start the STEP importer: %1").arg(workerProcess_->errorString()));
        documentState_ = DocumentState::WorkerFailed;
        emit documentStateChanged();
    });
    connect(workerProcess_.get(), qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](const int, const QProcess::ExitStatus) {
                if (!stoppingWorker_ && activeExportRequestId_ != 0) {
                    activeExportRequestId_ = 0;
                    exportWorkspaceController_.handleExportFailed(tr("The export worker exited unexpectedly"));
                    return;
                }
                if (stoppingWorker_ || documentState_ != DocumentState::Opening || activeRequestId_ != 0) return;
                setErrorMessage(tr("The STEP importer exited before it became ready."));
                documentState_ = DocumentState::WorkerFailed;
                emit documentStateChanged();
            });
}

ApplicationController::~ApplicationController()
{
    if (workerProcess_->state() != QProcess::NotRunning) {
        workerProcess_->terminate();
        if (!workerProcess_->waitForFinished(1'000)) {
            workerProcess_->kill();
            workerProcess_->waitForFinished(1'000);
        }
    }
    if (!stagedWorkerPath_.isEmpty()) QFile::remove(stagedWorkerPath_);
}

InspectorMode ApplicationController::inspectorMode() const noexcept
{
    return activeNodeId_.isEmpty() ? InspectorMode::Document : InspectorMode::Component;
}

void ApplicationController::setWorkspace(const Workspace workspace)
{
    if (workspace_ == workspace) {
        return;
    }
    workspace_ = workspace;
    emit workspaceChanged();
}

void ApplicationController::setActiveNodeId(const QString& activeNodeId)
{
    setSelection(activeNodeId.isEmpty() ? QStringList{} : QStringList{activeNodeId}, activeNodeId);
}

void ApplicationController::selectNode(const QString& nodeId, const bool additive)
{
    if (nodeId.isEmpty()) {
        if (!additive) setSelection({}, {});
        return;
    }
    if (!additive) {
        setSelection({nodeId}, nodeId);
        return;
    }

    auto selected = selectedNodeIds_;
    if (selected.contains(nodeId)) {
        selected.removeAll(nodeId);
        setSelection(selected, selected.isEmpty() ? QString{} : selected.back());
    } else {
        selected.append(nodeId);
        setSelection(selected, nodeId);
    }
}

void ApplicationController::selectNodes(const QStringList& nodeIds, const QString& activeNodeId, const bool additive)
{
    if (!additive) {
        setSelection(nodeIds, activeNodeId);
        return;
    }
    auto selected = selectedNodeIds_;
    for (const auto& nodeId : nodeIds)
        if (!selected.contains(nodeId)) selected.append(nodeId);
    setSelection(selected, activeNodeId);
}

void ApplicationController::setSelection(const QStringList& nodeIds, const QString& activeNodeId)
{
    QStringList normalized;
    for (const auto& nodeId : nodeIds) {
        if (!nodeId.isEmpty() && !normalized.contains(nodeId)) normalized.append(nodeId);
    }
    const auto nextActiveNodeId = normalized.contains(activeNodeId)
        ? activeNodeId : normalized.isEmpty() ? QString{} : normalized.back();
    if (selectedNodeIds_ == normalized && activeNodeId_ == nextActiveNodeId) return;

    const auto previousMode = inspectorMode();
    const auto activeChanged = activeNodeId_ != nextActiveNodeId;
    selectedNodeIds_ = normalized;
    activeNodeId_ = nextActiveNodeId;
    if (activeChanged) emit activeNodeIdChanged();
    emit selectionChanged();
    if (previousMode != inspectorMode()) {
        emit inspectorModeChanged();
    }
    applyActiveGeometryToMeasurement();
    emit componentPropertiesChanged();
}

void ApplicationController::openFile(const QUrl& file)
{
    platform::startupMark("openFile");
    setErrorMessage({});
    if (!file.isLocalFile() || !QFileInfo::exists(file.toLocalFile())) {
        setErrorMessage(tr("The selected file does not exist or is not a local file."));
        documentState_ = DocumentState::Invalid;
        emit documentStateChanged();
        return;
    }
    const QFileInfo sourceFile(file.toLocalFile());
    const auto suffix = sourceFile.suffix().toLower();
    if (suffix != QStringLiteral("step") && suffix != QStringLiteral("stp")) {
        setErrorMessage(tr("Loupe can open STEP files with a .step or .stp extension."));
        documentState_ = DocumentState::Invalid;
        emit documentStateChanged();
        return;
    }
    viewerPresentation_ = ViewerPresentation::Full;
    sectionController_.setEnabled(false);
    measurementController_.clearPicks();
    if (workerProcess_->state() != QProcess::NotRunning) {
        stoppingWorker_ = true;
        workerProcess_->kill();
        workerProcess_->waitForFinished(1'000);
        stoppingWorker_ = false;
    }
    setErrorMessage({});
    setActiveNodeId({});
    assemblyTreeModel_.replaceSnapshot({});
    pendingPath_ = sourceFile.absoluteFilePath();
    // Opening time starts now, as the user sees it.
    openClock_.start();
    openViewMs_ = openPreviewMs_ = openFullMs_ = -1;
    openSizeBytes_ = sourceFile.size();
    openRecorded_ = false;
    emit openTimingChanged();
    if (documentPath_ != pendingPath_) {
        documentPath_ = pendingPath_;
        emit documentNameChanged();
    }
    // This file's saved views, as they were left.
    savedViews_.setDocument(documentPath_);
    pendingSource_ = cache::SourceFingerprint::fromFile(pendingPath_);
    if (!pendingSource_) {
        setErrorMessage(tr("Loupe could not read the selected file."));
        documentState_ = DocumentState::Invalid;
        emit documentStateChanged();
        return;
    }
    pendingUnitOverride_.reset();
    if (overrideStore_) {
        if (const auto stored = overrideStore_->get(*pendingSource_); stored && (stored->unit == QStringLiteral("mm") || stored->unit == QStringLiteral("in"))) {
            pendingUnitOverride_ = stored->unit;
        }
    }
    snapshotJson_.clear();
    exportWorkspaceController_.reset();
    drawingWorkspaceController_.reset();
    // Output goes beside the file by default ("<name> export", "<name> drawings"), or under
    // Documents/Loupe when the file's folder cannot be written to (a read-only share).
    {
        const QFileInfo sourceFolder(sourceFile.absolutePath());
        const auto root = sourceFolder.isWritable()
            ? sourceFile.absolutePath()
            : QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(QStringLiteral("Loupe"));
        exportWorkspaceController_.setDefaultDestination(
            QDir(root).filePath(tr("%1 export").arg(sourceFile.completeBaseName())));
        drawingWorkspaceController_.setDefaultDestination(
            QDir(root).filePath(tr("%1 drawings").arg(sourceFile.completeBaseName())));
    }
    geometryCacheArchive_.clear();
    geometryReplayTimer_.stop();
    geometryReplayQueue_.clear();
    geometryReplayIndex_ = 0;
    ++geometryReplayGeneration_;
    meshRevisionBySegment_.clear();
    sharedMeshPayloads_.clear();
    sharedMeshRevisionByKey_.clear();
    sharedEdgesRevisionByKey_.clear();
    edgeRevisionByNode_.clear();
    cachedGeometryLoaded_ = false;
    geometryByNode_.clear();
    parentByNode_.clear();
    definitionByNode_.clear();
    visibilityModel_.setNodes({});
    opacityByNode_.clear();
    transparentWithoutEdges_.clear();
    unselectableNodes_.clear();
    measurementController_.clearPinned();
    materialByNode_.clear();
    appearanceByNode_.clear();
    importedColorByNode_.clear();
    segmentColorsByNode_.clear();
    explodeController_.clear();
    importedFaceColors_.clear();
    importedUncolouredFace_ = false;
    fileColorsUniform_ = false;
    partColorKeys_.clear();
    partColorPositionByKey_.clear();
    autoPartColorByKey_.clear();
    displayNameByNode_.clear();
    persistedAppearanceByKey_.clear();
    modelExtentMm_ = 0.0;
    effectiveUnit_ = QStringLiteral("mm");
    connectionAttempts_ = 0;
    activeRequestId_ = 0;
    activeExportRequestId_ = 0;
    activeAssemblyExportRequestId_ = 0;
    assemblyExportMessage_.clear();
    cacheHit_ = false;
    serverName_ = worker::newWorkerServerName();
    adoptedRequestId_ = 0;
    documentState_ = DocumentState::Opening;
    resetImportProgress();
    emit snapshotChanged();
    emit componentPropertiesChanged();
    emit modelExtentChanged();
    emit effectiveUnitChanged();
    emit cacheHitChanged();
    if (cacheStore_ && !pendingUnitOverride_) {
        if (const auto cached = cacheStore_->readSnapshotForSource(*pendingSource_, kImporterVersion, kMeshProfile)) {
            applySnapshotToTree(*cached);
            snapshotJson_ = QString::fromUtf8(*cached);
            exportWorkspaceController_.replaceSnapshot(snapshotJson_);
            drawingWorkspaceController_.replaceSnapshot(snapshotJson_);
            documentState_ = DocumentState::TreeReady;
            cacheHit_ = true;
            emit snapshotChanged();
            emit componentPropertiesChanged();
            emit documentStateChanged();
            emit cacheHitChanged();
        }
        if (const auto cachedGeometry = cacheStore_->readGeometryForSource(*pendingSource_, kImporterVersion, kGeometryCacheProfile)) {
            geometryCacheArchive_ = *cachedGeometry;
            cachedGeometryLoaded_ = true;
            replayCachedGeometry(geometryCacheArchive_);
        }
    }
    // The importer started at launch on this very file, when it is the one Loupe was opened
    // with: it has been importing since then, so it is connected to rather than replaced.
    // Not with a remembered unit, which it was not started with.
    if (auto prelaunched = worker::takePrelaunchedWorker(pendingPath_)) {
        if (!pendingUnitOverride_ && prelaunched->executable == workerExecutable_) {
            useWorkerProcess(std::move(prelaunched->process));
            serverName_ = prelaunched->serverName;
            adoptedRequestId_ = prelaunched->requestId;
            emit documentStateChanged();
            setImportProgress(tr("Starting STEP importer"), 0.005);
            connectWorker();
            return;
        }
        prelaunched->process->kill();
        prelaunched->process->waitForFinished(1'000);
    }
    emit documentStateChanged();
    auto launchWorker = workerExecutable_;
#ifdef Q_OS_MACOS
    const auto applicationDirectory = QDir(QCoreApplication::applicationDirPath()).canonicalPath();
    const auto workerDirectory = QFileInfo(workerExecutable_).absoluteDir().canonicalPath();
    const auto workerIsBundled = !applicationDirectory.isEmpty() && workerDirectory == applicationDirectory;
    if (workerIsBundled && stagedWorkerPath_.isEmpty()) {
        stagedWorkerPath_ = QStringLiteral("/tmp/loupe-worker-%1-%2")
                                .arg(QCoreApplication::applicationPid())
                                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
        if (!QFile::copy(workerExecutable_, stagedWorkerPath_)
            || !QFile::setPermissions(stagedWorkerPath_, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                                                         | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                                                         | QFileDevice::ReadOther | QFileDevice::ExeOther)) {
            QFile::remove(stagedWorkerPath_);
            stagedWorkerPath_.clear();
            setErrorMessage(tr("Could not stage the STEP importer for launch."));
            documentState_ = DocumentState::WorkerFailed;
            emit documentStateChanged();
            return;
        }
    }
    if (workerIsBundled) launchWorker = stagedWorkerPath_;
#endif
    platform::startupMark("worker launching");
    workerProcess_->start(launchWorker, {QStringLiteral("--server-name"), serverName_});
}

void ApplicationController::cancelImport()
{
    if (!importInProgress_) return;
    preservePreviewOnCancel_ = documentState_ == DocumentState::TreeReady && previewGeometryReceived_;
    if (activeRequestId_ != 0) workerClient_.cancel(activeRequestId_);
}

void ApplicationController::fitView()
{
    emit fitRequested();
}

void ApplicationController::viewNormalToFace()
{
    emit viewNormalToFaceRequested();
}

void ApplicationController::requestExplodedExport()
{
    emit explodedExportRequested();
}

void ApplicationController::isolateActiveNode()
{
    const auto selected = geometryNodesForSelection();
    // Selection can arrive just before geometry (for example while an import
    // is streaming). Keep the requested isolate presentation state so it is
    // applied consistently once the visibility model has nodes.
    if (!selected.isEmpty()) visibilityModel_.isolate(selected);
    else if (selectedNodeIds_.isEmpty()) return;
    viewerPresentation_ = ViewerPresentation::Isolate;
    emit viewerPresentationChanged();
}

void ApplicationController::toggleIsolateActiveNode()
{
    if (viewerPresentation_ == ViewerPresentation::Isolate && visibilityModel_.canRestore()) {
        restoreVisibility();
        return;
    }
    isolateActiveNode();
}

void ApplicationController::hideActiveNode()
{
    const auto selected = geometryNodesForSelection();
    if (selected.isEmpty()) return;
    visibilityModel_.hide(selected);
}

void ApplicationController::hideOtherNodes()
{
    const auto selected = geometryNodesForSelection();
    if (!selected.isEmpty()) visibilityModel_.isolate(selected);
    else if (selectedNodeIds_.isEmpty()) return;
    viewerPresentation_ = ViewerPresentation::Isolate;
    emit viewerPresentationChanged();
}

void ApplicationController::showAllNodes()
{
    visibilityModel_.showAll();
    viewerPresentation_ = ViewerPresentation::Full;
    emit viewerPresentationChanged();
}

void ApplicationController::restoreVisibility()
{
    visibilityModel_.restorePrevious();
    viewerPresentation_ = ViewerPresentation::Full;
    emit viewerPresentationChanged();
}

void ApplicationController::restoreFullAssembly()
{
    if (visibilityModel_.canRestore()) {
        restoreVisibility();
    } else {
        showAllNodes();
    }
}

bool ApplicationController::isNodeVisible(const QString& nodeId) const
{
    return visibilityModel_.isVisible(nodeId);
}

void ApplicationController::toggleTransparentSelection()
{
    const auto selected = geometryNodesForSelection();
    if (selected.isEmpty()) return;
    const bool allTransparent = std::ranges::all_of(selected, [this](const QString& nodeId) { return visibilityModel_.isGhosted(nodeId); });
    visibilityModel_.setGhosted(selected, !allTransparent);
}

void ApplicationController::makeOthersTransparent()
{
    const auto selected = geometryNodesForSelection();
    if (selected.isEmpty()) return;
    visibilityModel_.ghostComplements(selected);
    viewerPresentation_ = ViewerPresentation::Ghost;
    emit viewerPresentationChanged();
}

bool ApplicationController::isNodeTransparent(const QString& nodeId) const
{
    return visibilityModel_.isGhosted(nodeId);
}

bool ApplicationController::isSelectionTransparent() const
{
    const auto selected = geometryNodesForSelection();
    return !selected.isEmpty()
        && std::ranges::all_of(selected, [this](const QString& nodeId) { return visibilityModel_.isGhosted(nodeId); });
}

int ApplicationController::nodeTransparencyState(const QString& nodeId) const
{
    const auto parts = geometryNodesForAction(nodeId);
    if (parts.isEmpty()) return 0;
    const auto transparent = std::count_if(parts.cbegin(), parts.cend(),
                                           [this](const QString& part) { return visibilityModel_.isGhosted(part); });
    return transparent == 0 ? 0 : transparent == parts.size() ? 2 : 1;
}

void ApplicationController::setNodeTransparent(const QString& nodeId, const bool transparent)
{
    const auto parts = geometryNodesForAction(nodeId);
    if (!parts.isEmpty()) visibilityModel_.setGhosted(parts, transparent);
}

bool ApplicationController::nodeShowsTransparentEdges(const QString& nodeId) const
{
    return !transparentWithoutEdges_.contains(nodeId);
}

bool ApplicationController::selectionShowsTransparentEdges() const
{
    const auto selected = geometryNodesForSelection();
    return std::ranges::none_of(selected, [this](const QString& nodeId) { return transparentWithoutEdges_.contains(nodeId); });
}

void ApplicationController::setSelectionTransparentEdges(const bool shown)
{
    const auto selected = geometryNodesForSelection();
    if (selected.isEmpty()) return;
    for (const auto& nodeId : selected) {
        if (shown) transparentWithoutEdges_.remove(nodeId);
        else transparentWithoutEdges_.insert(nodeId);
    }
    // Nothing in the visibility model changed: say so here for the viewport to redraw.
    ++visibilityRevision_;
    emit visibilityChanged();
}

bool ApplicationController::isNodeSelectable(const QString& nodeId) const
{
    return !unselectableNodes_.contains(nodeId);
}

bool ApplicationController::selectionSelectable() const
{
    const auto selected = geometryNodesForSelection();
    return std::ranges::none_of(selected, [this](const QString& nodeId) { return unselectableNodes_.contains(nodeId); });
}

void ApplicationController::setSelectionSelectable(const bool selectable)
{
    const auto selected = geometryNodesForSelection();
    if (selected.isEmpty()) return;
    for (const auto& nodeId : selected) {
        if (selectable) unselectableNodes_.remove(nodeId);
        else unselectableNodes_.insert(nodeId);
    }
    ++visibilityRevision_;
    emit visibilityChanged();
}

void ApplicationController::markOpenMilestone(qint64& milestone)
{
    if (!openClock_.isValid() || milestone >= 0) return;
    milestone = openClock_.elapsed();
    emit openTimingChanged();
}

QVariantMap ApplicationController::openTiming() const
{
    return {{QStringLiteral("viewMs"), openViewMs_},
            {QStringLiteral("previewMs"), openPreviewMs_},
            {QStringLiteral("fullMs"), openFullMs_},
            {QStringLiteral("cached"), cachedGeometryLoaded_},
            {QStringLiteral("opening"), openClock_.isValid() && openFullMs_ < 0}};
}

void ApplicationController::recordOpen()
{
    if (openRecorded_ || openFullMs_ < 0) return;
    openRecorded_ = true;
    // Every part shown can only come after the first and before fully detailed.
    if (openPreviewMs_ >= 0) openPreviewMs_ = std::clamp(openPreviewMs_, std::max<qint64>(0, openViewMs_), openFullMs_);
    models::OpenHistory::shared().append({{QStringLiteral("file"), QFileInfo(documentPath_).fileName()},
                                          {QStringLiteral("path"), QDir::toNativeSeparators(documentPath_)},
                                          {QStringLiteral("sizeBytes"), openSizeBytes_},
                                          {QStringLiteral("parts"), static_cast<int>(geometryByNode_.size())},
                                          {QStringLiteral("cached"), cachedGeometryLoaded_},
                                          {QStringLiteral("viewMs"), openViewMs_},
                                          {QStringLiteral("previewMs"), openPreviewMs_},
                                          {QStringLiteral("fullMs"), openFullMs_},
                                          {QStringLiteral("openedAt"), QDateTime::currentDateTime().toString(Qt::ISODate)},
                                          {QStringLiteral("version"), applicationVersion()}});
    emit openTimingChanged();
}

QString ApplicationController::saveDisplayState(const QString& name)
{
    if (geometryByNode_.isEmpty()) return {};
    QJsonArray hidden;
    QJsonArray transparent;
    QJsonObject opacity;
    QJsonArray withoutEdges;
    int hiddenCount = 0;
    int transparentCount = 0;
    for (auto it = geometryByNode_.cbegin(); it != geometryByNode_.cend(); ++it) {
        const auto& nodeId = it.key();
        if (!visibilityModel_.isVisible(nodeId)) {
            hidden.append(nodeId);
            ++hiddenCount;
        } else if (visibilityModel_.isGhosted(nodeId)) {
            transparent.append(nodeId);
            ++transparentCount;
            if (opacityByNode_.contains(nodeId)) opacity.insert(nodeId, opacityByNode_.value(nodeId));
            if (transparentWithoutEdges_.contains(nodeId)) withoutEdges.append(nodeId);
        }
    }
    QStringList parts;
    if (viewerPresentation_ == ViewerPresentation::Isolate) parts << tr("Isolated");
    if (hiddenCount > 0) parts << tr("%1 hidden").arg(hiddenCount);
    if (transparentCount > 0) parts << tr("%1 transparent").arg(transparentCount);
    const auto summary = parts.isEmpty() ? tr("Everything shown") : parts.join(QStringLiteral(" · "));
    return savedViews_.add(QStringLiteral("display"), name, summary,
                           {{QStringLiteral("hidden"), hidden.toVariantList()},
                            {QStringLiteral("transparent"), transparent.toVariantList()},
                            {QStringLiteral("opacity"), opacity.toVariantMap()},
                            {QStringLiteral("withoutEdges"), withoutEdges.toVariantList()},
                            {QStringLiteral("presentation"), static_cast<int>(viewerPresentation_)}});
}

QString ApplicationController::saveExplodeLayout(const QString& name)
{
    if (!explodeController_.exploded()) return {};
    return savedViews_.add(QStringLiteral("layout"), name, explodeController_.layoutSummary(), explodeController_.layoutSnapshot());
}

bool ApplicationController::applyExplodeLayout(const QString& id)
{
    const auto snapshot = savedViews_.data(id);
    return !snapshot.isEmpty() && explodeController_.restoreLayout(snapshot);
}

bool ApplicationController::updateExplodeLayout(const QString& id)
{
    if (!explodeController_.exploded()) return false;
    return savedViews_.replace(id, explodeController_.layoutSummary(), explodeController_.layoutSnapshot());
}

QString ApplicationController::currentExplodeLayoutId() const
{
    if (!explodeController_.exploded() || explodeController_.showingAssembly()) return {};
    for (const auto& value : savedViews_.explodeLayouts()) {
        const auto id = value.toMap().value(QStringLiteral("id")).toString();
        if (explodeController_.layoutMatches(savedViews_.data(id))) return id;
    }
    return {};
}

bool ApplicationController::applyDisplayState(const QString& id)
{
    const auto state = savedViews_.data(id);
    if (state.isEmpty()) return false;
    QSet<QString> hidden;
    QSet<QString> transparent;
    for (const auto& value : state.value(QStringLiteral("hidden")).toList()) hidden.insert(value.toString());
    for (const auto& value : state.value(QStringLiteral("transparent")).toList()) transparent.insert(value.toString());
    // Each transparent part's own opacity and edges, as they were saved.
    const auto opacity = state.value(QStringLiteral("opacity")).toMap();
    for (const auto& nodeId : transparent) {
        if (opacity.contains(nodeId)) opacityByNode_.insert(nodeId, std::clamp(opacity.value(nodeId).toDouble(), 0.05, 0.995));
        else opacityByNode_.remove(nodeId);
        transparentWithoutEdges_.remove(nodeId);
    }
    for (const auto& value : state.value(QStringLiteral("withoutEdges")).toList()) transparentWithoutEdges_.insert(value.toString());
    visibilityModel_.applyState(hidden, transparent);
    const auto presentation = state.value(QStringLiteral("presentation")).toInt();
    viewerPresentation_ = presentation == static_cast<int>(ViewerPresentation::Isolate) ? ViewerPresentation::Isolate
                        : presentation == static_cast<int>(ViewerPresentation::Ghost) ? ViewerPresentation::Ghost
                                                                                         : ViewerPresentation::Full;
    emit viewerPresentationChanged();
    // Opacity and edges may have changed with nothing else: redraw either way.
    ++visibilityRevision_;
    emit visibilityChanged();
    return true;
}

void ApplicationController::resetDisplayState()
{
    showAllNodes();
}

QString ApplicationController::saveCameraView(const QString& name, const QVariantMap& camera, const QString& summary)
{
    if (camera.isEmpty()) return {};
    return savedViews_.add(QStringLiteral("camera"), name, summary, camera);
}

QString ApplicationController::assemblyExportStage() const
{
    if (assemblyExportStageKey_ == QStringLiteral("translate")) return tr("Translating to STEP");
    if (assemblyExportStageKey_ == QStringLiteral("write")) return tr("Writing the file");
    if (assemblyExportStageKey_ == QStringLiteral("mesh")) return tr("Meshing the parts");
    if (assemblyExportStageKey_ == QStringLiteral("draw")) return tr("Drawing the view");
    return tr("Preparing the assembly");
}

double ApplicationController::nodeOpacity(const QString& nodeId) const
{
    return visibilityModel_.isGhosted(nodeId) ? opacityByNode_.value(nodeId, kDefaultTransparentOpacity) : 1.0;
}

double ApplicationController::selectionOpacity() const
{
    for (const auto& nodeId : geometryNodesForSelection())
        if (visibilityModel_.isGhosted(nodeId)) return nodeOpacity(nodeId);
    return 1.0;
}

void ApplicationController::setSelectionOpacity(const double opacity)
{
    const auto selected = geometryNodesForSelection();
    if (selected.isEmpty() || !std::isfinite(opacity)) return;
    // Never fully clear: a part at 0 would be invisible yet still there, which is what
    // Hide is for.
    const double value = std::clamp(opacity, 0.05, 1.0);
    const bool opaque = value > 0.995;
    for (const auto& nodeId : selected) {
        // Opaque again forgets the setting, so the next Make transparent starts at the default.
        if (opaque) opacityByNode_.remove(nodeId);
        else opacityByNode_.insert(nodeId, value);
    }
    const bool changesState = std::ranges::any_of(selected, [&](const QString& nodeId) {
        return visibilityModel_.isGhosted(nodeId) == opaque;
    });
    if (changesState) {
        visibilityModel_.setGhosted(selected, !opaque);
    } else {
        // Same parts transparent, new opacity: nothing in the visibility model changed, so
        // say so here for the viewport to redraw them.
        ++visibilityRevision_;
        emit visibilityChanged();
    }
}

void ApplicationController::connectWorker()
{
    if (workerClient_.connectToServer(serverName_, 100)) {
        platform::startupMark("worker connected");
        // An importer started at launch is already importing the file, as this request.
        activeRequestId_ = adoptedRequestId_ != 0 ? workerClient_.adoptRequest(std::exchange(adoptedRequestId_, 0))
                                                  : workerClient_.openFile(pendingPath_, pendingUnitOverride_);
        return;
    }
    if (++connectionAttempts_ >= kWorkerConnectionAttempts) {
        setErrorMessage(tr("The STEP importer started but did not become ready."));
        documentState_ = DocumentState::WorkerFailed;
        emit documentStateChanged();
        return;
    }
    QTimer::singleShot(25, this, &ApplicationController::connectWorker);
}

void ApplicationController::setErrorMessage(const QString& errorMessage)
{
    if (errorMessage_ == errorMessage) {
        return;
    }
    errorMessage_ = errorMessage;
    emit errorMessageChanged();
}

void ApplicationController::resetImportProgress()
{
    importStage_ = tr("Preparing import");
    importProgress_ = 0.0;
    importElapsedSeconds_ = 0;
    importInProgress_ = true;
    preservePreviewOnCancel_ = false;
    previewGeometryReceived_ = false;
    importElapsed_.restart();
    importRemaining_.reset();
    importProgressTimer_.start();
    emit importProgressChanged();
}

void ApplicationController::setImportProgress(const QString& stage, const double fraction)
{
    importStage_ = stage;
    importProgress_ = std::max(importProgress_, std::clamp(fraction, 0.0, 1.0));
    importElapsedSeconds_ = importElapsed_.elapsed() / 1'000;
    importRemaining_.report(importElapsed_.elapsed(), importProgress_);
    if (importProgress_ >= 1.0) {
        importInProgress_ = false;
        importProgressTimer_.stop();
    }
    emit importProgressChanged();
}

void ApplicationController::appendCachedMesh(const QString& nodeId, const QString& segmentKey, const QString& sourceColor, const QByteArray& payload)
{
    if (geometryCacheArchive_.isEmpty()) geometryCacheArchive_.append(kGeometryCacheMagic);
    QDataStream stream(&geometryCacheArchive_, QIODevice::Append);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << quint8{1} << nodeId << segmentKey << sourceColor << payload;
}

void ApplicationController::replayGeometry()
{
    replayCachedGeometry(geometryCacheArchive_);
}

void ApplicationController::appendCachedEdges(const QString& nodeId, const QByteArray& payload)
{
    if (geometryCacheArchive_.isEmpty()) geometryCacheArchive_.append(kGeometryCacheMagic);
    QDataStream stream(&geometryCacheArchive_, QIODevice::Append);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << quint8{2} << nodeId << payload;
}

void ApplicationController::appendCachedShared(const quint8 kind, const QString& sourceKey, const QString& sourceColor,
                                               const QByteArray& payload)
{
    if (geometryCacheArchive_.isEmpty()) geometryCacheArchive_.append(kGeometryCacheMagic);
    QDataStream stream(&geometryCacheArchive_, QIODevice::Append);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << kind << sourceKey << sourceColor << payload;
}

void ApplicationController::appendCachedInstance(const quint8 kind, const QString& nodeId, const QString& segmentKey,
                                                 const QString& sourceKey, const QString& sourceColor,
                                                 const QVector<float>& placement)
{
    if (geometryCacheArchive_.isEmpty()) geometryCacheArchive_.append(kGeometryCacheMagic);
    QDataStream stream(&geometryCacheArchive_, QIODevice::Append);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << kind << nodeId << segmentKey << sourceKey << sourceColor << placement;
}

namespace {

// A rigid placement, row-major 3x4 [R | t], as the position and rotation a node takes.
std::pair<QVector3D, QQuaternion> nodeTransform(const QVector<float>& placement)
{
    if (placement.size() != 12) return {QVector3D{}, QQuaternion{}};
    QMatrix3x3 rotation;
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column) rotation(row, column) = placement.at(row * 4 + column);
    return {QVector3D{placement.at(3), placement.at(7), placement.at(11)}, QQuaternion::fromRotationMatrix(rotation).normalized()};
}

} // namespace

void ApplicationController::handleSharedMesh(const QString& sourceKey, const QString& sourceColor, const QByteArray& payload,
                                             const bool fromCache)
{
    if (!fromCache) appendCachedShared(3, sourceKey, sourceColor, payload);
    sharedMeshPayloads_.insert(sourceKey, payload);
    // A replay re-sends what the viewports were already given, so it keeps the revision.
    if (!fromCache || !sharedMeshRevisionByKey_.contains(sourceKey))
        sharedMeshRevisionByKey_.insert(sourceKey, ++geometryRevisionCounter_);
    emit sharedMeshReady(sourceKey, sourceColor, payload);
}

void ApplicationController::handleSharedEdges(const QString& sourceKey, const QByteArray& payload, const bool fromCache)
{
    if (!fromCache) appendCachedShared(4, sourceKey, {}, payload);
    if (!fromCache || !sharedEdgesRevisionByKey_.contains(sourceKey))
        sharedEdgesRevisionByKey_.insert(sourceKey, ++geometryRevisionCounter_);
    emit sharedEdgesReady(sourceKey, payload);
}

void ApplicationController::handleMeshInstance(const QString& nodeId, const QString& segmentKey, const QString& sourceKey,
                                               const QString& sourceColor, const QVector<float>& placement, const bool fromCache)
{
    const auto payload = sharedMeshPayloads_.value(sourceKey);
    if (payload.isEmpty() || placement.size() != 12) return;
    if (!fromCache) appendCachedInstance(5, nodeId, segmentKey, sourceKey, sourceColor, placement);
    if (!previewGeometryReceived_) {
        previewGeometryReceived_ = true;
        if (!fromCache) emit importProgressChanged();
    }
    markOpenMilestone(openViewMs_);
    noteImportedColor(nodeId, sourceColor);
    if (noteSegmentColor(nodeId, segmentKey, sourceColor, payload) && !fromCache && isNodeSelected(nodeId))
        emit componentPropertiesChanged();
    notePartBounds(nodeId, payload, placement);
    if (!fromCache || !meshRevisionBySegment_.contains(segmentKey))
        meshRevisionBySegment_.insert(segmentKey, ++geometryRevisionCounter_);
    const auto [position, rotation] = nodeTransform(placement);
    emit meshInstanceReady(nodeId, segmentKey, sourceKey, sourceColor, position, rotation);
}

void ApplicationController::handleEdgeInstance(const QString& nodeId, const QString& sourceKey, const QVector<float>& placement,
                                               const bool fromCache)
{
    if (placement.size() != 12) return;
    if (!fromCache) appendCachedInstance(6, nodeId, {}, sourceKey, {}, placement);
    if (!fromCache || !edgeRevisionByNode_.contains(nodeId)) edgeRevisionByNode_.insert(nodeId, ++geometryRevisionCounter_);
    const auto [position, rotation] = nodeTransform(placement);
    emit edgeInstanceReady(nodeId, sourceKey, position, rotation);
}

void ApplicationController::replayCachedGeometry(const QByteArray& archive)
{
    geometryReplayTimer_.stop();
    geometryReplayQueue_.clear();
    geometryReplayIndex_ = 0;
    const auto generation = ++geometryReplayGeneration_;
    if (!archive.startsWith(kGeometryCacheMagic) || archive.size() > 512LL * 1024LL * 1024LL) return;

    auto* watcher = new QFutureWatcher<std::optional<QVector<GeometryReplayEntry>>>(this);
    connect(watcher, &QFutureWatcher<std::optional<QVector<GeometryReplayEntry>>>::finished, this,
            [this, watcher, generation] {
        auto entries = watcher->result();
        watcher->deleteLater();
        if (generation != geometryReplayGeneration_ || !entries) return;
        geometryReplayQueue_ = std::move(*entries);
        geometryReplayIndex_ = 0;
        if (!geometryReplayQueue_.isEmpty()) geometryReplayTimer_.start();
    });
    watcher->setFuture(QtConcurrent::run([archive] { return decodeGeometryArchive(archive); }));
}

std::optional<QVector<ApplicationController::GeometryReplayEntry>> ApplicationController::decodeGeometryArchive(
    const QByteArray& archive)
{
    QBuffer buffer;
    buffer.setData(archive.sliced(4));
    if (!buffer.open(QIODevice::ReadOnly)) return std::nullopt;
    QDataStream stream(&buffer);
    stream.setVersion(QDataStream::Qt_6_0);
    QVector<GeometryReplayEntry> entries;
    QHash<QString, qsizetype> meshIndexes;
    QHash<QString, qsizetype> edgeIndexes;
    QHash<QString, qsizetype> sharedMeshIndexes;
    QHash<QString, qsizetype> sharedEdgeIndexes;
    // A later entry for the same key is the refined pass: it takes the earlier one's place,
    // which keeps a shared mesh ahead of the copies drawn from it.
    const auto place = [&entries](QHash<QString, qsizetype>& indexes, const QString& key, GeometryReplayEntry entry) {
        if (const auto existing = indexes.constFind(key); existing != indexes.cend()) {
            entries[*existing] = std::move(entry);
        } else {
            indexes.insert(key, entries.size());
            entries.append(std::move(entry));
        }
    };
    while (!stream.atEnd()) {
        quint8 kind{};
        QString nodeId;
        stream >> kind >> nodeId;
        if (kind == 3 || kind == 4) {
            // nodeId is the shared key.
            QString sourceColor;
            QByteArray payload;
            stream >> sourceColor >> payload;
            if (stream.status() != QDataStream::Ok || nodeId.isEmpty() || payload.isEmpty()) return std::nullopt;
            GeometryReplayEntry entry{kind, {}, {}, sourceColor, payload, nodeId, {}};
            place(kind == 3 ? sharedMeshIndexes : sharedEdgeIndexes, nodeId, std::move(entry));
            continue;
        }
        if (kind == 5 || kind == 6) {
            QString segmentKey, sourceKey, sourceColor;
            QVector<float> placement;
            stream >> segmentKey >> sourceKey >> sourceColor >> placement;
            if (stream.status() != QDataStream::Ok || nodeId.isEmpty() || sourceKey.isEmpty() || placement.size() != 12
                || (kind == 5 && segmentKey.isEmpty())) return std::nullopt;
            GeometryReplayEntry entry{kind, nodeId, segmentKey, sourceColor, {}, sourceKey, placement};
            if (kind == 5) place(meshIndexes, segmentKey, std::move(entry));
            else place(edgeIndexes, nodeId, std::move(entry));
            continue;
        }
        if (kind == 1) {
            QString segmentKey, sourceColor;
            QByteArray payload;
            stream >> segmentKey >> sourceColor >> payload;
            if (stream.status() != QDataStream::Ok || nodeId.isEmpty() || segmentKey.isEmpty() || payload.isEmpty()) return std::nullopt;
            GeometryReplayEntry entry{kind, nodeId, segmentKey, sourceColor, payload, {}, {}};
            if (const auto existing = meshIndexes.constFind(segmentKey); existing != meshIndexes.cend()) {
                entries[*existing] = std::move(entry);
            } else {
                meshIndexes.insert(segmentKey, entries.size());
                entries.append(std::move(entry));
            }
        } else if (kind == 2) {
            QByteArray payload;
            stream >> payload;
            if (stream.status() != QDataStream::Ok || nodeId.isEmpty() || payload.isEmpty()) return std::nullopt;
            GeometryReplayEntry entry{kind, nodeId, {}, {}, payload, {}, {}};
            if (const auto existing = edgeIndexes.constFind(nodeId); existing != edgeIndexes.cend()) {
                entries[*existing] = std::move(entry);
            } else {
                edgeIndexes.insert(nodeId, entries.size());
                entries.append(std::move(entry));
            }
        } else {
            return std::nullopt;
        }
    }
    return entries;
}

void ApplicationController::replayGeometryChunk()
{
    // As many entries as fit in a slice of one frame, rather than one per event-loop
    // turn: a large assembly has thousands of segments, and one at a time left the
    // replay paced by the event loop instead of by the work. The budget still returns
    // to the loop often enough that the UI keeps painting and accepting input.
    constexpr qint64 kReplaySliceMs = 8;
    QElapsedTimer slice;
    slice.start();
    const auto generation = geometryReplayGeneration_;
    do {
        if (geometryReplayIndex_ >= geometryReplayQueue_.size()) {
            geometryReplayQueue_.clear();
            geometryReplayIndex_ = 0;
            if (previewGeometryReceived_) emit importProgressChanged();
            // From the cache, every part is on screen once its geometry is replayed.
            if (cachedGeometryLoaded_) markOpenMilestone(openPreviewMs_);
            return;
        }
        auto entry = std::move(geometryReplayQueue_[geometryReplayIndex_++]);
        if (entry.kind == 1) {
            previewGeometryReceived_ = true;
            markOpenMilestone(openViewMs_);
            noteImportedColor(entry.nodeId, entry.sourceColor);
            static_cast<void>(noteSegmentColor(entry.nodeId, entry.segmentKey, entry.sourceColor, entry.payload));
            notePartBounds(entry.nodeId, entry.payload);
            // A replay re-sends what the viewports were already given, so it keeps the
            // revision; only geometry that arrived from the cache alone gets a new one.
            if (!meshRevisionBySegment_.contains(entry.segmentKey))
                meshRevisionBySegment_.insert(entry.segmentKey, ++geometryRevisionCounter_);
            emit meshReady(entry.nodeId, entry.segmentKey, entry.sourceColor, entry.payload);
        } else if (entry.kind == 2) {
            if (!edgeRevisionByNode_.contains(entry.nodeId))
                edgeRevisionByNode_.insert(entry.nodeId, ++geometryRevisionCounter_);
            emit edgeReady(entry.nodeId, entry.payload);
        } else if (entry.kind == 3) {
            handleSharedMesh(entry.sourceKey, entry.sourceColor, entry.payload, true);
        } else if (entry.kind == 4) {
            handleSharedEdges(entry.sourceKey, entry.payload, true);
        } else if (entry.kind == 5) {
            handleMeshInstance(entry.nodeId, entry.segmentKey, entry.sourceKey, entry.sourceColor, entry.placement, true);
        } else if (entry.kind == 6) {
            handleEdgeInstance(entry.nodeId, entry.sourceKey, entry.placement, true);
        }
        // A handler may have opened another file or restarted the replay.
        if (generation != geometryReplayGeneration_) return;
    } while (slice.elapsed() < kReplaySliceMs);
    geometryReplayTimer_.start();
}

void ApplicationController::saveGeometryCache()
{
    if (!cacheStore_ || !pendingSource_ || geometryCacheArchive_.size() <= 4) return;
    const cache::UnitDecision unit{effectiveUnit_, 1.0};
    const auto key = cache::CacheKey::from(*pendingSource_, kImporterVersion, kGeometryCacheProfile, unit);
    static_cast<void>(cacheStore_->put(key, geometryCacheArchive_, {*pendingSource_, kImporterVersion, kGeometryCacheProfile, unit}));
}

int ApplicationController::nodeVisibilityState(const QString& nodeId) const
{
    const auto parts = geometryNodesForAction(nodeId);
    if (parts.isEmpty()) return 0;
    const auto hidden = std::count_if(parts.cbegin(), parts.cend(),
                                      [this](const QString& part) { return !visibilityModel_.isVisible(part); });
    return hidden == 0 ? 0 : hidden == parts.size() ? 2 : 1;
}

void ApplicationController::setNodeHidden(const QString& nodeId, const bool hidden)
{
    const auto parts = geometryNodesForAction(nodeId);
    if (parts.isEmpty()) return;
    if (hidden) visibilityModel_.hide(parts);
    else visibilityModel_.show(parts);
}

int ApplicationController::hiddenPartCount() const
{
    int hidden = 0;
    for (auto it = geometryByNode_.cbegin(); it != geometryByNode_.cend(); ++it)
        if (!visibilityModel_.isVisible(it.key())) ++hidden;
    return hidden;
}

QString ApplicationController::suggestedAssemblyExportPath() const
{
    if (documentPath_.isEmpty()) return {};
    const QFileInfo source(documentPath_);
    return QDir(exportWorkspaceController_.destination()).filePath(
        tr("%1 without hidden parts.step").arg(source.completeBaseName()));
}

QUrl ApplicationController::suggestedAssemblyExportUrl() const
{
    const auto path = suggestedAssemblyExportPath();
    return path.isEmpty() ? QUrl{} : QUrl::fromLocalFile(path);
}

// The folder holding the suggestion if it exists yet, else the STEP file's own folder.
QUrl ApplicationController::suggestedAssemblyExportFolderUrl() const
{
    const auto path = suggestedAssemblyExportPath();
    if (path.isEmpty()) return {};
    const auto folder = QFileInfo(path).absolutePath();
    return QUrl::fromLocalFile(QDir(folder).exists() ? folder : QFileInfo(documentPath_).absolutePath());
}

bool ApplicationController::exportAssemblyWithoutHiddenParts(const QUrl& destination)
{
    if (activeAssemblyExportRequestId_ != 0 || documentState_ != DocumentState::TreeReady || importInProgress_) return false;
    if (!destination.isLocalFile()) return false;
    QStringList hidden;
    for (auto it = geometryByNode_.cbegin(); it != geometryByNode_.cend(); ++it)
        if (!visibilityModel_.isVisible(it.key())) hidden.append(it.key());
    assemblyExportMessage_.clear();
    assemblyExportSucceeded_ = false;
    activeAssemblyExportExploded_ = false;
    assemblyExportStageKey_ = QStringLiteral("copy");
    assemblyExportProgress_ = 0.0;
    emit assemblyExportProgressChanged();
    activeAssemblyExportRequestId_ = workerClient_.exportAssembly(destination.toLocalFile(), hidden, effectiveUnit_);
    emit assemblyExportChanged();
    return true;
}

QString ApplicationController::explodedExportFormat(const QUrl& destination)
{
    const auto suffix = QFileInfo(destination.toLocalFile()).suffix().toLower();
    if (suffix == QStringLiteral("step") || suffix == QStringLiteral("stp")) return QStringLiteral("step");
    if (suffix == QStringLiteral("stl") || suffix == QStringLiteral("dxf") || suffix == QStringLiteral("svg")
        || suffix == QStringLiteral("pdf"))
        return suffix;
    return {};
}

bool ApplicationController::exportExplodedAssembly(const QUrl& destination, const QVector3D& viewDirection,
                                                   const QVector3D& upDirection)
{
    if (activeAssemblyExportRequestId_ != 0 || documentState_ != DocumentState::TreeReady || importInProgress_) return false;
    if (!destination.isLocalFile()) return false;
    const auto format = explodedExportFormat(destination);
    if (format.isEmpty()) {
        assemblyExportSucceeded_ = false;
        assemblyExportMessage_ = tr("Choose a .step, .stl, .dxf, .svg or .pdf file to export the exploded assembly.");
        emit assemblyExportChanged();
        return false;
    }
    protocol::ExportAssembly request;
    request.destination = destination.toLocalFile();
    for (auto it = geometryByNode_.cbegin(); it != geometryByNode_.cend(); ++it)
        if (!visibilityModel_.isVisible(it.key())) request.excludedNodeIds.append(it.key());
    request.outputUnit = effectiveUnit_;
    request.format = format;
    const auto& transforms = explodeController_.ownTransforms();
    for (auto it = transforms.cbegin(); it != transforms.cend(); ++it) {
        const auto rotation = QQuaternion::fromRotationMatrix(it.value().toGenericMatrix<3, 3>()).normalized();
        const auto translation = it.value().column(3).toVector3D();
        request.placements.append({it.key(),
                                   {rotation.x(), rotation.y(), rotation.z(), rotation.scalar()},
                                   {translation.x(), translation.y(), translation.z()}});
    }
    request.viewDirection = {viewDirection.x(), viewDirection.y(), viewDirection.z()};
    request.upDirection = {upDirection.x(), upDirection.y(), upDirection.z()};
    assemblyExportMessage_.clear();
    assemblyExportSucceeded_ = false;
    activeAssemblyExportExploded_ = true;
    assemblyExportStageKey_ = QStringLiteral("copy");
    assemblyExportProgress_ = 0.0;
    emit assemblyExportProgressChanged();
    activeAssemblyExportRequestId_ = workerClient_.exportAssembly(std::move(request));
    emit assemblyExportChanged();
    return true;
}

QString ApplicationController::documentName() const
{
    return documentPath_.isEmpty() ? QString{} : QFileInfo(documentPath_).fileName();
}

qint64 ApplicationController::importEtaSeconds() const noexcept
{
    if (!importInProgress_ || importProgress_ >= 0.995) return -1;
    return importRemaining_.remainingSeconds(importElapsed_.elapsed());
}

void ApplicationController::applySnapshotToTree(const QByteArray& snapshot)
{
    const auto document = QJsonDocument::fromJson(snapshot);
    if (!document.isObject()) return;
    const auto nodes = document.object().value(QStringLiteral("nodes")).toArray();
    const auto importedUnit = document.object().value(QStringLiteral("effectiveUnit")).toString();
    if (importedUnit == QStringLiteral("mm") || importedUnit == QStringLiteral("in")) {
        if (effectiveUnit_ != importedUnit) {
            effectiveUnit_ = importedUnit;
            emit effectiveUnitChanged();
        }
        measurementController_.setEffectiveUnit(effectiveUnit_);
    }
    geometryByNode_.clear();
    parentByNode_.clear();
    definitionByNode_.clear();
    displayNameByNode_.clear();
    double modelExtent = 0.0;
    QStringList geometryInTreeOrder;
    for (const auto& value : document.object().value(QStringLiteral("geometry")).toArray()) {
        const auto object = value.toObject();
        const auto nodeId = object.value(QStringLiteral("nodeId")).toString();
        if (!nodeId.isEmpty()) geometryInTreeOrder.append(nodeId);
        const auto bounds = object.value(QStringLiteral("boundsMm")).toObject();
        if (!nodeId.isEmpty()) geometryByNode_.insert(nodeId, {object.value(QStringLiteral("surfaceAreaMm2")).toDouble(), object.value(QStringLiteral("volumeMm3")).toDouble(),
                                                                 QVector3D{static_cast<float>(bounds.value(QStringLiteral("width")).toDouble()), static_cast<float>(bounds.value(QStringLiteral("height")).toDouble()), static_cast<float>(bounds.value(QStringLiteral("depth")).toDouble())},
                                                                 object.value(QStringLiteral("longestEdgeMm")).toDouble(), object.value(QStringLiteral("circularRadiusMm")).toDouble(), object.value(QStringLiteral("planarFaceCount")).toInt()});
        modelExtent = std::max({modelExtent, bounds.value(QStringLiteral("width")).toDouble(), bounds.value(QStringLiteral("height")).toDouble(), bounds.value(QStringLiteral("depth")).toDouble()});
    }
    QStringList geometryNodeIds = geometryByNode_.keys();
    visibilityModel_.setNodes(geometryNodeIds);
    if (!qFuzzyCompare(modelExtentMm_ + 1.0, modelExtent + 1.0)) {
        modelExtentMm_ = modelExtent;
        emit modelExtentChanged();
    }
    QHash<QString, int> quantities;
    for (const auto& value : nodes) {
        const auto object = value.toObject();
        const auto definitionId = object.value(QStringLiteral("definitionId")).toString();
        if (!definitionId.isEmpty()) ++quantities[definitionId];
    }
    struct TreeSourceNode {
        QString id;
        QString parentId;
        int kind{};
        QString name;
        QString definitionId;
        bool hasChildren{};
        bool isImplementationWrapper{};
    };
    QVector<TreeSourceNode> treeSourceNodes;
    treeSourceNodes.reserve(nodes.size());
    for (const auto& value : nodes) {
        const auto object = value.toObject();
        if (object.value(QStringLiteral("kind")).toInt() == static_cast<int>(domain::NodeKind::Definition)) {
            continue;
        }
        TreeSourceNode node{
            object.value(QStringLiteral("id")).toString(),
            object.value(QStringLiteral("parentId")).toString(),
            object.value(QStringLiteral("kind")).toInt(),
            object.value(QStringLiteral("name")).toString(),
            object.value(QStringLiteral("definitionId")).toString(),
        };
        treeSourceNodes.push_back(std::move(node));
    }
    QHash<QString, int> treeSourceIndex;
    for (int index = 0; index < treeSourceNodes.size(); ++index) {
        treeSourceIndex.insert(treeSourceNodes.at(index).id, index);
    }
    for (int index = 0; index < treeSourceNodes.size(); ++index) {
        const auto parentIndex = treeSourceIndex.value(treeSourceNodes.at(index).parentId, -1);
        if (parentIndex >= 0) treeSourceNodes[parentIndex].hasChildren = true;
    }
    const auto isImplementationName = [](const QString& name) {
        static const QSet<QString> implementationNames{
            QStringLiteral("SOLID"), QStringLiteral("COMPOUND"), QStringLiteral("COMPSOLID"),
            QStringLiteral("SHELL"), QStringLiteral("WIRE"), QStringLiteral("FACE"),
            QStringLiteral("EDGE"), QStringLiteral("VERTEX"),
        };
        return implementationNames.contains(name.trimmed().toUpper());
    };
    for (auto& node : treeSourceNodes) {
        node.isImplementationWrapper = node.hasChildren && isImplementationName(node.name);
    }
    const auto visibleParentId = [&treeSourceNodes, &treeSourceIndex](QString parentId) {
        while (!parentId.isEmpty()) {
            const auto parentIndex = treeSourceIndex.value(parentId, -1);
            if (parentIndex < 0 || !treeSourceNodes.at(parentIndex).isImplementationWrapper) return parentId;
            parentId = treeSourceNodes.at(parentIndex).parentId;
        }
        return QString{};
    };
    std::vector<models::AssemblyTreeModel::SnapshotNode> treeNodes;
    treeNodes.reserve(static_cast<std::size_t>(treeSourceNodes.size()));
    for (const auto& node : treeSourceNodes) {
        const auto displayName = !node.hasChildren && isImplementationName(node.name) ? QStringLiteral("Body") : node.name;
        // Every node's ancestor-chain entry is kept regardless of whether it's
        // shown as its own row below -- mass/volume aggregation, hide/isolate,
        // and isNodeSelected all walk parentByNode_ generically, and an
        // implementation-wrapper node still sits on the chain between its
        // (reparented) children and its own parent even though the tree
        // widget flattens past it.
        parentByNode_.insert(node.id, node.parentId);
        displayNameByNode_.insert(node.id, displayName);
        if (!node.definitionId.isEmpty()) definitionByNode_.insert(node.id, node.definitionId);
        if (node.isImplementationWrapper) continue;
        treeNodes.push_back({
            node.id,
            visibleParentId(node.parentId),
            QString::number(node.kind),
            displayName,
            quantities.value(node.definitionId),
        });
    }
    assemblyTreeModel_.replaceSnapshot(treeNodes);
    // The exploded view moves nodes as the tree shows them, so a sub-assembly picked in the
    // tree moves as a whole and auto-explode comes apart level by level as the tree reads.
    QHash<QString, QString> explodeParents;
    for (const auto& node : treeSourceNodes) explodeParents.insert(node.id, visibleParentId(node.parentId));
    explodeController_.setHierarchy(explodeParents);
    // One palette slot per part definition, so every instance of a repeated part shares a
    // colour, taken in tree order so neighbouring parts sit far apart in the palette walk.
    partColorKeys_.clear();
    QSet<QString> seenPartColorKeys;
    for (const auto& nodeId : geometryInTreeOrder) {
        const auto key = definitionByNode_.value(nodeId, nodeId);
        if (!seenPartColorKeys.contains(key)) {
            seenPartColorKeys.insert(key);
            partColorKeys_.append(key);
        }
    }
    rebuildPartColorOrder();
    loadAppearanceOverrides();
    applyActiveGeometryToMeasurement();
}

QStringList ApplicationController::geometryNodesForAction(const QString& nodeId) const
{
    QStringList result;
    for (auto it = geometryByNode_.cbegin(); it != geometryByNode_.cend(); ++it) {
        auto current = it.key();
        while (!current.isEmpty()) {
            if (current == nodeId) {
                result.append(it.key());
                break;
            }
            current = parentByNode_.value(current);
        }
    }
    return result;
}

QStringList ApplicationController::geometryNodesForSelection() const
{
    QStringList result;
    for (const auto& nodeId : selectedNodeIds_) result.append(geometryNodesForAction(nodeId));
    result.removeDuplicates();
    return result;
}

bool ApplicationController::isNodeSelected(const QString& nodeId) const
{
    if (nodeId.isEmpty()) return false;
    for (const auto& selectedNodeId : selectedNodeIds_) {
        auto current = nodeId;
        while (!current.isEmpty()) {
            if (current == selectedNodeId) return true;
            current = parentByNode_.value(current);
        }
    }
    return false;
}

QStringList ApplicationController::geometryNodesForScope(const QString& scope) const
{
    const auto selected = geometryNodesForAction(activeNodeId_);
    if (scope != QStringLiteral("definition") || selected.isEmpty()) return selected;
    QStringList result;
    for (const auto& selectedNode : selected) {
        const auto definitionId = definitionByNode_.value(selectedNode);
        if (definitionId.isEmpty()) {
            result.append(selectedNode);
            continue;
        }
        for (auto it = definitionByNode_.cbegin(); it != definitionByNode_.cend(); ++it) {
            if (it.value() == definitionId && geometryByNode_.contains(it.key())) result.append(it.key());
        }
    }
    result.removeDuplicates();
    return result;
}

QStringList ApplicationController::appearanceTargetsForScope(const QString& scope) const
{
    const auto nodes = geometryNodesForScope(scope);
    if (scope != QStringLiteral("definition")) return nodes;
    QStringList targets;
    for (const auto& nodeId : nodes) {
        if (const auto definitionId = definitionByNode_.value(nodeId); !definitionId.isEmpty()) targets.append(definitionId);
    }
    targets.removeDuplicates();
    return targets;
}

void ApplicationController::loadAppearanceOverrides()
{
    if (!overrideStore_ || !pendingSource_) return;
    for (const auto& record : overrideStore_->appearances(*pendingSource_)) {
        persistedAppearanceByKey_.insert(record.scope + QLatin1Char('|') + record.targetId, record);
        applyAppearanceOverride(record);
    }
}

void ApplicationController::applyAppearanceOverride(const cache::AppearanceOverride& record)
{
    QStringList nodes;
    if (record.scope == QStringLiteral("definition")) {
        for (auto it = definitionByNode_.cbegin(); it != definitionByNode_.cend(); ++it) {
            if (it.value() == record.targetId && geometryByNode_.contains(it.key())) nodes.append(it.key());
        }
    } else if (geometryByNode_.contains(record.targetId)) {
        nodes.append(record.targetId);
    }
    const QColor color(record.color);
    for (const auto& nodeId : nodes) {
        if (!record.materialId.isEmpty() && materialLibrary_.find(record.materialId)) materialByNode_.insert(nodeId, record.materialId);
        if (color.isValid()) appearanceByNode_.insert(nodeId, color.name(QColor::HexRgb).toUpper());
    }
}

bool ApplicationController::saveAppearanceOverride(const cache::AppearanceOverride& record)
{
    if (!overrideStore_ || !pendingSource_ || record.targetId.isEmpty()) return false;
    const auto key = record.scope + QLatin1Char('|') + record.targetId;
    try {
        if (record.materialId.isEmpty() && record.color.isEmpty()) {
            overrideStore_->removeAppearance(*pendingSource_, record.targetId, record.scope);
            persistedAppearanceByKey_.remove(key);
        } else {
            overrideStore_->putAppearance(*pendingSource_, record);
            persistedAppearanceByKey_.insert(key, record);
        }
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void ApplicationController::applyActiveGeometryToMeasurement()
{
    // Volume and bounds are of the selection -- a part, a sub-assembly, several of either --
    // or, with nothing selected, of the whole model.
    auto parts = geometryNodesForSelection();
    const auto wholeModel = parts.isEmpty();
    if (wholeModel) parts = geometryByNode_.keys();
    parts.erase(std::remove_if(parts.begin(), parts.end(), [this](const QString& nodeId) { return !geometryByNode_.contains(nodeId); }),
                parts.end());
    if (parts.isEmpty()) {
        measurementController_.clearSelectedGeometry();
        return;
    }
    double area{};
    double volume{};
    for (const auto& nodeId : parts) {
        const auto& geometry = geometryByNode_[nodeId];
        area += geometry.surfaceAreaMm2;
        volume += geometry.volumeMm3;
    }
    QVector3D minimum;
    QVector3D maximum;
    const auto hasBox = explodeController_.assembledBox(wholeModel ? QStringList{} : parts, &minimum, &maximum);
    // One part keeps the size the worker measured on its CAD shape; several are boxed together
    // where they are drawn, which only the assembled part boxes know.
    const auto& first = geometryByNode_[parts.front()];
    const auto size = parts.size() == 1 || !hasBox ? first.boundsMm : maximum - minimum;
    measurementController_.setSelectedGeometry(area, volume, size);
    if (parts.size() == 1)
        measurementController_.setSelectedTopology(first.longestEdgeMm, first.circularRadiusMm, first.planarFaceCount);
    const auto scope = wholeModel ? tr("Whole model")
                     : selectedNodeIds_.size() == 1 ? displayNameByNode_.value(selectedNodeIds_.front(), tr("Component"))
                                                    : tr("%1 selected").arg(selectedNodeIds_.size());
    if (hasBox) measurementController_.setSelectionBox(minimum, maximum, scope, static_cast<int>(parts.size()));
    else measurementController_.setSelectionBox({1, 1, 1}, {0, 0, 0}, scope, static_cast<int>(parts.size()));
}

double ApplicationController::activeSurfaceAreaMm2() const
{
    double total{};
    for (const auto& nodeId : geometryNodesForSelection()) total += geometryByNode_.value(nodeId).surfaceAreaMm2;
    return total;
}

double ApplicationController::activeVolumeMm3() const
{
    double total{};
    for (const auto& nodeId : geometryNodesForSelection()) total += geometryByNode_.value(nodeId).volumeMm3;
    return total;
}

QString ApplicationController::activeMaterialId() const
{
    return materialByNode_.value(activeNodeId_);
}

double ApplicationController::estimatedMassKg() const
{
    double total{};
    for (const auto& nodeId : geometryNodesForSelection()) {
        const auto material = materialLibrary_.find(materialByNode_.value(nodeId));
        if (material) total += inspection::estimateMassKg(geometryByNode_.value(nodeId).volumeMm3, *material).value_or(0.0);
    }
    return total;
}

QString ApplicationController::estimatedMassLabel() const
{
    return formatMassKg(estimatedMassKg());
}

QString ApplicationController::formatMassKg(double massKg)
{
    const auto locale = QLocale{};
    const auto normalizedMassKg = std::max(0.0, massKg);
    if (normalizedMassKg < 1.0) {
        return tr("%1 g").arg(locale.toString(normalizedMassKg * 1000.0, 'f', 2));
    }
    return tr("%1 kg").arg(locale.toString(normalizedMassKg, 'f', 2));
}

bool ApplicationController::assignActiveMaterial(const QString& materialId)
{
    return assignActiveMaterial(materialId, QStringLiteral("occurrence"));
}

bool ApplicationController::assignActiveMaterial(const QString& materialId, const QString& scope)
{
    const auto nodes = geometryNodesForScope(scope);
    if (nodes.isEmpty()) return false;
    const auto targets = appearanceTargetsForScope(scope);
    if (targets.isEmpty()) return false;
    if (!materialId.isEmpty() && !materialLibrary_.find(materialId)) return false;
    for (const auto& target : targets) {
        auto override = persistedAppearanceByKey_.value(scope + QLatin1Char('|') + target);
        override.targetId = target;
        override.scope = scope;
        override.materialId = materialId;
        if (!saveAppearanceOverride(override)) return false;
    }
    if (materialId.isEmpty()) {
        for (const auto& nodeId : nodes) materialByNode_.remove(nodeId);
    } else {
        for (const auto& nodeId : nodes) materialByNode_.insert(nodeId, materialId);
    }
    emit componentPropertiesChanged();
    return true;
}

QString ApplicationController::activeAppearanceColor() const
{
    return appearanceByNode_.value(activeNodeId_);
}

QString ApplicationController::activeResolvedAppearanceColor() const
{
    if (activeNodeId_.isEmpty()) return {};
    // A subassembly or a multi-solid container draws nothing itself, so its colour is the
    // colour its geometry is drawn in. Without this the swatch fell back to a placeholder
    // the viewport never showed.
    if (geometryByNode_.contains(activeNodeId_) || importedColorByNode_.contains(activeNodeId_))
        return resolvedAppearanceColor(activeNodeId_, importedColorByNode_.value(activeNodeId_));
    auto nodes = geometryNodesForAction(activeNodeId_);
    nodes.sort();
    for (const auto& nodeId : nodes) {
        if (const auto color = resolvedAppearanceColor(nodeId, importedColorByNode_.value(nodeId)); !color.isEmpty())
            return color;
    }
    return resolvedAppearanceColor(activeNodeId_, {});
}

bool ApplicationController::setActiveAppearanceColor(const QString& color, const QString& scope)
{
    const QColor parsed(color);
    const auto nodes = geometryNodesForScope(scope);
    const auto targets = appearanceTargetsForScope(scope);
    if (!parsed.isValid() || nodes.isEmpty() || targets.isEmpty()) return false;
    const auto normalized = parsed.name(QColor::HexRgb).toUpper();
    for (const auto& target : targets) {
        auto override = persistedAppearanceByKey_.value(scope + QLatin1Char('|') + target);
        override.targetId = target;
        override.scope = scope;
        override.color = normalized;
        if (!saveAppearanceOverride(override)) return false;
    }
    for (const auto& nodeId : nodes) appearanceByNode_.insert(nodeId, normalized);
    emit componentPropertiesChanged();
    return true;
}

bool ApplicationController::clearActiveAppearanceColor(const QString& scope)
{
    const auto nodes = geometryNodesForScope(scope);
    const auto targets = appearanceTargetsForScope(scope);
    if (nodes.isEmpty() || targets.isEmpty()) return false;
    for (const auto& target : targets) {
        auto override = persistedAppearanceByKey_.value(scope + QLatin1Char('|') + target);
        override.targetId = target;
        override.scope = scope;
        override.color.clear();
        if (!saveAppearanceOverride(override)) return false;
    }
    for (const auto& nodeId : nodes) appearanceByNode_.remove(nodeId);
    emit componentPropertiesChanged();
    return true;
}

QString ApplicationController::resolvedAppearanceColor(const QString& nodeId, const QString& sourceColor) const
{
    if (const auto appearance = appearanceByNode_.value(nodeId); !appearance.isEmpty()) return appearance;
    if (const auto material = materialByNode_.value(nodeId); !material.isEmpty()) {
        if (const auto color = materialLibrary_.colorFor(material); !color.isEmpty()) return color;
    }
    // A file colour stands unless the user asked to recolour them all, or the file gave every
    // part the same one -- a CAD default, which tells parts apart no better than none.
    if (partPaletteStyle(partColorMode_)) {
        const bool fileColorCounts = !sourceColor.isEmpty() && !recolorFileColors_ && !fileColorsUniform_;
        if (!fileColorCounts) {
            if (const auto color = autoPartColor(nodeId); !color.isEmpty()) return color;
        }
    }
    // Empty when nothing colours the part: the caller draws its theme's neutral body colour,
    // the same one in the viewport and in the inspector swatch.
    return sourceColor;
}

void ApplicationController::setPartColorMode(const QString& mode)
{
    const auto normalized = partPaletteStyle(mode) ? mode : QStringLiteral("file");
    if (partColorMode_ == normalized) return;
    partColorMode_ = normalized;
    partColorsUpdated();
}

void ApplicationController::setRecolorFileColors(const bool recolor)
{
    if (recolorFileColors_ == recolor) return;
    recolorFileColors_ = recolor;
    partColorsUpdated();
}

void ApplicationController::setPartColorSeed(const int seed)
{
    const auto value = static_cast<std::uint32_t>(seed);
    if (partColorSeed_ == value) return;
    partColorSeed_ = value;
    rebuildPartColorOrder();
    partColorsUpdated();
}

void ApplicationController::setRenderMode(const int mode)
{
    const auto clamped = std::clamp(mode, 0, 2);
    if (renderMode_ == clamped) return;
    renderMode_ = clamped;
    emit renderModeChanged();
}

void ApplicationController::shufflePartColors()
{
    // Never back to 0, which is the unshuffled tree order.
    auto next = partColorSeed_ * 1664525U + 1013904223U;
    if (next == 0) next = 1;
    setPartColorSeed(static_cast<int>(next));
}

void ApplicationController::noteImportedColor(const QString& nodeId, const QString& color)
{
    static_cast<void>(nodeId);
    if (color.isEmpty()) importedUncolouredFace_ = true;
    else importedFaceColors_.insert(color);
    // Two or more parts, every face coloured, one colour throughout.
    const bool uniform = geometryByNode_.size() >= 2 && !importedUncolouredFace_ && importedFaceColors_.size() == 1;
    if (uniform == fileColorsUniform_) return;
    fileColorsUniform_ = uniform;
    emit partColorsChanged();
    if (partPaletteStyle(partColorMode_) && !recolorFileColors_) emit componentPropertiesChanged();
}

QString ApplicationController::nodeDisplayName(const QString& nodeId) const
{
    return displayNameByNode_.value(nodeId);
}

void ApplicationController::notePartBounds(const QString& nodeId, const QByteArray& payload)
{
    const auto bounds = protocol::meshBounds(payload);
    if (!bounds) return;
    explodeController_.includePartBounds(nodeId, {bounds->minimum[0], bounds->minimum[1], bounds->minimum[2]},
                                         {bounds->maximum[0], bounds->maximum[1], bounds->maximum[2]});
}

void ApplicationController::notePartBounds(const QString& nodeId, const QByteArray& sharedPayload, const QVector<float>& placement)
{
    if (placement.size() != 12) return;
    std::array<float, 12> matrix{};
    std::copy(placement.cbegin(), placement.cend(), matrix.begin());
    const auto bounds = protocol::meshBounds(sharedPayload, matrix);
    if (!bounds) return;
    explodeController_.includePartBounds(nodeId, {bounds->minimum[0], bounds->minimum[1], bounds->minimum[2]},
                                         {bounds->maximum[0], bounds->maximum[1], bounds->maximum[2]});
}

bool ApplicationController::noteSegmentColor(const QString& nodeId, const QString& segmentKey, const QString& color,
                                             const QByteArray& payload)
{
    auto& segments = segmentColorsByNode_[nodeId];
    // Keyed by segment, so the refined pass replaces the preview's entry rather than adding to it.
    segments.insert(segmentKey, {color, protocol::meshFaceArea(payload)});
    QHash<QString, double> areaByColor;
    for (const auto& segment : std::as_const(segments)) areaByColor[segment.color] += segment.areaMm2;
    QString dominant;
    double dominantArea = -1.0;
    for (auto it = areaByColor.cbegin(); it != areaByColor.cend(); ++it) {
        // Ties go to the lower colour, so the result does not depend on hash order.
        if (it.value() > dominantArea || (it.value() == dominantArea && it.key() < dominant)) {
            dominant = it.key();
            dominantArea = it.value();
        }
    }
    const auto previous = importedColorByNode_.value(nodeId);
    // Mostly uncoloured reads as the neutral body, which an absent entry gives.
    if (dominant.isEmpty()) importedColorByNode_.remove(nodeId);
    else importedColorByNode_.insert(nodeId, dominant);
    return importedColorByNode_.value(nodeId) != previous;
}

void ApplicationController::rebuildPartColorOrder()
{
    partColorPositionByKey_.clear();
    autoPartColorByKey_.clear();
    const auto ordered = render::partPaletteOrder(partColorKeys_, partColorSeed_);
    for (int position = 0; position < ordered.size(); ++position) partColorPositionByKey_.insert(ordered.at(position), position);
}

QString ApplicationController::autoPartColor(const QString& nodeId) const
{
    const auto style = partPaletteStyle(partColorMode_);
    if (!style) return {};
    const auto key = definitionByNode_.value(nodeId, nodeId);
    if (const auto cached = autoPartColorByKey_.constFind(key); cached != autoPartColorByKey_.cend()) return *cached;
    // A part the snapshot did not list (it should not happen) still gets a stable slot.
    const auto position = partColorPositionByKey_.value(key, static_cast<int>(qHash(key) % 4093U + static_cast<std::size_t>(partColorKeys_.size())));
    const auto color = render::partPaletteColor(*style, position, partColorSeed_).name(QColor::HexRgb).toUpper();
    autoPartColorByKey_.insert(key, color);
    return color;
}

void ApplicationController::partColorsUpdated()
{
    autoPartColorByKey_.clear();
    emit partColorsChanged();
    // Every viewport re-resolves its models' colours, once.
    emit componentPropertiesChanged();
}

bool ApplicationController::setUnitOverride(const QString& unit)
{
    if (!pendingSource_ || pendingPath_.isEmpty() || !overrideStore_ || (unit != QStringLiteral("mm") && unit != QStringLiteral("in"))) return false;
    try {
        overrideStore_->put(*pendingSource_, {unit, 1.0, QStringLiteral("User requested unit interpretation")});
    } catch (const std::exception&) {
        return false;
    }
    openFile(QUrl::fromLocalFile(pendingPath_));
    return true;
}

bool ApplicationController::setUpAxis(const QString& axis)
{
    if (axis != QStringLiteral("Z") && axis != QStringLiteral("Y")) return false;
    if (upAxis_ == axis) return true;
    upAxis_ = axis;
    // Deliberately no reimport, unlike a unit override: the up axis only decides
    // what a named view is called and which way it points, so the geometry, the
    // cache, and the assembly tree are all unaffected.
    emit upAxisChanged();
    return true;
}

QString ApplicationController::applicationVersion()
{
#ifdef LOUPE_RELEASE_LABEL
    // A beta says which beta it is: 0.1.5-beta.7, not just 0.1.5.
    if (const auto label = QStringLiteral(LOUPE_RELEASE_LABEL); platform::UpdateController::parseVersion(label))
        return label.startsWith(QLatin1Char('v')) ? label.mid(1) : label;
#endif
#ifdef LOUPE_VERSION
    return QStringLiteral(LOUPE_VERSION);
#else
    // Only reachable in a target that does not define it, such as a unit test binary.
    return QStringLiteral("unknown");
#endif
}

QVector3D ApplicationController::directionForStandardView(const QString& name) const
{
    // Direction the camera looks FROM, in model space.
    //
    // The renderer is Y-up but mechanical CAD is overwhelmingly Z-up, so the two
    // conventions disagree about which axis "Top" and "Front" mean. Resolving the
    // name here, in one table, is what stops a named view from silently producing
    // the wrong elevation in a 1:1 drawing.
    const bool zUp = upAxis_ != QStringLiteral("Y");
    if (name == QStringLiteral("Top")) return zUp ? QVector3D(0, 0, 1) : QVector3D(0, 1, 0);
    if (name == QStringLiteral("Bottom")) return zUp ? QVector3D(0, 0, -1) : QVector3D(0, -1, 0);
    // In a Z-up document the front elevation is seen looking along +Y, so the
    // camera sits on -Y.
    if (name == QStringLiteral("Front")) return zUp ? QVector3D(0, -1, 0) : QVector3D(0, 0, 1);
    if (name == QStringLiteral("Back")) return zUp ? QVector3D(0, 1, 0) : QVector3D(0, 0, -1);
    if (name == QStringLiteral("Right")) return {1, 0, 0};
    if (name == QStringLiteral("Left")) return {-1, 0, 0};
    return {};
}

QVector3D ApplicationController::upForStandardView(const QString& name) const
{
    // Same table, same reason as directionForStandardView: a named view that resolved its
    // direction one way and its up another would produce a rotated drawing that still
    // looked plausible. Every pair here is perpendicular by construction.
    const bool zUp = upAxis_ != QStringLiteral("Y");
    if (zUp) {
        // Plan views look along Z, so +Y reads up the page; every elevation looks
        // horizontally, so +Z does.
        if (name == QStringLiteral("Top") || name == QStringLiteral("Bottom")) return {0, 1, 0};
        return {0, 0, 1};
    }
    if (name == QStringLiteral("Top")) return {0, 0, -1};
    if (name == QStringLiteral("Bottom")) return {0, 0, 1};
    return {0, 1, 0};
}

QString ApplicationController::labelForStandardView(const QString& name) const
{
    // Names are already resolved against the document convention, so the label is
    // the name. Kept as a seam so a future convention (for example a Y-up
    // document labelled with its own axis letters) has one place to change.
    return name;
}

void ApplicationController::acceptViewSelection(const QString& nodeId, const double x, const double y, const double z, const double normalX, const double normalY, const double normalZ,
                                                const bool additive)
{
    if (nodeId.isEmpty()) return;
    selectNode(nodeId, additive);
    sectionController_.setCandidatePlane({static_cast<float>(normalX), static_cast<float>(normalY), static_cast<float>(normalZ)}, {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)});
}

void ApplicationController::acceptViewPick(const QString& nodeId, const double x, const double y, const double z, const double normalX, const double normalY, const double normalZ)
{
    if (nodeId.isEmpty()) return;
    sectionController_.setCandidatePlane({static_cast<float>(normalX), static_cast<float>(normalY), static_cast<float>(normalZ)},
                                         {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)});
    measurementController_.recordPick({static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)},
                                      {static_cast<float>(normalX), static_cast<float>(normalY), static_cast<float>(normalZ)},
                                      displayNameByNode_.value(nodeId, tr("Component")), tr("Surface point"));
}

bool ApplicationController::acceptMeasureEntity(const QString& nodeId, const QVariant& entity, const bool additive)
{
    if (nodeId.isEmpty() || !entity.canConvert<tools::MeasureEntity>()) return false;
    auto pick = entity.value<tools::MeasureEntity>();
    pick.nodeId = nodeId;
    pick.nodeLabel = displayNameByNode_.value(nodeId, tr("Component"));
    // As every pick does: a flat face offers its plane to the section tool.
    if (pick.kind == tools::MeasureEntity::Kind::Face && pick.planar)
        sectionController_.setCandidatePlane(pick.planeNormal, pick.planePoint);
    return measurementController_.acceptEntity(pick, additive);
}

bool ApplicationController::sectionAlongFace(const QString& nodeId, const QVariant& entity)
{
    if (nodeId.isEmpty() || !entity.canConvert<tools::MeasureEntity>()) return false;
    const auto face = entity.value<tools::MeasureEntity>();
    if (face.kind != tools::MeasureEntity::Kind::Face || !face.planar || face.planeNormal.isNull()) return false;
    sectionController_.usePlane(face.planeNormal, face.planePoint,
                                tr("Flat face on %1").arg(displayNameByNode_.value(nodeId, tr("Component"))));
    return true;
}

bool ApplicationController::acceptTopologyPick(const QString& nodeId, const QString& entityKind,
                                               const quint32 topologyId, const double x, const double y,
                                               const double z, const double normalX, const double normalY,
                                               const double normalZ, const double measureMm, const double radiusMm)
{
    if (nodeId.isEmpty()) return false;
    sectionController_.setCandidatePlane({static_cast<float>(normalX), static_cast<float>(normalY), static_cast<float>(normalZ)},
                                         {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)});
    return measurementController_.recordTopologyPick(entityKind, topologyId,
        {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)},
        {static_cast<float>(normalX), static_cast<float>(normalY), static_cast<float>(normalZ)},
        measureMm, radiusMm, displayNameByNode_.value(nodeId, tr("Component")));
}

} // namespace loupe::app
