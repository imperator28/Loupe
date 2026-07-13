#pragma once

#include <QObject>

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

public:
    explicit SectionController(QObject* parent = nullptr);

    void setEnabled(bool enabled);
    void setAxis(SectionAxis axis);
    Q_INVOKABLE void setAxisName(const QString& axisName);
    void setPosition(double position);
    void setFlipped(bool flipped);
    void setCapEnabled(bool enabled);
    void setSliceOnly(bool sliceOnly);
    [[nodiscard]] bool enabled() const noexcept { return enabled_; }
    [[nodiscard]] SectionAxis axis() const noexcept { return axis_; }
    [[nodiscard]] QString axisName() const;
    [[nodiscard]] double position() const noexcept { return position_; }
    [[nodiscard]] bool flipped() const noexcept { return flipped_; }
    [[nodiscard]] bool capEnabled() const noexcept { return capEnabled_; }
    [[nodiscard]] bool sliceOnly() const noexcept { return sliceOnly_; }
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
};

} // namespace loupe::app::tools
