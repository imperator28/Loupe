qt_add_library(loupe-protocol STATIC
    ProtocolJson.cpp
    ProtocolTypes.h
)
add_library(loupe::protocol ALIAS loupe-protocol)

target_include_directories(loupe-protocol PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(loupe-protocol PUBLIC Qt6::Core)

loupe_enable_warnings(loupe-protocol)
