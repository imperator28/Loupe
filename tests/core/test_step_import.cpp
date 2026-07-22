#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "core/import/StepImporter.h"
#include "core/units/UnitPolicy.h"
#include "fixtures/FixtureFactory.h"

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
