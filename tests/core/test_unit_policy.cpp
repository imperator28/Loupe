#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <stdexcept>

#include "core/units/UnitPolicy.h"

using namespace loupe::units;

TEST_CASE("inch metadata normalizes to millimeters", "[units]")
{
    const UnitEvidence evidence{{LengthUnit::Inch}, LengthUnit::Inch, 101.6, false};
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
    REQUIRE(decide(UnitEvidence{{LengthUnit::Unknown}, LengthUnit::Millimeter, 120.0, false}, std::nullopt)
                .blocksExport());
    REQUIRE(decide(UnitEvidence{{LengthUnit::Inch}, LengthUnit::Millimeter, 120.0, false}, std::nullopt)
                .blocksExport());
    REQUIRE(decide(
                UnitEvidence{{LengthUnit::Inch, LengthUnit::Millimeter}, LengthUnit::Millimeter, 120.0, false},
                std::nullopt)
                .blocksExport());
    REQUIRE(decide(
                UnitEvidence{{LengthUnit::Inch, LengthUnit::Millimeter}, LengthUnit::Mixed, 120.0, true},
                std::nullopt)
                .blocksExport());
}

TEST_CASE("malformed unit overrides are rejected", "[units]")
{
    const UnitEvidence evidence{{LengthUnit::Millimeter}, LengthUnit::Millimeter, 120.0, false};

    REQUIRE_THROWS_AS(decide(evidence, UnitOverride{LengthUnit::Inch, 0.0, "invalid"}), std::invalid_argument);
    REQUIRE_THROWS_AS(decide(evidence, UnitOverride{LengthUnit::Inch, -1.0, "invalid"}), std::invalid_argument);
    REQUIRE_THROWS_AS(
        decide(evidence, UnitOverride{LengthUnit::Inch, std::numeric_limits<double>::quiet_NaN(), "invalid"}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        decide(evidence, UnitOverride{LengthUnit::Inch, std::numeric_limits<double>::infinity(), "invalid"}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(decide(evidence, UnitOverride{LengthUnit::Unknown, 1.0, "invalid"}), std::invalid_argument);
    REQUIRE_THROWS_AS(decide(evidence, UnitOverride{LengthUnit::Mixed, 1.0, "invalid"}), std::invalid_argument);
}

TEST_CASE("non-finite normalized extent is suspicious", "[units]")
{
    const UnitEvidence nanEvidence{
        {LengthUnit::Millimeter}, LengthUnit::Millimeter, std::numeric_limits<double>::quiet_NaN(), false};
    const UnitEvidence infiniteEvidence{
        {LengthUnit::Millimeter}, LengthUnit::Millimeter, std::numeric_limits<double>::infinity(), false};

    REQUIRE(decide(nanEvidence, std::nullopt).confidence == UnitConfidence::Suspicious);
    REQUIRE(decide(nanEvidence, std::nullopt).blocksExport());
    REQUIRE(decide(infiniteEvidence, std::nullopt).confidence == UnitConfidence::Suspicious);
    REQUIRE(decide(infiniteEvidence, std::nullopt).blocksExport());
}
