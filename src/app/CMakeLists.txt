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
    cache/OverrideStore.cpp
    cache/OverrideStore.h
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
    render/DefinitionInstances.cpp
    render/DefinitionInstances.h
    render/SceneModel.cpp
    render/SceneModel.h
)

qt_add_qml_module(loupe-app
    URI Loupe.App
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/OpenStepDialog.qml
        qml/Theme.qml
        qml/inspect/InspectWorkspace.qml
        qml/inspect/InspectionDock.qml
        qml/inspect/ContextPanel.qml
        qml/inspect/MeasurementPanel.qml
        qml/inspect/SectionPanel.qml
        qml/inspect/CapturePanel.qml
)

target_link_libraries(loupe-app PRIVATE loupe::materials loupe::protocol Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Quick3D Qt6::Network Qt6::Sql)
target_include_directories(loupe-app PRIVATE "${PROJECT_SOURCE_DIR}/src")
loupe_enable_warnings(loupe-app)
