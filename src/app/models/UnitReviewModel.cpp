#include "app/models/UnitReviewModel.h"

namespace loupe::app::models {

UnitReviewModel::UnitReviewModel(QObject* parent)
    : QObject(parent)
{
}

QVector3D UnitReviewModel::correctedExtents() const noexcept
{
    const auto factor = proposedUnit_ == QStringLiteral("in") ? 25.4F : 1.0F;
    return normalizedExtents_ * factor;
}

void UnitReviewModel::setProposedUnit(const QString& unit)
{
    if (proposedUnit_ == unit) return;
    proposedUnit_ = unit;
    emit proposalChanged();
    emit previewChanged();
}

void UnitReviewModel::setNormalizedExtents(const QVector3D& extents)
{
    normalizedExtents_ = extents;
    emit previewChanged();
}

void UnitReviewModel::applyOverride(const QString&)
{
    if (effectiveUnit_ == proposedUnit_) return;
    effectiveUnit_ = proposedUnit_;
    emit effectiveUnitChanged();
}

} // namespace loupe::app::models
