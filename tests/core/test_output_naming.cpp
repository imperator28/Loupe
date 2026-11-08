#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "core/export/OutputNaming.h"

using loupe::exporting::detail::LeafNameRequest;
using loupe::exporting::detail::uniqueLeafNames;

namespace {

std::vector<LeafNameRequest> generated(const std::vector<std::string>& names)
{
    std::vector<LeafNameRequest> requests;
    for (const auto& name : names) requests.push_back({name, false});
    return requests;
}

} // namespace

TEST_CASE("distinct names are left alone", "[output-naming]")
{
    REQUIRE(uniqueLeafNames(generated({"Base Plate", "Bracket", "Slider"}))
            == std::vector<std::string>{"Base Plate", "Bracket", "Slider"});
}

TEST_CASE("generated names that clash get a sequence number in order", "[output-naming]")
{
    REQUIRE(uniqueLeafNames(generated({"Bracket", "Slider", "Bracket", "Bracket"}))
            == std::vector<std::string>{"Bracket", "Slider", "Bracket-2", "Bracket-3"});
}

TEST_CASE("names that differ only in case or Unicode form still clash", "[output-naming]")
{
    // "Bracket" and "BRACKET" are the same file on Windows, as are a precomposed and a
    // decomposed "é".
    REQUIRE(uniqueLeafNames(generated({"Bracket", "BRACKET", "Caf\xC3\xA9", "Cafe\xCC\x81"}))
            == std::vector<std::string>{"Bracket", "BRACKET-2", "Caf\xC3\xA9", "Cafe\xCC\x81-2"});
}

TEST_CASE("a part really named like a numbered copy keeps its name", "[output-naming]")
{
    // The second "Bracket" must not take "Bracket-2" from the part that is called that.
    REQUIRE(uniqueLeafNames(generated({"Bracket", "Bracket", "Bracket-2"}))
            == std::vector<std::string>{"Bracket", "Bracket-3", "Bracket-2"});
}

TEST_CASE("a typed name is never renamed and generated names move around it", "[output-naming]")
{
    const std::vector<LeafNameRequest> requests{{"Bracket", false}, {"Bracket", true}, {"Bracket", false}};
    REQUIRE(uniqueLeafNames(requests) == std::vector<std::string>{"Bracket-2", "Bracket", "Bracket-3"});
}

TEST_CASE("two typed names that clash are left for the plan to refuse", "[output-naming]")
{
    const std::vector<LeafNameRequest> requests{{"Bracket", true}, {"bracket", true}};
    REQUIRE(uniqueLeafNames(requests) == std::vector<std::string>{"Bracket", "bracket"});
}

TEST_CASE("names are sanitised before they are compared", "[output-naming]")
{
    // "Bolt:1" and "Bolt?1" both become "Bolt_1".
    REQUIRE(uniqueLeafNames(generated({"Bolt:1", "Bolt?1"})) == std::vector<std::string>{"Bolt_1", "Bolt_1-2"});
}

TEST_CASE("an unsafe name is passed through for the plan to report", "[output-naming]")
{
    REQUIRE(uniqueLeafNames(generated({"NUL", "Bracket"})) == std::vector<std::string>{"NUL", "Bracket"});
}

TEST_CASE("the same requests always give the same names", "[output-naming]")
{
    const auto requests = generated({"M5 Screw", "Guide Rod", "M5 Screw", "Guide Rod", "M5 Screw"});
    const auto first = uniqueLeafNames(requests);
    REQUIRE(first == std::vector<std::string>{"M5 Screw", "Guide Rod", "M5 Screw-2", "Guide Rod-2", "M5 Screw-3"});
    for (int run = 0; run < 5; ++run) REQUIRE(uniqueLeafNames(requests) == first);
}
