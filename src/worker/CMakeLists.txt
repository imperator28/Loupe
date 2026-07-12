qt_add_executable(loupe-worker
    main.cpp
    WorkerServer.cpp
    WorkerServer.h
    ImportSession.cpp
    ImportSession.h
)

target_link_libraries(loupe-worker PRIVATE loupe::protocol Qt6::Core Qt6::Network)
loupe_enable_warnings(loupe-worker)
