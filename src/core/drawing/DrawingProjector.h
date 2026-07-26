#pragma once

#include "core/drawing/DrawingContour.h"

#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>

#include <stdexcept>
#include <string>

// Orthographic projection of a 3D shape into a 2D drawing, via OpenCASCADE
// hidden-line removal.
//
// The scale is exact by construction, not by approximation: the projector is a
// rigid transform with a forced unit scale factor, so a 50 mm edge parallel to
// the view plane comes out as a 50 mm line. Gate A measured this at 0.0 error
// on all three axes of a known box.
namespace loupe::drawing {

enum class ContentMode {
    // The part's silhouette: only edges that actually bound material, plus
    // through-hole boundaries. Step, chamfer and fillet lines are dropped even
    // though hidden-line removal considers them visible, because they do not
    // describe where material ends -- a step's top face otherwise reads as a line
    // straight across the middle of the part.
    //
    // This answers "what shape do I cut from stock", which is a different question
    // from "what does this part look like".
    OuterContourOnly,
    // Outer profile and through-holes only. No tangent/smooth edges, no hidden
    // lines. This is the cut-ready mode: a cutter needs closed contours, not a
    // faithful picture.
    CutContours,
    // Every visible edge, including smooth ones, on separate layers. Reads like
    // a CAD drawing view; for templates and visual reference.
    TechnicalView,
};

struct ProjectionRequest final {
    TopoDS_Shape shape;
    // Direction the viewer looks FROM, i.e. the outward normal of the drawing
    // plane. Must not be parallel to upDirection.
    gp_Dir viewDirection{0.0, 0.0, 1.0};
    gp_Dir upDirection{0.0, 1.0, 0.0};
    ContentMode mode{ContentMode::CutContours};
    // Applied to the shape before projection, never to the projector: the
    // projector discards a scale baked into its own transform.
    double sourceToMillimeters{1.0};
    // Chordal deviation budget for curves that must be tessellated, and the
    // tolerance used when recovering a circular arc from spline geometry.
    // 0.01 mm is an order of magnitude below typical laser kerf.
    double deflectionMm{0.01};
    // Gap below which two edge ends are treated as the same point when
    // stitching. Derived from the shape's own tolerance when left at zero.
    double stitchToleranceMm{0.0};
    // Silhouette mode only: restrict projection to the shape's solids.
    //
    // A silhouette is a statement about where material is, and only a solid has an
    // inside. A loose shell, face or wire in a compound has no interior for the
    // region test to find, so its edges cannot be classified as material boundaries
    // and survive as stray contours. On by default for that reason; turn it off to
    // include every body regardless of type.
    bool silhouetteSolidsOnly{true};
};

// What the projection actually produced. Recorded because the requirements
// forbid silent degradation: a caller can report exactly how faithful a drawing
// is, and the arc-recovery rate is a tracked quality metric.
struct ProjectionStatistics final {
    int edges{};
    // Edges discarded by the outer-contour filter as interior to the silhouette.
    int interiorEdgesRemoved{};
    int analyticLines{};
    int analyticCircles{};
    int recoveredArcs{};
    int exactCubics{};
    int tessellatedCurves{};
    int coarseFallbackEdges{};
    int duplicatesRemoved{};
    int closedContours{};
    int openContours{};
};

struct ProjectionResult final {
    Drawing drawing;
    ProjectionStatistics statistics;
    // True when the exact algorithm produced nothing for the requested view and the
    // projection was retaken from a fractionally tilted direction. Carried explicitly rather
    // than left for the caller to infer from the warning list, because every consumer -- the
    // preview, the row message, the file -- has to disclose it.
    bool approximate{};
    // The direction actually projected from. Equal to the request's unless approximate.
    gp_Dir viewDirectionUsed{0.0, 0.0, 1.0};
};

class ProjectionError : public std::runtime_error {
public:
    enum class Code {
        EmptyShape,
        DegenerateView,
        InvalidTolerance,
        InvalidScale,
        ProjectionFailed,
        UnexpectedProjectionPlane,
    };

    ProjectionError(Code code, std::string message);
    [[nodiscard]] Code code() const noexcept { return code_; }

private:
    Code code_;
};

[[nodiscard]] ProjectionResult project(const ProjectionRequest& request);

} // namespace loupe::drawing
