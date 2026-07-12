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

add_executable(loupe-application-controller-tests
    app/test_application_controller.cpp
    "${PROJECT_SOURCE_DIR}/src/app/ApplicationController.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/ApplicationController.h"
)
target_link_libraries(loupe-application-controller-tests PRIVATE Qt6::Core Qt6::Test)
target_include_directories(loupe-application-controller-tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
loupe_enable_warnings(loupe-application-controller-tests)

add_test(NAME application-controller COMMAND loupe-application-controller-tests)

add_executable(loupe-qml-smoke-tests
    qml/qml_smoke.cpp
    "${PROJECT_SOURCE_DIR}/src/app/ApplicationController.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/ApplicationController.h"
)
target_link_libraries(loupe-qml-smoke-tests PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)
target_include_directories(loupe-qml-smoke-tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(loupe-qml-smoke-tests PRIVATE LOUPE_MAIN_QML_PATH="${PROJECT_SOURCE_DIR}/src/app/qml/Main.qml")
loupe_enable_warnings(loupe-qml-smoke-tests)

add_test(NAME qml-smoke COMMAND loupe-qml-smoke-tests)
set(_loupe_qt_debug_root "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/Qt6")
set_tests_properties(qml-smoke PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=minimal;QT_PLUGIN_PATH=${_loupe_qt_debug_root}/plugins;QML2_IMPORT_PATH=${_loupe_qt_debug_root}/qml"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin"
)

add_executable(loupe-assembly-tree-tests
    app/test_assembly_tree_model.cpp
    "${PROJECT_SOURCE_DIR}/src/app/models/AssemblyTreeModel.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/models/AssemblyTreeModel.h"
    "${PROJECT_SOURCE_DIR}/src/app/models/SelectionModel.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/models/SelectionModel.h"
)
target_link_libraries(loupe-assembly-tree-tests PRIVATE Qt6::Core Qt6::Test)
target_include_directories(loupe-assembly-tree-tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
loupe_enable_warnings(loupe-assembly-tree-tests)

add_test(NAME assembly-tree COMMAND loupe-assembly-tree-tests)
