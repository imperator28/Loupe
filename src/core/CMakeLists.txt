add_library(loupe-core STATIC
    domain/StableId.cpp
    drawing/DrawingContour.cpp
    drawing/DrawingPlan.cpp
    drawing/DrawingProjector.cpp
    drawing/DrawingValidator.cpp
    drawing/DrawingWriter.cpp
    drawing/DxfWriter.cpp
    drawing/PdfWriter.cpp
    drawing/SvgWriter.cpp
    export/ExportPlan.cpp
    export/OutputNaming.cpp
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
        # 2D drawing export: TKHLR provides hidden-line removal (the projection
        # engine), TKGeomBase the exact spline decomposition and controlled
        # tessellation, TKShHealing the edge-to-wire stitching, TKTopAlgo the
        # BRepLib plane the HLR result is expressed on. All four already arrive
        # transitively through TKXCAF -> TKVCAF -> TKV3d and are already packaged
        # in releases; listing them makes the dependency explicit rather than
        # incidental. No vcpkg.json change and no new shipped library.
        TKHLR
        TKGeomBase
        TKShHealing
        TKTopAlgo
    PRIVATE
        Qt6::Core
)

loupe_enable_warnings(loupe-core)
