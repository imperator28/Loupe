#pragma once

#include <QString>

#include <cstdint>
#include <memory>
#include <optional>

class QProcess;

namespace loupe::app::worker {

// The importer beside the app, or in the build tree when run from there.
[[nodiscard]] QString defaultWorkerExecutable();
// Environment and channels every importer process is started with.
void configureWorkerProcess(QProcess& process);
// A fresh local-socket name for one importer.
[[nodiscard]] QString newWorkerServerName();

// The file Loupe was launched with is handed to an importer at the top of main(), so the
// import runs while the window is still being built instead of after it. The first
// document to open that file takes the importer over; it is already importing, as the
// request below, and keeps the events it produced until a client connects.
struct PrelaunchedWorker final {
    std::unique_ptr<QProcess> process;
    QString executable;
    QString serverName;
    QString path;
    std::uint64_t requestId{};
};

void prelaunchWorker(const QString& path);
// The importer started for this file, if it is still running and nobody took it yet.
[[nodiscard]] std::optional<PrelaunchedWorker> takePrelaunchedWorker(const QString& path);
// Stops an importer no document took (the file was refused before reaching one).
void discardPrelaunchedWorker();

} // namespace loupe::app::worker
