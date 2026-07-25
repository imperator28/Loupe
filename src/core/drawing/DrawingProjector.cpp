#include "core/drawing/DrawingProjector.h"

#include <BRepAlgoAPI_Splitter.hxx>
#include <BndLib_Add2dCurve.hxx>
#include <Bnd_Box2d.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepLib.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <Geom2dAdaptor_Curve.hxx>
#include <Geom2dConvert_BSplineCurveToBezierCurve.hxx>
#include <Geom2d_BSplineCurve.hxx>
#include <Geom2d_BezierCurve.hxx>
#include <Geom2d_Curve.hxx>
#include <Geom_Plane.hxx>
#include <Poly_Triangulation.hxx>
#include <Geom_Surface.hxx>
#include <HLRAlgo_Projector.hxx>
#include <HLRBRep_Algo.hxx>
#include <HLRBRep_HLRToShape.hxx>
#include <NCollection_HSequence.hxx>
#include <Precision.hxx>
#include <ShapeAnalysis_FreeBounds.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <gp.hxx>
#include <gp_Pln.hxx>
#include <NCollection_List.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax3.hxx>
#include <gp_Vec2d.hxx>
#include <gp_Circ2d.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <optional>
#include <map>
#include <set>
#include <utility>

namespace loupe::drawing {
namespace {

constexpr double twoPi = 2.0 * std::numbers::pi;
constexpr double angleEpsilon = 1.0e-12;
constexpr int arcRecoverySamples = 12;
using ShapeSequence = NCollection_HSequence<TopoDS_Shape>;

[[nodiscard]] double normalizeToZeroTwoPi(double angle)
{
    angle = std::fmod(angle, twoPi);
    if (angle < 0.0) angle += twoPi;
    return angle;
}

// Build an arc from its centre and three points on it. Using the midpoint to
// pick the direction avoids having to reason about a gp_Circ2d's own axis sense,
// which is a common source of reversed arcs.
[[nodiscard]] Arc arcThrough(const gp_Pnt2d& centre, const double radius, const gp_Pnt2d& start,
                             const gp_Pnt2d& middle, const gp_Pnt2d& end, const bool fullCircle)
{
    const double startAngle = std::atan2(start.Y() - centre.Y(), start.X() - centre.X());
    if (fullCircle) return {centre, radius, startAngle, twoPi};
    const double endAngle = std::atan2(end.Y() - centre.Y(), end.X() - centre.X());
    const double middleAngle = std::atan2(middle.Y() - centre.Y(), middle.X() - centre.X());
    const double counterClockwise = normalizeToZeroTwoPi(endAngle - startAngle);
    const double middleOffset = normalizeToZeroTwoPi(middleAngle - startAngle);
    if (middleOffset <= counterClockwise + angleEpsilon) return {centre, radius, startAngle, counterClockwise};
    return {centre, radius, startAngle, counterClockwise - twoPi};
}

// Circle through three points, by intersecting perpendicular bisectors. Returns
// nothing when the points are collinear, which is also how a straight or
// near-straight curve is rejected from arc recovery.
[[nodiscard]] std::optional<std::pair<gp_Pnt2d, double>> circleThrough(const gp_Pnt2d& a, const gp_Pnt2d& b,
                                                                      const gp_Pnt2d& c)
{
    const double ax = a.X();
    const double ay = a.Y();
    const double bx = b.X();
    const double by = b.Y();
    const double cx = c.X();
    const double cy = c.Y();
    const double determinant = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
    if (std::abs(determinant) < 1.0e-12) return std::nullopt;
    const double aSq = ax * ax + ay * ay;
    const double bSq = bx * bx + by * by;
    const double cSq = cx * cx + cy * cy;
    const gp_Pnt2d centre((aSq * (by - cy) + bSq * (cy - ay) + cSq * (ay - by)) / determinant,
                          (aSq * (cx - bx) + bSq * (ax - cx) + cSq * (bx - ax)) / determinant);
    return std::pair{centre, centre.Distance(a)};
}

// Gate A measured that only 32 of 171 edges on a real filleted part arrive as
// analytic geometry -- the rest are B-splines, including features that are
// geometrically circular arcs. Recovering those matters because DXF can encode a
// true arc exactly (as ARC, or as a polyline bulge) but has to approximate
// anything else, so without this most real parts lose their exact arcs.
[[nodiscard]] std::optional<Arc> recoverArc(const Geom2dAdaptor_Curve& adaptor, const double first,
                                            const double last, const double tolerance)
{
    if (last - first <= Precision::PConfusion()) return std::nullopt;
    const gp_Pnt2d start = adaptor.Value(first);
    const gp_Pnt2d middle = adaptor.Value(0.5 * (first + last));
    const gp_Pnt2d end = adaptor.Value(last);

    const auto fitted = circleThrough(start, middle, end);
    if (!fitted) return std::nullopt;
    const auto& [centre, radius] = *fitted;
    if (!std::isfinite(radius) || radius <= tolerance) return std::nullopt;

    // A nearly straight curve also "fits" a circle -- an enormous one -- and every
    // sample sits on it within tolerance. Recovering that as an arc gains nothing
    // and risks a wildly wrong bulge, so treat it as not circular.
    const double chord = start.Distance(end);
    if (chord > 0.0 && radius > chord * 1000.0) return std::nullopt;

    for (int index = 0; index <= arcRecoverySamples; ++index) {
        const double parameter = first + (last - first) * index / arcRecoverySamples;
        if (std::abs(adaptor.Value(parameter).Distance(centre) - radius) > tolerance) return std::nullopt;
    }

    // Only call it a full circle when the ends really do meet, measured against
    // the radius rather than an absolute distance.
    //
    // This guard exists because of a real defect: with an absolute 0.01 mm test, a
    // 0.3 mm-radius arc sweeping ~358 degrees counted as closed (0.01 mm is 3% of
    // that radius), so it was completed to a full 360 degrees. On one corpus part
    // that silently invented six complete circles where the model has none --
    // visible in the output and reported by review. Completing an arc the caller
    // did not ask for is exactly the kind of quiet geometry change a cut path must
    // never contain.
    const double closureTolerance = std::min(tolerance, radius * 1.0e-3);
    return arcThrough(centre, radius, start, middle, end, chord <= closureTolerance);
}

[[nodiscard]] Cubic cubicFromBezier(const occ::handle<Geom2d_BezierCurve>& bezier)
{
    return {bezier->Pole(1), bezier->Pole(2), bezier->Pole(3), bezier->Pole(4)};
}

// Raise a Bezier to degree 3 so it maps onto the IR's Cubic. Degree elevation is
// exact for a non-rational curve: the shape is unchanged, only its
// representation. Callers must already have excluded rational curves.
[[nodiscard]] occ::handle<Geom2d_BezierCurve> asCubicBezier(const occ::handle<Geom2d_BezierCurve>& source)
{
    auto copy = occ::handle<Geom2d_BezierCurve>::DownCast(source->Copy());
    if (copy->Degree() < 3) copy->Increase(3);
    return copy;
}

void appendTessellated(const Geom2dAdaptor_Curve& adaptor, const double first, const double last,
                       const double deflection, std::vector<Primitive>& out)
{
    // Halve the requested deflection: this class measures chord-to-midpoint
    // distance, which approximates true deflection rather than bounding it.
    GCPnts_QuasiUniformDeflection sampler(adaptor, std::max(deflection * 0.5, Precision::Confusion()),
                                          first, last);
    if (!sampler.IsDone() || sampler.NbPoints() < 2) {
        out.push_back(Segment{adaptor.Value(first), adaptor.Value(last)});
        return;
    }
    for (int index = 1; index < sampler.NbPoints(); ++index) {
        out.push_back(Segment{adaptor.Value(sampler.Parameter(index)),
                              adaptor.Value(sampler.Parameter(index + 1))});
    }
}

template <typename CurveHandle>
[[nodiscard]] CurveHandle trimmedCopy(const CurveHandle& source, const double first, const double last)
{
    auto copy = CurveHandle::DownCast(source->Copy());
    const double low = std::max(first, copy->FirstParameter());
    const double high = std::min(last, copy->LastParameter());
    if (high - low <= Precision::PConfusion()) return copy;
    const bool trimsStart = low > copy->FirstParameter() + Precision::PConfusion();
    const bool trimsEnd = high < copy->LastParameter() - Precision::PConfusion();
    if (trimsStart || trimsEnd) copy->Segment(low, high);
    return copy;
}

void appendPolylineFromPoles(const occ::handle<Geom2d_BSplineCurve>& spline, std::vector<Primitive>& out)
{
    for (int index = 1; index < spline->NbPoles(); ++index) {
        out.push_back(Segment{spline->Pole(index), spline->Pole(index + 1)});
    }
}

void appendBSpline(const Geom2dAdaptor_Curve& adaptor, const double first, const double last,
                   const ProjectionRequest& request, ProjectionStatistics& statistics,
                   std::vector<Primitive>& out)
{
    const auto source = adaptor.BSpline();
    if (source.IsNull()) {
        appendTessellated(adaptor, first, last, request.deflectionMm, out);
        ++statistics.tessellatedCurves;
        return;
    }
    // OCCT's hidden-line edge builder falls back to a 15-pole, degree-1 spline
    // for curve types it cannot classify: a 14-segment polyline with no
    // tolerance control. The geometry is still emitted exactly as given, but the
    // drawing must say it was coarsened -- silent degradation on a cut path is
    // precisely what the requirements forbid.
    if (source->Degree() == 1 && source->NbPoles() == 15) ++statistics.coarseFallbackEdges;

    const auto spline = trimmedCopy(source, first, last);
    if (spline->Degree() == 1) {
        appendPolylineFromPoles(spline, out);
        return;
    }
    if (const auto arc = recoverArc(adaptor, first, last, request.deflectionMm)) {
        out.push_back(*arc);
        ++statistics.recoveredArcs;
        return;
    }
    if (!spline->IsRational() && spline->Degree() <= 3) {
        try {
            Geom2dConvert_BSplineCurveToBezierCurve converter(spline);
            for (int index = 1; index <= converter.NbArcs(); ++index) {
                out.push_back(cubicFromBezier(asCubicBezier(converter.Arc(index))));
                ++statistics.exactCubics;
            }
            return;
        } catch (const Standard_Failure&) {
            // Fall through to tessellation rather than dropping the edge.
        }
    }
    appendTessellated(adaptor, first, last, request.deflectionMm, out);
    ++statistics.tessellatedCurves;
}

void appendPrimitives(const occ::handle<Geom2d_Curve>& curve, const double first, const double last,
                      const ProjectionRequest& request, ProjectionStatistics& statistics,
                      std::vector<Primitive>& out)
{
    const Geom2dAdaptor_Curve adaptor(curve, first, last);
    switch (adaptor.GetType()) {
    case GeomAbs_Line:
        out.push_back(Segment{adaptor.Value(first), adaptor.Value(last)});
        ++statistics.analyticLines;
        return;
    case GeomAbs_Circle: {
        const gp_Circ2d circle = adaptor.Circle();
        const bool fullCircle = (last - first) >= twoPi - 1.0e-9;
        out.push_back(arcThrough(circle.Location(), circle.Radius(), adaptor.Value(first),
                                 adaptor.Value(0.5 * (first + last)), adaptor.Value(last), fullCircle));
        ++statistics.analyticCircles;
        return;
    }
    case GeomAbs_BezierCurve: {
        const auto source = adaptor.Bezier();
        if (!source.IsNull() && !source->IsRational() && source->Degree() <= 3) {
            const auto bezier = trimmedCopy(source, first, last);
            if (bezier->Degree() == 1) {
                out.push_back(Segment{bezier->Pole(1), bezier->Pole(2)});
                ++statistics.analyticLines;
                return;
            }
            out.push_back(cubicFromBezier(asCubicBezier(bezier)));
            ++statistics.exactCubics;
            return;
        }
        break;
    }
    case GeomAbs_BSplineCurve:
        appendBSpline(adaptor, first, last, request, statistics, out);
        return;
    default:
        // Ellipses, hyperbolas, parabolas and offset curves are tessellated
        // within the deflection budget. An ellipse arises when a circular
        // feature is viewed obliquely; for a cut drawing the user views normal
        // to the cut face, so true circles dominate and this path is secondary.
        // A native ellipse primitive is a recorded deferred improvement.
        break;
    }
    appendTessellated(adaptor, first, last, request.deflectionMm, out);
    ++statistics.tessellatedCurves;
}

// Hidden-line result edges carry no 3D curve, only a stored pcurve, so the
// accessor has to be the one that returns whatever the edge stored. Note this is
// NOT CurveOnPlane: that projects a 3D curve onto a supplied plane and therefore
// returns null for every edge here.
[[nodiscard]] occ::handle<Geom2d_Curve> pcurveOf(const TopoDS_Edge& edge, double& first, double& last)
{
    occ::handle<Geom2d_Curve> curve;
    occ::handle<Geom_Surface> surface;
    TopLoc_Location location;
    BRep_Tool::CurveOnSurface(edge, curve, surface, location, first, last);
    return curve;
}

using EdgeKey = std::array<long long, 6>;

// Canonical key for an edge's projected geometry, endpoint order normalised so a
// reversed duplicate matches. Deduplicating matters because a sharp edge lying
// exactly on a silhouette appears in both result compounds, and cutting the same
// contour twice scorches the material.
[[nodiscard]] EdgeKey edgeKeyFor(const gp_Pnt2d& start, const gp_Pnt2d& middle, const gp_Pnt2d& end,
                                 const double tolerance)
{
    const auto quantise = [tolerance](const double value) {
        return static_cast<long long>(std::llround(value / tolerance));
    };
    gp_Pnt2d low = start;
    gp_Pnt2d high = end;
    if (high.X() < low.X() || (high.X() == low.X() && high.Y() < low.Y())) std::swap(low, high);
    return {quantise(low.X()), quantise(low.Y()), quantise(middle.X()),
            quantise(middle.Y()), quantise(high.X()), quantise(high.Y())};
}

// ---------------------------------------------------------------------------
// Outer-contour filter.
//
// The insight this rests on: hidden-line removal already produces the outer
// boundary edges. Isolating a silhouette is therefore a filtering problem, not a
// reconstruction problem -- no planar boolean union, and no dependency on
// contours being closed, which matters because closure is imperfect.
//
// The test for "does this edge bound material" is local and exact in intent: step
// just off the edge to either side, and ask whether that point is inside the
// part's projected footprint. Material on one side only means the edge is a
// boundary (outer profile, or a through-hole rim). Material on BOTH sides means
// the edge is interior -- a step top, a chamfer boundary, a fillet tangent -- and
// is dropped.
//
// The mesh is used only as the inside/outside oracle. Emitted geometry stays the
// exact analytic curve, so a borderline decision changes whether an edge is kept,
// never where it lies.
// ---------------------------------------------------------------------------

struct ProjectedTriangle final {
    gp_Pnt2d a;
    gp_Pnt2d b;
    gp_Pnt2d c;
    double minX{};
    double maxX{};
    double minY{};
    double maxY{};
};

[[nodiscard]] bool pointInTriangle(const ProjectedTriangle& triangle, const gp_Pnt2d& point)
{
    if (point.X() < triangle.minX || point.X() > triangle.maxX
        || point.Y() < triangle.minY || point.Y() > triangle.maxY) {
        return false;
    }
    const auto cross = [](const gp_Pnt2d& from, const gp_Pnt2d& to, const gp_Pnt2d& probe) {
        return (to.X() - from.X()) * (probe.Y() - from.Y()) - (to.Y() - from.Y()) * (probe.X() - from.X());
    };
    const double d1 = cross(triangle.a, triangle.b, point);
    const double d2 = cross(triangle.b, triangle.c, point);
    const double d3 = cross(triangle.c, triangle.a, point);
    const bool anyNegative = d1 < 0.0 || d2 < 0.0 || d3 < 0.0;
    const bool anyPositive = d1 > 0.0 || d2 > 0.0 || d3 > 0.0;
    // Winding is unknown and mixed across a triangulation, so accept either sense.
    return !(anyNegative && anyPositive);
}

class FootprintOracle final {
public:
    FootprintOracle(const TopoDS_Shape& shape, const gp_Trsf& projection, const double deflection)
    {
        // Triangulate to the same tolerance the drawing is built to. The mesh only
        // answers inside/outside, so its error budget is a keep-or-drop margin.
        BRepMesh_IncrementalMesh mesher(shape, std::max(deflection, Precision::Confusion()), false, 0.3, true);
        static_cast<void>(mesher.IsDone());
        for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
            const TopoDS_Face face = TopoDS::Face(explorer.Current());
            TopLoc_Location location;
            const auto triangulation = BRep_Tool::Triangulation(face, location);
            if (triangulation.IsNull()) continue;
            const gp_Trsf faceTransform = location.Transformation();
            for (int index = 1; index <= triangulation->NbTriangles(); ++index) {
                int one = 0;
                int two = 0;
                int three = 0;
                triangulation->Triangle(index).Get(one, two, three);
                const auto flatten = [&](const int node) {
                    gp_Pnt point = triangulation->Node(node).Transformed(faceTransform);
                    point.Transform(projection);
                    return gp_Pnt2d(point.X(), point.Y());
                };
                ProjectedTriangle triangle{flatten(one), flatten(two), flatten(three)};
                triangle.minX = std::min({triangle.a.X(), triangle.b.X(), triangle.c.X()});
                triangle.maxX = std::max({triangle.a.X(), triangle.b.X(), triangle.c.X()});
                triangle.minY = std::min({triangle.a.Y(), triangle.b.Y(), triangle.c.Y()});
                triangle.maxY = std::max({triangle.a.Y(), triangle.b.Y(), triangle.c.Y()});
                triangles_.push_back(triangle);
            }
        }
    }

