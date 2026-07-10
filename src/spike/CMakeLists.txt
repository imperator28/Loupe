add_executable(loupe-spike main.cpp)
target_link_libraries(loupe-spike PRIVATE loupe::core)
loupe_enable_warnings(loupe-spike)
