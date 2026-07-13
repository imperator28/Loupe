#pragma once

#include <QObject>
#include <QSize>

namespace loupe::app::tools {

class CaptureController final : public QObject {
    Q_OBJECT

public:
    explicit CaptureController(QObject* parent = nullptr);

    void setViewportSize(const QSize& size);
    void setScale(int scale);
    void setTransparentBackground(bool transparent);
    [[nodiscard]] QString format() const { return QStringLiteral("png"); }
    [[nodiscard]] QSize resolvedSize() const;
    [[nodiscard]] bool transparentBackground() const noexcept { return transparentBackground_; }

private:
    QSize viewportSize_;
    int scale_{1};
    bool transparentBackground_{true};
};

} // namespace loupe::app::tools
