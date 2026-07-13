#pragma once

#include <TopoDS_Shape.hxx>

namespace loupe::inspection {

struct TopologyAnalysis final {
    bool valid{false};
    double longestEdgeMm{};
    double circularRadiusMm{};
    int planarFaceCount{};
};

[[nodiscard]] TopologyAnalysis analyzeTopology(const TopoDS_Shape& shape, double sourceToMillimeters = 1.0);

} // namespace loupe::inspection
