qt_add_executable(loupe-app
    main.cpp
    ApplicationController.cpp
    ApplicationController.h
    models/AssemblyTreeModel.cpp
    models/AssemblyTreeModel.h
    models/SelectionModel.cpp
    models/SelectionModel.h
    models/VisibilityModel.cpp
    models/VisibilityModel.h
    models/UnitReviewModel.cpp
    models/UnitReviewModel.h
    models/MaterialLibraryModel.cpp
    models/MaterialLibraryModel.h
    models/ThemePreference.cpp
    models/ThemePreference.h
    cache/OverrideStore.cpp
    cache/OverrideStore.h
    cache/MaterialStore.cpp
    cache/MaterialStore.h
    cache/CacheKey.h
    cache/CacheStore.cpp
    cache/CacheStore.h
    cache/SourceFingerprint.cpp
    cache/SourceFingerprint.h
    worker/WorkerClient.cpp
    worker/WorkerClient.h
    tools/MeasurementController.cpp
    tools/MeasurementController.h
    tools/CaptureController.cpp
    tools/CaptureController.h
    tools/SectionController.cpp
    tools/SectionController.h
    render/MeshGeometry.cpp
    render/MeshGeometry.h
    render/CadEdgeGeometry.cpp
    render/CadEdgeGeometry.h
    render/DefinitionInstances.cpp
    render/DefinitionInstances.h
    render/SceneModel.cpp
    render/SceneModel.h
)

if(APPLE)
    set_target_properties(loupe-app PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME Loupe
        MACOSX_BUNDLE_GUI_IDENTIFIER com.imperator28.Loupe
        OUTPUT_NAME Loupe
    )
endif()

qt_add_qml_module(loupe-app
    URI Loupe.App
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/OpenStepDialog.qml
        qml/Theme.qml
        qml/inspect/InspectWorkspace.qml
        qml/inspect/ViewportNavigation.qml
        qml/inspect/ViewportVisualTheme.qml
        qml/inspect/ThemedButton.qml
        qml/inspect/ThemedToolButton.qml
        qml/inspect/ThemedTabButton.qml
        qml/inspect/StepViewport.qml
        qml/inspect/InspectionDock.qml
        qml/inspect/ContextPanel.qml
        qml/inspect/MeasurementPanel.qml
        qml/inspect/SectionPanel.qml
        qml/inspect/CapturePanel.qml
)

target_link_libraries(loupe-app PRIVATE loupe::materials loupe::protocol PNG::PNG Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Quick3D Qt6::Network Qt6::Sql)
target_include_directories(loupe-app PRIVATE "${PROJECT_SOURCE_DIR}/src")
loupe_enable_warnings(loupe-app)
