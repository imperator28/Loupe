#include "app/platform/StartupTrace.h"

#include <QElapsedTimer>
#include <QString>
#include <QtGlobal>

#include <cstdio>
#include <mutex>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace loupe::app::platform {
namespace {

struct Trace final {
    std::FILE* file{};
    std::mutex mutex;
#ifdef Q_OS_WIN
    ULONGLONG createdFileTime{};
#else
    QElapsedTimer sinceStart;
#endif

    Trace()
    {
        const auto path = qEnvironmentVariable("LOUPE_STARTUP_TRACE");
        if (path.isEmpty()) return;
#ifdef Q_OS_WIN
        if (::_wfopen_s(&file, reinterpret_cast<const wchar_t*>(path.utf16()), L"a") != 0) file = nullptr;
        FILETIME created{}, exited{}, kernel{}, user{};
        if (::GetProcessTimes(::GetCurrentProcess(), &created, &exited, &kernel, &user))
            createdFileTime = (static_cast<ULONGLONG>(created.dwHighDateTime) << 32) | created.dwLowDateTime;
#else
        file = std::fopen(path.toUtf8().constData(), "a");
        sinceStart.start();
#endif
    }

    double elapsedMs() const
    {
#ifdef Q_OS_WIN
        FILETIME now{};
        ::GetSystemTimePreciseAsFileTime(&now);
        const auto ticks = (static_cast<ULONGLONG>(now.dwHighDateTime) << 32) | now.dwLowDateTime;
        return static_cast<double>(ticks - createdFileTime) / 10'000.0;
#else
        return static_cast<double>(sinceStart.nsecsElapsed()) / 1.0e6;
#endif
    }
};

Trace& trace()
{
    static Trace instance;
    return instance;
}

} // namespace

bool startupTraceEnabled()
{
    return trace().file != nullptr;
}

void startupMark(const char* label)
{
    auto& t = trace();
    if (!t.file) return;
    const std::lock_guard lock(t.mutex);
    std::fprintf(t.file, "%9.1f %s\n", t.elapsedMs(), label);
    std::fflush(t.file);
}

} // namespace loupe::app::platform
