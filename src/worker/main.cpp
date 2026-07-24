#include "worker/WorkerServer.h"

#include <QCoreApplication>
#include <QUuid>

// --- Crash diagnostics -------------------------------------------------------
// The worker runs as a child process; a hard crash (WER exception 0xC0000409,
// FAST_FAIL_FATAL_APP_EXIT) means the CRT called abort()/terminate() — a C++
// exception reached std::terminate instead of the tessellation loop's catch
// blocks. These handlers capture the in-flight exception's real type/message
// and a backtrace so we can find the actual throw site. Output goes to the
// file named by LOUPE_CRASH_LOG (fallback: stderr) so it survives even when the
// parent does not forward the worker's stderr.
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <typeinfo>

#include <Standard_Failure.hxx>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#endif

namespace {

std::FILE* openCrashLog()
{
    const auto path = qEnvironmentVariable("LOUPE_CRASH_LOG");
    if (!path.isEmpty()) {
#ifdef _WIN32
        std::FILE* file = nullptr;
        if (::_wfopen_s(&file, reinterpret_cast<const wchar_t*>(path.utf16()), L"a") == 0 && file) {
            return file;
        }
#else
        if (std::FILE* file = std::fopen(path.toUtf8().constData(), "a")) return file;
#endif
    }
    return stderr;
}

void writeBacktrace(std::FILE* out)
{
#ifdef _WIN32
    void* frames[64];
    const USHORT count = ::CaptureStackBackTrace(0, 64, frames, nullptr);
    const HANDLE process = ::GetCurrentProcess();
    ::SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
    ::SymInitialize(process, nullptr, TRUE);
    alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO) + 256] = {};
    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(storage);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 255;
    for (USHORT i = 0; i < count; ++i) {
        const auto address = reinterpret_cast<DWORD64>(frames[i]);
        char moduleName[MAX_PATH] = "?";
        HMODULE module = nullptr;
        if (::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                 reinterpret_cast<LPCSTR>(frames[i]), &module) && module) {
            ::GetModuleFileNameA(module, moduleName, MAX_PATH);
        }
        DWORD64 displacement = 0;
        if (::SymFromAddr(process, address, &displacement, symbol)) {
            std::fprintf(out, "  #%02u %s!%s+0x%llx\n", i, moduleName, symbol->Name,
                         static_cast<unsigned long long>(displacement));
        } else {
            std::fprintf(out, "  #%02u %s @ 0x%llx\n", i, moduleName, static_cast<unsigned long long>(address));
        }
    }
    ::SymCleanup(process);
#else
    (void)out;
#endif
}

void reportInFlightException(std::FILE* out)
{
    if (auto current = std::current_exception()) {
        try {
            std::rethrow_exception(current);
        } catch (const Standard_Failure& failure) {
            std::fprintf(out, "in-flight OCCT Standard_Failure [%s]: %s\n",
                         typeid(failure).name(),
                         failure.GetMessageString() ? failure.GetMessageString() : "(no message)");
        } catch (const std::exception& error) {
            std::fprintf(out, "in-flight std::exception [%s]: %s\n", typeid(error).name(), error.what());
        } catch (...) {
            std::fprintf(out, "in-flight non-standard exception (unknown type)\n");
        }
    } else {
        std::fprintf(out, "no in-flight exception (direct abort)\n");
    }
}

[[noreturn]] void onTerminate()
{
    std::FILE* out = openCrashLog();
    std::fprintf(out, "\n==== [loupe-worker] std::terminate ====\n");
    reportInFlightException(out);
    writeBacktrace(out);
    std::fflush(out);
    std::_Exit(3);
}

void onAbort(int)
{
    std::FILE* out = openCrashLog();
    std::fprintf(out, "\n==== [loupe-worker] SIGABRT ====\n");
    reportInFlightException(out);
    writeBacktrace(out);
    std::fflush(out);
    std::_Exit(3);
}

} // namespace

int main(int argc, char* argv[])
{
    std::set_terminate(onTerminate);
    std::signal(SIGABRT, onAbort);

    QCoreApplication application(argc, argv);
    QString serverName;
    const auto arguments = application.arguments();
    const auto option = arguments.indexOf(QStringLiteral("--server-name"));
    if (option >= 0 && option + 1 < arguments.size()) {
        serverName = arguments.at(option + 1);
    }
    if (serverName.isEmpty()) {
        serverName = QStringLiteral("loupe-%1-%2").arg(qEnvironmentVariable("USERNAME"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    }

    loupe::worker::WorkerServer server;
    if (!server.listen(serverName)) {
        return 2;
    }
    return application.exec();
}
