#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/units/UnitPolicy.h"

using namespace loupe::units;

TEST_CASE("inch metadata normalizes to millimeters", "[units]")
{
    const UnitEvidence evidence{{LengthUnit::Inch}, LengthUnit::Inch, 4.0, false};
    const UnitDecision result = decide(evidence, std::nullopt);

    REQUIRE(result.effectiveUnit == LengthUnit::Inch);
    REQUIRE(result.sourceToMillimeters == Catch::Approx(25.4));
    REQUIRE(result.confidence == UnitConfidence::Confirmed);
}

TEST_CASE("tiny declared millimeter assembly is suspicious", "[units]")
{
    const UnitEvidence evidence{{LengthUnit::Millimeter}, LengthUnit::Millimeter, 4.0, false};

    REQUIRE(decide(evidence, std::nullopt).confidence == UnitConfidence::Suspicious);
}

TEST_CASE("manual inch override is explicit and traceable", "[units]")
{
    const UnitEvidence evidence{{LengthUnit::Millimeter}, LengthUnit::Millimeter, 4.0, false};
    const UnitOverride overrideValue{LengthUnit::Inch, 1.0, "user reviewed alternate extents"};
    const UnitDecision result = decide(evidence, overrideValue);

    REQUIRE(result.confidence == UnitConfidence::UserOverride);
    REQUIRE(result.sourceToMillimeters == Catch::Approx(25.4));
    REQUIRE(result.reason == "user reviewed alternate extents");
}

TEST_CASE("missing or mixed units require resolution", "[units]")
{
    REQUIRE(decide(UnitEvidence{{}, LengthUnit::Unknown, 120.0, false}, std::nullopt).blocksExport());
    REQUIRE(decide(
                UnitEvidence{{LengthUnit::Inch, LengthUnit::Millimeter}, LengthUnit::Mixed, 120.0, true},
                std::nullopt)
                .blocksExport());
}
