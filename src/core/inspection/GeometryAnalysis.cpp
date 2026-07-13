#include "core/inspection/GeometryAnalysis.h"

#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>

#include <cmath>

namespace loupe::inspection {

GeometryAnalysis analyze(const TopoDS_Shape& shape, const double sourceToMillimeters)
{
    if (shape.IsNull() || !std::isfinite(sourceToMillimeters) || sourceToMillimeters <= 0.0) return {};

    GProp_GProps surfaceProperties;
    GProp_GProps volumeProperties;
    BRepGProp::SurfaceProperties(shape, surfaceProperties);
    BRepGProp::VolumeProperties(shape, volumeProperties);

    Bnd_Box bounds;
    BRepBndLib::Add(shape, bounds);
    if (bounds.IsVoid()) return {};

    double minX{};
    double minY{};
    double minZ{};
    double maxX{};
    double maxY{};
    double maxZ{};
    bounds.Get(minX, minY, minZ, maxX, maxY, maxZ);

    const auto surfaceArea = surfaceProperties.Mass() * sourceToMillimeters * sourceToMillimeters;
    const auto volume = volumeProperties.Mass() * sourceToMillimeters * sourceToMillimeters * sourceToMillimeters;
    const BoundsMm extents{(maxX - minX) * sourceToMillimeters, (maxY - minY) * sourceToMillimeters, (maxZ - minZ) * sourceToMillimeters};
    const bool valid = std::isfinite(surfaceArea) && std::isfinite(volume)
        && std::isfinite(extents.width) && std::isfinite(extents.height) && std::isfinite(extents.depth);
    return {valid, surfaceArea, volume, extents};
}

} // namespace loupe::inspection
