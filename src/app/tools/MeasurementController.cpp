#include "app/tools/MeasurementController.h"

#include <cmath>
#include <algorithm>

namespace {

QString formatValue(const double value)
{
    const auto rounded = std::round(value);
    const auto normalized = std::abs(value - rounded) <= 1.0e-6 ? rounded : value;
    return QString::number(normalized, 'g', 8);
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
    longestEdgeMm_ = 0.0;
    circularRadiusMm_ = 0.0;
    planarFaceCount_ = 0;
    emit resultChanged();
}

void MeasurementController::setSelectedTopology(const double longestEdgeMm, const double circularRadiusMm, const int planarFaceCount)
{
    longestEdgeMm_ = std::isfinite(longestEdgeMm) && longestEdgeMm >= 0.0 ? longestEdgeMm : 0.0;
    circularRadiusMm_ = std::isfinite(circularRadiusMm) && circularRadiusMm >= 0.0 ? circularRadiusMm : 0.0;
    planarFaceCount_ = std::max(0, planarFaceCount);
    emit resultChanged();
}

void MeasurementController::recordPoint(const QVector3D& pointMm)
{
    recordPick(pointMm, {});
}

void MeasurementController::recordPick(const QVector3D& pointMm, const QVector3D& normal)
{
    if (mode_ != MeasurementMode::PointToPoint && mode_ != MeasurementMode::SurfaceToSurface && mode_ != MeasurementMode::Angle) return;
    if (pickedPointsMm_.size() == 2) pickedPointsMm_.clear();
    if (pickedNormals_.size() == 2) pickedNormals_.clear();
    pickedPointsMm_.append(pointMm);
    pickedNormals_.append(normal);
    emit resultChanged();
}

void MeasurementController::clearPicks()
{
    if (pickedPointsMm_.isEmpty()) return;
    pickedPointsMm_.clear();
    pickedNormals_.clear();
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
    if ((mode_ == MeasurementMode::PointToPoint || mode_ == MeasurementMode::SurfaceToSurface) && pickedPointsMm_.size() == 2) return pointDistance(pickedPointsMm_.at(0), pickedPointsMm_.at(1)).value;
    if (mode_ == MeasurementMode::Angle && pickedNormals_.size() == 2) {
        const auto first = pickedNormals_.at(0).normalized();
        const auto second = pickedNormals_.at(1).normalized();
        if (first.isNull() || second.isNull()) return 0.0;
        constexpr double radiansToDegrees = 180.0 / 3.14159265358979323846;
        return std::acos(std::clamp(static_cast<double>(QVector3D::dotProduct(first, second)), -1.0, 1.0)) * radiansToDegrees;
    }
    if (!hasSelectedGeometry_) return 0.0;
    const auto divisor = effectiveUnit_ == QStringLiteral("in") ? 25.4 : 1.0;
    switch (mode_) {
    case MeasurementMode::SurfaceArea: return surfaceAreaMm2_ / (divisor * divisor);
    case MeasurementMode::Volume: return volumeMm3_ / (divisor * divisor * divisor);
    case MeasurementMode::EdgeLength: return longestEdgeMm_ / divisor;
    case MeasurementMode::RadiusDiameter: return circularRadiusMm_ / divisor;
    default: return 0.0;
    }
}

QString MeasurementController::resultUnit() const
{
    if (mode_ == MeasurementMode::PointToPoint || mode_ == MeasurementMode::SurfaceToSurface) return pickedPointsMm_.size() == 2 ? effectiveUnit_ : QString{};
    if (mode_ == MeasurementMode::Angle) return pickedNormals_.size() == 2 ? QStringLiteral("°") : QString{};
    switch (mode_) {
    case MeasurementMode::SurfaceArea: return effectiveUnit_ == QStringLiteral("in") ? QStringLiteral("in²") : QStringLiteral("mm²");
    case MeasurementMode::Volume: return effectiveUnit_ == QStringLiteral("in") ? QStringLiteral("in³") : QStringLiteral("mm³");
    case MeasurementMode::Bounds: return effectiveUnit_;
    case MeasurementMode::EdgeLength: return effectiveUnit_;
    case MeasurementMode::RadiusDiameter: return effectiveUnit_;
    default: return {};
    }
}

QString MeasurementController::resultLabel() const
{
    if (mode_ == MeasurementMode::PointToPoint || mode_ == MeasurementMode::SurfaceToSurface || mode_ == MeasurementMode::Angle) {
        return pickedPointsMm_.size() == 2 ? QStringLiteral("%1 %2").arg(formatValue(resultValue()), resultUnit()) : QString{};
    }
    if (!hasSelectedGeometry_) return {};
    if (mode_ == MeasurementMode::Bounds) {
        const auto divisor = effectiveUnit_ == QStringLiteral("in") ? 25.4F : 1.0F;
        return QStringLiteral("%1 × %2 × %3 %4")
            .arg(formatValue(boundsMm_.x() / divisor), formatValue(boundsMm_.y() / divisor), formatValue(boundsMm_.z() / divisor), resultUnit());
    }
    if (mode_ == MeasurementMode::EdgeLength) return QStringLiteral("Longest edge: %1 %2").arg(formatValue(resultValue()), resultUnit());
    if (mode_ == MeasurementMode::RadiusDiameter) return QStringLiteral("Radius: %1 %2 · Diameter: %3 %2").arg(formatValue(resultValue()), resultUnit(), formatValue(resultValue() * 2.0));
    const auto unit = resultUnit();
    return unit.isEmpty() ? QString{} : QStringLiteral("%1 %2").arg(formatValue(resultValue()), unit);
}

} // namespace loupe::app::tools
