# mimalloc for the import worker, where nearly all of opening a file allocates: STEP parsing,
# B-rep, meshing. It replaces malloc for the whole worker process -- statically on macOS, where
# it becomes the default malloc zone, and through mimalloc's redirect DLL on Windows (which
# must ship beside mimalloc.dll; scripts/package/stage-windows.sh checks it does). The app
# keeps the system allocator. The worker reports which allocator is in effect in its import
# metrics, so a build where the override did not take is caught (tests/worker).
option(LOUPE_WORKER_MIMALLOC "Use mimalloc in the import worker where it is available" ON)
if (LOUPE_WORKER_MIMALLOC)
    find_package(mimalloc CONFIG QUIET)
endif()

set(loupe_worker_sources
    main.cpp
    WorkerServer.cpp
    WorkerServer.h
    ImportSession.cpp
    ImportSession.h
)

function(loupe_add_worker target use_mimalloc)
    qt_add_executable(${target} ${loupe_worker_sources})
    if (use_mimalloc)
        # First, so its malloc is the one the worker's own code and the static libraries bind to.
        target_link_libraries(${target} PRIVATE $<IF:$<TARGET_EXISTS:mimalloc-static>,mimalloc-static,mimalloc>)
        target_compile_definitions(${target} PRIVATE LOUPE_WORKER_MIMALLOC=1)
        if (WIN32)
            # The redirect takes effect only when mimalloc.dll comes first in the executable's
            # import table, which the link order does not guarantee: mimalloc's own minject
            # moves it there. Without it the worker runs on the system allocator, and says so.
            find_program(LOUPE_MINJECT minject
                HINTS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/mimalloc" NO_DEFAULT_PATH)
            if (LOUPE_MINJECT)
                add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND "${LOUPE_MINJECT}" --force --inplace "$<TARGET_FILE:${target}>"
                    COMMENT "Putting mimalloc.dll first in ${target}'s imports")
            else()
                message(WARNING "minject not found: ${target} may not get mimalloc's malloc")
            endif()
        endif()
    endif()
    target_link_libraries(${target} PRIVATE loupe::core loupe::protocol Qt6::Core Qt6::Network)
    loupe_enable_warnings(${target})
    if (MSVC)
        # Emit debug info (kept in a separate .pdb, no size cost to the .exe) and
        # link dbghelp so the crash handler in main.cpp can symbolicate its
        # backtrace. This turns an otherwise-silent worker crash into a logged
        # exception type, message, and stack — see LOUPE_CRASH_LOG.
        target_compile_options(${target} PRIVATE /Zi)
        target_link_options(${target} PRIVATE /DEBUG)
        target_link_libraries(${target} PRIVATE Dbghelp)
    endif()
endfunction()

loupe_add_worker(loupe-worker "${mimalloc_FOUND}")
set_property(GLOBAL PROPERTY LOUPE_WORKER_USES_MIMALLOC "${mimalloc_FOUND}")
if (mimalloc_FOUND AND LOUPE_BUILD_TESTS)
    # The same worker on the system allocator, for the allocator benchmark (scripts/benchmark).
    loupe_add_worker(loupe-worker-system-allocator FALSE)
endif()

qt_add_executable(loupe-import-benchmark
    ImportBenchmark.cpp
)
target_link_libraries(loupe-import-benchmark PRIVATE loupe::protocol Qt6::Core Qt6::Network)
loupe_enable_warnings(loupe-import-benchmark)
