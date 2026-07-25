#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/drawing/DrawingProjector.h"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <gp_Ax2.hxx>

#include <array>
#include <cmath>
#include <numbers>
#include <utility>

namespace {

using loupe::drawing::Arc;
using loupe::drawing::ContentMode;
using loupe::drawing::LayerRole;
using loupe::drawing::ProjectionError;
using loupe::drawing::ProjectionRequest;
using loupe::drawing::Segment;

ProjectionRequest requestFor(const TopoDS_Shape& shape, const gp_Dir& viewDirection)
{
    ProjectionRequest request;
    request.shape = shape;
    request.viewDirection = viewDirection;
    // Any up vector not parallel to the view direction works; the drawing frame
    // orientation does not affect measured size.
    request.upDirection = std::abs(viewDirection.Z()) > 0.9 ? gp_Dir(0.0, 1.0, 0.0) : gp_Dir(0.0, 0.0, 1.0);
    return request;
}

TopoDS_Shape drilledPlate()
{
    const TopoDS_Shape plate = BRepPrimAPI_MakeBox(40.0, 30.0, 5.0).Shape();
    const TopoDS_Shape drill =
        BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(20.0, 15.0, -1.0), gp_Dir(0.0, 0.0, 1.0)), 5.0, 7.0).Shape();
    return BRepAlgoAPI_Cut(plate, drill).Shape();
}

int countPrimitives(const loupe::drawing::Drawing& drawing)
{
    int count = 0;
    for (const auto& layer : drawing.layers) {
        for (const auto& contour : layer.contours) count += static_cast<int>(contour.primitives.size());
    }
    return count;
}

TEST_CASE("projection preserves exact millimetre size from every standard axis", "[drawing-projection]")
{
    // This is the feature's central claim: a 1:1 drawing must measure the part.
    // Gate A proved it at 0.0 error; this keeps it proven.
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(100.0, 50.0, 10.0).Shape();
    const std::array<std::pair<gp_Dir, std::pair<double, double>>, 6> cases{{
        {gp_Dir(0.0, 0.0, 1.0), {100.0, 50.0}},
        {gp_Dir(0.0, 0.0, -1.0), {100.0, 50.0}},
        {gp_Dir(0.0, 1.0, 0.0), {100.0, 10.0}},
        {gp_Dir(0.0, -1.0, 0.0), {100.0, 10.0}},
        {gp_Dir(1.0, 0.0, 0.0), {50.0, 10.0}},
        {gp_Dir(-1.0, 0.0, 0.0), {50.0, 10.0}},
    }};

    for (const auto& [direction, expected] : cases) {
        const auto result = loupe::drawing::project(requestFor(box, direction));
        const auto bounds = result.drawing.bounds();
        REQUIRE(bounds.valid);
        // The drawing frame may swap which dimension lands on which axis, so
        // compare the pair without assuming an ordering.
        const double large = std::max(bounds.width(), bounds.height());
        const double small = std::min(bounds.width(), bounds.height());
        REQUIRE(large == Catch::Approx(std::max(expected.first, expected.second)).margin(1.0e-9));
        REQUIRE(small == Catch::Approx(std::min(expected.first, expected.second)).margin(1.0e-9));
    }
}

TEST_CASE("source units are converted so the drawing is always millimetres", "[drawing-projection]")
{
    // A 2 inch cube in a document whose unit is inches must measure 50.8 mm.
    const TopoDS_Shape cube = BRepPrimAPI_MakeBox(2.0, 2.0, 2.0).Shape();
    auto request = requestFor(cube, gp_Dir(0.0, 0.0, 1.0));
    request.sourceToMillimeters = 25.4;

    const auto bounds = loupe::drawing::project(request).drawing.bounds();

    REQUIRE(bounds.width() == Catch::Approx(50.8).margin(1.0e-9));
    REQUIRE(bounds.height() == Catch::Approx(50.8).margin(1.0e-9));
}

TEST_CASE("a drilled plate keeps its hole as an analytic arc", "[drawing-projection]")
{
    // Circles must survive as arcs, not tessellated polylines: DXF can encode an
    // arc exactly, and a 200-segment hole makes a cutter dwell oddly.
    const auto result = loupe::drawing::project(requestFor(drilledPlate(), gp_Dir(0.0, 0.0, 1.0)));

    REQUIRE(result.statistics.analyticCircles > 0);

    bool foundHoleRadius = false;
    for (const auto& layer : result.drawing.layers) {
        for (const auto& contour : layer.contours) {
            for (const auto& primitive : contour.primitives) {
                if (const auto* arc = std::get_if<Arc>(&primitive)) {
                    if (std::abs(arc->radius - 5.0) < 1.0e-6) foundHoleRadius = true;
                }
            }
        }
    }
    REQUIRE(foundHoleRadius);
}

