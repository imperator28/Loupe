#pragma once

#include <QObject>
#include <QVector3D>

namespace loupe::app::models {

class UnitReviewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString effectiveUnit READ effectiveUnit NOTIFY effectiveUnitChanged)
    Q_PROPERTY(QString proposedUnit READ proposedUnit WRITE setProposedUnit NOTIFY proposalChanged)
    Q_PROPERTY(QVector3D normalizedExtents READ normalizedExtents WRITE setNormalizedExtents NOTIFY previewChanged)
    Q_PROPERTY(QVector3D correctedExtents READ correctedExtents NOTIFY previewChanged)

public:
    explicit UnitReviewModel(QObject* parent = nullptr);
    [[nodiscard]] QString effectiveUnit() const { return effectiveUnit_; }
    [[nodiscard]] QString proposedUnit() const { return proposedUnit_; }
    [[nodiscard]] QVector3D normalizedExtents() const noexcept { return normalizedExtents_; }
    [[nodiscard]] QVector3D correctedExtents() const noexcept;
    void setProposedUnit(const QString& unit);
    void setNormalizedExtents(const QVector3D& extents);
    Q_INVOKABLE void applyOverride(const QString& reason);

signals:
    void effectiveUnitChanged();
    void proposalChanged();
    void previewChanged();

private:
    QString effectiveUnit_{QStringLiteral("mm")};
    QString proposedUnit_{QStringLiteral("mm")};
    QVector3D normalizedExtents_;
};

} // namespace loupe::app::models
