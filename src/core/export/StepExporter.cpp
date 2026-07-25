#include "core/export/StepExporter.h"
#include "core/export/AtomicExportFile.h"
#include "core/export/ShapeSelection.h"

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

ExportResult StepExporter::write(const import::ImportResult& imported, const OutputRow& output) const
{
    if (output.format() != Format::Step) throw std::invalid_argument("STEP exporter requires a STEP output row");
    if (!imported.native || imported.native->document.IsNull()) throw std::runtime_error("import has no native XCAF document");
    const auto& node = detail::selectedNode(imported, output.nodeId());
    TopoDS_Shape shape = detail::localShape(imported, node);
    if (output.coordinates() == Coordinates::Assembly) shape = detail::placedInAssembly(shape, node);
    // OCCT may normalize source STEP coordinates into the XCAF native unit.  Rebase the
    // reviewed source->output factor once, preserving an explicit override's multiplier.
    shape = detail::scaledAboutOrigin(shape, output.sourceToOutputScale() * detail::nativeUnitRebase(imported));

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
