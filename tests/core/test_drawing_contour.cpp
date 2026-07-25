#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/drawing/DrawingContour.h"

#include <numbers>

namespace {

using loupe::drawing::Arc;
using loupe::drawing::Bounds2d;
using loupe::drawing::Contour;
using loupe::drawing::Cubic;
using loupe::drawing::Drawing;
using loupe::drawing::Layer;
using loupe::drawing::LayerRole;
using loupe::drawing::Primitive;
using loupe::drawing::Segment;

constexpr double pi = std::numbers::pi;

// Catch's Approx is relative, so Approx(0.0) demands bit-exact equality. Arc
// geometry runs through trig, and cos(pi/2) is 6e-17 rather than 0, so any
// comparison against zero needs an absolute margin. A trillionth of a
// millimetre is far tighter than any geometric tolerance that matters while
// still tolerating IEEE noise.
constexpr double zeroMargin = 1.0e-12;

Drawing drawingWith(std::vector<Primitive> primitives, const bool closed = false)
{
    Drawing drawing;
    Layer layer;
    layer.role = LayerRole::Cut;
    layer.name = loupe::drawing::defaultLayerName(LayerRole::Cut);
    layer.contours.push_back(Contour{std::move(primitives), closed});
    drawing.layers.push_back(std::move(layer));
    return drawing;
}

TEST_CASE("segment bounds cover both endpoints", "[drawing-contour]")
{
    const auto bounds = loupe::drawing::primitiveBounds(Segment{{10.0, 40.0}, {30.0, 5.0}});

    REQUIRE(bounds.valid);
    REQUIRE(bounds.minX == Catch::Approx(10.0));
    REQUIRE(bounds.maxX == Catch::Approx(30.0));
    REQUIRE(bounds.minY == Catch::Approx(5.0));
    REQUIRE(bounds.maxY == Catch::Approx(40.0));
    REQUIRE(bounds.width() == Catch::Approx(20.0));
    REQUIRE(bounds.height() == Catch::Approx(35.0));
}

TEST_CASE("full circle bounds are the circumscribing square", "[drawing-contour]")
{
    const auto bounds = loupe::drawing::primitiveBounds(Arc{{5.0, -3.0}, 4.0, 0.0, 2.0 * pi});

    REQUIRE(bounds.minX == Catch::Approx(1.0));
    REQUIRE(bounds.maxX == Catch::Approx(9.0));
    REQUIRE(bounds.minY == Catch::Approx(-7.0));
    REQUIRE(bounds.maxY == Catch::Approx(1.0));
}

TEST_CASE("quarter arc bounds exclude quadrant points outside the sweep", "[drawing-contour]")
{
    // First quadrant only: (1,0) to (0,1) about the origin. A naive
    // circumscribing square would report -1..1 on both axes.
    const auto bounds = loupe::drawing::primitiveBounds(Arc{{0.0, 0.0}, 1.0, 0.0, pi / 2.0});

    REQUIRE(bounds.minX == Catch::Approx(0.0).margin(zeroMargin));
    REQUIRE(bounds.maxX == Catch::Approx(1.0));
    REQUIRE(bounds.minY == Catch::Approx(0.0).margin(zeroMargin));
    REQUIRE(bounds.maxY == Catch::Approx(1.0));
}

TEST_CASE("clockwise arc bounds follow the negative sweep", "[drawing-contour]")
{
    // Start at (0,1) sweeping clockwise to (1,0): same span as the quarter arc
    // above, reached the other way round.
    const auto bounds = loupe::drawing::primitiveBounds(Arc{{0.0, 0.0}, 1.0, pi / 2.0, -pi / 2.0});

    REQUIRE(bounds.minX == Catch::Approx(0.0).margin(zeroMargin));
    REQUIRE(bounds.maxX == Catch::Approx(1.0));
    REQUIRE(bounds.minY == Catch::Approx(0.0).margin(zeroMargin));
    REQUIRE(bounds.maxY == Catch::Approx(1.0));
}

TEST_CASE("cubic bounds are tight, not the control hull", "[drawing-contour]")
{
    // Control points reach y = 30 but the curve itself only reaches 22.5. The
    // preview's measured extents are the user's scale check, so an inflated
    // number would misreport the part size.
    const Cubic cubic{{0.0, 0.0}, {0.0, 30.0}, {10.0, 30.0}, {10.0, 0.0}};
    const auto bounds = loupe::drawing::primitiveBounds(cubic);

    REQUIRE(bounds.minX == Catch::Approx(0.0));
    REQUIRE(bounds.maxX == Catch::Approx(10.0));
    REQUIRE(bounds.minY == Catch::Approx(0.0));
    REQUIRE(bounds.maxY == Catch::Approx(22.5));
    REQUIRE(bounds.maxY < 30.0);
}

TEST_CASE("primitive endpoints resolve for every kind", "[drawing-contour]")
{
    const Primitive segment = Segment{{1.0, 2.0}, {3.0, 4.0}};
    REQUIRE(loupe::drawing::primitiveStart(segment).X() == Catch::Approx(1.0));
    REQUIRE(loupe::drawing::primitiveEnd(segment).Y() == Catch::Approx(4.0));

    const Primitive arc = Arc{{0.0, 0.0}, 2.0, 0.0, pi};
    REQUIRE(loupe::drawing::primitiveStart(arc).X() == Catch::Approx(2.0));
    REQUIRE(loupe::drawing::primitiveEnd(arc).X() == Catch::Approx(-2.0));

    const Primitive cubic = Cubic{{0.0, 0.0}, {1.0, 1.0}, {2.0, 1.0}, {3.0, 0.0}};
    REQUIRE(loupe::drawing::primitiveEnd(cubic).X() == Catch::Approx(3.0));
}

TEST_CASE("translation moves geometry to the margin without changing size", "[drawing-contour]")
{
    const auto drawing = drawingWith({Segment{{-15.0, 100.0}, {85.0, 150.0}}});
    const auto moved = drawing.translatedToOrigin(5.0);
    const auto before = drawing.bounds();
    const auto after = moved.bounds();

    REQUIRE(after.minX == Catch::Approx(5.0));
    REQUIRE(after.minY == Catch::Approx(5.0));
    // Scale must be untouched: a translation is the only post-projection
    // transform precisely so 1:1 cannot be perturbed.
    REQUIRE(after.width() == Catch::Approx(before.width()));
    REQUIRE(after.height() == Catch::Approx(before.height()));
}

TEST_CASE("translation leaves arc angles untouched", "[drawing-contour]")
{
    const auto drawing = drawingWith({Arc{{0.0, 0.0}, 3.0, pi / 4.0, pi / 2.0}});
    const auto moved = drawing.translatedToOrigin(0.0);
    const auto& arc = std::get<Arc>(moved.layers.front().contours.front().primitives.front());

    REQUIRE(arc.startAngle == Catch::Approx(pi / 4.0));
    REQUIRE(arc.sweepAngle == Catch::Approx(pi / 2.0));
}

TEST_CASE("vertical mirror inverts arc sweep and preserves extents", "[drawing-contour]")
{
    constexpr double pageHeight = 40.0;
    const auto drawing = drawingWith({Arc{{10.0, 10.0}, 5.0, 0.0, pi / 2.0}});
    const auto flipped = drawing.mirroredVertically(pageHeight);

    const auto& arc = std::get<Arc>(flipped.layers.front().contours.front().primitives.front());
    REQUIRE(arc.centre.Y() == Catch::Approx(pageHeight - 10.0));
    // Sign inversion is the whole point: mirroring positions but not sweeps
    // would turn a fillet into a gouge.
    REQUIRE(arc.sweepAngle == Catch::Approx(-pi / 2.0));
    REQUIRE(arc.startAngle == Catch::Approx(0.0));

    const auto before = drawing.bounds();
    const auto after = flipped.bounds();
    REQUIRE(after.width() == Catch::Approx(before.width()));
    REQUIRE(after.height() == Catch::Approx(before.height()));
}

TEST_CASE("mirrored arc endpoints match reflected originals", "[drawing-contour]")
{
    constexpr double pageHeight = 40.0;
    const Arc original{{10.0, 10.0}, 5.0, 0.0, pi / 2.0};
    const auto drawing = drawingWith({original});
    const auto flipped = drawing.mirroredVertically(pageHeight);
    const auto& arc = std::get<Arc>(flipped.layers.front().contours.front().primitives.front());

    // Reflecting the original endpoints must land exactly on the mirrored arc's
    // endpoints; if the angle transform were wrong the arc would still span the
    // right box while bulging the wrong way, which no bounds check would catch.
    REQUIRE(arc.startPoint().X() == Catch::Approx(original.startPoint().X()));
    REQUIRE(arc.startPoint().Y() == Catch::Approx(pageHeight - original.startPoint().Y()));
    REQUIRE(arc.endPoint().X() == Catch::Approx(original.endPoint().X()));
    REQUIRE(arc.endPoint().Y() == Catch::Approx(pageHeight - original.endPoint().Y()));
}

TEST_CASE("mirroring twice is the identity", "[drawing-contour]")
{
    constexpr double pageHeight = 63.5;
    const auto drawing = drawingWith({Segment{{1.0, 2.0}, {9.0, 20.0}},
                                      Arc{{4.0, 4.0}, 2.0, pi / 3.0, -pi / 6.0},
                                      Cubic{{0.0, 0.0}, {1.0, 5.0}, {4.0, 5.0}, {5.0, 0.0}}});
    const auto twice = drawing.mirroredVertically(pageHeight).mirroredVertically(pageHeight);

    const auto& original = drawing.layers.front().contours.front().primitives;
    const auto& restored = twice.layers.front().contours.front().primitives;
    REQUIRE(restored.size() == original.size());

    const auto& originalArc = std::get<Arc>(original[1]);
    const auto& restoredArc = std::get<Arc>(restored[1]);
    REQUIRE(restoredArc.centre.Y() == Catch::Approx(originalArc.centre.Y()));
    REQUIRE(restoredArc.startAngle == Catch::Approx(originalArc.startAngle));
    REQUIRE(restoredArc.sweepAngle == Catch::Approx(originalArc.sweepAngle));

    const auto& restoredCubic = std::get<Cubic>(restored[2]);
    REQUIRE(restoredCubic.firstControl.Y() == Catch::Approx(5.0));
}

TEST_CASE("closure flag survives transforms", "[drawing-contour]")
{
    const auto closed = drawingWith({Segment{{0.0, 0.0}, {10.0, 0.0}}}, true);
    REQUIRE(closed.layers.front().contours.front().closed);
    REQUIRE(closed.translatedToOrigin(2.0).layers.front().contours.front().closed);
    REQUIRE(closed.mirroredVertically(10.0).layers.front().contours.front().closed);

    const auto open = drawingWith({Segment{{0.0, 0.0}, {10.0, 0.0}}}, false);
    REQUIRE_FALSE(open.mirroredVertically(10.0).layers.front().contours.front().closed);
}

TEST_CASE("drawing aggregates bounds across layers and reports counts", "[drawing-contour]")
{
    Drawing drawing;
    Layer cut;
    cut.role = LayerRole::Cut;
    cut.contours.push_back(Contour{{Segment{{0.0, 0.0}, {10.0, 10.0}}}, true});
    Layer reference;
    reference.role = LayerRole::Reference;
    reference.contours.push_back(Contour{{Segment{{-5.0, 20.0}, {0.0, 25.0}}}, false});
    drawing.layers.push_back(std::move(cut));
    drawing.layers.push_back(std::move(reference));

    const auto bounds = drawing.bounds();
    REQUIRE(bounds.minX == Catch::Approx(-5.0));
    REQUIRE(bounds.maxY == Catch::Approx(25.0));
    REQUIRE(drawing.contourCount() == 2);
    REQUIRE_FALSE(drawing.empty());
}

TEST_CASE("an empty drawing has invalid bounds and zero size", "[drawing-contour]")
{
    const Drawing drawing;

    REQUIRE(drawing.empty());
    REQUIRE(drawing.contourCount() == 0);
    REQUIRE_FALSE(drawing.bounds().valid);
    REQUIRE(drawing.bounds().width() == Catch::Approx(0.0));
    // Transforms on empty input must not fault or invent geometry.
    REQUIRE(drawing.translatedToOrigin(5.0).empty());
    REQUIRE(drawing.mirroredVertically(5.0).empty());
}

TEST_CASE("warnings are queryable by code", "[drawing-contour]")
{
    Drawing drawing;
    drawing.warnings.push_back({std::string(loupe::drawing::warningCode::openContour), 3});

    REQUIRE(drawing.warningCount(loupe::drawing::warningCode::openContour) == 3);
    REQUIRE(drawing.warningCount(loupe::drawing::warningCode::coarseCurveFallback) == 0);
}

TEST_CASE("layer roles have distinct default names", "[drawing-contour]")
{
    REQUIRE(loupe::drawing::defaultLayerName(LayerRole::Cut) == "CUT");
    REQUIRE(loupe::drawing::defaultLayerName(LayerRole::Outline) == "OUTLINE");
    REQUIRE(loupe::drawing::defaultLayerName(LayerRole::Hidden) == "HIDDEN");
    REQUIRE(loupe::drawing::defaultLayerName(LayerRole::Reference) == "REFERENCE");
    // Layer 0 is reserved in DXF; no role may collide with it.
    REQUIRE(loupe::drawing::defaultLayerName(LayerRole::Smooth) != "0");
}

} // namespace
