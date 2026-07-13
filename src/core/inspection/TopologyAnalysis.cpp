#include "core/inspection/TopologyAnalysis.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>

#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>

#include <cmath>
#include <algorithm>

namespace loupe::inspection {

TopologyAnalysis analyzeTopology(const TopoDS_Shape& shape, const double sourceToMillimeters)
{
    if (shape.IsNull() || !std::isfinite(sourceToMillimeters) || sourceToMillimeters <= 0.0) return {};
    TopologyAnalysis analysis;
    for (TopExp_Explorer explorer(shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        const auto edge = TopoDS::Edge(explorer.Current());
        GProp_GProps properties;
        BRepGProp::LinearProperties(edge, properties);
        analysis.longestEdgeMm = std::max(analysis.longestEdgeMm, properties.Mass() * sourceToMillimeters);
        BRepAdaptor_Curve curve(edge);
        if (curve.GetType() == GeomAbs_Circle) analysis.circularRadiusMm = std::max(analysis.circularRadiusMm, curve.Circle().Radius() * sourceToMillimeters);
    }
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        if (BRepAdaptor_Surface(TopoDS::Face(explorer.Current())).GetType() == GeomAbs_Plane) ++analysis.planarFaceCount;
    }
    analysis.valid = std::isfinite(analysis.longestEdgeMm) && std::isfinite(analysis.circularRadiusMm);
    return analysis;
}

} // namespace loupe::inspection
