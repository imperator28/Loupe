cmake_minimum_required(VERSION 3.28)

project(Loupe VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LOUPE_BUILD_TESTS "Build Loupe tests" ON)
option(LOUPE_ENABLE_QML_PROFILING "Enable the QML profiler/debug services" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(LoupeWarnings)

find_package(OpenCASCADE CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(xxHash CONFIG REQUIRED)
find_package(PNG REQUIRED)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Concurrent Gui Qml Quick QuickControls2 Quick3D Network Test QuickTest Sql)
qt_standard_project_setup(REQUIRES 6.8)

add_subdirectory(src/core)
add_subdirectory(src/spike)
add_subdirectory(src/protocol)
add_subdirectory(src/worker)
add_subdirectory(src/app)

if(LOUPE_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()
