#include "app/ApplicationController.h"

#include <QCoreApplication>
#include <QFileInfo>
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
    connectionAttempts_ = 0;
    activeRequestId_ = 0;
    serverName_ = QStringLiteral("loupe-shell-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    documentState_ = DocumentState::Opening;
    emit snapshotChanged();
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

} // namespace loupe::app