    [[nodiscard]] bool usable() const noexcept { return !triangles_.empty(); }

    [[nodiscard]] bool contains(const gp_Pnt2d& point) const
    {
        for (const auto& triangle : triangles_) {
            if (pointInTriangle(triangle, point)) return true;
        }
        return false;
    }

private:
    std::vector<ProjectedTriangle> triangles_;
};

// ---------------------------------------------------------------------------
// Silhouette by region classification.
//
// The outer contour is already present in the hidden-line output; the problem is
// deciding WHICH of those edges bound material. Two earlier framings were wrong:
//
//   * Classifying each edge by probing to either side. Hidden-line removal splits
//     edges at visibility changes, not at boundary/interior transitions, so an edge
//     that is partly boundary gets one verdict for its whole length -- leaving
//     over-extended stubs -- and a probe near a thin wall straddles it, dropping a
//     genuine boundary edge. No probe distance satisfies both.
//   * Taking edges belonging to exactly one projected triangle. Front and back
//     faces project onto the same area, so the projected triangles overlap
//     arbitrarily and are not a valid planar complex; shared-edge counting does not
//     describe the union boundary at all.
//
// What works is to treat it as an area question, which is what a silhouette is.
// Split a canvas face by the projected edges to get the planar regions they define,
// ask of each region whether it is inside the part's footprint, and keep the edges
// that separate an inside region from an outside one. Those edges are the original
// analytic curves, so the output stays exact -- the mesh is consulted only to
// classify a region, never to produce geometry.
// ---------------------------------------------------------------------------

// A point guaranteed to lie inside a planar face, taken from its own triangulation.
// An area centroid is not safe here: a split region can be L-shaped or annular, and
// its centroid can fall outside it or inside a hole.
[[nodiscard]] std::optional<gp_Pnt2d> interiorPointOf(const TopoDS_Face& face)
{
    BRepMesh_IncrementalMesh mesher(face, 0.5, false, 0.5, false);
    static_cast<void>(mesher.IsDone());
    TopLoc_Location location;
    const auto triangulation = BRep_Tool::Triangulation(face, location);
    if (triangulation.IsNull() || triangulation->NbTriangles() < 1) return std::nullopt;
    const gp_Trsf transform = location.Transformation();

    // Largest triangle, so the sample sits as far from an edge as the mesh allows.
    double bestArea = -1.0;
    gp_Pnt2d best;
    for (int index = 1; index <= triangulation->NbTriangles(); ++index) {
        int a = 0;
        int b = 0;
        int c = 0;
        triangulation->Triangle(index).Get(a, b, c);
        const gp_Pnt pa = triangulation->Node(a).Transformed(transform);
        const gp_Pnt pb = triangulation->Node(b).Transformed(transform);
        const gp_Pnt pc = triangulation->Node(c).Transformed(transform);
        const double area = std::abs((pb.X() - pa.X()) * (pc.Y() - pa.Y())
                                     - (pb.Y() - pa.Y()) * (pc.X() - pa.X())) * 0.5;
        if (area > bestArea) {
            bestArea = area;
            best = gp_Pnt2d((pa.X() + pb.X() + pc.X()) / 3.0, (pa.Y() + pb.Y() + pc.Y()) / 3.0);
        }
    }
    if (bestArea <= 0.0) return std::nullopt;
    return best;
}

// Edges separating inside from outside, i.e. the silhouette. Returns empty when the
// split fails, so the caller can report rather than silently emit the full edge set.
[[nodiscard]] occ::handle<ShapeSequence> silhouetteEdges(const occ::handle<ShapeSequence>& edges,
                                                        const FootprintOracle& oracle,
                                                        ProjectionStatistics& statistics)
{
    auto boundary = occ::handle<ShapeSequence>(new ShapeSequence());
    if (edges.IsNull() || edges->IsEmpty()) return boundary;

    Bnd_Box2d extent;
    for (int index = 1; index <= edges->Length(); ++index) {
        const TopoDS_Edge edge = TopoDS::Edge(edges->Value(index));
        double first = 0.0;
        double last = 0.0;
        const auto curve = pcurveOf(edge, first, last);
        if (curve.IsNull()) continue;
        BndLib_Add2dCurve::Add(Geom2dAdaptor_Curve(curve, first, last), 0.0, extent);
    }
    if (extent.IsVoid()) return boundary;
    double xMin = 0.0;
    double yMin = 0.0;
    double xMax = 0.0;
    double yMax = 0.0;
    extent.Get(xMin, yMin, xMax, yMax);
    // Pad so the canvas rim can never coincide with real geometry, which would make
    // an outer region ambiguous.
    const double pad = std::max(1.0, std::max(xMax - xMin, yMax - yMin) * 0.1);

    TopoDS_Face canvas;
    try {
        canvas = BRepBuilderAPI_MakeFace(gp_Pln(gp::XOY()), xMin - pad, xMax + pad,
                                        yMin - pad, yMax + pad).Face();
    } catch (const Standard_Failure&) { return boundary; }
    if (canvas.IsNull()) return boundary;

    // Hidden-line edges carry only a pcurve, no 3D curve. The splitter is a 3D
    // boolean and dereferences that curve, so handing them over as they arrive
    // crashes it outright. Build the 3D curves first; this is the one place the
    // pcurve-only nature of HLR output has to be repaired rather than worked around.
    BRep_Builder builder;
    TopoDS_Compound toolCompound;
    builder.MakeCompound(toolCompound);
    int usableTools = 0;
    for (int index = 1; index <= edges->Length(); ++index) {
        const TopoDS_Edge edge = TopoDS::Edge(edges->Value(index));
        double first = 0.0;
        double last = 0.0;
        const auto curve = pcurveOf(edge, first, last);
        // A degenerate or zero-extent edge has no meaningful curve for the boolean
        // to intersect against, and feeding one in is a way to crash it rather than
        // get an error back.
        if (curve.IsNull() || !(last - first > Precision::PConfusion())) continue;
        if (BRep_Tool::Degenerated(edge)) continue;
        const Geom2dAdaptor_Curve adaptor(curve, first, last);
        if (adaptor.Value(first).Distance(adaptor.Value(last)) < Precision::Confusion()
            && std::abs(last - first) < Precision::PConfusion()) {
            continue;
        }
        // Per edge rather than on the whole compound: one bad edge should cost one
        // edge, not the entire silhouette.
        try {
            if (!BRepLib::BuildCurves3d(edge)) continue;
        } catch (const Standard_Failure&) { continue; }
        builder.Add(toolCompound, edge);
        ++usableTools;
    }
    if (usableTools == 0) return boundary;

    NCollection_List<TopoDS_Shape> arguments;
    arguments.Append(canvas);
    NCollection_List<TopoDS_Shape> tools;
    for (TopExp_Explorer it(toolCompound, TopAbs_EDGE); it.More(); it.Next()) tools.Append(it.Current());

    TopoDS_Shape split;
    try {
        BRepAlgoAPI_Splitter splitter;
        splitter.SetArguments(arguments);
        splitter.SetTools(tools);
        splitter.Build();
        if (!splitter.IsDone()) return boundary;
        split = splitter.Shape();
    } catch (const Standard_Failure&) { return boundary; }
    if (split.IsNull()) return boundary;

    // Count, per edge, how many INSIDE regions it bounds. One means it separates
    // material from empty space, so it is on the silhouette; two means it is interior.
    std::map<const void*, std::pair<TopoDS_Edge, int>> tally;
    int insideRegions = 0;
    for (TopExp_Explorer faces(split, TopAbs_FACE); faces.More(); faces.Next()) {
        const TopoDS_Face region = TopoDS::Face(faces.Current());
        const auto sample = interiorPointOf(region);
        if (!sample || !oracle.contains(*sample)) continue;
        ++insideRegions;
        for (TopExp_Explorer regionEdges(region, TopAbs_EDGE); regionEdges.More(); regionEdges.Next()) {
            // An edge embedded inside a region rather than bounding it -- a fragment
            // that never closes, typically a pocket outline whose interior is still
            // material -- is adjacent to exactly one face too, so the count below
            // cannot tell it from a real boundary. OCCT marks such an edge INTERNAL,
            // which is the distinction. Without this, those fragments survive as
            // stray dashes inside the silhouette.
            const TopAbs_Orientation orientation = regionEdges.Current().Orientation();
            if (orientation == TopAbs_INTERNAL || orientation == TopAbs_EXTERNAL) continue;
            const TopoDS_Edge edge = TopoDS::Edge(regionEdges.Current());
            const void* key = edge.TShape().get();
            auto found = tally.find(key);
            if (found == tally.end()) tally.emplace(key, std::pair{edge, 1});
            else ++found->second.second;
        }
    }
    if (insideRegions == 0) return boundary;

    for (const auto& [key, entry] : tally) {
        if (entry.second == 1) boundary->Append(entry.first);
        else ++statistics.interiorEdgesRemoved;
    }
    return boundary;
}

void appendUniqueEdges(const TopoDS_Shape& compound, const double tolerance,
                       const occ::handle<ShapeSequence>& edges, std::set<EdgeKey>& seen,
                       ProjectionStatistics& statistics)
{
    if (compound.IsNull()) return;
    for (TopExp_Explorer explorer(compound, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
        double first = 0.0;
        double last = 0.0;
        const auto curve = pcurveOf(edge, first, last);
        if (curve.IsNull()) continue;
        const Geom2dAdaptor_Curve adaptor(curve, first, last);
        const auto key = edgeKeyFor(adaptor.Value(first), adaptor.Value(0.5 * (first + last)),
                                    adaptor.Value(last), tolerance);
        if (!seen.insert(key).second) {
            ++statistics.duplicatesRemoved;
            continue;
        }
        edges->Append(edge);
    }
}

[[nodiscard]] std::vector<Primitive> primitivesOfEdge(const TopoDS_Edge& edge, const bool reversed,
                                                      const ProjectionRequest& request,
                                                      ProjectionStatistics& statistics)
{
    double first = 0.0;
    double last = 0.0;
    const auto curve = pcurveOf(edge, first, last);
    std::vector<Primitive> primitives;
    if (curve.IsNull()) return primitives;
    ++statistics.edges;
    appendPrimitives(curve, first, last, request, statistics, primitives);
    if (reversed) {
        std::reverse(primitives.begin(), primitives.end());
        for (auto& primitive : primitives) primitive = reversedPrimitive(primitive);
    }
    return primitives;
}

[[nodiscard]] std::vector<Contour> stitchContours(const occ::handle<ShapeSequence>& edges,
                                                  const double tolerance, const ProjectionRequest& request,
                                                  ProjectionStatistics& statistics)
{
    std::vector<Contour> contours;
    if (edges.IsNull() || edges->IsEmpty()) return contours;

    // Hidden-line output is a bag of independent edges with unshared vertices;
    // a cutter needs closed loops, so they have to be connected first.
    auto wires = ShapeAnalysis_FreeBounds::ConnectEdgesToWires(edges, tolerance, false);
    if (wires.IsNull()) return contours;

    // Second pass: join the chains the first pass left open, closing gaps that
    // remain because hidden-line removal splits an edge at every visibility
    // change and the pieces do not share vertices.
    //
    // Only the OPEN wires are eligible. ConnectWiresToWires builds wires of
    // maximum length, so handing it everything merges distinct closed loops --
    // adjacent hole boundaries, typically -- into one longer open chain.
    // Measured: passing all wires took corpus closure from 79% down to 68%,
    // because one part lost five already-closed loops that way. Partitioning
    // first keeps the gain without the regression.
    auto openWires = occ::handle<ShapeSequence>(new ShapeSequence());
    std::vector<TopoDS_Shape> alreadyClosed;
    for (int index = 1; index <= wires->Length(); ++index) {
        const TopoDS_Shape& candidate = wires->Value(index);
        if (candidate.IsNull()) continue;
        if (BRep_Tool::IsClosed(candidate)) alreadyClosed.push_back(candidate);
        else openWires->Append(candidate);
    }
    if (openWires->Length() > 1) {
        // A wider tolerance is reasonable here: the candidates are whole chain
        // ends rather than arbitrary edges, so a generous join is less likely to
        // connect something unrelated.
        if (const auto joined = ShapeAnalysis_FreeBounds::ConnectWiresToWires(
                openWires, std::max(tolerance * 10.0, 0.05), false);
            !joined.IsNull()) {
            auto merged = occ::handle<ShapeSequence>(new ShapeSequence());
            for (const auto& closed : alreadyClosed) merged->Append(closed);
            for (int index = 1; index <= joined->Length(); ++index) merged->Append(joined->Value(index));
            wires = merged;
        }
    }

    for (int wireIndex = 1; wireIndex <= wires->Length(); ++wireIndex) {
        const TopoDS_Shape& candidate = wires->Value(wireIndex);
        if (candidate.IsNull() || candidate.ShapeType() != TopAbs_WIRE) continue;
        const TopoDS_Wire wire = TopoDS::Wire(candidate);

        Contour contour;
        for (BRepTools_WireExplorer explorer(wire); explorer.More(); explorer.Next()) {
            auto primitives = primitivesOfEdge(explorer.Current(),
                                               explorer.Orientation() == TopAbs_REVERSED, request, statistics);
            contour.primitives.insert(contour.primitives.end(), primitives.begin(), primitives.end());
        }
        // A wire the ordered explorer cannot walk still holds geometry worth
        // emitting; fall back to unordered traversal rather than dropping it.
        if (contour.primitives.empty()) {
            for (TopExp_Explorer explorer(wire, TopAbs_EDGE); explorer.More(); explorer.Next()) {
                auto primitives = primitivesOfEdge(TopoDS::Edge(explorer.Current()), false, request, statistics);
                contour.primitives.insert(contour.primitives.end(), primitives.begin(), primitives.end());
            }
        }
        if (contour.primitives.empty()) continue;

        const auto start = primitiveStart(contour.primitives.front());
        const auto end = primitiveEnd(contour.primitives.back());
        contour.closed = start.Distance(end) <= tolerance;
        if (contour.closed) ++statistics.closedContours;
        else ++statistics.openContours;
        contours.push_back(std::move(contour));
    }
    return contours;
}

void validate(const ProjectionRequest& request)
{
    if (request.shape.IsNull()) {
        throw ProjectionError(ProjectionError::Code::EmptyShape, "the shape to project is empty");
    }
    if (!std::isfinite(request.deflectionMm) || request.deflectionMm <= 0.0) {
        throw ProjectionError(ProjectionError::Code::InvalidTolerance,
                              "the deflection tolerance must be a positive number of millimetres");
    }
    if (!std::isfinite(request.sourceToMillimeters) || request.sourceToMillimeters <= 0.0) {
        throw ProjectionError(ProjectionError::Code::InvalidScale,
                              "the source-to-millimetre scale must be positive");
    }
    // A view direction parallel to the up direction leaves the in-plane axis
    // undefined, and OCCT throws from deep inside the axis constructor rather
    // than reporting anything a user could act on.
    if (std::abs(request.viewDirection.Dot(request.upDirection)) > 1.0 - 1.0e-9) {
        throw ProjectionError(ProjectionError::Code::DegenerateView,
                              "the view direction and the up direction must not be parallel");
    }
}

[[nodiscard]] TopoDS_Shape scaledToMillimetres(const TopoDS_Shape& shape, const double sourceToMillimetres)
{
    if (sourceToMillimetres == 1.0) return shape;
    // Scale the shape, never the projector: an orthographic projector forces its
    // own transform's scale factor to 1 and silently discards any scale baked
    // into it, which would leave the drawing at source scale.
    gp_Trsf scale;
    scale.SetScale(gp_Pnt(0.0, 0.0, 0.0), sourceToMillimetres);
    return BRepBuilderAPI_Transform(shape, scale, true).Shape();
}

struct HiddenLineOutput final {
    TopoDS_Shape sharp;
    TopoDS_Shape outline;
    TopoDS_Shape smooth;
};

// The transform hidden-line removal projects through. Reused verbatim for mesh
// nodes so the footprint oracle lands in exactly the same 2D frame as the edges;
// any divergence here would make the inside/outside test meaningless.
[[nodiscard]] gp_Trsf projectionTransform(const ProjectionRequest& request)
{
    gp_Trsf transform;
    transform.SetTransformation(gp_Ax3(gp_Pnt(0.0, 0.0, 0.0), request.viewDirection,
                                       request.upDirection.Crossed(request.viewDirection)));
    return transform;
}

[[nodiscard]] HiddenLineOutput runHiddenLineRemoval(const TopoDS_Shape& shape, const ProjectionRequest& request)
{
    const auto plane = BRepLib::Plane();
    // BRepLib's plane is a mutable global static that results are expressed on.
    // If anything replaced it, every coordinate below would be measured against
    // a different frame -- and silently, since the geometry would still look
    // plausible. This is also why projection jobs must not run concurrently.
    if (plane.IsNull() || !plane->Position().Direction().IsEqual(gp_Dir(0.0, 0.0, 1.0), 1.0e-12)) {
        throw ProjectionError(ProjectionError::Code::UnexpectedProjectionPlane,
                              "the global projection plane is not the expected XOY plane");
    }

    try {
        occ::handle<HLRBRep_Algo> algorithm = new HLRBRep_Algo();
        // A single pre-located shape, and no isoparametric lines: a drawing wants
        // edges, and adding located sub-shapes separately is quadratic.
        algorithm->Add(shape, 0);
        algorithm->Projector(HLRAlgo_Projector(
            gp_Ax2(gp_Pnt(0.0, 0.0, 0.0), request.viewDirection,
                   request.upDirection.Crossed(request.viewDirection))));
        algorithm->Update();
        algorithm->Hide();

        HLRBRep_HLRToShape toShape(algorithm);
        HiddenLineOutput output;
        output.sharp = toShape.VCompound();
        output.outline = toShape.OutLineVCompound();
        if (request.mode == ContentMode::TechnicalView) output.smooth = toShape.Rg1LineVCompound();
        return output;
    } catch (const Standard_Failure& error) {
        throw ProjectionError(ProjectionError::Code::ProjectionFailed,
                              std::string("hidden line removal failed: ") + error.GetMessageString());
    }
}

void addWarning(Drawing& drawing, const std::string_view code, const int count)
{
    if (count > 0) drawing.warnings.push_back({std::string(code), count});
}

} // namespace

ProjectionError::ProjectionError(const Code code, std::string message)
    : std::runtime_error(std::move(message))
    , code_(code)
{
}

ProjectionResult project(const ProjectionRequest& request)
{
    validate(request);

    const auto shape = scaledToMillimetres(request.shape, request.sourceToMillimeters);
    const auto hiddenLine = runHiddenLineRemoval(shape, request);
    const double tolerance = request.stitchToleranceMm > 0.0
        ? request.stitchToleranceMm : std::max(1.0e-3, request.deflectionMm);

    ProjectionResult result;
    auto& statistics = result.statistics;

    // The footprint oracle is only built for the silhouette mode, since meshing
    // costs real time and nothing else needs an inside/outside test.
    std::optional<FootprintOracle> oracle;
    if (request.mode == ContentMode::OuterContourOnly) {
        oracle.emplace(shape, projectionTransform(request), request.deflectionMm);
        if (!oracle->usable()) {
            // No triangulation means no way to tell interior from boundary. Say so
            // instead of quietly emitting the full edge set, which looks similar
            // and is a different cut path.
            result.drawing.warnings.push_back({std::string(warningCode::silhouetteUnavailable), 1});
            oracle.reset();
        }
    }

    // Sharp edges and silhouettes together form the cut profile, so they are
    // deduplicated against each other and stitched as one set: a hole boundary
    // that is also a silhouette must become one contour, not two.
    {
        auto edges = occ::handle<ShapeSequence>(new ShapeSequence());
        std::set<EdgeKey> seen;
        appendUniqueEdges(hiddenLine.sharp, tolerance, edges, seen, statistics);
        appendUniqueEdges(hiddenLine.outline, tolerance, edges, seen, statistics);
        if (oracle) {
            // Reduce to the edges that separate material from empty space. If the
            // split fails we keep nothing rather than quietly emitting the full edge
            // set, because the two look similar and one is the wrong cut path.
            const auto boundary = silhouetteEdges(edges, *oracle, statistics);
            if (boundary.IsNull() || boundary->IsEmpty()) {
                result.drawing.warnings.push_back(
                    {std::string(warningCode::silhouetteUnavailable), 1});
                edges = occ::handle<ShapeSequence>(new ShapeSequence());
            } else {
                edges = boundary;
            }
        }
        auto contours = stitchContours(edges, tolerance, request, statistics);
        if (!contours.empty()) {
            result.drawing.layers.push_back(
                Layer{defaultLayerName(LayerRole::Cut), LayerRole::Cut, std::move(contours)});
        }
    }

    // Smooth (tangent-continuous) edges are reference detail, never cut
    // geometry, so they get their own layer and are only produced at all for the
    // technical view.
    if (!hiddenLine.smooth.IsNull()) {
        auto edges = occ::handle<ShapeSequence>(new ShapeSequence());
        std::set<EdgeKey> seen;
        appendUniqueEdges(hiddenLine.smooth, tolerance, edges, seen, statistics);
        auto contours = stitchContours(edges, tolerance, request, statistics);
        if (!contours.empty()) {
            result.drawing.layers.push_back(
                Layer{defaultLayerName(LayerRole::Smooth), LayerRole::Smooth, std::move(contours)});
        }
    }

    addWarning(result.drawing, warningCode::openContour, statistics.openContours);
    addWarning(result.drawing, warningCode::coarseCurveFallback, statistics.coarseFallbackEdges);
    addWarning(result.drawing, warningCode::duplicateEdgeRemoved, statistics.duplicatesRemoved);
    return result;
}

} // namespace loupe::drawing
