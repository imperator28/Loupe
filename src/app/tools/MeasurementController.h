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
    Q_PROPERTY(double resultValue READ resultValue NOTIFY resultChanged)
    Q_PROPERTY(QString resultUnit READ resultUnit NOTIFY resultChanged)
    Q_PROPERTY(QString resultLabel READ resultLabel NOTIFY resultChanged)

public:
    explicit MeasurementController(QObject* parent = nullptr);

    void setEffectiveUnit(const QString& unit);
    void setMode(MeasurementMode mode);
    void setSelectedGeometry(double surfaceAreaMm2, double volumeMm3, const QVector3D& boundsMm);
    void clearSelectedGeometry();
    Q_INVOKABLE void setModeName(const QString& modeName);
    [[nodiscard]] MeasurementMode mode() const noexcept { return mode_; }
    [[nodiscard]] QString pickInstruction() const;
    [[nodiscard]] MeasurementResult pointDistance(const QVector3D& first, const QVector3D& second) const;
    [[nodiscard]] double resultValue() const;
    [[nodiscard]] QString resultUnit() const;
    [[nodiscard]] QString resultLabel() const;

signals:
    void modeChanged();
    void resultChanged();

private:
    QString effectiveUnit_{QStringLiteral("mm")};
    MeasurementMode mode_{MeasurementMode::PointToPoint};
    double surfaceAreaMm2_{};
    double volumeMm3_{};
    QVector3D boundsMm_;
    bool hasSelectedGeometry_{false};
};

} // namespace loupe::app::tools
