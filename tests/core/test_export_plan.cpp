#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <utility>

#include "core/export/ExportPlan.h"

using namespace loupe::exporting;

namespace {

PlanRequest baseRequest()
{
    return {{CheckedSelection{"bolt", SelectionKind::Occurrence}},
            {{"bolt", "root/assembly/bolt[1]"}},
            {},
            "C:/exports",
            Format::Stl,
            Coordinates::Assembly,
            Grouping::SeparateFiles,
            StepOutputUnit::Requested,
            1.0};
}

TEST_CASE("reviewed output leaf names override component leaves", "[export-plan]")
{
    PlanRequest request = baseRequest();
    request.outputLeafNames = {{"bolt", "Bracket-001"}};

    const ExportPlan plan = buildPlan(request);

    REQUIRE(plan.outputs().front().finalPath() == "C:/exports/Bracket-001.stl");
}

} // namespace

template <typename T>
concept HasRvalueOutputs = requires(const T&& value) { std::move(value).outputs(); };

template <typename T>
concept HasRvalueCheckedSelections = requires(const T&& value) { std::move(value).checkedSelections(); };

template <typename T>
concept HasRvalueFinalPath = requires(const T&& value) { std::move(value).finalPath(); };

static_assert(!HasRvalueOutputs<ExportPlan>);
static_assert(!HasRvalueCheckedSelections<SelectionDraft>);
static_assert(!HasRvalueFinalPath<OutputRow>);

TEST_CASE("plan requests default to STEP format", "[export-plan]")
{
    const PlanRequest request{};

    REQUIRE(request.format == Format::Step);
}

TEST_CASE("highlighting a draft selection does not check it", "[export-plan]")
{
    SelectionDraft draft{};
    draft.highlight("bolt");

    REQUIRE(draft.highlightedNodeId() == "bolt");
    REQUIRE(draft.checkedSelections().empty());
}

TEST_CASE("checking a selection does not clear the highlight", "[export-plan]")
{
    SelectionDraft draft{};
    draft.highlight("nut");
    draft.setChecked({"bolt", SelectionKind::Occurrence}, true);

    REQUIRE(draft.highlightedNodeId() == "nut");
    REQUIRE(draft.checkedSelections() == std::vector<CheckedSelection>{{"bolt", SelectionKind::Occurrence}});
}

TEST_CASE("empty selection throws a plan error", "[export-plan]")
{
    PlanRequest request = baseRequest();
    request.selections.clear();

    REQUIRE_THROWS_AS(buildPlan(request), PlanError);
}

TEST_CASE("blank destination throws a plan error", "[export-plan]")
{
    PlanRequest request = baseRequest();
    request.destination = " \t";

    REQUIRE_THROWS_AS(buildPlan(request), PlanError);
}

TEST_CASE("duplicate final paths are rejected", "[export-plan]")
{
    PlanRequest request = baseRequest();
    request.selections = {{"a", SelectionKind::Occurrence}, {"b", SelectionKind::Occurrence}};
    request.hierarchyPaths = {{"a", "root/a:b"}, {"b", "root/a?b"}};

    REQUIRE_THROWS_AS(buildPlan(request), PlanError);
}

TEST_CASE("Windows final-path collisions are case insensitive", "[export-plan]")
{
    PlanRequest request = baseRequest();
    request.selections = {{"upper", SelectionKind::Occurrence}, {"lower", SelectionKind::Occurrence}};
    request.hierarchyPaths = {{"upper", "root/Bolt"}, {"lower", "root/bolt"}};

    REQUIRE_THROWS_AS(buildPlan(request), PlanError);
}

TEST_CASE("Windows final-path collisions fold accented UTF-8 names", "[export-plan]")
{
    PlanRequest request = baseRequest();
    request.selections = {{"upper", SelectionKind::Occurrence}, {"lower", SelectionKind::Occurrence}};
    request.hierarchyPaths = {{"upper", "root/É"}, {"lower", "root/é"}};

    REQUIRE_THROWS_AS(buildPlan(request), PlanError);
}

