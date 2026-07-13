#include "app/tools/MeasurementController.h"

#include <cmath>

namespace {

QString formatValue(const double value)
{
    return QString::number(value, 'g', 8);
}

} // namespace

namespace loupe::app::tools {

MeasurementController::MeasurementController(QObject* parent)
    : QObject(parent)
{
}

void MeasurementController::setEffectiveUnit(const QString& unit)
{
    const auto normalized = unit == QStringLiteral("in") ? unit : QStringLiteral("mm");
    if (effectiveUnit_ == normalized) return;
    effectiveUnit_ = normalized;
    emit resultChanged();
}

void MeasurementController::setMode(const MeasurementMode mode)
{
    if (mode_ == mode) {
        return;
    }
    mode_ = mode;
    emit modeChanged();
    emit resultChanged();
}

void MeasurementController::setSelectedGeometry(const double surfaceAreaMm2, const double volumeMm3, const QVector3D& boundsMm)
{
    hasSelectedGeometry_ = std::isfinite(surfaceAreaMm2) && surfaceAreaMm2 >= 0.0
        && std::isfinite(volumeMm3) && volumeMm3 >= 0.0
        && std::isfinite(boundsMm.x()) && std::isfinite(boundsMm.y()) && std::isfinite(boundsMm.z());
    surfaceAreaMm2_ = hasSelectedGeometry_ ? surfaceAreaMm2 : 0.0;
    volumeMm3_ = hasSelectedGeometry_ ? volumeMm3 : 0.0;
    boundsMm_ = hasSelectedGeometry_ ? boundsMm : QVector3D{};
    emit resultChanged();
}

void MeasurementController::clearSelectedGeometry()
{
    if (!hasSelectedGeometry_) return;
    hasSelectedGeometry_ = false;
    surfaceAreaMm2_ = 0.0;
    volumeMm3_ = 0.0;
    boundsMm_ = {};
    emit resultChanged();
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

double MeasurementController::resultValue() const
{
    if (!hasSelectedGeometry_) return 0.0;
    const auto divisor = effectiveUnit_ == QStringLiteral("in") ? 25.4 : 1.0;
    switch (mode_) {
    case MeasurementMode::SurfaceArea: return surfaceAreaMm2_ / (divisor * divisor);
    case MeasurementMode::Volume: return volumeMm3_ / (divisor * divisor * divisor);
    default: return 0.0;
    }
}

QString MeasurementController::resultUnit() const
{
    switch (mode_) {
    case MeasurementMode::SurfaceArea: return effectiveUnit_ == QStringLiteral("in") ? QStringLiteral("in²") : QStringLiteral("mm²");
    case MeasurementMode::Volume: return effectiveUnit_ == QStringLiteral("in") ? QStringLiteral("in³") : QStringLiteral("mm³");
    case MeasurementMode::Bounds: return effectiveUnit_;
    default: return {};
    }
}

QString MeasurementController::resultLabel() const
{
    if (!hasSelectedGeometry_) return {};
    if (mode_ == MeasurementMode::Bounds) {
        const auto divisor = effectiveUnit_ == QStringLiteral("in") ? 25.4F : 1.0F;
        return QStringLiteral("%1 × %2 × %3 %4")
            .arg(formatValue(boundsMm_.x() / divisor), formatValue(boundsMm_.y() / divisor), formatValue(boundsMm_.z() / divisor), resultUnit());
    }
    const auto unit = resultUnit();
    return unit.isEmpty() ? QString{} : QStringLiteral("%1 %2").arg(formatValue(resultValue()), unit);
}

} // namespace loupe::app::tools
