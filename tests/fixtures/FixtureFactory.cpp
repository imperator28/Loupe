#include "fixtures/FixtureFactory.h"

#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <DESTEP_Parameters.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <TDocStd_Document.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <BRep_Builder.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <stdexcept>

namespace loupe::tests {
namespace {

occ::handle<TDocStd_Document> makeDocument(const FixtureUnit unit)
{
    auto document = occ::handle<TDocStd_Document>();
    XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", document);
    XCAFDoc_DocumentTool::SetLengthUnit(
        document,
        unit == FixtureUnit::Inch ? 1.0 : 1.0,
        unit == FixtureUnit::Inch ? UnitsMethods_LengthUnit_Inch : UnitsMethods_LengthUnit_Millimeter);
    return document;
}

std::filesystem::path write(const occ::handle<TDocStd_Document>& document, const std::filesystem::path& file, const FixtureUnit unit)
{
    std::filesystem::create_directories(file.parent_path().empty() ? std::filesystem::current_path() : file.parent_path());
    STEPCAFControl_Writer writer;
    writer.SetNameMode(true);
    writer.SetColorMode(true);
    DESTEP_Parameters parameters;
    parameters.WriteUnit = unit == FixtureUnit::Inch ? UnitsMethods_LengthUnit_Inch : UnitsMethods_LengthUnit_Millimeter;
    if (!writer.Perform(document, file.string().c_str(), parameters)) {
        throw std::runtime_error("failed to write generated STEP fixture");
    }
    return file;
}

} // namespace

std::filesystem::path writeRepeatedBoxAssembly(const std::filesystem::path& file, const FixtureUnit unit)
{
    const auto document = makeDocument(unit);
    const auto shapes = XCAFDoc_DocumentTool::ShapeTool(document->Main());
    const TopoDS_Shape definition = BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape();
    const auto definitionLabel = shapes->AddShape(definition, false);
    BRep_Builder builder;
    TopoDS_Compound assembly;
    builder.MakeCompound(assembly);
    builder.Add(assembly, definition);
    gp_Trsf shift;
    shift.SetTranslation(gp_Vec(25.0, 0.0, 0.0));
    builder.Add(assembly, BRepBuilderAPI_Transform(definition, shift).Shape());
    const auto assemblyLabel = shapes->AddShape(assembly, true);
    (void)definitionLabel;
    (void)assemblyLabel;
    return write(document, file, unit);
}

std::filesystem::path writeTwoDefinitionAssembly(const std::filesystem::path& file)
{
    const auto document = makeDocument(FixtureUnit::Millimeter);
    const auto shapes = XCAFDoc_DocumentTool::ShapeTool(document->Main());
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape();
    const TopoDS_Shape cylinder = BRepPrimAPI_MakeCylinder(3.0, 17.0).Shape();
    shapes->AddShape(box, false);
    shapes->AddShape(cylinder, false);
    BRep_Builder builder;
    TopoDS_Compound assembly;
    builder.MakeCompound(assembly);
    builder.Add(assembly, box);
    gp_Trsf shift;
    shift.SetTranslation(gp_Vec(30.0, 0.0, 0.0));
    builder.Add(assembly, BRepBuilderAPI_Transform(cylinder, shift).Shape());
    shapes->AddShape(assembly, true);
    return write(document, file, FixtureUnit::Millimeter);
}

std::filesystem::path writeNestedBoxAssembly(const std::filesystem::path& file)
{
    const auto document = makeDocument(FixtureUnit::Millimeter);
    const auto shapes = XCAFDoc_DocumentTool::ShapeTool(document->Main());
    const auto box = shapes->AddShape(BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape(), false);
    const auto subassembly = shapes->NewShape();
    gp_Trsf componentShift;
    componentShift.SetTranslation(gp_Vec(10.0, 0.0, 0.0));
    shapes->AddComponent(subassembly, box, TopLoc_Location(componentShift));
    const auto root = shapes->NewShape();
    gp_Trsf assemblyShift;
    assemblyShift.SetTranslation(gp_Vec(20.0, 0.0, 0.0));
    shapes->AddComponent(root, subassembly, TopLoc_Location(assemblyShift));
    shapes->UpdateAssemblies();
    return write(document, file, FixtureUnit::Millimeter);
}

std::filesystem::path writeFlatTwoSolidStep(const std::filesystem::path& file)
{
    const auto document = makeDocument(FixtureUnit::Millimeter);
    const auto shapes = XCAFDoc_DocumentTool::ShapeTool(document->Main());
    shapes->AddShape(BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape(), false);
    shapes->AddShape(BRepPrimAPI_MakeBox(5.0, 5.0, 5.0).Shape(), false);
    return write(document, file, FixtureUnit::Millimeter);
}

std::filesystem::path writeSingleCylinderStep(const std::filesystem::path& file)
{
    const auto document = makeDocument(FixtureUnit::Millimeter);
    XCAFDoc_DocumentTool::ShapeTool(document->Main())->AddShape(
        BRepPrimAPI_MakeCylinder(5.0, 20.0).Shape(), false);
    return write(document, file, FixtureUnit::Millimeter);
}

// One free shape whose own definition is a compound of disjoint solids, i.e. a
// single STEP product/component containing multiple separately meaningful
// bodies (the shape a `PlanRequest::Body` split should apply to) -- distinct
// from writeFlatTwoSolidStep, where each solid is its own top-level free shape.
std::filesystem::path writeMultiSolidBodyStep(const std::filesystem::path& file)
{
    const auto document = makeDocument(FixtureUnit::Millimeter);
    const auto shapes = XCAFDoc_DocumentTool::ShapeTool(document->Main());
    BRep_Builder builder;
    TopoDS_Compound body;
    builder.MakeCompound(body);
    builder.Add(body, BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape());
    gp_Trsf shift;
    shift.SetTranslation(gp_Vec(30.0, 0.0, 0.0));
    builder.Add(body, BRepBuilderAPI_Transform(BRepPrimAPI_MakeBox(6.0, 6.0, 6.0).Shape(), shift).Shape());
    shapes->AddShape(body, false);
    return write(document, file, FixtureUnit::Millimeter);
}

} // namespace loupe::tests
