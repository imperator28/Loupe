#pragma once

#include <QObject>
#include <QString>
#include <QVector3D>

namespace loupe::app::tools {

struct MeasurementResult final {
    double value{};
    QString unitLabel;
};

class MeasurementController final : public QObject {
    Q_OBJECT

public:
    explicit MeasurementController(QObject* parent = nullptr);

    void setEffectiveUnit(const QString& unit);
    [[nodiscard]] MeasurementResult pointDistance(const QVector3D& first, const QVector3D& second) const;

private:
    QString effectiveUnit_{QStringLiteral("mm")};
};

} // namespace loupe::app::tools
