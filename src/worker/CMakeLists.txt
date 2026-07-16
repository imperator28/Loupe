qt_add_executable(loupe-worker
    main.cpp
    WorkerServer.cpp
    WorkerServer.h
    ImportSession.cpp
    ImportSession.h
)

target_link_libraries(loupe-worker PRIVATE loupe::core loupe::protocol Qt6::Core Qt6::Network)
loupe_enable_warnings(loupe-worker)

qt_add_executable(loupe-import-benchmark
    ImportBenchmark.cpp
)
target_link_libraries(loupe-import-benchmark PRIVATE loupe::protocol Qt6::Core Qt6::Network)
loupe_enable_warnings(loupe-import-benchmark)
