#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/inspection/GeometryAnalysis.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>

#include <numbers>

TEST_CASE("geometry analysis reports box area volume and bounds", "[inspection][geometry]")
{
    const auto analysis = loupe::inspection::analyze(BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape());

    REQUIRE(analysis.valid);
    REQUIRE(analysis.surfaceAreaMm2 == Catch::Approx(2200.0));
    REQUIRE(analysis.volumeMm3 == Catch::Approx(6000.0));
    REQUIRE(analysis.boundsMm.width == Catch::Approx(10.0));
    REQUIRE(analysis.boundsMm.height == Catch::Approx(20.0));
    REQUIRE(analysis.boundsMm.depth == Catch::Approx(30.0));
}

TEST_CASE("geometry analysis reports cylindrical volume", "[inspection][geometry]")
{
    const auto analysis = loupe::inspection::analyze(BRepPrimAPI_MakeCylinder(3.0, 17.0).Shape());

    REQUIRE(analysis.valid);
    REQUIRE(analysis.volumeMm3 == Catch::Approx(153.0 * std::numbers::pi));
}

TEST_CASE("geometry analysis normalizes length area and volume to millimeters", "[inspection][geometry]")
{
    const auto analysis = loupe::inspection::analyze(BRepPrimAPI_MakeBox(1.0, 2.0, 3.0).Shape(), 25.4);

    REQUIRE(analysis.valid);
    REQUIRE(analysis.boundsMm.width == Catch::Approx(25.4));
    REQUIRE(analysis.surfaceAreaMm2 == Catch::Approx(22.0 * 25.4 * 25.4));
    REQUIRE(analysis.volumeMm3 == Catch::Approx(6.0 * 25.4 * 25.4 * 25.4));
}
