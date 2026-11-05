#pragma once

#include <TopoDS_Shape.hxx>

#include <optional>

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

[[nodiscard]] GeometryAnalysis analyze(const TopoDS_Shape& shape, double sourceToMillimeters = 1.0);

// The shape's axis-aligned extents. Once the shape is meshed, OCCT bounds it by its triangulation,
// so the extents follow what the viewport draws; analyze() measures them the same way.
[[nodiscard]] std::optional<BoundsMm> measureBounds(const TopoDS_Shape& shape, double sourceToMillimeters = 1.0);

} // namespace loupe::inspection
