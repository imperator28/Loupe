add_library(loupe-core STATIC
    domain/StableId.cpp
    export/ExportPlan.cpp
    export/StepExporter.cpp
    export/StlExporter.cpp
    import/StepImporter.cpp
    inspection/GeometryAnalysis.cpp
    inspection/TopologyAnalysis.cpp
    report/EvidenceWriter.cpp
    units/UnitPolicy.cpp
    validation/OutputValidator.cpp
)
add_library(loupe::core ALIAS loupe-core)

add_library(loupe-materials STATIC
    inspection/MaterialProperties.cpp
    inspection/MaterialProperties.h
)
add_library(loupe::materials ALIAS loupe-materials)
target_include_directories(loupe-materials PUBLIC "${PROJECT_SOURCE_DIR}/src")
loupe_enable_warnings(loupe-materials)

target_include_directories(loupe-core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(loupe-core
    PUBLIC
        nlohmann_json::nlohmann_json
        xxHash::xxhash
        TKXCAF
        TKDESTEP
        TKCAF
        TKBRep
        TKPrim
        TKMesh
        TKDESTL
)

loupe_enable_warnings(loupe-core)
