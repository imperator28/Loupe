#include "app/ApplicationController.h"
#include "app/cache/CacheKey.h"
#include "app/cache/CacheStore.h"
#include "app/cache/SourceFingerprint.h"
#include "core/domain/AssemblyTypes.h"
#include "core/inspection/MaterialProperties.h"

#include <QCoreApplication>
#include <QBuffer>
#include <QColor>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

namespace {

const auto kImporterVersion = QStringLiteral("step-importer-4");
const auto kMeshProfile = QStringLiteral("progressive-1");
const auto kGeometryCacheProfile = QStringLiteral("progressive-2-topology-geometry");
constexpr auto kGeometryCacheMagic = "LGC1";
constexpr auto kWorkerConnectionAttempts = 4'800;

QString defaultWorkerExecutable()
{
    auto name = QStringLiteral("loupe-worker");
#ifdef Q_OS_WIN
    name += QStringLiteral(".exe");
#endif
    const auto applicationDir = QDir(QCoreApplication::applicationDirPath());
    const auto packagedWorker = applicationDir.filePath(name);
    if (QFileInfo::exists(packagedWorker)) {
        return packagedWorker;
    }

    const auto buildTreeWorkers = {
        QDir::cleanPath(applicationDir.filePath(QStringLiteral("../worker/%1").arg(name))),
        QDir::cleanPath(applicationDir.filePath(QStringLiteral("../../../../worker/%1").arg(name))),
    };
    for (const auto& buildTreeWorker : buildTreeWorkers) {
        if (QFileInfo::exists(buildTreeWorker)) {
            return buildTreeWorker;
        }
    }

    return packagedWorker;
}

} // namespace

