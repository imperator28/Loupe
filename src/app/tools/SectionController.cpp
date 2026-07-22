#include "app/tools/SectionController.h"

#include <QColor>

#include <algorithm>

namespace loupe::app::tools {

SectionController::SectionController(QObject* parent)
    : QObject(parent)
{
}

void SectionController::setEnabled(const bool enabled)
{
    if (enabled_ == enabled) return;
    enabled_ = enabled;
    emit changed();
}

void SectionController::setAxis(const SectionAxis axis)
{
    if (axis_ == axis && !usingSelectedPlane_) return;
    axis_ = axis;
    usingSelectedPlane_ = false;
    position_ = 0.0;
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
    planeNormal_ = normal.normalized();
    planeOffset_ = QVector3D::dotProduct(planeNormal_, point);
    hasSelectedPlane_ = true;
    emit changed();
}

void SectionController::useSelectedPlane()
{
    if (!hasSelectedPlane_ || usingSelectedPlane_) return;
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
