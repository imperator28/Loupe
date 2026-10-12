#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/drawing/DrawingPreview.h"
#include "core/drawing/DrawingProjector.h"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_NurbsConvert.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRep_Builder.hxx>
#include <gp_Trsf.hxx>
#include <gp_Ax1.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax3.hxx>
#include <gp_Pln.hxx>
#include <TopoDS_Compound.hxx>

#include <nlohmann/json.hpp>

#include <array>
#include <vector>
#include <variant>
#include <algorithm>
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
        + statistics.exactCubics + statistics.tessellatedCurves + statistics.edgeOnCircles + statistics.arcFittedCurves;
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

TEST_CASE("silhouette mode ignores non-solid bodies", "[drawing-projection][silhouette]")
{
    // Only a solid bounds material, so only a solid can be classified inside or
    // outside. A loose face in the same compound contributes edges no region test
    // can place, and those survived as stray contours: one corpus part emitted
    // fourteen contours where it has one outline. Restricting to solids fixed it.
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(40.0, 30.0, 10.0).Shape();
    const TopoDS_Shape strayFace =
        BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(5.0, 5.0, 20.0), gp_Dir(0.0, 0.0, 1.0)),
                                0.0, 12.0, 0.0, 8.0).Face();
    BRep_Builder builder;
    TopoDS_Compound mixed;
    builder.MakeCompound(mixed);
    builder.Add(mixed, box);
    builder.Add(mixed, strayFace);

    auto request = requestFor(mixed, gp_Dir(0.0, 0.0, 1.0));
    request.mode = ContentMode::OuterContourOnly;
    const auto filtered = loupe::drawing::project(request);

    // Just the box outline, and the extents are the box's alone.
    REQUIRE(filtered.statistics.closedContours == 1);
    REQUIRE(filtered.statistics.openContours == 0);
    const auto bounds = filtered.drawing.bounds();
    REQUIRE(bounds.width() == Catch::Approx(40.0).margin(1.0e-6));
    REQUIRE(bounds.height() == Catch::Approx(30.0).margin(1.0e-6));

    // Dropping part of the model silently would be a surprising way to get a clean
    // drawing, so it has to be reported.
    REQUIRE(filtered.drawing.warningCount(loupe::drawing::warningCode::nonSolidBodiesIgnored) > 0);

    // Opting out keeps every body, which is what the option is for.
    request.silhouetteSolidsOnly = false;
    const auto unfiltered = loupe::drawing::project(request);
    REQUIRE(unfiltered.drawing.warningCount(loupe::drawing::warningCode::nonSolidBodiesIgnored) == 0);
    REQUIRE(unfiltered.drawing.bounds().width() >= bounds.width());
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

namespace {

struct PrimitiveCounts final {
    int segments{};
    int arcs{};
    int cubics{};
    std::vector<Arc> arcList;
};

PrimitiveCounts countByType(const loupe::drawing::Drawing& drawing)
{
    PrimitiveCounts counts;
    for (const auto& layer : drawing.layers) {
        for (const auto& contour : layer.contours) {
            for (const auto& primitive : contour.primitives) {
                if (std::holds_alternative<Segment>(primitive)) ++counts.segments;
                else if (const auto* arc = std::get_if<Arc>(&primitive)) { ++counts.arcs; counts.arcList.push_back(*arc); }
                else ++counts.cubics;
            }
        }
    }
    return counts;
}

} // namespace

TEST_CASE("a view picked from a face's mesh normal still draws the hole as a circle", "[drawing-projection][snap]")
{
    // Picking a face hands over the display mesh's float normal, a few micro-radians off
    // the face. HLR keeps a circle a circle only when its axis is parallel to the view to
    // 1e-12 rad, so the hole came back as an ellipse and was drawn as a polygon.
    const gp_Dir meshNormal(1.0e-6, -0.7e-6, 1.0);
    const auto result = loupe::drawing::project(requestFor(drilledPlate(), meshNormal));

    REQUIRE(result.snappedToGeometry);
    REQUIRE(result.viewDirectionUsed.IsParallel(gp_Dir(0.0, 0.0, 1.0), 1.0e-12));
    REQUIRE(result.statistics.tessellatedCurves == 0);
    const auto counts = countByType(result.drawing);
    REQUIRE(std::ranges::any_of(counts.arcList, [](const Arc& arc) { return std::abs(arc.radius - 5.0) < 1.0e-6; }));
    REQUIRE(counts.segments == 4);
}

TEST_CASE("an exact standard view is never snapped onto a nearby face", "[drawing-projection][snap]")
{
    // A face tilted a fraction off a principal plane must not drag the named view with it.
    gp_Trsf tilt;
    tilt.SetRotation(gp_Ax1(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(1.0, 0.0, 0.0)), 2.0e-4);
    const TopoDS_Shape tilted = BRepBuilderAPI_Transform(drilledPlate(), tilt).Shape();
    const auto result = loupe::drawing::project(requestFor(tilted, gp_Dir(0.0, 0.0, 1.0)));

    REQUIRE_FALSE(result.snappedToGeometry);
    REQUIRE(result.viewDirectionUsed.IsEqual(gp_Dir(0.0, 0.0, 1.0), 1.0e-15));
}

