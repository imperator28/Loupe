#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/inspection/MaterialProperties.h"

TEST_CASE("material library estimates mass from normalized cubic millimeters", "[inspection][material]")
{
    const auto aluminum = loupe::inspection::MaterialLibrary::find("aluminum-6061");

    REQUIRE(aluminum.has_value());
    REQUIRE(loupe::inspection::estimateMassKg(1'000'000.0, *aluminum) == Catch::Approx(2.70));
}

TEST_CASE("material estimate rejects nonphysical inputs", "[inspection][material]")
{
    const loupe::inspection::Material invalid{"invalid", "Invalid", -1.0};

    REQUIRE_FALSE(loupe::inspection::estimateMassKg(1000.0, invalid).has_value());
    REQUIRE_FALSE(loupe::inspection::estimateMassKg(-1000.0, loupe::inspection::Material{"steel", "Steel", 7.85}).has_value());
}
