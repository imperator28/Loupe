#include "core/inspection/GeometryAnalysis.h"

#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>

#include <cmath>

namespace loupe::inspection {

std::optional<BoundsMm> measureBounds(const TopoDS_Shape& shape, const double sourceToMillimeters)
{
    if (shape.IsNull() || !std::isfinite(sourceToMillimeters) || sourceToMillimeters <= 0.0) return std::nullopt;
    Bnd_Box bounds;
    BRepBndLib::Add(shape, bounds);
    if (bounds.IsVoid()) return std::nullopt;

    double minX{};
    double minY{};
    double minZ{};
    double maxX{};
    double maxY{};
    double maxZ{};
    bounds.Get(minX, minY, minZ, maxX, maxY, maxZ);
    const BoundsMm extents{(maxX - minX) * sourceToMillimeters, (maxY - minY) * sourceToMillimeters, (maxZ - minZ) * sourceToMillimeters};
    if (!std::isfinite(extents.width) || !std::isfinite(extents.height) || !std::isfinite(extents.depth)) return std::nullopt;
    return extents;
}

GeometryAnalysis analyze(const TopoDS_Shape& shape, const double sourceToMillimeters)
{
    if (shape.IsNull() || !std::isfinite(sourceToMillimeters) || sourceToMillimeters <= 0.0) return {};

    GProp_GProps surfaceProperties;
    GProp_GProps volumeProperties;
    BRepGProp::SurfaceProperties(shape, surfaceProperties);
    BRepGProp::VolumeProperties(shape, volumeProperties);

    const auto extents = measureBounds(shape, sourceToMillimeters);
    if (!extents) return {};

    const auto surfaceArea = surfaceProperties.Mass() * sourceToMillimeters * sourceToMillimeters;
    const auto volume = volumeProperties.Mass() * sourceToMillimeters * sourceToMillimeters * sourceToMillimeters;
    const bool valid = std::isfinite(surfaceArea) && std::isfinite(volume);
    return {valid, surfaceArea, volume, *extents};
}

} // namespace loupe::inspection
