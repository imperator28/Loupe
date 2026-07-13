#pragma once

#include <QObject>
#include <QSize>

namespace loupe::app::tools {

class CaptureController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(double scale READ scale WRITE setCustomScale NOTIFY changed)
    Q_PROPERTY(bool transparentBackground READ transparentBackground WRITE setTransparentBackground NOTIFY changed)
    Q_PROPERTY(bool includeMeasurements READ includeMeasurements WRITE setIncludeMeasurements NOTIFY changed)
    Q_PROPERTY(bool includeSectionCaps READ includeSectionCaps WRITE setIncludeSectionCaps NOTIFY changed)
    Q_PROPERTY(int resolvedWidth READ resolvedWidth NOTIFY changed)
    Q_PROPERTY(int resolvedHeight READ resolvedHeight NOTIFY changed)

public:
    explicit CaptureController(QObject* parent = nullptr);

    void setViewportSize(const QSize& size);
    Q_INVOKABLE void setViewportSize(int width, int height);
    void setScale(int scale);
    Q_INVOKABLE void setCustomScale(double scale);
    void setTransparentBackground(bool transparent);
    void setIncludeMeasurements(bool include);
    void setIncludeSectionCaps(bool include);
    [[nodiscard]] QString format() const { return QStringLiteral("png"); }
    [[nodiscard]] QSize resolvedSize() const;
    [[nodiscard]] int resolvedWidth() const { return resolvedSize().width(); }
    [[nodiscard]] int resolvedHeight() const { return resolvedSize().height(); }
    [[nodiscard]] double scale() const noexcept { return scale_; }
    [[nodiscard]] bool transparentBackground() const noexcept { return transparentBackground_; }
    [[nodiscard]] bool includeMeasurements() const noexcept { return includeMeasurements_; }
    [[nodiscard]] bool includeSectionCaps() const noexcept { return includeSectionCaps_; }

signals:
    void changed();

private:
    QSize viewportSize_;
    double scale_{1.0};
    bool transparentBackground_{true};
    bool includeMeasurements_{true};
    bool includeSectionCaps_{true};
};

} // namespace loupe::app::tools
