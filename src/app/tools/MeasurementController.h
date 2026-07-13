#pragma once

#include <QObject>
#include <QString>
#include <QVector3D>

namespace loupe::app::tools {

enum class MeasurementMode {
    PointToPoint,
    EdgeLength,
    SurfaceToSurface,
    RadiusDiameter,
    Angle,
    SurfaceArea,
    Volume,
    Bounds,
};

struct MeasurementResult final {
    double value{};
    QString unitLabel;
};

class MeasurementController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(MeasurementMode mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(QString pickInstruction READ pickInstruction NOTIFY modeChanged)

public:
    explicit MeasurementController(QObject* parent = nullptr);

    void setEffectiveUnit(const QString& unit);
    void setMode(MeasurementMode mode);
    Q_INVOKABLE void setModeName(const QString& modeName);
    [[nodiscard]] MeasurementMode mode() const noexcept { return mode_; }
    [[nodiscard]] QString pickInstruction() const;
    [[nodiscard]] MeasurementResult pointDistance(const QVector3D& first, const QVector3D& second) const;

signals:
    void modeChanged();

private:
    QString effectiveUnit_{QStringLiteral("mm")};
    MeasurementMode mode_{MeasurementMode::PointToPoint};
};

} // namespace loupe::app::tools
