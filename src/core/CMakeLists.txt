add_library(loupe-core STATIC domain/StableId.cpp)
add_library(loupe::core ALIAS loupe-core)

target_include_directories(loupe-core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(loupe-core
    PUBLIC
        nlohmann_json::nlohmann_json
        xxHash::xxhash
)

loupe_enable_warnings(loupe-core)