TEST_CASE("UTF-8 hierarchy leaves remain distinct output names", "[export-plan]")
{
    PlanRequest request = baseRequest();
    request.selections = {{"first", SelectionKind::Occurrence}, {"second", SelectionKind::Occurrence}};
    request.hierarchyPaths = {{"first", "root/零件"}, {"second", "root/螺栓"}};

    const ExportPlan plan = buildPlan(request);

    REQUIRE(plan.outputs().size() == 2);
    REQUIRE((plan.outputs()[0].finalPath() == "C:/exports/零件.stl"
             || plan.outputs()[1].finalPath() == "C:/exports/零件.stl"));
    REQUIRE((plan.outputs()[0].finalPath() == "C:/exports/螺栓.stl"
             || plan.outputs()[1].finalPath() == "C:/exports/螺栓.stl"));
}

TEST_CASE("malformed UTF-8 hierarchy leaves are rejected", "[export-plan]")
{
    PlanRequest request = baseRequest();
    request.hierarchyPaths = {{"bolt", std::string{"root/"} + static_cast<char>(0xc3)}};

    REQUIRE_THROWS_AS(buildPlan(request), PlanError);
}

TEST_CASE("non-finite source-to-millimeter scale is rejected", "[export-plan]")
{
    PlanRequest request = baseRequest();
    request.unitDecision.sourceToMillimeters = std::numeric_limits<double>::quiet_NaN();

    REQUIRE_THROWS_AS(buildPlan(request), PlanError);
}

TEST_CASE("invalid public enum values are rejected", "[export-plan]")
{
    PlanRequest request = baseRequest();
    request.format = static_cast<Format>(99);
    REQUIRE_THROWS_AS(buildPlan(request), PlanError);

    request = baseRequest();
    request.coordinates = static_cast<Coordinates>(99);
    REQUIRE_THROWS_AS(buildPlan(request), PlanError);

    request = baseRequest();
    request.grouping = static_cast<Grouping>(99);
    REQUIRE_THROWS_AS(buildPlan(request), PlanError);

    request = baseRequest();
    request.stepOutputUnit = static_cast<StepOutputUnit>(99);
    REQUIRE_THROWS_AS(buildPlan(request), PlanError);

    request = baseRequest();
    request.selections.front().kind = static_cast<SelectionKind>(99);
    REQUIRE_THROWS_AS(buildPlan(request), PlanError);

    SelectionDraft draft{};
    REQUIRE_THROWS_AS(draft.setChecked({"invalid", static_cast<SelectionKind>(99)}, true), PlanError);
}

TEST_CASE("occurrence selection cannot use local coordinates", "[export-plan]")
{
    PlanRequest request = baseRequest();
    request.coordinates = Coordinates::Local;

    REQUIRE_THROWS_AS(buildPlan(request), PlanError);
}

TEST_CASE("preserving a definition as an assembly is rejected", "[export-plan]")
{
    PlanRequest request = baseRequest();
    request.selections = {{"def", SelectionKind::Definition}};
    request.hierarchyPaths = {{"def", "root/def"}};
    request.grouping = Grouping::PreserveAssembly;

    REQUIRE_THROWS_AS(buildPlan(request), PlanError);
}

TEST_CASE("multi-selection grouping is rejected until exporters implement it", "[export-plan]")
{
    PlanRequest request = baseRequest();
    request.selections = {{"assembly", SelectionKind::Subassembly}};
    request.hierarchyPaths = {{"assembly", "root/assembly"}};
    request.grouping = Grouping::PreserveAssembly;
    REQUIRE_THROWS_AS(buildPlan(request), PlanError);

    request.selections = {{"body", SelectionKind::Body}};
    request.hierarchyPaths = {{"body", "root/body"}};
    request.grouping = Grouping::FlattenMultiBody;
    REQUIRE_THROWS_AS(buildPlan(request), PlanError);
}

TEST_CASE("suspicious unit decisions block plans", "[export-plan]")
{
    PlanRequest request = baseRequest();
    request.unitDecision = {loupe::units::LengthUnit::Millimeter,
                            loupe::units::UnitConfidence::Suspicious,
                            1.0,
                            "extent needs review"};

    REQUIRE_THROWS_AS(buildPlan(request), PlanError);
}