TEST_CASE("a hole seen obliquely is drawn as arcs within tolerance, not a polygon", "[drawing-projection][snap]")
{
    // At 45 degrees a 5 mm hole is an ellipse. DXF R12 has no ellipse, and tessellating one
    // to 0.01 mm takes hundreds of segments; a chain of checked arcs holds the same tolerance.
    const gp_Dir oblique(0.0, -1.0, 1.0);
    auto request = requestFor(drilledPlate(), oblique);
    request.upDirection = gp_Dir(0.0, 1.0, 1.0);
    const auto result = loupe::drawing::project(request);

    REQUIRE_FALSE(result.snappedToGeometry);
    REQUIRE(result.statistics.arcFittedCurves > 0);
    REQUIRE(result.statistics.tessellatedCurves == 0);
    const auto counts = countByType(result.drawing);
    REQUIRE(counts.arcs >= 8);
    REQUIRE(counts.arcs < 200);
    // Every arc lies on one of the hole's two rims as they truly project, within the
    // drawing tolerance. The rims are projected through the same frame the projector uses.
    gp_Trsf frame;
    frame.SetTransformation(gp_Ax3(gp_Pnt(0.0, 0.0, 0.0), result.viewDirectionUsed,
                                   request.upDirection.Crossed(result.viewDirectionUsed)));
    std::vector<gp_Pnt2d> rims;
    for (const double height : {0.0, 5.0}) {
        for (int step = 0; step < 4000; ++step) {
            const double angle = 2.0 * std::numbers::pi * step / 4000.0;
            gp_Pnt point(20.0 + 5.0 * std::cos(angle), 15.0 + 5.0 * std::sin(angle), height);
            point.Transform(frame);
            rims.emplace_back(point.X(), point.Y());
        }
    }
    for (const auto& arc : counts.arcList) {
        for (int step = 0; step <= 8; ++step) {
            const auto sample = arc.pointAtAngle(arc.startAngle + arc.sweepAngle * step / 8.0);
            double nearest = 1.0e9;
            for (const auto& rim : rims) nearest = std::min(nearest, rim.Distance(sample));
            REQUIRE(nearest < request.deflectionMm);
        }
    }
}

TEST_CASE("a hole seen exactly edge-on is a straight line, not OCCT's coarse polyline", "[drawing-projection]")
{
    // HLR gives up on an edge-on circle and emits a fixed 15-point polyline, which for a
    // full circle runs out and back over the same line: fourteen overlapping segments on a
    // cut path. It is exactly a line, and is emitted as one run each way at most.
    const auto result = loupe::drawing::project(requestFor(drilledPlate(), gp_Dir(1.0, 0.0, 0.0)));

    REQUIRE(result.statistics.coarseFallbackEdges == 0);
    REQUIRE(result.drawing.warningCount(loupe::drawing::warningCode::coarseCurveFallback) == 0);
}

TEST_CASE("a hole stored as one closed spline is still drawn as a circle", "[drawing-projection]")
{
    // ECAD part libraries routinely write a whole hole as a single closed NURBS curve. Arc
    // recovery fitted a circle through the curve's start, middle and end -- and on a closed
    // curve the start and end are one point, so it never fitted, and every hole was
    // tessellated into segments.
    const TopoDS_Shape splines = BRepBuilderAPI_NurbsConvert(drilledPlate(), true).Shape();
    const auto result = loupe::drawing::project(requestFor(splines, gp_Dir(0.0, 0.0, 1.0)));

    REQUIRE(result.statistics.tessellatedCurves == 0);
    REQUIRE(result.statistics.recoveredArcs > 0);
    const auto counts = countByType(result.drawing);
    REQUIRE(counts.segments == 4);
    REQUIRE(std::ranges::any_of(counts.arcList, [](const Arc& arc) {
        return std::abs(arc.radius - 5.0) < 1.0e-6 && arc.isFullCircle();
    }));
}

TEST_CASE("the preview draws a small hole round, not as a polygon", "[drawing-projection][preview]")
{
    // The preview zooms to fit the part, so a fixed 0.05 mm chord tolerance drew a 0.9 mm
    // hole on a 5 mm connector as an octagon -- making a true arc look faceted.
    const TopoDS_Shape plate = BRepPrimAPI_MakeBox(5.0, 5.0, 2.0).Shape();
    const TopoDS_Shape drill =
        BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(2.5, 2.5, -1.0), gp_Dir(0.0, 0.0, 1.0)), 0.45, 4.0).Shape();
    const auto result = loupe::drawing::project(requestFor(BRepAlgoAPI_Cut(plate, drill).Shape(), gp_Dir(0.0, 0.0, 1.0)));
    const auto preview = nlohmann::json::parse(loupe::drawing::encodePreview(result));

    int roundest = 0;
    for (const auto& layer : preview.at("layers")) {
        for (const auto& contour : layer.at("contours")) {
            roundest = std::max(roundest, static_cast<int>(contour.at("points").size() / 2));
        }
    }
    REQUIRE(roundest >= 64);
}
