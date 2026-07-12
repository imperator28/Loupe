qt_add_executable(loupe-app
    main.cpp
    ApplicationController.cpp
    ApplicationController.h
)

qt_add_qml_module(loupe-app
    URI Loupe.App
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
)

target_link_libraries(loupe-app PRIVATE Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
target_include_directories(loupe-app PRIVATE "${PROJECT_SOURCE_DIR}/src")
loupe_enable_warnings(loupe-app)
