#include "app/platform/TempHygiene.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

#if defined(Q_OS_UNIX)
#include <cerrno>
#include <csignal>
#endif

namespace loupe::app::platform {

TempSweep sweepStaleTempFiles(const QString& directory, const qint64 currentPid,
                              const std::function<bool(qint64)>& isRunning)
{
    TempSweep sweep;
    // loupe-worker-<pid>-<8 hex> and loupe-<pid>-<8 hex>: the pid is the Loupe that made it.
    static const QRegularExpression owned(QStringLiteral("^loupe-(?:worker-)?(\\d+)-[0-9a-fA-F]{8}$"));
    const auto entries = QDir(directory).entryInfoList(QDir::Files | QDir::System | QDir::Hidden | QDir::NoDotAndDotDot);
    for (const auto& entry : entries) {
        const auto match = owned.match(entry.fileName());
        if (!match.hasMatch()) continue;
        const auto pid = match.captured(1).toLongLong();
        if (pid == currentPid || isRunning(pid)) continue;
        if (QFile::remove(entry.absoluteFilePath())) ++sweep.removed;
    }
    return sweep;
}

bool processIsRunning(const qint64 pid)
{
#if defined(Q_OS_UNIX)
    if (pid <= 0) return false;
    // Signal 0 checks without sending: it fails with ESRCH when there is no such process.
    return ::kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
#else
    Q_UNUSED(pid);
    return true;
#endif
}

void sweepStaleTempFilesAtLaunch()
{
    const auto pid = QCoreApplication::applicationPid();
    // The importer copies and connections live directly in /tmp (see ApplicationController).
#if defined(Q_OS_UNIX)
    sweepStaleTempFiles(QStringLiteral("/tmp"), pid, processIsRunning);
#endif
    // A downloaded update the app was never restarted into, or the leftovers of one it was.
    const QFileInfo update(QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath(QStringLiteral("loupe-update")));
    if (update.isDir() && update.lastModified().secsTo(QDateTime::currentDateTime()) > 24 * 60 * 60)
        QDir(update.absoluteFilePath()).removeRecursively();
}

} // namespace loupe::app::platform
