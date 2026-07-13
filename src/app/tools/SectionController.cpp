#include "app/tools/SectionController.h"

namespace loupe::app::tools {

SectionController::SectionController(QObject* parent)
    : QObject(parent)
{
}

void SectionController::setEnabled(const bool enabled)
{
    enabled_ = enabled;
}

void SectionController::setAxis(const SectionAxis axis)
{
    axis_ = axis;
}

} // namespace loupe::app::tools
