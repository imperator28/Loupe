add_library(loupe-core INTERFACE)
add_library(loupe::core ALIAS loupe-core)

target_include_directories(loupe-core INTERFACE "${PROJECT_SOURCE_DIR}/src")
loupe_enable_warnings(loupe-core)
