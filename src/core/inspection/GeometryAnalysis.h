#pragma once

#include <TopoDS_Shape.hxx>

namespace loupe::inspection {

struct BoundsMm final {
    double width{};
    double height{};
    double depth{};
};

struct GeometryAnalysis final {
    bool valid{false};
    double surfaceAreaMm2{};
    double volumeMm3{};
    BoundsMm boundsMm;
};

[[nodiscard]] GeometryAnalysis analyze(const TopoDS_Shape& shape);

} // namespace loupe::inspection
