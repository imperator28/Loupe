find_package(Catch2 3 CONFIG REQUIRED)

add_executable(loupe-core-tests
    core/test_bootstrap.cpp
    core/test_stable_id.cpp
)
target_link_libraries(loupe-core-tests PRIVATE Catch2::Catch2WithMain loupe::core)

include(Catch)
catch_discover_tests(loupe-core-tests)
