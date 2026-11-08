#include "app/worker/WorkerLaunch.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QUuid>

namespace loupe::app::worker {
namespace {

// Matches the first request a fresh WorkerClient would send, though nothing depends on that:
// the client adopting the importer skips its own numbering past it.
constexpr std::uint64_t kPrelaunchRequestId = 1;

std::optional<PrelaunchedWorker>& prelaunched()
{
    static std::optional<PrelaunchedWorker> worker;
    return worker;
}

void stop(QProcess& process)
{
    if (process.state() == QProcess::NotRunning) return;
    process.kill();
    process.waitForFinished(1'000);
}

} // namespace

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

void configureWorkerProcess(QProcess& process)
{
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.remove(QStringLiteral("__CFBundleIdentifier"));
    environment.remove(QStringLiteral("XPC_SERVICE_NAME"));
    environment.remove(QStringLiteral("XPC_FLAGS"));
    process.setProcessEnvironment(environment);
    process.setProcessChannelMode(QProcess::ForwardedChannels);
}

QString newWorkerServerName()
{
    return QStringLiteral("/tmp/loupe-%1-%2")
        .arg(QCoreApplication::applicationPid())
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
}

void prelaunchWorker(const QString& path)
{
#ifdef Q_OS_MACOS
    // A bundled worker is copied out of the bundle before each launch (see
    // ApplicationController::openFile); that stays the document's job there.
    Q_UNUSED(path);
#else
    discardPrelaunchedWorker();
    PrelaunchedWorker worker;
    worker.executable = defaultWorkerExecutable();
    worker.serverName = newWorkerServerName();
    worker.path = QFileInfo(path).absoluteFilePath();
    worker.requestId = kPrelaunchRequestId;
    worker.process = std::make_unique<QProcess>();
    configureWorkerProcess(*worker.process);
    worker.process->start(worker.executable,
                          {QStringLiteral("--server-name"), worker.serverName, QStringLiteral("--open"), worker.path,
                           QStringLiteral("--request-id"), QString::number(worker.requestId)});
    if (worker.process->state() == QProcess::NotRunning) return;
    prelaunched() = std::move(worker);
#endif
}

std::optional<PrelaunchedWorker> takePrelaunchedWorker(const QString& path)
{
    auto& worker = prelaunched();
    if (!worker || worker->path != QFileInfo(path).absoluteFilePath()) return std::nullopt;
    auto taken = std::exchange(worker, std::nullopt);
    if (taken->process->state() != QProcess::Running) return std::nullopt;
    return taken;
}

void discardPrelaunchedWorker()
{
    if (auto& worker = prelaunched()) {
        stop(*worker->process);
        worker.reset();
    }
}

} // namespace loupe::app::worker