TEST_CASE("the outer profile of a plate closes", "[drawing-projection]")
{
    // A cutter needs closed contours; an open outer profile is a defect.
    const auto result = loupe::drawing::project(requestFor(drilledPlate(), gp_Dir(0.0, 0.0, 1.0)));

    REQUIRE(result.statistics.closedContours > 0);
    REQUIRE(result.drawing.warningCount(loupe::drawing::warningCode::openContour)
            == result.statistics.openContours);
}

TEST_CASE("cut mode emits only cut geometry while technical view adds smooth edges", "[drawing-projection]")
{
    // A filleted box has tangent-continuous edges, which belong on a reference
    // layer and must never reach a cut layer.
    const TopoDS_Shape cylinder = BRepPrimAPI_MakeCylinder(8.0, 20.0).Shape();

    auto cutRequest = requestFor(cylinder, gp_Dir(1.0, 0.0, 0.0));
    cutRequest.mode = ContentMode::CutContours;
    const auto cut = loupe::drawing::project(cutRequest);
    for (const auto& layer : cut.drawing.layers) {
        REQUIRE(layer.role != LayerRole::Smooth);
    }

    auto technicalRequest = requestFor(cylinder, gp_Dir(1.0, 0.0, 0.0));
    technicalRequest.mode = ContentMode::TechnicalView;
    const auto technical = loupe::drawing::project(technicalRequest);
    REQUIRE(countPrimitives(technical.drawing) >= countPrimitives(cut.drawing));
}

TEST_CASE("statistics account for every converted edge", "[drawing-projection]")
{
    const auto result = loupe::drawing::project(requestFor(drilledPlate(), gp_Dir(0.0, 0.0, 1.0)));
    const auto& statistics = result.statistics;

    REQUIRE(statistics.edges > 0);
    const int classified = statistics.analyticLines + statistics.analyticCircles + statistics.recoveredArcs
        + statistics.exactCubics + statistics.tessellatedCurves;
    REQUIRE(classified >= statistics.edges);
    REQUIRE(statistics.closedContours + statistics.openContours > 0);
}

TEST_CASE("projection never invents a full circle from an open arc", "[drawing-projection]")
{
    // Regression. Arc recovery used an absolute closure tolerance, so a small
    // radius arc sweeping nearly all the way round counted as closed and was
    // completed to a full 360 degrees. On a corpus part that produced six
    // complete circles the model does not contain.
    //
    // A drilled plate has genuine full circles and genuine partial fillet arcs, so
    // any full circle here must have a radius matching a real feature rather than
    // a completed fragment.
    const auto result = loupe::drawing::project(requestFor(drilledPlate(), gp_Dir(0.0, 0.0, 1.0)));

    for (const auto& layer : result.drawing.layers) {
        for (const auto& contour : layer.contours) {
            for (const auto& primitive : contour.primitives) {
                const auto* arc = std::get_if<Arc>(&primitive);
                if (arc == nullptr || !arc->isFullCircle()) continue;
                // The only circular feature in this fixture is the 5 mm hole.
                REQUIRE(arc->radius == Catch::Approx(5.0).margin(1.0e-6));
            }
        }
    }
}

