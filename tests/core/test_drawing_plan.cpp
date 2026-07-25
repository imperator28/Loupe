#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/drawing/DrawingPlan.h"

#include <algorithm>
#include <string>

namespace {

using loupe::drawing::DrawingContent;
using loupe::drawing::DrawingFormat;
using loupe::drawing::DrawingPlanError;
using loupe::drawing::DrawingPlanRequest;
using loupe::drawing::DrawingSelection;
using loupe::drawing::Vector3;

DrawingSelection selectionFor(std::string drawingId, std::string nodeId, std::string viewLabel,
                              const Vector3 direction = {0.0, 0.0, 1.0})
{
    DrawingSelection selection;
    selection.drawingId = std::move(drawingId);
    selection.nodeId = std::move(nodeId);
    selection.viewLabel = std::move(viewLabel);
    selection.viewDirection = direction;
    selection.upDirection = std::abs(direction.z) > 0.9 ? Vector3{0.0, 1.0, 0.0} : Vector3{0.0, 0.0, 1.0};
    return selection;
}

DrawingPlanRequest requestWith(std::vector<DrawingSelection> selections)
{
    DrawingPlanRequest request;
    request.selections = std::move(selections);
    request.destination = "/out";
    request.format = DrawingFormat::Dxf;
    for (const auto& selection : request.selections) {
        request.hierarchyPaths.emplace(selection.nodeId, "Assembly/" + selection.nodeId);
    }
    return request;
}

// The plan's reference accessors delete their rvalue overloads on purpose, so a plan
// has to be named before anything is read from it.
std::string fingerprintOf(const DrawingPlanRequest& request)
{
    const auto plan = loupe::drawing::buildDrawingPlan(request);
    return plan.fingerprint();
}

std::string firstPathOf(const DrawingPlanRequest& request)
{
    const auto plan = loupe::drawing::buildDrawingPlan(request);
    return plan.outputs().front().finalPath();
}

DrawingPlanError::Code codeOf(const DrawingPlanRequest& request)
{
    try {
        static_cast<void>(loupe::drawing::buildDrawingPlan(request));
    } catch (const DrawingPlanError& error) {
        return error.code();
    }
    return DrawingPlanError::Code::InvalidEnumValue; // unreachable when a throw is expected
}

TEST_CASE("a queued drawing becomes one reviewed output row", "[drawing-plan]")
{
    const auto plan = loupe::drawing::buildDrawingPlan(
        requestWith({selectionFor("d1", "plate", "Top")}));

    REQUIRE(plan.outputs().size() == 1);
    const auto& row = plan.outputs().front();
    REQUIRE(row.drawingId() == "d1");
    REQUIRE(row.nodeId() == "plate");
    REQUIRE(row.format() == DrawingFormat::Dxf);
    REQUIRE(row.scale() == Catch::Approx(1.0));
    REQUIRE(row.finalPath() == "/out/plate-top.dxf");
    REQUIRE_FALSE(plan.fingerprint().empty());
}

TEST_CASE("one part queued at several views produces distinct filenames", "[drawing-plan]")
{
    // The motivating case for the whole queue design: a bracket routinely needs
    // several views, and they must not collide or need manual renaming.
    const auto plan = loupe::drawing::buildDrawingPlan(requestWith({
        selectionFor("d1", "plate", "Top", {0.0, 0.0, 1.0}),
        selectionFor("d2", "plate", "Front", {0.0, -1.0, 0.0}),
        selectionFor("d3", "plate", "Normal to face", {1.0, 0.0, 0.0}),
    }));

    REQUIRE(plan.outputs().size() == 3);
    std::vector<std::string> paths;
    for (const auto& row : plan.outputs()) paths.push_back(row.finalPath());
    std::ranges::sort(paths);
    REQUIRE(paths[0] == "/out/plate-front.dxf");
    REQUIRE(paths[1] == "/out/plate-normal-to-face.dxf");
    REQUIRE(paths[2] == "/out/plate-top.dxf");
}

TEST_CASE("a non-unit scale is named in the file", "[drawing-plan]")
{
    // Finding out a drawing is half size at the cutter is too late.
    auto selection = selectionFor("d1", "plate", "Top");
    selection.scaleNumerator = 1;
    selection.scaleDenominator = 2;
    const auto plan = loupe::drawing::buildDrawingPlan(requestWith({selection}));

    REQUIRE(plan.outputs().front().finalPath() == "/out/plate-top-1to2.dxf");
    REQUIRE(plan.outputs().front().scale() == Catch::Approx(0.5));
}

TEST_CASE("format decides the extension", "[drawing-plan]")
{
    for (const auto& [format, extension] :
         std::vector<std::pair<DrawingFormat, std::string>>{{DrawingFormat::Dxf, ".dxf"},
                                                            {DrawingFormat::Svg, ".svg"},
                                                            {DrawingFormat::Pdf, ".pdf"}}) {
        auto request = requestWith({selectionFor("d1", "plate", "Top")});
        request.format = format;
        const auto plan = loupe::drawing::buildDrawingPlan(request);
        REQUIRE(plan.outputs().front().finalPath().ends_with(extension));
    }
}

TEST_CASE("the plan is canonical regardless of queue order", "[drawing-plan]")
{
    const auto a = selectionFor("d1", "alpha", "Top");
    const auto b = selectionFor("d2", "beta", "Front", {0.0, -1.0, 0.0});

    const auto forward = loupe::drawing::buildDrawingPlan(requestWith({a, b}));
    const auto reversed = loupe::drawing::buildDrawingPlan(requestWith({b, a}));

    REQUIRE(forward.fingerprint() == reversed.fingerprint());
    REQUIRE(forward.outputs() == reversed.outputs());
}

TEST_CASE("the fingerprint covers everything that changes the drawing", "[drawing-plan]")
{
    const auto baseline = loupe::drawing::buildDrawingPlan(
        requestWith({selectionFor("d1", "plate", "Top")}));

    // View direction: same part, same name, different geometry.
    auto rotated = selectionFor("d1", "plate", "Top");
    rotated.viewDirection = {0.0, 0.0, -1.0};
    REQUIRE(fingerprintOf(requestWith({rotated}))
            != baseline.fingerprint());

    // Up direction changes the in-plane orientation.
    auto rolled = selectionFor("d1", "plate", "Top");
    rolled.upDirection = {1.0, 0.0, 0.0};
    REQUIRE(fingerprintOf(requestWith({rolled}))
            != baseline.fingerprint());

    // Content mode changes which edges are emitted.
    auto silhouette = selectionFor("d1", "plate", "Top");
    silhouette.content = DrawingContent::OuterContourOnly;
    REQUIRE(fingerprintOf(requestWith({silhouette}))
            != baseline.fingerprint());

    // Scale changes the measured size, which is the whole point of the feature.
    auto halved = selectionFor("d1", "plate", "Top");
    halved.scaleDenominator = 2;
    REQUIRE(fingerprintOf(requestWith({halved}))
            != baseline.fingerprint());

    // Format changes the file written.
    auto svg = requestWith({selectionFor("d1", "plate", "Top")});
    svg.format = DrawingFormat::Svg;
    REQUIRE(fingerprintOf(svg) != baseline.fingerprint());
}

TEST_CASE("a reviewed leaf name overrides the generated one", "[drawing-plan]")
{
    auto request = requestWith({selectionFor("d1", "plate", "Top")});
    request.outputLeafNames.emplace("d1", "bracket-blank");

    REQUIRE(firstPathOf(request)
            == "/out/bracket-blank.dxf");
}

TEST_CASE("an empty queue is refused", "[drawing-plan]")
{
    REQUIRE(codeOf(requestWith({})) == DrawingPlanError::Code::EmptySelection);
}

TEST_CASE("a blank destination is refused", "[drawing-plan]")
{
    auto request = requestWith({selectionFor("d1", "plate", "Top")});
    request.destination = "   ";
    REQUIRE(codeOf(request) == DrawingPlanError::Code::BlankDestination);
}

TEST_CASE("a blocking unit decision refuses the whole batch", "[drawing-plan]")
{
    // Reuses the existing unit gate rather than a parallel one: a drawing whose unit
    // is unresolved cannot be 1:1 by definition.
    auto request = requestWith({selectionFor("d1", "plate", "Top")});
    request.unitDecision = {loupe::units::LengthUnit::Unknown,
                            loupe::units::UnitConfidence::MissingOrMixed, 1.0, "unresolved"};
    REQUIRE(codeOf(request) == DrawingPlanError::Code::UnitDecisionBlocksExport);
}

TEST_CASE("a missing hierarchy path is refused", "[drawing-plan]")
{
    auto request = requestWith({selectionFor("d1", "plate", "Top")});
    request.hierarchyPaths.clear();
    REQUIRE(codeOf(request) == DrawingPlanError::Code::MissingHierarchyPath);
}

TEST_CASE("a duplicate drawing id is refused", "[drawing-plan]")
{
    // Results are addressed by drawing ID, so a duplicate makes a result ambiguous.
    auto first = selectionFor("same", "plate", "Top");
    auto second = selectionFor("same", "plate", "Front", {0.0, -1.0, 0.0});
    REQUIRE(codeOf(requestWith({first, second})) == DrawingPlanError::Code::DuplicateDrawingId);
}

TEST_CASE("colliding output paths are refused", "[drawing-plan]")
{
    auto request = requestWith({selectionFor("d1", "plate", "Top"),
                                selectionFor("d2", "plate", "Front", {0.0, -1.0, 0.0})});
    request.outputLeafNames.emplace("d1", "same-name");
    request.outputLeafNames.emplace("d2", "same-name");
    REQUIRE(codeOf(request) == DrawingPlanError::Code::OutputPathCollision);
}

TEST_CASE("case-insensitive collisions are caught as Windows would", "[drawing-plan]")
{
    // Shares ExportPlan's naming rules rather than reimplementing them.
    auto request = requestWith({selectionFor("d1", "plate", "Top"),
                                selectionFor("d2", "plate", "Front", {0.0, -1.0, 0.0})});
    request.outputLeafNames.emplace("d1", "Blank");
    request.outputLeafNames.emplace("d2", "blank");
    REQUIRE(codeOf(request) == DrawingPlanError::Code::OutputPathCollision);
}

TEST_CASE("a reserved Windows device name is refused", "[drawing-plan]")
{
    auto request = requestWith({selectionFor("d1", "plate", "Top")});
    request.outputLeafNames.emplace("d1", "NUL");
    REQUIRE(codeOf(request) == DrawingPlanError::Code::UnsafeOutputName);
}

TEST_CASE("a non-positive scale ratio is refused", "[drawing-plan]")
{
    auto zero = selectionFor("d1", "plate", "Top");
    zero.scaleDenominator = 0;
    REQUIRE(codeOf(requestWith({zero})) == DrawingPlanError::Code::InvalidScale);

    auto negative = selectionFor("d1", "plate", "Top");
    negative.scaleNumerator = -1;
    REQUIRE(codeOf(requestWith({negative})) == DrawingPlanError::Code::InvalidScale);
}

TEST_CASE("a degenerate view is refused", "[drawing-plan]")
{
    // Parallel view and up leave the in-plane axis undefined; caught here rather than
    // deep inside the geometry kernel where the message means nothing to a user.
    auto parallel = selectionFor("d1", "plate", "Top");
    parallel.upDirection = parallel.viewDirection;
    REQUIRE(codeOf(requestWith({parallel})) == DrawingPlanError::Code::DegenerateView);

    auto zeroLength = selectionFor("d1", "plate", "Top");
    zeroLength.viewDirection = {0.0, 0.0, 0.0};
    REQUIRE(codeOf(requestWith({zeroLength})) == DrawingPlanError::Code::DegenerateView);
}

TEST_CASE("a non-positive tolerance is refused", "[drawing-plan]")
{
    auto request = requestWith({selectionFor("d1", "plate", "Top")});
    request.deflectionMm = 0.0;
    REQUIRE(codeOf(request) == DrawingPlanError::Code::InvalidTolerance);
}

} // namespace
