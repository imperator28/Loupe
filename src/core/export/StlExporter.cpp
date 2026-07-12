#include "core/export/StlExporter.h"
#include "core/export/AtomicExportFile.h"

#include <BRepBuilderAPI_Transform.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <StlAPI_Writer.hxx>
#include <TDF_Tool.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <filesystem>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace loupe::exporting {
namespace {

struct Triangle { std::array<float, 12> values{}; std::array<char, 2> attribute{}; };

bool validTriangle(const Triangle& value)
{
    const auto& v = value.values;
    const float abx=v[6]-v[3], aby=v[7]-v[4], abz=v[8]-v[5], acx=v[9]-v[3], acy=v[10]-v[4], acz=v[11]-v[5];
    const float cx=aby*acz-abz*acy, cy=abz*acx-abx*acz, cz=abx*acy-aby*acx;
    const float area=cx*cx+cy*cy+cz*cz, normal=v[0]*v[0]+v[1]*v[1]+v[2]*v[2];
    return std::isfinite(area) && std::isfinite(normal) && area > 0.0F && normal > 0.0F;
}

void filterBinaryStl(const std::filesystem::path& source, const std::filesystem::path& destination)
{
    std::ifstream input(source, std::ios::binary); if (!input) throw std::runtime_error("unable to read STL payload");
    std::array<char, 80> header{}; std::uint32_t count{}; input.read(header.data(), 80); input.read(reinterpret_cast<char*>(&count), 4);
    std::vector<Triangle> kept; kept.reserve(count);
    for (std::uint32_t index{}; index < count; ++index) { Triangle triangle; input.read(reinterpret_cast<char*>(triangle.values.data()), 48); input.read(triangle.attribute.data(), 2); if (!input) throw std::runtime_error("invalid STL payload"); if (validTriangle(triangle)) kept.push_back(triangle); }
    if (kept.empty()) throw std::runtime_error("STL filtering removed every triangle");
    std::ofstream output(destination, std::ios::binary); if (!output) throw std::runtime_error("unable to write filtered STL payload");
    const auto written=static_cast<std::uint32_t>(kept.size()); output.write(header.data(),80); output.write(reinterpret_cast<const char*>(&written),4);
    for (const auto& triangle : kept) { output.write(reinterpret_cast<const char*>(triangle.values.data()),48); output.write(triangle.attribute.data(),2); }
    if (!output) throw std::runtime_error("unable to finalize filtered STL payload");
}

const domain::AssemblyNode& selectedNode(const import::ImportResult& imported, const OutputRow& output)
{
    const auto found = std::ranges::find_if(imported.snapshot.nodes, [&output](const auto& node) { return node.id == output.nodeId(); });
    if (found == imported.snapshot.nodes.end()) throw std::runtime_error("selected export node was not found");
    return *found;
}

TDF_Label labelFor(const import::ImportResult& imported, const domain::AssemblyNode& node)
{
    TDF_Label label;
    TDF_Tool::Label(imported.native->document->GetData(), node.hierarchyPath.c_str(), label);
    if (label.IsNull()) {
        throw std::runtime_error("selected export label was not found");
    }
    return label;
}

TopoDS_Shape selectedShape(const import::ImportResult& imported, const domain::AssemblyNode& node, const OutputRow& output)
{
    const auto shapes = XCAFDoc_DocumentTool::ShapeTool(imported.native->document->Main());
    TDF_Label label = labelFor(imported, node);
    if (XCAFDoc_ShapeTool::IsComponent(label)) XCAFDoc_ShapeTool::GetReferredShape(label, label);
    TopoDS_Shape shape = shapes->GetShape(label);
    if (shape.IsNull()) throw std::runtime_error("selected export shape is empty");

    gp_Trsf transform;
    transform.SetValues(node.placement.columnMajor[0], node.placement.columnMajor[4], node.placement.columnMajor[8], node.placement.columnMajor[12],
                        node.placement.columnMajor[1], node.placement.columnMajor[5], node.placement.columnMajor[9], node.placement.columnMajor[13],
                        node.placement.columnMajor[2], node.placement.columnMajor[6], node.placement.columnMajor[10], node.placement.columnMajor[14]);
    if (output.coordinates() == Coordinates::Assembly) shape = BRepBuilderAPI_Transform(shape, transform, true).Shape();
    double nativeMeters = 0.001;
    XCAFDoc_DocumentTool::GetLengthUnit(imported.native->document, nativeMeters);
    const auto declared = imported.unitEvidence.declaredRepresentationUnits.empty()
        ? units::LengthUnit::Millimeter : imported.unitEvidence.declaredRepresentationUnits.front();
    const double shapeScale = output.sourceToOutputScale()
        * (nativeMeters * 1000.0 / units::millimetersPerUnit(declared));
    if (shapeScale != 1.0) {
        gp_Trsf scale;
        scale.SetScale(gp_Pnt(0.0, 0.0, 0.0), shapeScale);
        shape = BRepBuilderAPI_Transform(shape, scale, true).Shape();
    }
    return shape;
}

} // namespace

ExportResult StlExporter::write(const import::ImportResult& imported, const OutputRow& output) const
{
    if (output.format() != Format::Stl) throw std::invalid_argument("STL exporter requires an STL output row");
    if (!imported.native || imported.native->document.IsNull()) throw std::runtime_error("import has no native XCAF document");
    const TopoDS_Shape shape = selectedShape(imported, selectedNode(imported, output), output);
    BRepMesh_IncrementalMesh mesher(shape, 0.1);
    if (!mesher.IsDone()) throw std::runtime_error("STL triangulation failed");

    const std::filesystem::path final(output.finalPath());
    std::filesystem::create_directories(final.parent_path());
    detail::AtomicExportFile partial(final);
    StlAPI_Writer writer;
    writer.ASCIIMode() = false;
    const auto raw = partial.partial().string() + ".raw";
    if (!writer.Write(shape, raw.c_str())) {
        throw std::runtime_error("STL writer failed");
    }
    try { filterBinaryStl(raw, partial.partial()); std::filesystem::remove(raw); } catch (...) { std::error_code ignored; std::filesystem::remove(raw, ignored); throw; }
    partial.finalize();
    return {true, 1, true, units::LengthUnit::Millimeter};
}

} // namespace loupe::exporting
