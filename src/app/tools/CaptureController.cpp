#include "app/tools/CaptureController.h"

#include <algorithm>

namespace loupe::app::tools {

CaptureController::CaptureController(QObject* parent)
    : QObject(parent)
{
}

void CaptureController::setViewportSize(const QSize& size)
{
    viewportSize_ = size.expandedTo({0, 0});
    emit changed();
}

void CaptureController::setScale(const int scale)
{
    setCustomScale(static_cast<double>(scale));
}

void CaptureController::setCustomScale(const double scale)
{
    const auto clamped = std::clamp(scale, 1.0, 4.0);
    if (qFuzzyCompare(scale_ + 1.0, clamped + 1.0)) return;
    scale_ = clamped;
    emit changed();
}

void CaptureController::setTransparentBackground(const bool transparent)
{
    if (transparentBackground_ == transparent) return;
    transparentBackground_ = transparent;
    emit changed();
}

void CaptureController::setIncludeMeasurements(const bool include)
{
    if (includeMeasurements_ == include) return;
    includeMeasurements_ = include;
    emit changed();
}

void CaptureController::setIncludeSectionCaps(const bool include)
{
    if (includeSectionCaps_ == include) return;
    includeSectionCaps_ = include;
}

QSize CaptureController::resolvedSize() const
{
    return {qRound(viewportSize_.width() * scale_), qRound(viewportSize_.height() * scale_)};
}

} // namespace loupe::app::tools
