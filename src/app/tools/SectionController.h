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
    Q_PROPERTY(QString sliceBorderColor READ sliceBorderColor WRITE setSliceBorderColor NOTIFY changed)
    Q_PROPERTY(double sliceBorderWidth READ sliceBorderWidth WRITE setSliceBorderWidth NOTIFY changed)
    Q_PROPERTY(bool hasSelectedPlane READ hasSelectedPlane NOTIFY changed)
    Q_PROPERTY(bool usingSelectedPlane READ usingSelectedPlane NOTIFY changed)
    Q_PROPERTY(QVector3D planeNormal READ planeNormal NOTIFY changed)
    Q_PROPERTY(double planeOffset READ planeOffset NOTIFY changed)
    Q_PROPERTY(QVector3D effectiveNormal READ effectiveNormal NOTIFY changed)
    Q_PROPERTY(double effectiveOffset READ effectiveOffset NOTIFY changed)
    Q_PROPERTY(bool interacting READ interacting NOTIFY interactionChanged)
    // What the section's own plane was taken from ("Flat face on Bracket-2"), while it is in use.
    Q_PROPERTY(QString planeLabel READ planeLabel NOTIFY changed)
    // Waiting for a face to be clicked to section along its plane.
    Q_PROPERTY(bool pickingPlane READ pickingPlane NOTIFY pickingPlaneChanged)
    // A section about a picked axis (a hole's, a shaft's): "across" it -- square to it, the
    // default, which needs no other reference -- or "through" it, a plane containing the axis
    // turned axisAngle degrees about it from a fixed start (level where it can be).
    Q_PROPERTY(bool usingAxis READ usingAxis NOTIFY changed)
    Q_PROPERTY(QString axisMode READ axisMode WRITE setAxisMode NOTIFY changed)
    Q_PROPERTY(double axisAngle READ axisAngle WRITE setAxisAngle NOTIFY changed)

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
    void setSliceBorderColor(const QString& color);
    void setSliceBorderWidth(double width);
    void setCandidatePlane(const QVector3D& normal, const QVector3D& point);
    Q_INVOKABLE void useSelectedPlane();
    // Section along this plane (a picked flat face's, in scene coordinates), named `label`.
    // Ends a plane pick.
    Q_INVOKABLE void usePlane(const QVector3D& normal, const QVector3D& point, const QString& label);
    // Section about this axis (scene coordinates): across it at `point`, named `label`. Ends
    // a plane pick.
    Q_INVOKABLE void useAxis(const QVector3D& point, const QVector3D& direction, const QString& label);
    void setAxisMode(const QString& mode);
    void setAxisAngle(double degrees);
    [[nodiscard]] bool usingAxis() const noexcept { return usingAxis_ && usingSelectedPlane_; }
    [[nodiscard]] QString axisMode() const { return axisThrough_ ? QStringLiteral("through") : QStringLiteral("across"); }
    [[nodiscard]] double axisAngle() const noexcept { return axisAngle_; }
    // The plane through the axis at `degrees`: its normal (square to the axis).
    [[nodiscard]] static QVector3D throughAxisNormal(const QVector3D& direction, double degrees);
    Q_INVOKABLE void beginPlanePick();
    Q_INVOKABLE void cancelPlanePick();
    Q_INVOKABLE void beginInteraction();
    Q_INVOKABLE void previewPosition(double position);
    Q_INVOKABLE void commitInteraction();
    Q_INVOKABLE void cancelInteraction();
    [[nodiscard]] bool enabled() const noexcept { return enabled_; }
    [[nodiscard]] SectionAxis axis() const noexcept { return axis_; }
    [[nodiscard]] QString axisName() const;
    [[nodiscard]] double position() const noexcept { return position_; }
    [[nodiscard]] bool flipped() const noexcept { return flipped_; }
    [[nodiscard]] bool capEnabled() const noexcept { return capEnabled_; }
    [[nodiscard]] bool sliceOnly() const noexcept { return sliceOnly_; }
    [[nodiscard]] QString sliceDisplay() const { return sliceDisplay_; }
    [[nodiscard]] QString sliceBorderColor() const { return sliceBorderColor_; }
    [[nodiscard]] double sliceBorderWidth() const noexcept { return sliceBorderWidth_; }
    [[nodiscard]] bool hasSelectedPlane() const noexcept { return hasSelectedPlane_; }
    [[nodiscard]] bool usingSelectedPlane() const noexcept { return usingSelectedPlane_; }
    [[nodiscard]] QVector3D planeNormal() const noexcept { return planeNormal_; }
    [[nodiscard]] double planeOffset() const noexcept { return planeOffset_; }
    [[nodiscard]] QVector3D effectiveNormal() const noexcept;
    [[nodiscard]] double effectiveOffset() const noexcept;
    [[nodiscard]] bool interacting() const noexcept { return interacting_; }
    [[nodiscard]] QString planeLabel() const { return usingSelectedPlane_ ? planeLabel_ : QString{}; }
    [[nodiscard]] bool pickingPlane() const noexcept { return pickingPlane_; }
    [[nodiscard]] int exportMutationCount() const noexcept { return 0; }

signals:
    void changed();
    void interactionChanged();
    void pickingPlaneChanged();

private:
    bool enabled_{false};
    SectionAxis axis_{SectionAxis::Z};
    double position_{};
    bool flipped_{false};
    bool capEnabled_{true};
    bool sliceOnly_{false};
    QString sliceDisplay_{QStringLiteral("filled-outline")};
    QString sliceBorderColor_;
    double sliceBorderWidth_{1.5};
    bool hasSelectedPlane_{false};
    bool usingSelectedPlane_{false};
    // The plane in use, and the last face clicked's, which the older "use selected face"
    // command takes.
    QVector3D planeNormal_{0.0F, 0.0F, 1.0F};
    double planeOffset_{};
    QVector3D candidateNormal_{0.0F, 0.0F, 1.0F};
    double candidateOffset_{};
    bool interacting_{false};
    double interactionStartPosition_{};
    QString planeLabel_;
    bool pickingPlane_{false};
    // The section was on when a face pick began, and comes back if the pick is cancelled.
    bool resumeAfterPick_{false};
    bool usingAxis_{false};
    bool axisThrough_{false};
    double axisAngle_{};
    QVector3D axisPoint_;
    QVector3D axisDirection_{0.0F, 0.0F, 1.0F};
    void applyAxisPlane();
};

} // namespace loupe::app::tools