TEST_CASE("output and fingerprint are deterministic in hierarchy order", "[export-plan]")
{
    PlanRequest request = baseRequest();
    request.selections = {{"late", SelectionKind::Definition}, {"early", SelectionKind::Definition}};
    request.hierarchyPaths = {{"late", "root/zeta"}, {"early", "root/alpha"}};

    const ExportPlan first = buildPlan(request);
    const ExportPlan second = buildPlan(request);

    REQUIRE(first.outputs().size() == 2);
    REQUIRE(first.outputs()[0].nodeId() == "early");
    REQUIRE(first.outputs()[0].finalPath() == "C:/exports/alpha.stl");
    REQUIRE(first.fingerprint() == second.fingerprint());
}

TEST_CASE("permuted selections preserve output ordering and fingerprint", "[export-plan]")
{
    PlanRequest firstRequest = baseRequest();
    firstRequest.selections = {{"late", SelectionKind::Definition}, {"early", SelectionKind::Definition}};
    firstRequest.hierarchyPaths = {{"late", "root/zeta"}, {"early", "root/alpha"}};
    PlanRequest secondRequest = firstRequest;
    secondRequest.selections = {{"early", SelectionKind::Definition}, {"late", SelectionKind::Definition}};

    const ExportPlan first = buildPlan(firstRequest);
    const ExportPlan second = buildPlan(secondRequest);

    REQUIRE(first.outputs() == second.outputs());
    REQUIRE(first.fingerprint() == second.fingerprint());
}

TEST_CASE("fingerprint changes when a reviewed output option changes", "[export-plan]")
{
    PlanRequest assemblyRequest = baseRequest();
    assemblyRequest.selections = {{"def", SelectionKind::Definition}};
    assemblyRequest.hierarchyPaths = {{"def", "root/def"}};
    PlanRequest localRequest = assemblyRequest;
    localRequest.coordinates = Coordinates::Local;

    const ExportPlan assembly = buildPlan(assemblyRequest);
    const ExportPlan local = buildPlan(localRequest);

    REQUIRE(assembly.outputs().front().coordinates() != local.outputs().front().coordinates());
    REQUIRE(assembly.fingerprint() != local.fingerprint());
}

TEST_CASE("missing hierarchy paths throw instead of treating IDs as paths", "[export-plan]")
{
    PlanRequest request = baseRequest();
    request.hierarchyPaths.clear();

    REQUIRE_THROWS_AS(buildPlan(request), PlanError);
}

TEST_CASE("STL plans force an effective millimeter output unit", "[export-plan]")
{
    PlanRequest request = baseRequest();
    request.unitDecision.sourceToMillimeters = 25.4;
    request.requestedUnitToMillimeters = 25.4;
    request.stepOutputUnit = StepOutputUnit::Requested;

    const ExportPlan result = buildPlan(request);

    REQUIRE(result.outputs().front().stepOutputUnit() == StepOutputUnit::Millimeter);
    REQUIRE(result.outputs().front().sourceToOutputScale() == 25.4);
}

TEST_CASE("STEP requested units use the reviewed requested scale", "[export-plan]")
{
    PlanRequest request = baseRequest();
    request.format = Format::Step;
    request.unitDecision.sourceToMillimeters = 25.4;
    request.requestedUnitToMillimeters = 25.4;

    const ExportPlan result = buildPlan(request);

    REQUIRE(result.outputs().front().sourceToOutputScale() == 1.0);
}

TEST_CASE("Windows reserved device names are rejected after sanitization", "[export-plan]")
{
    PlanRequest request = baseRequest();
    request.hierarchyPaths = {{"bolt", "root/Con.txt"}};

    REQUIRE_THROWS_AS(buildPlan(request), PlanError);
}

TEST_CASE("Windows trailing dots and spaces are rejected", "[export-plan]")
{
    PlanRequest request = baseRequest();
    request.hierarchyPaths = {{"bolt", "root/bolt. "}};

    REQUIRE_THROWS_AS(buildPlan(request), PlanError);
}
