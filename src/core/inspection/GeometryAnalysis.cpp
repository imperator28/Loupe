#include "core/inspection/GeometryAnalysis.h"

#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>

#include <cmath>

namespace loupe::inspection {

GeometryAnalysis analyze(const TopoDS_Shape& shape)
{
    if (shape.IsNull()) return {};

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

    const auto surfaceArea = surfaceProperties.Mass();
    const auto volume = volumeProperties.Mass();
    const BoundsMm extents{maxX - minX, maxY - minY, maxZ - minZ};
    const bool valid = std::isfinite(surfaceArea) && std::isfinite(volume)
        && std::isfinite(extents.width) && std::isfinite(extents.height) && std::isfinite(extents.depth);
    return {valid, surfaceArea, volume, extents};
}

} // namespace loupe::inspection
