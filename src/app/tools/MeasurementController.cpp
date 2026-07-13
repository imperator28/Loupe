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

MeasurementResult MeasurementController::pointDistance(const QVector3D& first, const QVector3D& second) const
{
    const auto millimeters = static_cast<double>((second - first).length());
    const auto divisor = effectiveUnit_ == QStringLiteral("in") ? 25.4 : 1.0;
    return {millimeters / divisor, effectiveUnit_};
}

} // namespace loupe::app::tools
