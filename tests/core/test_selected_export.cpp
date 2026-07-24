#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/export/ExportPlan.h"
#include "core/export/StepExporter.h"
#include "core/export/StlExporter.h"
#include "core/import/StepImporter.h"
#include "core/import/StepImporterTestSupport.h"
#include "core/export/AtomicExportFile.h"
#include "fixtures/FixtureFactory.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string_view>

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <TopLoc_Location.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

namespace {

using loupe::domain::NodeKind;
using loupe::exporting::CheckedSelection;
using loupe::exporting::Coordinates;
using loupe::exporting::Format;
using loupe::exporting::Grouping;
using loupe::exporting::PlanRequest;
using loupe::exporting::SelectionKind;
using loupe::exporting::StepOutputUnit;

class ScopedExportDirectory {
public:
    explicit ScopedExportDirectory(const std::string_view name)
        : path_(std::filesystem::temp_directory_path() / ("loupe-selected-export-" + std::string(name)))
    {
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }

    ~ScopedExportDirectory() { std::filesystem::remove_all(path_); }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

loupe::exporting::OutputRow outputFor(const loupe::import::ImportResult& imported,
                                      const NodeKind nodeKind,
                                      const SelectionKind selectionKind,
                                      const Format format,
                                      const std::filesystem::path& directory,
                                      const std::string& nodeId = {})
{
    const auto node = std::ranges::find_if(imported.snapshot.nodes, [&nodeId, nodeKind](const auto& candidate) {
        return candidate.kind == nodeKind && (nodeId.empty() || candidate.id == nodeId);
    });
    REQUIRE(node != imported.snapshot.nodes.end());
    PlanRequest request;
    request.selections = {{node->id, selectionKind}};
    request.hierarchyPaths = {{node->id, node->hierarchyPath}};
    request.destination = directory.string();
    request.format = format;
    request.coordinates = Coordinates::Assembly;
    request.grouping = Grouping::SeparateFiles;
    request.stepOutputUnit = StepOutputUnit::Millimeter;
    request.unitDecision = {loupe::units::LengthUnit::Millimeter,
                            loupe::units::UnitConfidence::Confirmed,
                            1.0,
                            "fixture is millimetres"};
    const auto plan = loupe::exporting::buildPlan(request);
    return plan.outputs().front();
}

std::uint32_t littleEndianU32(const std::array<char, 4>& bytes)
{
    return std::bit_cast<std::uint32_t>(bytes);
}

struct StlTriangle {
    std::array<float, 3> normal{};
    std::array<std::array<float, 3>, 3> vertices{};
};

std::vector<StlTriangle> readBinaryStl(const std::filesystem::path& file)
{
    std::ifstream input(file, std::ios::binary);
    REQUIRE(input);
    std::array<char, 80> header{};
    std::array<char, 4> countBytes{};
    input.read(header.data(), static_cast<std::streamsize>(header.size()));
    input.read(countBytes.data(), static_cast<std::streamsize>(countBytes.size()));
    REQUIRE(input.gcount() == static_cast<std::streamsize>(countBytes.size()));
    std::vector<StlTriangle> triangles(littleEndianU32(countBytes));
    for (auto& triangle : triangles) {
        input.read(reinterpret_cast<char*>(triangle.normal.data()), static_cast<std::streamsize>(sizeof(triangle.normal)));
        input.read(reinterpret_cast<char*>(triangle.vertices.data()), static_cast<std::streamsize>(sizeof(triangle.vertices)));
        std::array<char, 2> attribute{};
        input.read(attribute.data(), static_cast<std::streamsize>(attribute.size()));
        REQUIRE(input);
    }
    return triangles;
}

void requireBounds(const std::vector<StlTriangle>& triangles,
                   const float minX, const float maxX,
                   const float minY, const float maxY,
                   const float minZ, const float maxZ)
{
    REQUIRE_FALSE(triangles.empty());
    float actualMinX = triangles.front().vertices.front()[0];
    float actualMaxX = actualMinX;
    float actualMinY = triangles.front().vertices.front()[1];
    float actualMaxY = actualMinY;
    float actualMinZ = triangles.front().vertices.front()[2];
    float actualMaxZ = actualMinZ;
    for (const auto& triangle : triangles) {
        for (const auto& vertex : triangle.vertices) {
            actualMinX = std::min(actualMinX, vertex[0]); actualMaxX = std::max(actualMaxX, vertex[0]);
            actualMinY = std::min(actualMinY, vertex[1]); actualMaxY = std::max(actualMaxY, vertex[1]);
            actualMinZ = std::min(actualMinZ, vertex[2]); actualMaxZ = std::max(actualMaxZ, vertex[2]);
        }
    }
    REQUIRE(actualMinX == Catch::Approx(minX).margin(0.001F)); REQUIRE(actualMaxX == Catch::Approx(maxX).margin(0.001F));
    REQUIRE(actualMinY == Catch::Approx(minY).margin(0.001F)); REQUIRE(actualMaxY == Catch::Approx(maxY).margin(0.001F));
    REQUIRE(actualMinZ == Catch::Approx(minZ).margin(0.001F)); REQUIRE(actualMaxZ == Catch::Approx(maxZ).margin(0.001F));
}

void requireOutwardWinding(const std::vector<StlTriangle>& triangles)
{
    for (const auto& triangle : triangles) {
        const auto& a = triangle.vertices[0]; const auto& b = triangle.vertices[1]; const auto& c = triangle.vertices[2];
        const std::array<float, 3> cross{{(b[1] - a[1]) * (c[2] - a[2]) - (b[2] - a[2]) * (c[1] - a[1]),
                                          (b[2] - a[2]) * (c[0] - a[0]) - (b[0] - a[0]) * (c[2] - a[2]),
                                          (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])}};
        const float dot = cross[0] * triangle.normal[0] + cross[1] * triangle.normal[1] + cross[2] * triangle.normal[2];
        REQUIRE(dot > 0.0F);
    }
}

TEST_CASE("definition STEP export writes the selected unique definition", "[export][step]")
{
    const ScopedExportDirectory directory{"definition"};
    const auto source = loupe::tests::writeTwoDefinitionAssembly(directory.path() / "two-definitions.step");
    const auto imported = loupe::import::StepImporter {}.read(source);
    std::vector<std::string> definitions;
    for (const auto& node : imported.snapshot.nodes) if (node.kind == NodeKind::Definition) definitions.push_back(node.id);
    REQUIRE(definitions.size() == 2);
    const auto output = outputFor(imported, NodeKind::Definition, SelectionKind::Definition, Format::Step, directory.path(), definitions.back());

    const auto result = loupe::exporting::StepExporter {}.write(imported, output);

    REQUIRE(result.written);
    REQUIRE(result.outputCount == 1);
    REQUIRE(result.outputUnit == loupe::units::LengthUnit::Millimeter);
    REQUIRE(std::filesystem::exists(output.finalPath()));
    REQUIRE_FALSE(std::filesystem::exists(output.finalPath() + ".partial"));
    const auto roundTrip = loupe::import::StepImporter {}.read(output.finalPath());
    REQUIRE(roundTrip.definitionCount == 1);
    REQUIRE(roundTrip.occurrenceCount == 0);
    REQUIRE(roundTrip.unitEvidence.normalizedLongestExtentMm == Catch::Approx(17.0));
}

TEST_CASE("STEP export retains and writes the requested inch unit", "[export][step][inch]")
{
    const ScopedExportDirectory directory{"step-inch"};
    const auto source = loupe::tests::writeRepeatedBoxAssembly(directory.path() / "repeated.step", loupe::tests::FixtureUnit::Inch);
    const auto imported = loupe::import::StepImporter {}.read(source);
    const auto definition = std::ranges::find_if(imported.snapshot.nodes, [](const auto& node) { return node.kind == NodeKind::Definition; });
    REQUIRE(definition != imported.snapshot.nodes.end());
    PlanRequest request;
    request.selections = {{definition->id, SelectionKind::Definition}};
    request.hierarchyPaths = {{definition->id, definition->hierarchyPath}};
    request.destination = directory.path().string(); request.format = Format::Step; request.coordinates = Coordinates::Local;
    request.grouping = Grouping::SeparateFiles; request.stepOutputUnit = StepOutputUnit::Requested;
    request.requestedUnitToMillimeters = 25.4;
    request.unitDecision = {loupe::units::LengthUnit::Inch, loupe::units::UnitConfidence::UserOverride, 25.4, "inch fixture"};
    const auto plan = loupe::exporting::buildPlan(request);
    const auto& output = plan.outputs().front();
    REQUIRE(output.stepOutputUnit() == StepOutputUnit::Inch);
    REQUIRE(output.sourceToOutputScale() == Catch::Approx(1.0));

    const auto result = loupe::exporting::StepExporter {}.write(imported, output);
    const auto roundTrip = loupe::import::StepImporter {}.read(output.finalPath());
    REQUIRE(result.outputUnit == loupe::units::LengthUnit::Inch);
    REQUIRE(roundTrip.unitEvidence.declaredRepresentationUnits == std::vector {loupe::units::LengthUnit::Inch});
    REQUIRE(roundTrip.unitEvidence.normalizedLongestExtentMm == Catch::Approx(254.0));
}

TEST_CASE("occurrence STL export is binary millimetres at its assembly placement", "[export][stl]")
{
    const ScopedExportDirectory directory{"stl"};
    const auto source = loupe::tests::writeRepeatedBoxAssembly(directory.path() / "repeated.step", loupe::tests::FixtureUnit::Millimeter);
    const auto imported = loupe::import::StepImporter {}.read(source);
    const auto occurrence = std::ranges::find_if(imported.snapshot.nodes, [](const auto& node) {
        return node.kind == NodeKind::Occurrence && node.placement.columnMajor[12] == 25.0;
    });
    REQUIRE(occurrence != imported.snapshot.nodes.end());
    const auto output = outputFor(imported, NodeKind::Occurrence, SelectionKind::Occurrence, Format::Stl, directory.path(), occurrence->id);

    const auto result = loupe::exporting::StlExporter {}.write(imported, output);

    REQUIRE(result.written);
    REQUIRE(result.binary);
    REQUIRE(result.outputCount == 1);
    REQUIRE(result.outputUnit == loupe::units::LengthUnit::Millimeter);
    const auto triangles = readBinaryStl(output.finalPath());
    requireBounds(triangles, 25.0F, 35.0F, 0.0F, 10.0F, 0.0F, 10.0F);
    REQUIRE_FALSE(std::filesystem::exists(output.finalPath() + ".partial"));
}

TEST_CASE("inch mirrored STL export scales placement and preserves outward winding", "[export][stl][inch][mirror]")
{
    const ScopedExportDirectory directory{"stl-inch-mirror"};
    const auto source = loupe::tests::writeRepeatedBoxAssembly(directory.path() / "repeated.step", loupe::tests::FixtureUnit::Inch);
    auto imported = loupe::import::StepImporter {}.read(source);
    auto occurrence = std::ranges::find_if(imported.snapshot.nodes, [](const auto& node) {
        return node.kind == NodeKind::Occurrence && node.hierarchyPath.ends_with(":2");
    });
    REQUIRE(occurrence != imported.snapshot.nodes.end());
    occurrence->placement.columnMajor[0] = -1.0;
    occurrence->placement.columnMajor[12] = 889.0;
    PlanRequest request;
    request.selections = {{occurrence->id, SelectionKind::Occurrence}}; request.hierarchyPaths = {{occurrence->id, occurrence->hierarchyPath}};
    request.destination = directory.path().string(); request.format = Format::Stl; request.coordinates = Coordinates::Assembly; request.grouping = Grouping::SeparateFiles;
    request.unitDecision = {loupe::units::LengthUnit::Inch, loupe::units::UnitConfidence::UserOverride, 25.4, "inch fixture"};
    const auto plan = loupe::exporting::buildPlan(request);
    const auto& inchOutput = plan.outputs().front();

    const auto result = loupe::exporting::StlExporter {}.write(imported, inchOutput);
    REQUIRE(result.outputUnit == loupe::units::LengthUnit::Millimeter);
    const auto triangles = readBinaryStl(inchOutput.finalPath());
    requireBounds(triangles, 635.0F, 889.0F, 0.0F, 254.0F, 0.0F, 254.0F);
    requireOutwardWinding(triangles);
}

TEST_CASE("split body STL export contains only its own solid", "[export][stl][multi-solid]")
{
    const ScopedExportDirectory directory{"multi-solid-stl"};
    const auto source = loupe::tests::writeMultiSolidBodyStep(directory.path() / "multi-solid.step");
    const auto imported = loupe::import::StepImporter {}.read(source);
    const auto container = std::ranges::find_if(imported.snapshot.nodes, [](const auto& node) {
        return node.kind == NodeKind::Body && !node.subSolidIndex;
    });
    REQUIRE(container != imported.snapshot.nodes.end());
    REQUIRE(container->bodyIds.size() == 2);

    const auto boxBody = outputFor(imported, NodeKind::Body, SelectionKind::Body, Format::Stl, directory.path(), container->bodyIds[0]);
    const auto boxResult = loupe::exporting::StlExporter {}.write(imported, boxBody);
    REQUIRE(boxResult.written);
    requireBounds(readBinaryStl(boxBody.finalPath()), 0.0F, 10.0F, 0.0F, 10.0F, 0.0F, 10.0F);

    const auto secondBody = outputFor(imported, NodeKind::Body, SelectionKind::Body, Format::Stl, directory.path(), container->bodyIds[1]);
    const auto secondResult = loupe::exporting::StlExporter {}.write(imported, secondBody);
    REQUIRE(secondResult.written);
    requireBounds(readBinaryStl(secondBody.finalPath()), 30.0F, 36.0F, 0.0F, 6.0F, 0.0F, 6.0F);
}

TEST_CASE("split body STEP export round-trips as a single-solid part", "[export][step][multi-solid]")
{
    const ScopedExportDirectory directory{"multi-solid-step"};
    const auto source = loupe::tests::writeMultiSolidBodyStep(directory.path() / "multi-solid.step");
    const auto imported = loupe::import::StepImporter {}.read(source);
    const auto container = std::ranges::find_if(imported.snapshot.nodes, [](const auto& node) {
        return node.kind == NodeKind::Body && !node.subSolidIndex;
    });
    REQUIRE(container != imported.snapshot.nodes.end());

    const auto boxBody = outputFor(imported, NodeKind::Body, SelectionKind::Body, Format::Step, directory.path(), container->bodyIds[0]);
    const auto result = loupe::exporting::StepExporter {}.write(imported, boxBody);
    REQUIRE(result.written);
    const auto roundTrip = loupe::import::StepImporter {}.read(boxBody.finalPath());
    REQUIRE(roundTrip.snapshot.classification == loupe::domain::InputClass::SinglePart);
    REQUIRE(roundTrip.unitEvidence.normalizedLongestExtentMm == Catch::Approx(10.0));
}

TEST_CASE("in-memory XCAF traversal composes nested assembly locations", "[import][nested]")
{
    occ::handle<TDocStd_Document> document;
    XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", document);
    const auto shapes = XCAFDoc_DocumentTool::ShapeTool(document->Main());
    const auto definition = shapes->AddShape(BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape(), false);
    BRep_Builder builder;
    TopoDS_Compound empty;
    builder.MakeCompound(empty);
    const auto subassembly = shapes->AddShape(empty, true);
    gp_Trsf child; child.SetTranslation(gp_Vec(10.0, 0.0, 0.0));
    shapes->AddComponent(subassembly, definition, TopLoc_Location(child));
    TopoDS_Compound rootShape;
    builder.MakeCompound(rootShape);
    const auto root = shapes->AddShape(rootShape, true);
    gp_Trsf parent; parent.SetTranslation(gp_Vec(20.0, 0.0, 0.0));
    shapes->AddComponent(root, subassembly, TopLoc_Location(parent));
    shapes->UpdateAssemblies();
    const auto snapshot = loupe::import::detail::snapshotForTesting(document, root);
    REQUIRE(std::ranges::any_of(snapshot.nodes, [](const auto& node) { return node.kind == NodeKind::Subassembly; }));
    const auto occurrence = std::ranges::find_if(snapshot.nodes, [](const auto& node) { return node.kind == NodeKind::Occurrence; });
    REQUIRE(occurrence != snapshot.nodes.end());
    REQUIRE(occurrence->placement.columnMajor[12] == Catch::Approx(30.0));
}

TEST_CASE("atomic export replacement uses unique partials and cleans them", "[export][atomic]")
{
    const ScopedExportDirectory directory{"atomic"};
    const auto final = directory.path() / "result.bin";
    { std::ofstream old(final, std::ios::binary); old << "old"; }
    loupe::exporting::detail::AtomicExportFile first(final), second(final);
    REQUIRE(first.partial() != second.partial());
    { std::ofstream written(first.partial(), std::ios::binary); written << "new"; }
    first.finalize();
    std::ifstream input(final, std::ios::binary); std::string textValue; input >> textValue;
    REQUIRE(textValue == "new");
    REQUIRE_FALSE(std::filesystem::exists(first.partial()));
    { std::ofstream orphan(second.partial(), std::ios::binary); orphan << "orphan"; }
}

} // namespace
