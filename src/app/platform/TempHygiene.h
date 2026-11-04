#pragma once

#include <QString>

#include <functional>

namespace loupe::app::platform {

// What a Loupe that did not exit cleanly (a crash, a force quit) leaves behind, cleared on
// the next launch. A clean exit removes these itself:
//   loupe-worker-<pid>-<id>   macOS: the copy of the importer each tab launches from
//   loupe-<pid>-<id>          the connection to that importer (macOS, Linux)
//   loupe-update/             a downloaded update, once it is a day old
// Only entries whose Loupe process is gone are removed, so a second running Loupe keeps its own.
struct TempSweep {
    int removed{};
};

// The sweep over one directory, with how to tell a running process; for the app and for tests.
TempSweep sweepStaleTempFiles(const QString& directory, qint64 currentPid,
                              const std::function<bool(qint64)>& processIsRunning);
// Whether a process with this id is running (always true where it cannot be told).
[[nodiscard]] bool processIsRunning(qint64 pid);
// The launch-time sweep of the system temp folder and the update folder.
void sweepStaleTempFilesAtLaunch();

} // namespace loupe::app::platform
