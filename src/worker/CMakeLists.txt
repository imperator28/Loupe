qt_add_executable(loupe-worker
    main.cpp
    WorkerServer.cpp
    WorkerServer.h
    ImportSession.cpp
    ImportSession.h
)

target_link_libraries(loupe-worker PRIVATE loupe::core loupe::protocol Qt6::Core Qt6::Network)
loupe_enable_warnings(loupe-worker)
if (MSVC)
    # Emit debug info (kept in a separate .pdb, no size cost to the .exe) and
    # link dbghelp so the crash handler in main.cpp can symbolicate its
    # backtrace. This turns an otherwise-silent worker crash into a logged
    # exception type, message, and stack — see LOUPE_CRASH_LOG.
    target_compile_options(loupe-worker PRIVATE /Zi)
    target_link_options(loupe-worker PRIVATE /DEBUG)
    target_link_libraries(loupe-worker PRIVATE Dbghelp)
endif()

qt_add_executable(loupe-import-benchmark
    ImportBenchmark.cpp
)
target_link_libraries(loupe-import-benchmark PRIVATE loupe::protocol Qt6::Core Qt6::Network)
loupe_enable_warnings(loupe-import-benchmark)