namespace loupe::app {

ApplicationController::ApplicationController(QObject* parent)
    : ApplicationController(defaultWorkerExecutable(), cache::CacheStore::defaultRoot(), parent)
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
    auto workerEnvironment = QProcessEnvironment::systemEnvironment();
    workerEnvironment.remove(QStringLiteral("__CFBundleIdentifier"));
    workerEnvironment.remove(QStringLiteral("XPC_SERVICE_NAME"));
    workerEnvironment.remove(QStringLiteral("XPC_FLAGS"));
    workerProcess_.setProcessEnvironment(workerEnvironment);
    workerProcess_.setWorkingDirectory(QFileInfo(workerExecutable_).absolutePath());
    workerProcess_.setProcessChannelMode(QProcess::ForwardedChannels);
    connect(&workerProcess_, &QProcess::started, this, [this] {
        setImportProgress(tr("Starting STEP importer"), 0.005);
        connectWorker();
    });
    connect(&workerProcess_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (stoppingWorker_) {
            return;
        }
        setErrorMessage(tr("Could not start the STEP importer: %1").arg(workerProcess_.errorString()));
        documentState_ = DocumentState::WorkerFailed;
        emit documentStateChanged();
    });
    connect(&workerProcess_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
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
    connect(&workerClient_, &worker::WorkerClient::snapshotReady, this, [this](quint64 requestId, const QByteArray& snapshot) {
        if (requestId != activeRequestId_) return;
        if (cacheHit_ && !snapshotJson_.isEmpty()) {
            // The cache already established the identical tree and replayed its geometry. Keep it visible
            // while the worker retains the XCAF document and streams any settled replacements.
            return;
        }
        applySnapshotToTree(snapshot);
        snapshotJson_ = QString::fromUtf8(snapshot);
        exportWorkspaceController_.replaceSnapshot(snapshotJson_);
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
        if (!sourceColor.isEmpty() && importedColorByNode_.value(nodeId) != sourceColor) {
            importedColorByNode_.insert(nodeId, sourceColor);
            if (nodeId == activeNodeId_) emit componentPropertiesChanged();
        }
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
        if (nodeId == activeNodeId_) {
            applyActiveGeometryToMeasurement();
            emit componentPropertiesChanged();
        }
    });
    connect(&workerClient_, &worker::WorkerClient::edgeReady, this, [this](quint64 requestId, const QString& nodeId, const QByteArray& edgeJson) {
        if (requestId != activeRequestId_) return;
        if (cacheHit_ && cachedGeometryLoaded_) return;
        appendCachedEdges(nodeId, edgeJson);
        emit edgeReady(nodeId, edgeJson);
    });
    connect(&workerClient_, &worker::WorkerClient::progress, this, [this](quint64 requestId, const QString& stage, const double fraction) {
        if (requestId != activeRequestId_) return;
        setImportProgress(stage, fraction);
        if (fraction >= 1.0) {
            exportWorkspaceController_.setDocumentReady(true);
            saveGeometryCache();
        }
    });
    connect(&workerClient_, &worker::WorkerClient::requestCanceled, this, [this](quint64 requestId) {
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
        if (requestId == activeExportRequestId_) {
            activeExportRequestId_ = 0;
            exportWorkspaceController_.handleExportFailed(message.isEmpty() ? code : message);
            return;
        }
        if (requestId != activeRequestId_) return;
        const auto detail = message.isEmpty() ? code : message;
        setErrorMessage(tr("The STEP importer could not open this file: %1").arg(detail));
        documentState_ = DocumentState::WorkerFailed;
        emit documentStateChanged();
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
        if (activeExportRequestId_ == 0) return;
        activeExportRequestId_ = 0;
        exportWorkspaceController_.handleExportFailed(message);
    });
    connect(&workerClient_, &worker::WorkerClient::connectionLost, this, [this] {
        if (activeExportRequestId_ == 0) return;
        activeExportRequestId_ = 0;
        exportWorkspaceController_.handleExportFailed(tr("Connection to the export worker was lost"));
    });
    connect(&visibilityModel_, &models::VisibilityModel::presentationChanged, this, [this] {
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

ApplicationController::~ApplicationController()
{
    if (workerProcess_.state() != QProcess::NotRunning) {
        workerProcess_.terminate();
        if (!workerProcess_.waitForFinished(1'000)) {
            workerProcess_.kill();
            workerProcess_.waitForFinished(1'000);
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
    if (activeNodeId_ == activeNodeId) {
        return;
    }
    const auto previousMode = inspectorMode();
    activeNodeId_ = activeNodeId;
    emit activeNodeIdChanged();
    if (previousMode != inspectorMode()) {
        emit inspectorModeChanged();
    }
    applyActiveGeometryToMeasurement();
    emit componentPropertiesChanged();
}

void ApplicationController::openFile(const QUrl& file)
{
    setErrorMessage({});
    if (!file.isLocalFile() || !QFileInfo::exists(file.toLocalFile())) {
        setErrorMessage(tr("The selected file does not exist or is not a local file."));
        documentState_ = DocumentState::Invalid;
        emit documentStateChanged();
        return;
    }
    viewerPresentation_ = ViewerPresentation::Full;
    sectionController_.setEnabled(false);
    measurementController_.clearPicks();
    if (workerProcess_.state() != QProcess::NotRunning) {
        stoppingWorker_ = true;
        workerProcess_.kill();
        workerProcess_.waitForFinished(1'000);
        stoppingWorker_ = false;
    }
    setErrorMessage({});
    setActiveNodeId({});
    assemblyTreeModel_.replaceSnapshot({});
    pendingPath_ = QFileInfo(file.toLocalFile()).absoluteFilePath();
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
    geometryCacheArchive_.clear();
    geometryReplayTimer_.stop();
    geometryReplayQueue_.clear();
    geometryReplayIndex_ = 0;
    ++geometryReplayGeneration_;
    cachedGeometryLoaded_ = false;
    geometryByNode_.clear();
    parentByNode_.clear();
    definitionByNode_.clear();
    visibilityModel_.setNodes({});
    materialByNode_.clear();
    appearanceByNode_.clear();
    importedColorByNode_.clear();
    displayNameByNode_.clear();
    persistedAppearanceByKey_.clear();
    modelExtentMm_ = 0.0;
    effectiveUnit_ = QStringLiteral("mm");
    connectionAttempts_ = 0;
    activeRequestId_ = 0;
    activeExportRequestId_ = 0;
    cacheHit_ = false;
    serverName_ = QStringLiteral("/tmp/loupe-%1-%2")
                      .arg(QCoreApplication::applicationPid())
                      .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
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
    workerProcess_.start(launchWorker, {QStringLiteral("--server-name"), serverName_});
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

void ApplicationController::isolateActiveNode()
{
    if (activeNodeId_.isEmpty()) return;
    visibilityModel_.isolate(geometryNodesForAction(activeNodeId_));
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
    if (activeNodeId_.isEmpty()) return;
    visibilityModel_.hide(geometryNodesForAction(activeNodeId_));
}

void ApplicationController::hideOtherNodes()
{
    if (activeNodeId_.isEmpty()) return;
    visibilityModel_.isolate(geometryNodesForAction(activeNodeId_));
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

void ApplicationController::connectWorker()
{
    if (workerClient_.connectToServer(serverName_, 100)) {
        activeRequestId_ = workerClient_.openFile(pendingPath_, pendingUnitOverride_);
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
    importProgressTimer_.start();
    emit importProgressChanged();
}

void ApplicationController::setImportProgress(const QString& stage, const double fraction)
{
    importStage_ = stage;
    importProgress_ = std::max(importProgress_, std::clamp(fraction, 0.0, 1.0));
    importElapsedSeconds_ = importElapsed_.elapsed() / 1'000;
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
    while (!stream.atEnd()) {
        quint8 kind{};
        QString nodeId;
        stream >> kind >> nodeId;
        if (kind == 1) {
            QString segmentKey, sourceColor;
            QByteArray payload;
            stream >> segmentKey >> sourceColor >> payload;
            if (stream.status() != QDataStream::Ok || nodeId.isEmpty() || segmentKey.isEmpty() || payload.isEmpty()) return std::nullopt;
            GeometryReplayEntry entry{kind, nodeId, segmentKey, sourceColor, payload};
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
            GeometryReplayEntry entry{kind, nodeId, {}, {}, payload};
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
    if (geometryReplayIndex_ >= geometryReplayQueue_.size()) {
        geometryReplayQueue_.clear();
        geometryReplayIndex_ = 0;
        if (previewGeometryReceived_) emit importProgressChanged();
        return;
    }

    auto entry = std::move(geometryReplayQueue_[geometryReplayIndex_++]);
    if (entry.kind == 1) {
        previewGeometryReceived_ = true;
        if (!entry.sourceColor.isEmpty()) importedColorByNode_.insert(entry.nodeId, entry.sourceColor);
        emit meshReady(entry.nodeId, entry.segmentKey, entry.sourceColor, entry.payload);
    } else {
        emit edgeReady(entry.nodeId, entry.payload);
    }
    geometryReplayTimer_.start();
}

void ApplicationController::saveGeometryCache()
{
    if (!cacheStore_ || !pendingSource_ || geometryCacheArchive_.size() <= 4) return;
    const cache::UnitDecision unit{effectiveUnit_, 1.0};
    const auto key = cache::CacheKey::from(*pendingSource_, kImporterVersion, kGeometryCacheProfile, unit);
    static_cast<void>(cacheStore_->put(key, geometryCacheArchive_, {*pendingSource_, kImporterVersion, kGeometryCacheProfile, unit}));
}

qint64 ApplicationController::importEtaSeconds() const noexcept
{
    if (importProgress_ < 0.12 || importProgress_ >= 0.995 || importElapsedSeconds_ < 1) return -1;
    return static_cast<qint64>((static_cast<double>(importElapsedSeconds_) / importProgress_) * (1.0 - importProgress_));
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
    for (const auto& value : document.object().value(QStringLiteral("geometry")).toArray()) {
        const auto object = value.toObject();
        const auto nodeId = object.value(QStringLiteral("nodeId")).toString();
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
        if (node.isImplementationWrapper) continue;
        const auto displayName = !node.hasChildren && isImplementationName(node.name) ? QStringLiteral("Body") : node.name;
        parentByNode_.insert(node.id, node.parentId);
        displayNameByNode_.insert(node.id, displayName);
        if (!node.definitionId.isEmpty()) definitionByNode_.insert(node.id, node.definitionId);
        treeNodes.push_back({
            node.id,
            visibleParentId(node.parentId),
            QString::number(node.kind),
            displayName,
            quantities.value(node.definitionId),
        });
    }
    assemblyTreeModel_.replaceSnapshot(treeNodes);
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
    const auto it = geometryByNode_.constFind(activeNodeId_);
    if (it == geometryByNode_.cend()) {
        measurementController_.clearSelectedGeometry();
        return;
    }
    const auto& geometry = *it;
    measurementController_.setSelectedGeometry(geometry.surfaceAreaMm2, geometry.volumeMm3, geometry.boundsMm);
    measurementController_.setSelectedTopology(geometry.longestEdgeMm, geometry.circularRadiusMm, geometry.planarFaceCount);
}

double ApplicationController::activeSurfaceAreaMm2() const noexcept
{
    return geometryByNode_.value(activeNodeId_).surfaceAreaMm2;
}

double ApplicationController::activeVolumeMm3() const noexcept
{
    return geometryByNode_.value(activeNodeId_).volumeMm3;
}

QString ApplicationController::activeMaterialId() const
{
    return materialByNode_.value(activeNodeId_);
}

double ApplicationController::estimatedMassKg() const noexcept
{
    const auto material = materialLibrary_.find(activeMaterialId());
    if (!material) return 0.0;
    return inspection::estimateMassKg(activeVolumeMm3(), *material).value_or(0.0);
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
    return resolvedAppearanceColor(activeNodeId_, importedColorByNode_.value(activeNodeId_));
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
    return sourceColor.isEmpty() ? QStringLiteral("#67D5C0") : sourceColor;
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

void ApplicationController::acceptViewSelection(const QString& nodeId, const double x, const double y, const double z, const double normalX, const double normalY, const double normalZ)
{
    if (nodeId.isEmpty()) return;
    setActiveNodeId(nodeId);
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
