#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/export/AssemblyExporter.h"
#include "core/import/StepImporter.h"

#include <BRepBndLib.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <Bnd_Box.hxx>
#include <Quantity_Color.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <TopLoc_Location.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <filesystem>
#include <string>

namespace {

using loupe::domain::NodeKind;

class ScopedDirectory {
public:
    explicit ScopedDirectory(const std::string& name)
        : path_(std::filesystem::temp_directory_path() / ("loupe-assembly-export-" + name))
    {
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }
    ~ScopedDirectory() { std::filesystem::remove_all(path_); }
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

TDF_Label named(const TDF_Label& label, const char* name)
{
    TDataStd_Name::Set(label, TCollection_ExtendedString(name));
    return label;
}

TopLoc_Location at(const double x, const double y = 0.0)
{
    gp_Trsf shift;
    shift.SetTranslation(gp_Vec(x, y, 0.0));
    return TopLoc_Location(shift);
}

// A supplier-style assembly: a red Bracket, a Fasteners subassembly holding two Screws,
// and a Cover. Stripping the fasteners must leave Bracket and Cover in place, coloured,
// and drop the then-empty Fasteners subassembly.
std::filesystem::path writeSupplierAssembly(const std::filesystem::path& file)
{
    occ::handle<TDocStd_Document> document;
    XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", document);
    XCAFDoc_DocumentTool::SetLengthUnit(document, 1.0, UnitsMethods_LengthUnit_Millimeter);
    const auto shapes = XCAFDoc_DocumentTool::ShapeTool(document->Main());
    const auto colors = XCAFDoc_DocumentTool::ColorTool(document->Main());

    const auto bracket = named(shapes->AddShape(BRepPrimAPI_MakeBox(40.0, 20.0, 5.0).Shape(), false), "Bracket");
    colors->SetColor(bracket, Quantity_Color(0.8, 0.1, 0.1, Quantity_TOC_RGB), XCAFDoc_ColorGen);
    const auto screw = named(shapes->AddShape(BRepPrimAPI_MakeCylinder(1.5, 10.0).Shape(), false), "Screw");
    const auto cover = named(shapes->AddShape(BRepPrimAPI_MakeBox(40.0, 20.0, 1.0).Shape(), false), "Cover");

    const auto fasteners = named(shapes->NewShape(), "Fasteners");
    shapes->AddComponent(fasteners, screw, at(5.0, 5.0));
    shapes->AddComponent(fasteners, screw, at(35.0, 5.0));

    const auto root = named(shapes->NewShape(), "Drawer");
    shapes->AddComponent(root, bracket, at(0.0));
    shapes->AddComponent(root, fasteners, at(0.0));
    shapes->AddComponent(root, cover, at(100.0));
    shapes->UpdateAssemblies();

    STEPCAFControl_Writer writer;
    writer.SetNameMode(true);
    writer.SetColorMode(true);
    REQUIRE(writer.Perform(document, file.string().c_str()));
    return file;
}

std::vector<std::string> namesOfKind(const loupe::import::ImportResult& imported, const NodeKind kind)
{
    std::vector<std::string> names;
    for (const auto& node : imported.snapshot.nodes)
        if (node.kind == kind) names.push_back(node.name);
    std::sort(names.begin(), names.end());
    return names;
}

} // namespace

TEST_CASE("an assembly re-exports without its hidden parts, keeping its tree", "[export][assembly]")
{
    ScopedDirectory directory("strip");
    const auto source = writeSupplierAssembly(directory.path() / "supplier.step");
    const auto imported = loupe::import::StepImporter{}.read(source);
    REQUIRE(namesOfKind(imported, NodeKind::Occurrence) == std::vector<std::string>{"Bracket", "Cover", "Screw", "Screw"});

    loupe::exporting::AssemblyExportRequest request;
    request.destination = directory.path() / "stripped.step";
    for (const auto& node : imported.snapshot.nodes)
        if (node.name == "Screw" && node.kind == NodeKind::Occurrence) request.excludedNodeIds.insert(node.id);
    REQUIRE(request.excludedNodeIds.size() == 2);

    const auto result = loupe::exporting::AssemblyExporter{}.write(imported, request);
    CHECK(result.partCount == 2);
    CHECK(result.removedCount == 2);

    const auto stripped = loupe::import::StepImporter{}.read(request.destination);
    // The parts that were kept, by name; the screws gone; and the Fasteners subassembly,
    // empty without them, dropped rather than left as an empty node.
    CHECK(namesOfKind(stripped, NodeKind::Occurrence) == std::vector<std::string>{"Bracket", "Cover"});
    CHECK(namesOfKind(stripped, NodeKind::Subassembly).empty());
    CHECK(namesOfKind(stripped, NodeKind::Root) == std::vector<std::string>{"Drawer"});

    // Placements kept: the Cover still sits 100 mm along x.
    const auto& native = *stripped.native;
    bool coverFound = false;
    for (std::size_t index = 0; index < native.shapes.size(); ++index) {
        Bnd_Box bounds;
        BRepBndLib::Add(native.shapes[index].Moved(TopLoc_Location(native.shapePlacements[index])), bounds);
        double xMin, yMin, zMin, xMax, yMax, zMax;
        bounds.Get(xMin, yMin, zMin, xMax, yMax, zMax);
        if (zMax - zMin < 2.0) {
            coverFound = true;
            CHECK(xMin == Catch::Approx(100.0).margin(1e-3));
        }
    }
    CHECK(coverFound);

    // Colours kept: the Bracket is still red.
    const auto colors = XCAFDoc_DocumentTool::ColorTool(native.document->Main());
    bool redFound = false;
    for (std::size_t index = 0; index < native.definitionLabels.size(); ++index) {
        Quantity_Color color;
        if (colors->GetColor(native.definitionLabels[index], XCAFDoc_ColorGen, color)
            || colors->GetColor(native.definitionLabels[index], XCAFDoc_ColorSurf, color)) {
            redFound = redFound || (color.Red() > 0.5 && color.Green() < 0.2);
        }
    }
    CHECK(redFound);
}

TEST_CASE("an inch file re-exports in inches at true size", "[export][assembly][inch]")
{
    ScopedDirectory directory("inch");
    const auto source = writeSupplierAssembly(directory.path() / "supplier.step");
    const auto imported = loupe::import::StepImporter{}.read(source);
    loupe::exporting::AssemblyExportRequest request;
    request.destination = directory.path() / "inches.step";
    request.outputUnit = loupe::units::LengthUnit::Inch;
    static_cast<void>(loupe::exporting::AssemblyExporter{}.write(imported, request));

    const auto reread = loupe::import::StepImporter{}.read(request.destination);
    CHECK(loupe::units::sourceUnit(reread.unitEvidence) == loupe::units::LengthUnit::Inch);
    // Still 40 mm long once OCCT reads it back into millimetres.
    Bnd_Box bounds;
    for (std::size_t index = 0; index < reread.native->shapes.size(); ++index)
        BRepBndLib::Add(reread.native->shapes[index].Moved(TopLoc_Location(reread.native->shapePlacements[index])), bounds);
    double xMin, yMin, zMin, xMax, yMax, zMax;
    bounds.Get(xMin, yMin, zMin, xMax, yMax, zMax);
    CHECK(xMax - xMin == Catch::Approx(140.0).margin(0.01)); // bracket at 0 to cover end at 140
}

TEST_CASE("hiding every part is refused rather than writing an empty file", "[export][assembly]")
{
    ScopedDirectory directory("empty");
    const auto source = writeSupplierAssembly(directory.path() / "supplier.step");
    const auto imported = loupe::import::StepImporter{}.read(source);
    loupe::exporting::AssemblyExportRequest request;
    request.destination = directory.path() / "nothing.step";
    for (const auto& node : imported.snapshot.nodes)
        if (node.kind == NodeKind::Occurrence) request.excludedNodeIds.insert(node.id);
    CHECK_THROWS(loupe::exporting::AssemblyExporter{}.write(imported, request));
    CHECK(!std::filesystem::exists(request.destination));
}
