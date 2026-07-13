#include "app/ApplicationController.h"
#include "app/cache/CacheKey.h"
#include "app/cache/CacheStore.h"
#include "app/cache/SourceFingerprint.h"
#include "core/inspection/MaterialProperties.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QUrl>
#include <QUuid>

namespace {

QString defaultWorkerExecutable()
{
    auto name = QStringLiteral("loupe-worker");
#ifdef Q_OS_WIN
    name += QStringLiteral(".exe");
#endif
    return QCoreApplication::applicationDirPath() + QLatin1Char('/') + name;
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
    } catch (const std::exception&) {
    }
    connect(&workerProcess_, &QProcess::started, this, &ApplicationController::connectWorker);
    connect(&workerProcess_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        documentState_ = DocumentState::WorkerFailed;
        emit documentStateChanged();
    });
    connect(&workerClient_, &worker::WorkerClient::snapshotReady, this, [this](quint64 requestId, const QByteArray& snapshot) {
        if (requestId != activeRequestId_) return;
        applySnapshotToTree(snapshot);
        snapshotJson_ = QString::fromUtf8(snapshot);
        if (cacheStore_ && pendingSource_) {
            const auto document = QJsonDocument::fromJson(snapshot).object();
            const cache::UnitDecision unit{document.value(QStringLiteral("effectiveUnit")).toString(), document.value(QStringLiteral("sourceToMillimeters")).toDouble(1.0)};
            const auto key = cache::CacheKey::from(*pendingSource_, QStringLiteral("step-importer-1"), QStringLiteral("snapshot-1"), unit);
            static_cast<void>(cacheStore_->put(key, snapshot, {*pendingSource_, QStringLiteral("step-importer-1"), QStringLiteral("snapshot-1"), unit}));
        }
        documentState_ = DocumentState::TreeReady;
        emit snapshotChanged();
        emit documentStateChanged();
    });
    connect(&workerClient_, &worker::WorkerClient::meshReady, this, [this](quint64 requestId, const QString&, const QByteArray& meshJson) {
        if (requestId != activeRequestId_) return;
        emit meshReady(meshJson);
    });
    connect(&workerClient_, &worker::WorkerClient::requestFailed, this, [this](quint64 requestId, const QString&, const QString&, bool) {
        if (requestId != activeRequestId_) return;
        documentState_ = DocumentState::WorkerFailed;
        emit documentStateChanged();
    });
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
    emit componentPropertiesChanged();
}

void ApplicationController::openFile(const QUrl& file)
{
    if (!file.isLocalFile() || !QFileInfo::exists(file.toLocalFile())) {
        documentState_ = DocumentState::Invalid;
        emit documentStateChanged();
        return;
    }
    if (workerProcess_.state() != QProcess::NotRunning) {
        workerProcess_.kill();
        workerProcess_.waitForFinished(1'000);
    }
    pendingPath_ = file.toLocalFile();
    pendingSource_ = cache::SourceFingerprint::fromFile(pendingPath_);
    if (!pendingSource_) {
        documentState_ = DocumentState::Invalid;
        emit documentStateChanged();
        return;
    }
    snapshotJson_.clear();
    geometryByNode_.clear();
    materialByNode_.clear();
    connectionAttempts_ = 0;
    activeRequestId_ = 0;
    cacheHit_ = false;
    serverName_ = QStringLiteral("loupe-shell-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    documentState_ = DocumentState::Opening;
    emit snapshotChanged();
    emit componentPropertiesChanged();
    emit cacheHitChanged();
    if (cacheStore_) {
        if (const auto cached = cacheStore_->readSnapshotForSource(*pendingSource_, QStringLiteral("step-importer-1"), QStringLiteral("snapshot-1"))) {
            applySnapshotToTree(*cached);
            snapshotJson_ = QString::fromUtf8(*cached);
            documentState_ = DocumentState::TreeReady;
            cacheHit_ = true;
            emit snapshotChanged();
            emit componentPropertiesChanged();
            emit documentStateChanged();
            emit cacheHitChanged();
        }
    }
    emit documentStateChanged();
    workerProcess_.start(workerExecutable_, {QStringLiteral("--server-name"), serverName_});
}

void ApplicationController::connectWorker()
{
    if (workerClient_.connectToServer(serverName_, 100)) {
        activeRequestId_ = workerClient_.openFile(pendingPath_);
        return;
    }
    if (++connectionAttempts_ >= 50) {
        documentState_ = DocumentState::WorkerFailed;
        emit documentStateChanged();
        return;
    }
    QTimer::singleShot(25, this, &ApplicationController::connectWorker);
}

void ApplicationController::applySnapshotToTree(const QByteArray& snapshot)
{
    const auto document = QJsonDocument::fromJson(snapshot);
    if (!document.isObject()) return;
    const auto nodes = document.object().value(QStringLiteral("nodes")).toArray();
    geometryByNode_.clear();
    for (const auto& value : document.object().value(QStringLiteral("geometry")).toArray()) {
        const auto object = value.toObject();
        const auto nodeId = object.value(QStringLiteral("nodeId")).toString();
        if (!nodeId.isEmpty()) geometryByNode_.insert(nodeId, {object.value(QStringLiteral("surfaceAreaMm2")).toDouble(), object.value(QStringLiteral("volumeMm3")).toDouble()});
    }
    QHash<QString, int> quantities;
    for (const auto& value : nodes) {
        const auto object = value.toObject();
        const auto definitionId = object.value(QStringLiteral("definitionId")).toString();
        if (!definitionId.isEmpty()) ++quantities[definitionId];
    }
    std::vector<models::AssemblyTreeModel::SnapshotNode> treeNodes;
    treeNodes.reserve(static_cast<std::size_t>(nodes.size()));
    for (const auto& value : nodes) {
        const auto object = value.toObject();
        const auto definitionId = object.value(QStringLiteral("definitionId")).toString();
        treeNodes.push_back({
            object.value(QStringLiteral("id")).toString(),
            object.value(QStringLiteral("parentId")).toString(),
            QString::number(object.value(QStringLiteral("kind")).toInt()),
            object.value(QStringLiteral("name")).toString(),
            quantities.value(definitionId),
        });
    }
    assemblyTreeModel_.replaceSnapshot(treeNodes);
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
    const auto material = inspection::MaterialLibrary::find(activeMaterialId().toStdString());
    if (!material) return 0.0;
    return inspection::estimateMassKg(activeVolumeMm3(), *material).value_or(0.0);
}

bool ApplicationController::assignActiveMaterial(const QString& materialId)
{
    if (activeNodeId_.isEmpty() || !geometryByNode_.contains(activeNodeId_) || !inspection::MaterialLibrary::find(materialId.toStdString())) return false;
    materialByNode_.insert(activeNodeId_, materialId);
    emit componentPropertiesChanged();
    return true;
}

} // namespace loupe::app
