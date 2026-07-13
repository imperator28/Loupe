find_package(Catch2 3 CONFIG REQUIRED)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_loupe_qt_runtime_root "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/Qt6")
    set(_loupe_qt_runtime_bin "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin")
else()
    set(_loupe_qt_runtime_root "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/Qt6")
    set(_loupe_qt_runtime_bin "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
endif()

add_executable(loupe-core-tests
    core/test_bootstrap.cpp
    core/test_stable_id.cpp
    core/test_unit_policy.cpp
    core/test_step_import.cpp
    core/test_export_plan.cpp
    core/test_selected_export.cpp
    core/test_output_validation.cpp
    core/test_geometry_analysis.cpp
    core/test_material_properties.cpp
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
    fixtures/FixtureFactory.cpp
)
target_link_libraries(loupe-worker-process-tests PRIVATE Qt6::Core Qt6::Network Qt6::Test loupe::core)
target_include_directories(loupe-worker-process-tests PRIVATE "${PROJECT_SOURCE_DIR}/tests")
target_compile_definitions(loupe-worker-process-tests PRIVATE LOUPE_WORKER_PATH="$<TARGET_FILE:loupe-worker>")
add_dependencies(loupe-worker-process-tests loupe-worker)
loupe_enable_warnings(loupe-worker-process-tests)

add_test(NAME worker-process COMMAND loupe-worker-process-tests)

add_executable(loupe-application-controller-tests
    app/test_application_controller.cpp
    fixtures/FixtureFactory.cpp
    "${PROJECT_SOURCE_DIR}/src/app/ApplicationController.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/ApplicationController.h"
    "${PROJECT_SOURCE_DIR}/src/app/models/AssemblyTreeModel.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/models/AssemblyTreeModel.h"
    "${PROJECT_SOURCE_DIR}/src/app/worker/WorkerClient.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/worker/WorkerClient.h"
    "${PROJECT_SOURCE_DIR}/src/app/tools/MeasurementController.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/tools/MeasurementController.h"
    "${PROJECT_SOURCE_DIR}/src/app/tools/CaptureController.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/tools/CaptureController.h"
    "${PROJECT_SOURCE_DIR}/src/app/tools/SectionController.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/tools/SectionController.h"
)
target_link_libraries(loupe-application-controller-tests PRIVATE loupe::core loupe::protocol Qt6::Core Qt6::Gui Qt6::Network Qt6::Test)
target_include_directories(loupe-application-controller-tests PRIVATE "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/tests")
target_compile_definitions(loupe-application-controller-tests PRIVATE LOUPE_WORKER_PATH="$<TARGET_FILE:loupe-worker>")
add_dependencies(loupe-application-controller-tests loupe-worker)
loupe_enable_warnings(loupe-application-controller-tests)

add_test(NAME application-controller COMMAND loupe-application-controller-tests)
set_tests_properties(application-controller PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=minimal;QT_PLUGIN_PATH=${_loupe_qt_runtime_root}/plugins"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_loupe_qt_runtime_bin}"
)

add_executable(loupe-worker-client-tests
    app/test_worker_client.cpp
    "${PROJECT_SOURCE_DIR}/src/app/worker/WorkerClient.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/worker/WorkerClient.h"
)
target_link_libraries(loupe-worker-client-tests PRIVATE loupe::protocol Qt6::Core Qt6::Network Qt6::Test)
target_include_directories(loupe-worker-client-tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
loupe_enable_warnings(loupe-worker-client-tests)

add_test(NAME worker-client COMMAND loupe-worker-client-tests)

add_executable(loupe-qml-smoke-tests
    qml/qml_smoke.cpp
    "${PROJECT_SOURCE_DIR}/src/app/ApplicationController.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/ApplicationController.h"
    "${PROJECT_SOURCE_DIR}/src/app/tools/MeasurementController.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/tools/MeasurementController.h"
    "${PROJECT_SOURCE_DIR}/src/app/tools/CaptureController.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/tools/CaptureController.h"
    "${PROJECT_SOURCE_DIR}/src/app/tools/SectionController.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/tools/SectionController.h"
)
target_link_libraries(loupe-qml-smoke-tests PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)
target_include_directories(loupe-qml-smoke-tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(loupe-qml-smoke-tests PRIVATE
    LOUPE_MAIN_QML_PATH="${PROJECT_SOURCE_DIR}/src/app/qml/Main.qml"
    LOUPE_INSPECT_QML_DIR="${PROJECT_SOURCE_DIR}/src/app/qml/inspect"
)
loupe_enable_warnings(loupe-qml-smoke-tests)

add_test(NAME qml-smoke COMMAND loupe-qml-smoke-tests)
set_tests_properties(qml-smoke PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=minimal;QT_PLUGIN_PATH=${_loupe_qt_runtime_root}/plugins;QML2_IMPORT_PATH=${_loupe_qt_runtime_root}/qml"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_loupe_qt_runtime_bin}"
)

add_executable(loupe-qml-tests
    qml/qml_tests_main.cpp
)
target_link_libraries(loupe-qml-tests PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::QuickTest)
target_compile_definitions(loupe-qml-tests PRIVATE QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
loupe_enable_warnings(loupe-qml-tests)

add_test(NAME qml-inspection-tools COMMAND loupe-qml-tests)
set_tests_properties(qml-inspection-tools PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=minimal;QT_QUICK_BACKEND=software;QT_PLUGIN_PATH=${_loupe_qt_runtime_root}/plugins;QML2_IMPORT_PATH=${_loupe_qt_runtime_root}/qml"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_loupe_qt_runtime_bin}"
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

add_executable(loupe-scene-model-tests
    app/test_scene_model.cpp
    "${PROJECT_SOURCE_DIR}/src/app/render/MeshGeometry.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/render/MeshGeometry.h"
    "${PROJECT_SOURCE_DIR}/src/app/render/DefinitionInstances.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/render/DefinitionInstances.h"
    "${PROJECT_SOURCE_DIR}/src/app/render/SceneModel.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/render/SceneModel.h"
)
target_link_libraries(loupe-scene-model-tests PRIVATE Qt6::Core Qt6::Gui Qt6::Quick3D Qt6::Test)
target_include_directories(loupe-scene-model-tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
loupe_enable_warnings(loupe-scene-model-tests)

add_test(NAME scene-model COMMAND loupe-scene-model-tests)
set_tests_properties(scene-model PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=minimal;QT_PLUGIN_PATH=${_loupe_qt_runtime_root}/plugins;QML2_IMPORT_PATH=${_loupe_qt_runtime_root}/qml"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_loupe_qt_runtime_bin}"
)

add_executable(loupe-visibility-model-tests
    app/test_visibility_model.cpp
    "${PROJECT_SOURCE_DIR}/src/app/models/VisibilityModel.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/models/VisibilityModel.h"
)
target_link_libraries(loupe-visibility-model-tests PRIVATE Qt6::Core Qt6::Test)
target_include_directories(loupe-visibility-model-tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
loupe_enable_warnings(loupe-visibility-model-tests)

add_test(NAME visibility-model COMMAND loupe-visibility-model-tests)

add_executable(loupe-override-store-tests
    app/test_override_store.cpp
    "${PROJECT_SOURCE_DIR}/src/app/cache/OverrideStore.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/cache/OverrideStore.h"
    "${PROJECT_SOURCE_DIR}/src/app/models/UnitReviewModel.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/models/UnitReviewModel.h"
)
target_link_libraries(loupe-override-store-tests PRIVATE Qt6::Core Qt6::Gui Qt6::Sql Qt6::Test)
target_include_directories(loupe-override-store-tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
loupe_enable_warnings(loupe-override-store-tests)

add_test(NAME override-store COMMAND loupe-override-store-tests)
set_tests_properties(override-store PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=minimal;QT_PLUGIN_PATH=${_loupe_qt_runtime_root}/plugins"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_loupe_qt_runtime_bin}"
)

add_executable(loupe-inspection-tools-tests
    app/test_inspection_tools.cpp
    "${PROJECT_SOURCE_DIR}/src/app/tools/MeasurementController.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/tools/MeasurementController.h"
    "${PROJECT_SOURCE_DIR}/src/app/tools/CaptureController.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/tools/CaptureController.h"
    "${PROJECT_SOURCE_DIR}/src/app/tools/SectionController.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/tools/SectionController.h"
)
target_link_libraries(loupe-inspection-tools-tests PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
target_include_directories(loupe-inspection-tools-tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
loupe_enable_warnings(loupe-inspection-tools-tests)

add_test(NAME inspection-tools COMMAND loupe-inspection-tools-tests)
set_tests_properties(inspection-tools PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=minimal;QT_PLUGIN_PATH=${_loupe_qt_runtime_root}/plugins"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_loupe_qt_runtime_bin}"
)

add_executable(loupe-cache-store-tests
    app/test_cache_store.cpp
    "${PROJECT_SOURCE_DIR}/src/app/cache/CacheKey.h"
    "${PROJECT_SOURCE_DIR}/src/app/cache/CacheStore.cpp"
    "${PROJECT_SOURCE_DIR}/src/app/cache/CacheStore.h"
)
target_link_libraries(loupe-cache-store-tests PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
target_include_directories(loupe-cache-store-tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
loupe_enable_warnings(loupe-cache-store-tests)

add_test(NAME cache-store COMMAND loupe-cache-store-tests)
set_tests_properties(cache-store PROPERTIES
    ENVIRONMENT "QT_PLUGIN_PATH=${_loupe_qt_runtime_root}/plugins"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_loupe_qt_runtime_bin}"
)
