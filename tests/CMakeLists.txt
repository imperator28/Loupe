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
