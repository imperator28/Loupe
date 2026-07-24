#include "core/export/StepExporter.h"
#include "core/export/AtomicExportFile.h"

#include <DESTEP_Parameters.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <TCollection_AsciiString.hxx>
#include <TDF_Tool.hxx>
#include <TDocStd_Document.hxx>
#include <TopoDS_Shape.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <gp_Trsf.hxx>
#include <gp_Pnt.hxx>

#include <filesystem>
#include <stdexcept>

namespace loupe::exporting {
namespace {

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

} // namespace

ExportResult StepExporter::write(const import::ImportResult& imported, const OutputRow& output) const
{
    if (output.format() != Format::Step) throw std::invalid_argument("STEP exporter requires a STEP output row");
    if (!imported.native || imported.native->document.IsNull()) throw std::runtime_error("import has no native XCAF document");
    const auto& node = selectedNode(imported, output);
    TopoDS_Shape shape = localShape(imported, node);
    if (output.coordinates() == Coordinates::Assembly) {
        gp_Trsf transform;
        transform.SetValues(node.placement.columnMajor[0], node.placement.columnMajor[4], node.placement.columnMajor[8], node.placement.columnMajor[12],
                            node.placement.columnMajor[1], node.placement.columnMajor[5], node.placement.columnMajor[9], node.placement.columnMajor[13],
                            node.placement.columnMajor[2], node.placement.columnMajor[6], node.placement.columnMajor[10], node.placement.columnMajor[14]);
        shape = BRepBuilderAPI_Transform(shape, transform, true).Shape();
    }
    double nativeMeters = 0.001;
    XCAFDoc_DocumentTool::GetLengthUnit(imported.native->document, nativeMeters);
    const auto declared = imported.unitEvidence.declaredRepresentationUnits.empty()
        ? units::LengthUnit::Millimeter : imported.unitEvidence.declaredRepresentationUnits.front();
    const double declaredMillimeters = units::millimetersPerUnit(declared);
    // OCCT may normalize source STEP coordinates into the XCAF native unit.  Rebase the
    // reviewed source->output factor once, preserving an explicit override's multiplier.
    const double shapeScale = output.sourceToOutputScale() * (nativeMeters * 1000.0 / declaredMillimeters);
    if (shapeScale != 1.0) {
        gp_Trsf scale;
        scale.SetScale(gp_Pnt(0.0, 0.0, 0.0), shapeScale);
        shape = BRepBuilderAPI_Transform(shape, scale, true).Shape();
    }

    occ::handle<TDocStd_Document> document;
    XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", document);
    const auto shapes = XCAFDoc_DocumentTool::ShapeTool(document->Main());
    shapes->AddShape(shape, false);
    const bool inches = output.stepOutputUnit() == StepOutputUnit::Inch;
    XCAFDoc_DocumentTool::SetLengthUnit(
        document, 1.0, inches ? UnitsMethods_LengthUnit_Inch : UnitsMethods_LengthUnit_Millimeter);

    const std::filesystem::path final(output.finalPath());
    std::filesystem::create_directories(final.parent_path());
    detail::AtomicExportFile partial(final);
    STEPCAFControl_Writer writer;
    DESTEP_Parameters parameters;
    parameters.WriteUnit = inches ? UnitsMethods_LengthUnit_Inch : UnitsMethods_LengthUnit_Millimeter;
    if (!writer.Perform(document, partial.partial().string().c_str(), parameters)) {
        throw std::runtime_error("STEP writer failed");
    }
    partial.finalize();
    return {true, 1, false, inches ? units::LengthUnit::Inch : units::LengthUnit::Millimeter};
}

} // namespace loupe::exporting
