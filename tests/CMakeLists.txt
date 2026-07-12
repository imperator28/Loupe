find_package(Catch2 3 CONFIG REQUIRED)

add_executable(loupe-core-tests
    core/test_bootstrap.cpp
    core/test_stable_id.cpp
    core/test_unit_policy.cpp
    core/test_step_import.cpp
    core/test_export_plan.cpp
    core/test_selected_export.cpp
    core/test_output_validation.cpp
    core/test_spike_cli.cpp
    fixtures/FixtureFactory.cpp
)
target_link_libraries(loupe-core-tests PRIVATE Catch2::Catch2WithMain loupe::core)
target_include_directories(loupe-core-tests PRIVATE "${PROJECT_SOURCE_DIR}/tests")
target_compile_definitions(loupe-core-tests PRIVATE LOUPE_SPIKE_PATH="$<TARGET_FILE:loupe-spike>")
add_dependencies(loupe-core-tests loupe-spike)
loupe_enable_warnings(loupe-core-tests)

include(Catch)
catch_discover_tests(loupe-core-tests)

add_executable(loupe-protocol-tests
    protocol/test_protocol.cpp
)
target_link_libraries(loupe-protocol-tests PRIVATE Qt6::Test loupe::protocol)
target_include_directories(loupe-protocol-tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
loupe_enable_warnings(loupe-protocol-tests)

add_test(NAME protocol COMMAND loupe-protocol-tests)

add_executable(loupe-worker-process-tests
    worker/test_worker_process.cpp
)
target_link_libraries(loupe-worker-process-tests PRIVATE Qt6::Core Qt6::Network Qt6::Test)
target_compile_definitions(loupe-worker-process-tests PRIVATE LOUPE_WORKER_PATH="$<TARGET_FILE:loupe-worker>")
add_dependencies(loupe-worker-process-tests loupe-worker)
loupe_enable_warnings(loupe-worker-process-tests)

add_test(NAME worker-process COMMAND loupe-worker-process-tests)
