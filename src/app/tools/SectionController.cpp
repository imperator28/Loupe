#include "app/tools/SectionController.h"

#include <cmath>

#include <QColor>

#include <algorithm>

namespace loupe::app::tools {

SectionController::SectionController(QObject* parent)
    : QObject(parent)
{
}

void SectionController::setEnabled(const bool enabled)
{
    // Turned on or off by hand while picking a face: that is what stays.
    resumeAfterPick_ = false;
    if (enabled_ == enabled) return;
    enabled_ = enabled;
    if (!enabled_) cancelPlanePick();
    emit changed();
}

void SectionController::setAxis(const SectionAxis axis)
{
    if (axis_ == axis && !usingSelectedPlane_) return;
    axis_ = axis;
    usingSelectedPlane_ = false;
    usingAxis_ = false;
    position_ = 0.0;
    cancelPlanePick();
    emit changed();
}

void SectionController::setAxisName(const QString& axisName)
{
    const auto normalized = axisName.toUpper();
    if (normalized == QStringLiteral("X")) setAxis(SectionAxis::X);
    else if (normalized == QStringLiteral("Y")) setAxis(SectionAxis::Y);
    else setAxis(SectionAxis::Z);
}

QString SectionController::axisName() const
{
    switch (axis_) {
    case SectionAxis::X: return QStringLiteral("X");
    case SectionAxis::Y: return QStringLiteral("Y");
    case SectionAxis::Z: return QStringLiteral("Z");
    }
    return {};
}

void SectionController::setPosition(const double position)
{
    if (qFuzzyCompare(position_ + 1.0, position + 1.0)) return;
    position_ = position;
    emit changed();
}

void SectionController::setFlipped(const bool flipped)
{
    if (flipped_ == flipped) return;
    flipped_ = flipped;
    emit changed();
}

void SectionController::setCapEnabled(const bool enabled)
{
    if (capEnabled_ == enabled) return;
    capEnabled_ = enabled;
    emit changed();
}

void SectionController::setSliceOnly(const bool sliceOnly)
{
    if (sliceOnly_ == sliceOnly) return;
    sliceOnly_ = sliceOnly;
    emit changed();
}

void SectionController::setSliceDisplay(const QString& sliceDisplay)
{
    const auto normalized = sliceDisplay == QStringLiteral("outline") ? sliceDisplay
                          : sliceDisplay == QStringLiteral("filled") ? sliceDisplay
                                                                        : QStringLiteral("filled-outline");
    if (sliceDisplay_ == normalized) return;
    sliceDisplay_ = normalized;
    emit changed();
}

void SectionController::setSliceBorderColor(const QString& color)
{
    const auto normalized = color.trimmed();
    if (!normalized.isEmpty() && !QColor::isValidColorName(normalized)) return;
    if (sliceBorderColor_ == normalized) return;
    sliceBorderColor_ = normalized;
    emit changed();
}

void SectionController::setSliceBorderWidth(const double width)
{
    const auto normalized = std::clamp(width, 0.5, 8.0);
    if (qFuzzyCompare(sliceBorderWidth_ + 1.0, normalized + 1.0)) return;
    sliceBorderWidth_ = normalized;
    emit changed();
}

void SectionController::setCandidatePlane(const QVector3D& normal, const QVector3D& point)
{
    if (normal.isNull()) return;
    // Only a candidate: the plane a section is using changes when a plane is chosen, never
    // because another part was clicked.
    candidateNormal_ = normal.normalized();
    candidateOffset_ = QVector3D::dotProduct(candidateNormal_, point);
    hasSelectedPlane_ = true;
    emit changed();
}

void SectionController::usePlane(const QVector3D& normal, const QVector3D& point, const QString& label)
{
    if (normal.isNull()) return;
    planeNormal_ = normal.normalized();
    planeOffset_ = QVector3D::dotProduct(planeNormal_, point);
    usingSelectedPlane_ = true;
    usingAxis_ = false;
    planeLabel_ = label;
    position_ = 0.0;
    enabled_ = true;
    resumeAfterPick_ = false;
    cancelPlanePick();
    emit changed();
}

QVector3D SectionController::throughAxisNormal(const QVector3D& direction, const double degrees)
{
    const auto axis = direction.normalized();
    // At 0 degrees the plane holds the axis and the vertical (Z), so it reads upright; for an
    // axis that is itself vertical, it holds X instead. Then it turns about the axis.
    const auto upright = std::abs(axis.z()) < 0.999F ? QVector3D(0, 0, 1) : QVector3D(1, 0, 0);
    const auto start = QVector3D::crossProduct(axis, upright).normalized();
    const auto side = QVector3D::crossProduct(axis, start).normalized();
    const auto radians = degrees * 3.14159265358979323846 / 180.0;
    return (start * static_cast<float>(std::cos(radians)) + side * static_cast<float>(std::sin(radians))).normalized();
}

void SectionController::applyAxisPlane()
{
    planeNormal_ = axisThrough_ ? throughAxisNormal(axisDirection_, axisAngle_) : axisDirection_;
    planeOffset_ = QVector3D::dotProduct(planeNormal_, axisPoint_);
}

void SectionController::useAxis(const QVector3D& point, const QVector3D& direction, const QString& label)
{
    if (direction.isNull() || !std::isfinite(point.x()) || !std::isfinite(direction.x())) return;
    axisPoint_ = point;
    axisDirection_ = direction.normalized();
    axisThrough_ = false;
    axisAngle_ = 0.0;
    applyAxisPlane();
    usingSelectedPlane_ = true;
    usingAxis_ = true;
    planeLabel_ = label;
    position_ = 0.0;
    enabled_ = true;
    resumeAfterPick_ = false;
    cancelPlanePick();
    emit changed();
}

void SectionController::setAxisMode(const QString& mode)
{
    const auto through = mode == QStringLiteral("through");
    if (!usingAxis() || through == axisThrough_) return;
    axisThrough_ = through;
    // Across and through measure their offsets from different planes.
    position_ = 0.0;
    applyAxisPlane();
    emit changed();
}

void SectionController::setAxisAngle(const double degrees)
{
    if (!std::isfinite(degrees)) return;
    auto wrapped = std::fmod(degrees, 360.0);
    if (wrapped < 0.0) wrapped += 360.0;
    if (std::abs(wrapped - axisAngle_) < 1e-9) return;
    axisAngle_ = wrapped;
    if (usingAxis() && axisThrough_) applyAxisPlane();
    emit changed();
}

void SectionController::beginPlanePick()
{
    if (pickingPlane_) return;
    pickingPlane_ = true;
    // The section in place would hide the very faces it cut, and a click would land on the
    // part cut away: it is off while a face is picked, and back if the pick is cancelled.
    if (enabled_) {
        enabled_ = false;
        resumeAfterPick_ = true;
        emit changed();
    }
    emit pickingPlaneChanged();
}

void SectionController::cancelPlanePick()
{
    if (!pickingPlane_) return;
    pickingPlane_ = false;
    if (resumeAfterPick_) {
        resumeAfterPick_ = false;
        enabled_ = true;
        emit changed();
    }
    emit pickingPlaneChanged();
}

void SectionController::useSelectedPlane()
{
    if (!hasSelectedPlane_) return;
    if (usingSelectedPlane_ && planeNormal_ == candidateNormal_ && qFuzzyCompare(planeOffset_ + 1.0, candidateOffset_ + 1.0))
        return;
    planeNormal_ = candidateNormal_;
    planeOffset_ = candidateOffset_;
    usingAxis_ = false;
    planeLabel_ = tr("The last face clicked");
    usingSelectedPlane_ = true;
    position_ = 0.0;
    emit changed();
}

void SectionController::beginInteraction()
{
    if (interacting_) return;
    interactionStartPosition_ = position_;
    interacting_ = true;
    emit interactionChanged();
}

void SectionController::previewPosition(const double position)
{
    if (!interacting_) beginInteraction();
    setPosition(position);
}

void SectionController::commitInteraction()
{
    if (!interacting_) return;
    interacting_ = false;
    emit interactionChanged();
    // Force one exact geometry rebuild even though the final offset is unchanged.
    emit changed();
}

void SectionController::cancelInteraction()
{
    if (!interacting_) return;
    interacting_ = false;
    position_ = interactionStartPosition_;
    emit interactionChanged();
    emit changed();
}

QVector3D SectionController::effectiveNormal() const noexcept
{
    if (usingSelectedPlane_) return planeNormal_;

    switch (axis_) {
    case SectionAxis::X: return {1.0F, 0.0F, 0.0F};
    case SectionAxis::Y: return {0.0F, 1.0F, 0.0F};
    case SectionAxis::Z: return {0.0F, 0.0F, 1.0F};
    }
    return {0.0F, 0.0F, 1.0F};
}

double SectionController::effectiveOffset() const noexcept
{
    return (usingSelectedPlane_ ? planeOffset_ : 0.0) + position_;
}

} // namespace loupe::app::tools
