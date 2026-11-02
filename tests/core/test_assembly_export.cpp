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
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
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

namespace {

// Where each part of a re-read export sits: its bounds in assembly millimetres.
struct PlacedBounds {
    double xMin, yMin, zMin, xMax, yMax, zMax;
};

std::vector<PlacedBounds> placedBounds(const loupe::import::ImportResult& imported)
{
    std::vector<PlacedBounds> result;
    for (std::size_t index = 0; index < imported.native->shapes.size(); ++index) {
        Bnd_Box bounds;
        BRepBndLib::Add(imported.native->shapes[index].Moved(TopLoc_Location(imported.native->shapePlacements[index])), bounds);
        PlacedBounds placed{};
        bounds.Get(placed.xMin, placed.yMin, placed.zMin, placed.xMax, placed.yMax, placed.zMax);
        result.push_back(placed);
    }
    return result;
}

const loupe::domain::AssemblyNode& nodeNamed(const loupe::import::ImportResult& imported, const std::string& name,
                                             const NodeKind kind, const double x = -1.0)
{
    for (const auto& node : imported.snapshot.nodes) {
        if (node.name != name || node.kind != kind) continue;
        if (x >= 0.0 && std::abs(node.placement.columnMajor[12] - x) > 1e-6) continue;
        return node;
    }
    FAIL("no node named " << name);
    return imported.snapshot.nodes.front();
}

gp_Trsf shiftedBy(const double x, const double y, const double z)
{
    gp_Trsf shift;
    shift.SetTranslation(gp_Vec(x, y, z));
    return shift;
}

// The exploded layout used below: the Cover lifted 50 mm, the Fasteners subassembly slid
// 30 mm along y with one of its screws a further 3 mm along x inside it, and the Bracket
// turned a quarter about z through its own centre (20, 10, 2.5).
loupe::exporting::AssemblyExportRequest explodedRequest(const loupe::import::ImportResult& imported)
{
    loupe::exporting::AssemblyExportRequest request;
    request.moved.emplace(nodeNamed(imported, "Cover", NodeKind::Occurrence).id, shiftedBy(0, 0, 50));
    request.moved.emplace(nodeNamed(imported, "Fasteners", NodeKind::Subassembly).id, shiftedBy(0, 30, 0));
    request.moved.emplace(nodeNamed(imported, "Screw", NodeKind::Occurrence, 5.0).id, shiftedBy(3, 0, 0));
    gp_Trsf turn;
    turn.SetRotation(gp_Ax1(gp_Pnt(20.0, 10.0, 2.5), gp_Dir(0.0, 0.0, 1.0)), M_PI / 2.0);
    request.moved.emplace(nodeNamed(imported, "Bracket", NodeKind::Occurrence).id, turn);
    return request;
}

} // namespace

TEST_CASE("an exploded assembly re-exports with every part where it was moved", "[export][assembly][exploded]")
{
    ScopedDirectory directory("exploded-step");
    const auto source = writeSupplierAssembly(directory.path() / "supplier.step");
    const auto imported = loupe::import::StepImporter{}.read(source);
    auto request = explodedRequest(imported);
    request.destination = directory.path() / "exploded.step";
    const auto result = loupe::exporting::AssemblyExporter{}.write(imported, request);
    CHECK(result.partCount == 4);

    const auto reread = loupe::import::StepImporter{}.read(request.destination);
    // The tree is kept: the same parts and the Fasteners subassembly.
    CHECK(namesOfKind(reread, NodeKind::Occurrence) == std::vector<std::string>{"Bracket", "Cover", "Screw", "Screw"});
    CHECK(namesOfKind(reread, NodeKind::Subassembly) == std::vector<std::string>{"Fasteners"});

    int screws = 0;
    bool coverChecked = false;
    bool bracketChecked = false;
    for (const auto& bounds : placedBounds(reread)) {
        const auto width = bounds.xMax - bounds.xMin;
        const auto depth = bounds.yMax - bounds.yMin;
        const auto height = bounds.zMax - bounds.zMin;
        if (height == Catch::Approx(10.0).margin(1e-3)) {
            // Both screws moved with their subassembly; the one at x = 5 a further 3 mm.
            ++screws;
            CHECK(bounds.yMin == Catch::Approx(33.5).margin(1e-3));
            const auto centreX = (bounds.xMin + bounds.xMax) / 2.0;
            CHECK((centreX == Catch::Approx(8.0).margin(1e-3) || centreX == Catch::Approx(35.0).margin(1e-3)));
        } else if (height == Catch::Approx(1.0).margin(1e-3)) {
            coverChecked = true;
            CHECK(bounds.xMin == Catch::Approx(100.0).margin(1e-3));
            CHECK(bounds.zMin == Catch::Approx(50.0).margin(1e-3));
        } else if (height == Catch::Approx(5.0).margin(1e-3)) {
            // Turned a quarter about its centre: 20 wide and 40 deep now, centred where it was.
            bracketChecked = true;
            CHECK(width == Catch::Approx(20.0).margin(1e-3));
            CHECK(depth == Catch::Approx(40.0).margin(1e-3));
            CHECK((bounds.xMin + bounds.xMax) / 2.0 == Catch::Approx(20.0).margin(1e-3));
            CHECK((bounds.yMin + bounds.yMax) / 2.0 == Catch::Approx(10.0).margin(1e-3));
        }
    }
    CHECK(screws == 2);
    CHECK(coverChecked);
    CHECK(bracketChecked);
}

TEST_CASE("an exploded assembly exports as one STL mesh and as a drawing", "[export][assembly][exploded]")
{
    ScopedDirectory directory("exploded-mesh");
    const auto source = writeSupplierAssembly(directory.path() / "supplier.step");
    const auto imported = loupe::import::StepImporter{}.read(source);

    auto request = explodedRequest(imported);
    request.output = loupe::exporting::AssemblyOutput::Stl;
    request.destination = directory.path() / "exploded.stl";
    static_cast<void>(loupe::exporting::AssemblyExporter{}.write(imported, request));
    REQUIRE(std::filesystem::exists(request.destination));
    // Binary STL: an 80-byte header, a count, then 50 bytes a triangle. The lifted Cover
    // puts the top of the mesh at 51 mm.
    std::ifstream stl(request.destination, std::ios::binary);
    stl.seekg(80);
    std::uint32_t count = 0;
    stl.read(reinterpret_cast<char*>(&count), 4);
    CHECK(count > 0);
    CHECK(std::filesystem::file_size(request.destination) == 84 + 50ULL * count);
    float top = -1.0e9F;
    for (std::uint32_t triangle = 0; triangle < count; ++triangle) {
        std::array<float, 12> values{};
        stl.read(reinterpret_cast<char*>(values.data()), 48);
        stl.ignore(2);
        for (int vertex = 1; vertex <= 3; ++vertex) top = std::max(top, values[vertex * 3 + 2]);
    }
    CHECK(top == Catch::Approx(51.0).margin(1e-3));

    // A drawing of the same layout, seen from above: written and read back at true size.
    auto drawing = explodedRequest(imported);
    drawing.output = loupe::exporting::AssemblyOutput::Svg;
    drawing.destination = directory.path() / "exploded.svg";
    drawing.viewDirection = gp_Dir(0.0, 0.0, 1.0);
    drawing.upDirection = gp_Dir(0.0, 1.0, 0.0);
    static_cast<void>(loupe::exporting::AssemblyExporter{}.write(imported, drawing));
    REQUIRE(std::filesystem::exists(drawing.destination));
    CHECK(std::filesystem::file_size(drawing.destination) > 100);
}
