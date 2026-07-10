#include <catch2/catch_test_macros.hpp>

#include "core/domain/AssemblyTypes.h"
#include "core/domain/StableId.h"

TEST_CASE("stable IDs repeat for the same assembly occurrence", "[stable IDs]")
{
    const auto first = loupe::domain::stableId("sha256:file", "/0/2/5", "occurrence");
    const auto second = loupe::domain::stableId("sha256:file", "/0/2/5", "occurrence");

    REQUIRE(first == second);
}

TEST_CASE("stable IDs distinguish node kind and canonical path", "[stable IDs]")
{
    const auto occurrence = loupe::domain::stableId("sha256:file", "/0/2/5", "occurrence");

    REQUIRE(occurrence != loupe::domain::stableId("sha256:file", "/0/2/5", "definition"));
    REQUIRE(occurrence != loupe::domain::stableId("sha256:file", "/0/2/6", "occurrence"));
}
