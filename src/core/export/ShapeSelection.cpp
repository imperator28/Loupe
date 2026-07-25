#include "core/export/ShapeSelection.h"

#include <BRepBuilderAPI_Transform.hxx>
#include <TDF_Label.hxx>
#include <TDF_Tool.hxx>
#include <TDocStd_Document.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <algorithm>
#include <stdexcept>

namespace loupe::exporting::detail {
namespace {

[[nodiscard]] TDF_Label labelFor(const import::ImportResult& imported, const domain::AssemblyNode& node)
{
    TDF_Label label;
    TDF_Tool::Label(imported.native->document->GetData(), node.hierarchyPath.c_str(), label);
    if (label.IsNull()) throw std::runtime_error("selected export label was not found");
    return label;
}

} // namespace

const domain::AssemblyNode& selectedNode(const import::ImportResult& imported,
                                         const std::string_view nodeId)
{
    const auto found = std::ranges::find_if(
        imported.snapshot.nodes, [nodeId](const auto& node) { return node.id == nodeId; });
    if (found == imported.snapshot.nodes.end()) throw std::runtime_error("selected export node was not found");
    return *found;
}

TopoDS_Shape localShape(const import::ImportResult& imported, const domain::AssemblyNode& node)
{
    const auto shapes = XCAFDoc_DocumentTool::ShapeTool(imported.native->document->Main());
    TDF_Label label = labelFor(imported, node);
    if (XCAFDoc_ShapeTool::IsComponent(label)) XCAFDoc_ShapeTool::GetReferredShape(label, label);
    const TopoDS_Shape shape = shapes->GetShape(label);
    if (shape.IsNull()) throw std::runtime_error("selected export shape is empty");
    if (!node.subSolidIndex) return shape;
    int solidIndex = 0;
    for (TopExp_Explorer explorer(shape, TopAbs_SOLID); explorer.More(); explorer.Next(), ++solidIndex) {
        if (solidIndex == *node.subSolidIndex) return explorer.Current();
    }
    throw std::runtime_error("selected export sub-solid was not found");
}

TopoDS_Shape placedInAssembly(const TopoDS_Shape& shape, const domain::AssemblyNode& node)
{
    const auto& columns = node.placement.columnMajor;
    gp_Trsf transform;
    transform.SetValues(columns[0], columns[4], columns[8], columns[12],
                        columns[1], columns[5], columns[9], columns[13],
                        columns[2], columns[6], columns[10], columns[14]);
    return BRepBuilderAPI_Transform(shape, transform, true).Shape();
}

double nativeUnitRebase(const import::ImportResult& imported)
{
    double nativeMeters = 0.001;
    XCAFDoc_DocumentTool::GetLengthUnit(imported.native->document, nativeMeters);
    const auto declared = imported.unitEvidence.declaredRepresentationUnits.empty()
        ? units::LengthUnit::Millimeter
        : imported.unitEvidence.declaredRepresentationUnits.front();
    return nativeMeters * 1000.0 / units::millimetersPerUnit(declared);
}

TopoDS_Shape scaledAboutOrigin(const TopoDS_Shape& shape, const double factor)
{
    if (factor == 1.0) return shape;
    gp_Trsf scale;
    scale.SetScale(gp_Pnt(0.0, 0.0, 0.0), factor);
    return BRepBuilderAPI_Transform(shape, scale, true).Shape();
}

} // namespace loupe::exporting::detail
