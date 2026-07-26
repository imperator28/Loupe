#pragma once

#include <gp_Pnt2d.hxx>

#include <string>
#include <string_view>
#include <variant>
#include <vector>

// Format-neutral intermediate representation for a 2D drawing.
//
// This sits between projection (which knows OpenCASCADE) and the DXF/SVG/PDF
// writers (which know only this header). Two consequences that are the whole
// point of the layer:
//
//   * A writer can be tested against hand-built geometry, with no STEP file and
//     no projection run, so writer tests are fast and deterministic.
//   * The two highest-severity risks in the requirements -- mirrored output and
//     wrong scale -- reduce to a handful of pure functions here that are tested
//     once, instead of being re-derived as arithmetic inside three writers.
//
// Every coordinate is in millimetres. There is no other unit anywhere below
// this line; the source-unit conversion happens before projection.
namespace loupe::drawing {

// Angles are radians, measured counter-clockwise from +X, in the drawing's own
// (Y-up) frame. `sweep` is signed: positive counter-clockwise, negative
// clockwise. The sign is load-bearing -- it is what a vertical mirror has to
// invert, and what decides DXF arc ordering and the SVG sweep flag.
struct Arc final {
    gp_Pnt2d centre;
    double radius{};
    double startAngle{};
    double sweepAngle{};

    [[nodiscard]] gp_Pnt2d pointAtAngle(double angle) const;
    [[nodiscard]] gp_Pnt2d startPoint() const;
    [[nodiscard]] gp_Pnt2d endPoint() const;
    [[nodiscard]] bool isFullCircle() const noexcept;
};

struct Segment final {
    gp_Pnt2d start;
    gp_Pnt2d end;
};

// Cubic Bezier. Used for spline geometry that cannot be represented as a line
// or an arc; SVG and PDF emit these directly, DXF tessellates them.
struct Cubic final {
    gp_Pnt2d start;
    gp_Pnt2d firstControl;
    gp_Pnt2d secondControl;
    gp_Pnt2d end;
};

using Primitive = std::variant<Segment, Arc, Cubic>;

[[nodiscard]] gp_Pnt2d primitiveStart(const Primitive& primitive);
[[nodiscard]] gp_Pnt2d primitiveEnd(const Primitive& primitive);

// Same geometry traversed the other way. A stitched wire can contain an edge
// whose stored direction opposes the wire's, and a contour has to read as one
// continuous chain, so such an edge is reversed on the way into the IR.
[[nodiscard]] Primitive reversedPrimitive(const Primitive& primitive);

struct Bounds2d final {
    double minX{};
    double minY{};
    double maxX{};
    double maxY{};
    bool valid{false};

    void add(const gp_Pnt2d& point);
    void add(const Bounds2d& other);
    [[nodiscard]] double width() const noexcept { return valid ? maxX - minX : 0.0; }
    [[nodiscard]] double height() const noexcept { return valid ? maxY - minY : 0.0; }
};

// Tight bounds, not a control-point hull. The requirements make the preview's
// measured extents the user's primary defence against a wrong-scale export, so
// an inflated number would defeat it: a Bezier's control points can sit well
// outside the curve they describe.
[[nodiscard]] Bounds2d primitiveBounds(const Primitive& primitive);

// An ordered chain of primitives. `closed` is set by the stitching stage, not
// inferred here: a cutter needs closed contours, and whether closure succeeded
// within tolerance is a fact about the projection, not about this geometry.
struct Contour final {
    std::vector<Primitive> primitives;
    bool closed{false};
};

[[nodiscard]] Bounds2d contourBounds(const Contour& contour);

// What a layer is for, independent of what any format calls it. Writers map
// roles to their own layer or group names so a machine operator can assign
// power and speed per role.
enum class LayerRole { Cut, Outline, Smooth, Hidden, Reference };

[[nodiscard]] std::string defaultLayerName(LayerRole role);

struct Layer final {
    std::string name;
    LayerRole role{LayerRole::Cut};
    std::vector<Contour> contours;
};

[[nodiscard]] Bounds2d layerBounds(const Layer& layer);

// Machine-readable warning codes. A drawing that degraded must say so; silent
// degradation on a path that drives a cutter is the failure mode the
// requirements forbid outright.
namespace warningCode {
inline constexpr std::string_view openContour = "open_contour";
inline constexpr std::string_view coarseCurveFallback = "coarse_curve_fallback";
inline constexpr std::string_view duplicateEdgeRemoved = "duplicate_edge_removed";
// The outer-contour filter had no usable inside/outside reference, so the
// silhouette could not be isolated. Reported rather than silently falling back to
// the full edge set, which looks similar but is a different cut path.
inline constexpr std::string_view silhouetteUnavailable = "silhouette_unavailable";
// Silhouette mode ignored one or more non-solid bodies, which have no interior for
// the region test and would otherwise appear as stray contours.
inline constexpr std::string_view nonSolidBodiesIgnored = "non_solid_bodies_ignored";
// The exact algorithm returned nothing for this view, so the projection was retaken from a
// fractionally tilted direction. The drawing is no longer strictly 1:1 -- by roughly half a
// micron in 400 mm -- and must say so rather than imply an exactness it no longer has.
inline constexpr std::string_view approximateProjection = "approximate_projection";
} // namespace warningCode

struct DrawingWarning final {
    std::string code;
    int count{};
};

struct Drawing final {
    std::vector<Layer> layers;
    std::vector<DrawingWarning> warnings;

    // Computed rather than stored, so it can never go stale after a transform.
    [[nodiscard]] Bounds2d bounds() const;

    // Shift so the geometry's minimum corner sits at (margin, margin). This is
    // the only translation the pipeline applies after projection, and it is a
    // pure translation precisely so it cannot perturb scale.
    [[nodiscard]] Drawing translatedToOrigin(double marginMm) const;

    // Flip about y = pageHeight/2 for the Y-down formats (SVG, PDF). DXF model
    // space is Y-up and must NOT be mirrored.
    //
    // This also inverts every arc's start angle and sweep sign. Mirroring
    // positions while leaving sweeps alone would turn a fillet into a gouge --
    // and on a chiral part a mirrored file is scrapped stock, which is why this
    // is one tested function rather than per-writer arithmetic.
    [[nodiscard]] Drawing mirroredVertically(double pageHeightMm) const;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t contourCount() const noexcept;
    [[nodiscard]] int warningCount(std::string_view code) const noexcept;
};

} // namespace loupe::drawing
