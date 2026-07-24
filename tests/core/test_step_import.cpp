#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "core/import/StepImporter.h"
#include "core/units/UnitPolicy.h"
#include "fixtures/FixtureFactory.h"

#include <algorithm>
#include <vector>

namespace {

using loupe::domain::InputClass;
using loupe::tests::FixtureUnit;
using loupe::units::LengthUnit;

TEST_CASE("STEP importer preserves definition and repeated occurrences", "[step][import]")
{
    const auto file = loupe::tests::writeRepeatedBoxAssembly("repeated-box.step", FixtureUnit::Millimeter);
    const auto result = loupe::import::StepImporter {}.read(file);

    REQUIRE(result.snapshot.classification == InputClass::StructuredAssembly);
    REQUIRE(result.definitionCount == 1);
    REQUIRE(result.occurrenceCount == 2);
    REQUIRE(result.unitEvidence.xcafUnit == LengthUnit::Millimeter);
    REQUIRE(result.unitEvidence.declaredRepresentationUnits == std::vector {LengthUnit::Millimeter});
    REQUIRE(result.unitEvidence.normalizedLongestExtentMm > 10.0);
    REQUIRE(loupe::units::decide(result.unitEvidence, std::nullopt).confidence == loupe::units::UnitConfidence::Confirmed);
    std::vector<std::string> occurrenceDefinitions;
    for (const auto& node : result.snapshot.nodes) {
        if (node.kind == loupe::domain::NodeKind::Occurrence) occurrenceDefinitions.push_back(*node.definitionId);
    }
    REQUIRE(occurrenceDefinitions.size() == 2);
    REQUIRE(occurrenceDefinitions[0] == occurrenceDefinitions[1]);
    REQUIRE(result.native->shapes.size() == 2);
    REQUIRE(result.native->shapePlacements.size() == result.native->shapes.size());
    REQUIRE(result.native->definitionIds.size() == result.native->shapes.size());
    REQUIRE(result.native->definitionIds[0] == result.native->definitionIds[1]);

    const auto secondOccurrence = std::ranges::find_if(result.snapshot.nodes, [](const auto& node) {
        return node.kind == loupe::domain::NodeKind::Occurrence && node.hierarchyPath.ends_with(":2");
    });
    REQUIRE(secondOccurrence != result.snapshot.nodes.end());
    REQUIRE(secondOccurrence->placement.columnMajor[12] == Catch::Approx(25.0));
    REQUIRE(secondOccurrence->placement.columnMajor[0] == Catch::Approx(1.0));
    REQUIRE(secondOccurrence->placement.columnMajor[5] == Catch::Approx(1.0));
    REQUIRE(secondOccurrence->placement.columnMajor[10] == Catch::Approx(1.0));
    REQUIRE(result.native->shapePlacements[1].TranslationPart().X() == Catch::Approx(25.0));
}

TEST_CASE("STEP importer classifies flat and single-part files", "[step][classification]")
{
    const auto flat = loupe::import::StepImporter {}.read(loupe::tests::writeFlatTwoSolidStep("flat.step"));
    const auto single = loupe::import::StepImporter {}.read(loupe::tests::writeSingleCylinderStep("single.step"));

    REQUIRE(flat.snapshot.classification == InputClass::FlatMultiSolid);
    REQUIRE(single.snapshot.classification == InputClass::SinglePart);
}

TEST_CASE("STEP importer splits a multi-solid component's compound into selectable bodies", "[step][import][multi-solid]")
{
    const auto file = loupe::tests::writeMultiSolidBodyStep("multi-solid-body.step");
    const auto result = loupe::import::StepImporter {}.read(file);

    const auto container = std::ranges::find_if(result.snapshot.nodes, [](const auto& node) {
        return node.kind == loupe::domain::NodeKind::Body && node.subSolidIndex == std::nullopt;
    });
    REQUIRE(container != result.snapshot.nodes.end());
    REQUIRE(container->bodyIds.size() == 2);

    std::vector<const loupe::domain::AssemblyNode*> splitBodies;
    for (const auto& id : container->bodyIds) {
        const auto body = std::ranges::find_if(result.snapshot.nodes, [&id](const auto& node) { return node.id == id; });
        REQUIRE(body != result.snapshot.nodes.end());
        splitBodies.push_back(&*body);
    }
    REQUIRE(splitBodies[0]->kind == loupe::domain::NodeKind::Body);
    REQUIRE(splitBodies[0]->parentId == container->id);
    REQUIRE(splitBodies[0]->hierarchyPath == container->hierarchyPath);
    REQUIRE(splitBodies[0]->subSolidIndex == 0);
    REQUIRE(splitBodies[0]->name == "Body 1");
    REQUIRE(splitBodies[1]->subSolidIndex == 1);
    REQUIRE(splitBodies[1]->name == "Body 2");

    // Each split body is a physically distinct solid, not a repeated instance
    // of one part, so it must carry its own definitionId rather than the
    // container's -- otherwise Inspect's quantity grouping (which counts nodes
    // sharing a definitionId) would miscount unrelated siblings as copies.
    REQUIRE(splitBodies[0]->definitionId == splitBodies[0]->id);
    REQUIRE(splitBodies[1]->definitionId == splitBodies[1]->id);
    REQUIRE(splitBodies[0]->definitionId != splitBodies[1]->definitionId);
    REQUIRE(splitBodies[0]->definitionId != container->definitionId);

    // The container itself gets no mesh entry once split -- only its Body
    // children do -- so the same faces are never registered for rendering
    // twice (which would otherwise z-fight in the 3D viewport). It stays
    // exportable as one file regardless, since export re-resolves the shape
    // from hierarchyPath rather than from this array.
    REQUIRE(std::ranges::count(result.native->shapeNodeIds, container->id) == 0);
    REQUIRE(std::ranges::count(result.native->shapeNodeIds, splitBodies[0]->id) == 1);
    REQUIRE(std::ranges::count(result.native->shapeNodeIds, splitBodies[1]->id) == 1);
}

TEST_CASE("STEP importer keeps declared inch and transferred XCAF unit evidence distinct", "[step][inch metadata]")
{
    const auto file = loupe::tests::writeRepeatedBoxAssembly("repeated-inch.step", FixtureUnit::Inch);
    const auto result = loupe::import::StepImporter {}.read(file);

    REQUIRE(result.snapshot.classification == InputClass::StructuredAssembly);
    // OCCT 8 normalizes transferred geometry to the XCAF document's millimeter unit.
    REQUIRE(result.unitEvidence.xcafUnit == LengthUnit::Millimeter);
    REQUIRE(result.unitEvidence.declaredRepresentationUnits == std::vector {LengthUnit::Inch});
    REQUIRE(result.unitEvidence.normalizedLongestExtentMm == Catch::Approx(35.0 * 25.4));
    REQUIRE(loupe::units::decide(result.unitEvidence, std::nullopt).blocksExport());
}

} // namespace
