#include "app/tools/SectionController.h"

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
    emit changed();
}

} // namespace loupe::app::tools
