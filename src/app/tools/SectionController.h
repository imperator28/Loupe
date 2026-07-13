#pragma once

#include <QObject>

namespace loupe::app::tools {

enum class SectionAxis { X, Y, Z };

class SectionController final : public QObject {
    Q_OBJECT

public:
    explicit SectionController(QObject* parent = nullptr);

    void setEnabled(bool enabled);
    void setAxis(SectionAxis axis);
    [[nodiscard]] bool enabled() const noexcept { return enabled_; }
    [[nodiscard]] SectionAxis axis() const noexcept { return axis_; }
    [[nodiscard]] int exportMutationCount() const noexcept { return 0; }

private:
    bool enabled_{false};
    SectionAxis axis_{SectionAxis::Z};
};

} // namespace loupe::app::tools
