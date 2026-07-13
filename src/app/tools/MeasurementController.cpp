#include "app/tools/MeasurementController.h"

namespace loupe::app::tools {

MeasurementController::MeasurementController(QObject* parent)
    : QObject(parent)
{
}

void MeasurementController::setEffectiveUnit(const QString& unit)
{
    effectiveUnit_ = unit == QStringLiteral("in") ? unit : QStringLiteral("mm");
}

void MeasurementController::setMode(const MeasurementMode mode)
{
    if (mode_ == mode) {
        return;
    }
    mode_ = mode;
    emit modeChanged();
}

void MeasurementController::setModeName(const QString& modeName)
{
    const auto mode = modeName.toLower();
    if (mode == QStringLiteral("edge")) setMode(MeasurementMode::EdgeLength);
    else if (mode == QStringLiteral("surface-distance")) setMode(MeasurementMode::SurfaceToSurface);
    else if (mode == QStringLiteral("radius")) setMode(MeasurementMode::RadiusDiameter);
    else if (mode == QStringLiteral("angle")) setMode(MeasurementMode::Angle);
    else if (mode == QStringLiteral("area")) setMode(MeasurementMode::SurfaceArea);
    else if (mode == QStringLiteral("volume")) setMode(MeasurementMode::Volume);
    else if (mode == QStringLiteral("bounds")) setMode(MeasurementMode::Bounds);
    else setMode(MeasurementMode::PointToPoint);
}

QString MeasurementController::pickInstruction() const
{
    switch (mode_) {
    case MeasurementMode::PointToPoint: return QStringLiteral("Select two points");
    case MeasurementMode::EdgeLength: return QStringLiteral("Select an edge or curve");
    case MeasurementMode::SurfaceToSurface: return QStringLiteral("Select two planar surfaces");
    case MeasurementMode::RadiusDiameter: return QStringLiteral("Select a circular edge or face");
    case MeasurementMode::Angle: return QStringLiteral("Select two edges or faces");
    case MeasurementMode::SurfaceArea: return QStringLiteral("Select a surface");
    case MeasurementMode::Volume: return QStringLiteral("Select a closed solid");
    case MeasurementMode::Bounds: return QStringLiteral("Select a component");
    }
    return {};
}

MeasurementResult MeasurementController::pointDistance(const QVector3D& first, const QVector3D& second) const
{
    const auto millimeters = static_cast<double>((second - first).length());
    const auto divisor = effectiveUnit_ == QStringLiteral("in") ? 25.4 : 1.0;
    return {millimeters / divisor, effectiveUnit_};
}

} // namespace loupe::app::tools
