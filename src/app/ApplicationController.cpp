#include "app/ApplicationController.h"
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
    : ApplicationController(defaultWorkerExecutable(), parent)
{
}

ApplicationController::ApplicationController(const QString& workerExecutable, QObject* parent)
    : QObject(parent)
    , workerExecutable_(workerExecutable)
{
    connect(&workerProcess_, &QProcess::started, this, &ApplicationController::connectWorker);
    connect(&workerProcess_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        documentState_ = DocumentState::WorkerFailed;
        emit documentStateChanged();
    });
    connect(&workerClient_, &worker::WorkerClient::snapshotReady, this, [this](quint64 requestId, const QByteArray& snapshot) {
        if (requestId != activeRequestId_) return;
        applySnapshotToTree(snapshot);
        snapshotJson_ = QString::fromUtf8(snapshot);
        documentState_ = DocumentState::TreeReady;
        emit snapshotChanged();
        emit documentStateChanged();
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
    snapshotJson_.clear();
    geometryByNode_.clear();
    materialByNode_.clear();
    connectionAttempts_ = 0;
    activeRequestId_ = 0;
    serverName_ = QStringLiteral("loupe-shell-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    documentState_ = DocumentState::Opening;
    emit snapshotChanged();
    emit componentPropertiesChanged();
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
