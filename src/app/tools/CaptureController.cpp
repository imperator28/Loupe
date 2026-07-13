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
}

void CaptureController::setScale(const int scale)
{
    scale_ = std::clamp(scale, 1, 4);
}

void CaptureController::setTransparentBackground(const bool transparent)
{
    transparentBackground_ = transparent;
}

QSize CaptureController::resolvedSize() const
{
    return viewportSize_ * scale_;
}

} // namespace loupe::app::tools