TEST_CASE("silhouette mode keeps only edges that bound material", "[drawing-projection][silhouette]")
{
    // A stepped block: viewed from the side, the step's top face projects to a line
    // straight across the middle. It is genuinely visible, so hidden-line removal
    // reports it, but it does not describe where material ends and must not appear
    // in a cut outline.
    const TopoDS_Shape base = BRepPrimAPI_MakeBox(40.0, 20.0, 10.0).Shape();
    const TopoDS_Shape upper =
        BRepPrimAPI_MakeBox(gp_Pnt(0.0, 0.0, 10.0), 40.0, 10.0, 10.0).Shape();
    const TopoDS_Shape stepped = BRepAlgoAPI_Fuse(base, upper).Shape();

    auto cutRequest = requestFor(stepped, gp_Dir(0.0, 1.0, 0.0));
    cutRequest.mode = ContentMode::CutContours;
    const auto cut = loupe::drawing::project(cutRequest);

    auto silhouetteRequest = requestFor(stepped, gp_Dir(0.0, 1.0, 0.0));
    silhouetteRequest.mode = ContentMode::OuterContourOnly;
    const auto silhouette = loupe::drawing::project(silhouetteRequest);

    // The silhouette must be strictly simpler, and it must say what it dropped.
    REQUIRE(silhouette.statistics.interiorEdgesRemoved > 0);
    REQUIRE(countPrimitives(silhouette.drawing) < countPrimitives(cut.drawing));

    // Closed by construction: classifying regions and keeping their shared boundary
    // cannot leave a dangling end, which is what a cutter needs.
    REQUIRE(silhouette.statistics.closedContours >= 1);
    REQUIRE(silhouette.statistics.openContours == 0);

    // Same real-world size -- filtering must not move anything.
    const auto cutBounds = cut.drawing.bounds();
    const auto silhouetteBounds = silhouette.drawing.bounds();
    REQUIRE(silhouetteBounds.width() == Catch::Approx(cutBounds.width()).margin(1.0e-6));
    REQUIRE(silhouetteBounds.height() == Catch::Approx(cutBounds.height()).margin(1.0e-6));
}

TEST_CASE("silhouette mode preserves a through hole", "[drawing-projection][silhouette]")
{
    // A hole is a boundary: material on one side, empty space on the other. Dropping
    // it would be as wrong as keeping a step line.
    auto request = requestFor(drilledPlate(), gp_Dir(0.0, 0.0, 1.0));
    request.mode = ContentMode::OuterContourOnly;
    const auto result = loupe::drawing::project(request);

    REQUIRE(result.statistics.closedContours >= 2);
    REQUIRE(result.statistics.openContours == 0);

    bool keptHole = false;
    for (const auto& layer : result.drawing.layers) {
        for (const auto& contour : layer.contours) {
            for (const auto& primitive : contour.primitives) {
                if (const auto* arc = std::get_if<Arc>(&primitive)) {
                    if (std::abs(arc->radius - 5.0) < 1.0e-6) keptHole = true;
                }
            }
        }
    }
    REQUIRE(keptHole);
}

TEST_CASE("a view direction parallel to the up direction is refused", "[drawing-projection]")
{
    // OCCT would otherwise throw from inside an axis constructor, with nothing a
    // user could act on.
    ProjectionRequest request;
    request.shape = BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape();
    request.viewDirection = gp_Dir(0.0, 0.0, 1.0);
    request.upDirection = gp_Dir(0.0, 0.0, 1.0);

    REQUIRE_THROWS_AS(loupe::drawing::project(request), ProjectionError);
    try {
        loupe::drawing::project(request);
    } catch (const ProjectionError& error) {
        REQUIRE(error.code() == ProjectionError::Code::DegenerateView);
    }
}

TEST_CASE("an empty shape is refused", "[drawing-projection]")
{
    ProjectionRequest request;
    REQUIRE_THROWS_AS(loupe::drawing::project(request), ProjectionError);
}

TEST_CASE("a non-positive tolerance or scale is refused", "[drawing-projection]")
{
    auto request = requestFor(BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape(), gp_Dir(0.0, 0.0, 1.0));

    auto badDeflection = request;
    badDeflection.deflectionMm = 0.0;
    REQUIRE_THROWS_AS(loupe::drawing::project(badDeflection), ProjectionError);

    auto badScale = request;
    badScale.sourceToMillimeters = -1.0;
    REQUIRE_THROWS_AS(loupe::drawing::project(badScale), ProjectionError);
}

TEST_CASE("coincident sharp and silhouette edges are not emitted twice", "[drawing-projection]")
{
    // A cylinder viewed down its axis puts its rim in both result compounds. The
    // rim must appear once: cutting the same contour twice scorches material.
    const TopoDS_Shape cylinder = BRepPrimAPI_MakeCylinder(10.0, 20.0).Shape();
    const auto result = loupe::drawing::project(requestFor(cylinder, gp_Dir(0.0, 0.0, 1.0)));

    int fullCircles = 0;
    for (const auto& layer : result.drawing.layers) {
        for (const auto& contour : layer.contours) {
            for (const auto& primitive : contour.primitives) {
                if (const auto* arc = std::get_if<Arc>(&primitive)) {
                    if (arc->isFullCircle() && std::abs(arc->radius - 10.0) < 1.0e-6) ++fullCircles;
                }
            }
        }
    }
    REQUIRE(fullCircles == 1);
}

} // namespace
