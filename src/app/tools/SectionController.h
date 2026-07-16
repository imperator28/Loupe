#pragma once

#include <QObject>
#include <QVector3D>

namespace loupe::app::tools {

enum class SectionAxis { X, Y, Z };

class SectionController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(QString axisName READ axisName WRITE setAxisName NOTIFY changed)
    Q_PROPERTY(double position READ position WRITE setPosition NOTIFY changed)
    Q_PROPERTY(bool flipped READ flipped WRITE setFlipped NOTIFY changed)
    Q_PROPERTY(bool capEnabled READ capEnabled WRITE setCapEnabled NOTIFY changed)
    Q_PROPERTY(bool sliceOnly READ sliceOnly WRITE setSliceOnly NOTIFY changed)
    Q_PROPERTY(QString sliceDisplay READ sliceDisplay WRITE setSliceDisplay NOTIFY changed)
    Q_PROPERTY(bool hasSelectedPlane READ hasSelectedPlane NOTIFY changed)
    Q_PROPERTY(bool usingSelectedPlane READ usingSelectedPlane NOTIFY changed)
    Q_PROPERTY(QVector3D planeNormal READ planeNormal NOTIFY changed)
    Q_PROPERTY(double planeOffset READ planeOffset NOTIFY changed)

public:
    explicit SectionController(QObject* parent = nullptr);

    void setEnabled(bool enabled);
    void setAxis(SectionAxis axis);
    Q_INVOKABLE void setAxisName(const QString& axisName);
    void setPosition(double position);
    void setFlipped(bool flipped);
    void setCapEnabled(bool enabled);
    void setSliceOnly(bool sliceOnly);
    void setSliceDisplay(const QString& sliceDisplay);
    void setCandidatePlane(const QVector3D& normal, const QVector3D& point);
    Q_INVOKABLE void useSelectedPlane();
    [[nodiscard]] bool enabled() const noexcept { return enabled_; }
    [[nodiscard]] SectionAxis axis() const noexcept { return axis_; }
    [[nodiscard]] QString axisName() const;
    [[nodiscard]] double position() const noexcept { return position_; }
    [[nodiscard]] bool flipped() const noexcept { return flipped_; }
    [[nodiscard]] bool capEnabled() const noexcept { return capEnabled_; }
    [[nodiscard]] bool sliceOnly() const noexcept { return sliceOnly_; }
    [[nodiscard]] QString sliceDisplay() const { return sliceDisplay_; }
    [[nodiscard]] bool hasSelectedPlane() const noexcept { return hasSelectedPlane_; }
    [[nodiscard]] bool usingSelectedPlane() const noexcept { return usingSelectedPlane_; }
    [[nodiscard]] QVector3D planeNormal() const noexcept { return planeNormal_; }
    [[nodiscard]] double planeOffset() const noexcept { return planeOffset_; }
    [[nodiscard]] int exportMutationCount() const noexcept { return 0; }

signals:
    void changed();

private:
    bool enabled_{false};
    SectionAxis axis_{SectionAxis::Z};
    double position_{};
    bool flipped_{false};
    bool capEnabled_{true};
    bool sliceOnly_{false};
    QString sliceDisplay_{QStringLiteral("filled-outline")};
    bool hasSelectedPlane_{false};
    bool usingSelectedPlane_{false};
    QVector3D planeNormal_{0.0F, 0.0F, 1.0F};
    double planeOffset_{};
};

} // namespace loupe::app::tools
