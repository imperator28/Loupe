qt_add_executable(loupe-app
    main.cpp
    ApplicationController.cpp
    ApplicationController.h
    export/ExportWorkspaceController.cpp
    export/ExportWorkspaceController.h
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
    render/SectionMeshBuilder.cpp
    render/SectionMeshBuilder.h
    render/CadEdgeGeometry.cpp
    render/CadEdgeGeometry.h
    render/DefinitionInstances.cpp
    render/DefinitionInstances.h
    render/SceneModel.cpp
    render/SceneModel.h
)

set(loupe_branding_dir "${PROJECT_SOURCE_DIR}/assets/branding")
set(loupe_app_icon_svg "${loupe_branding_dir}/loupe-app-icon.svg")
set(loupe_app_icon_png "${loupe_branding_dir}/loupe-app-icon.png")

qt_add_resources(loupe-app "loupe-branding"
    PREFIX "/branding"
    BASE "${loupe_branding_dir}"
    FILES
        "${loupe_app_icon_svg}"
        "${loupe_app_icon_png}"
)

# The shipped binary is named Loupe on every platform, not the target name.
set_target_properties(loupe-app PROPERTIES OUTPUT_NAME Loupe)

if(APPLE)
    set(loupe_app_icon_macos "${loupe_branding_dir}/Loupe.icns")
    set_source_files_properties("${loupe_app_icon_macos}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    target_sources(loupe-app PRIVATE "${loupe_app_icon_macos}")
    set_target_properties(loupe-app PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME Loupe
        MACOSX_BUNDLE_GUI_IDENTIFIER com.imperator28.Loupe
        MACOSX_BUNDLE_ICON_FILE Loupe.icns
    )
    target_sources(loupe-app PRIVATE platform/WindowChrome.h platform/WindowChrome.mm)
    find_library(loupe_appkit_framework AppKit REQUIRED)
    target_link_libraries(loupe-app PRIVATE ${loupe_appkit_framework})
else()
    target_sources(loupe-app PRIVATE platform/WindowChrome.h platform/WindowChrome.cpp)
    if(WIN32)
        set(loupe_app_icon_windows_rc "${loupe_branding_dir}/Loupe.rc")
        set_property(SOURCE "${loupe_app_icon_windows_rc}" APPEND PROPERTY
            OBJECT_DEPENDS "${loupe_branding_dir}/Loupe.ico"
        )
        target_sources(loupe-app PRIVATE "${loupe_app_icon_windows_rc}")
    endif()
endif()

qt_add_qml_module(loupe-app
    URI Loupe.App
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/OpenStepDialog.qml
        qml/Theme.qml
        qml/export/ExportBucket.qml
        qml/export/ExportComponentPicker.qml
        qml/export/ExportPreview.qml
        qml/export/ExportSettings.qml
        qml/export/ExportWorkspace.qml
        qml/inspect/InspectWorkspace.qml
        qml/inspect/ViewportNavigation.qml
        qml/inspect/PointerInputRouter.qml
        qml/inspect/ProjectionControl.qml
        qml/inspect/SectionManipulator.qml
        qml/inspect/SectionClipMaterial.qml
        qml/inspect/SectionOffsetSlider.qml
        qml/inspect/MeasurementFaceHighlight.qml
        qml/inspect/ViewportVisualTheme.qml
        qml/inspect/AboutLoupeDialog.qml
        qml/inspect/InteractionGuide.qml
        qml/inspect/ThemedButton.qml
        qml/inspect/ThemedMenu.qml
        qml/inspect/ThemedMenuItem.qml
        qml/inspect/ThemedMenuSeparator.qml
        qml/inspect/ThemedToolButton.qml
        qml/inspect/ThemedTabButton.qml
        qml/inspect/ThemedCheckBox.qml
        qml/inspect/ThemedColorButton.qml
        qml/inspect/ThemedComboBox.qml
        qml/inspect/ThemedProgressBar.qml
        qml/inspect/ThemedSegmentedControl.qml
        qml/inspect/ThemedSpinBox.qml
        qml/inspect/ThemedSwitch.qml
        qml/inspect/ThemedRadioButton.qml
        qml/inspect/ThemedTextField.qml
        qml/inspect/ThemedToolTip.qml
        qml/inspect/ToolIcon.qml
        qml/inspect/ViewCube.qml
        qml/inspect/ElevatedPanel.qml
        qml/inspect/StepViewport.qml
        qml/inspect/InspectionDock.qml
        qml/inspect/ContextPanel.qml
        qml/inspect/MeasurementPanel.qml
        qml/inspect/SectionPanel.qml
        qml/inspect/CapturePanel.qml
    RESOURCES
        qml/inspect/shaders/section-clip.frag
)

target_link_libraries(loupe-app PRIVATE loupe::core loupe::materials loupe::protocol PNG::PNG Qt6::Concurrent Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Quick3D Qt6::Network Qt6::Sql Qt6::Svg)
target_include_directories(loupe-app PRIVATE "${PROJECT_SOURCE_DIR}/src")
if(LOUPE_ENABLE_QML_PROFILING)
    target_compile_definitions(loupe-app PRIVATE QT_QML_DEBUG)
endif()
loupe_enable_warnings(loupe-app)
