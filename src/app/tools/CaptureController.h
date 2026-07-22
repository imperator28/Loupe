#pragma once

#include <QObject>
#include <QImage>
#include <QSize>
#include <QUrl>

namespace loupe::app::tools {

class CaptureController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(double scale READ scale WRITE setCustomScale NOTIFY changed)
    Q_PROPERTY(bool transparentBackground READ transparentBackground WRITE setTransparentBackground NOTIFY changed)
    Q_PROPERTY(bool includeMeasurements READ includeMeasurements WRITE setIncludeMeasurements NOTIFY changed)
    Q_PROPERTY(bool includeSectionCaps READ includeSectionCaps WRITE setIncludeSectionCaps NOTIFY changed)
    Q_PROPERTY(int resolvedWidth READ resolvedWidth NOTIFY changed)
    Q_PROPERTY(int resolvedHeight READ resolvedHeight NOTIFY changed)
    Q_PROPERTY(double minimumScale READ minimumScale CONSTANT)
    Q_PROPERTY(double maximumScale READ maximumScale NOTIFY changed)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(bool lastSaveSucceeded READ lastSaveSucceeded NOTIFY statusChanged)
    Q_PROPERTY(bool inProgress READ inProgress NOTIFY progressChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)

public:
    explicit CaptureController(QObject* parent = nullptr);

    void setViewportSize(const QSize& size);
    Q_INVOKABLE void setViewportSize(int width, int height);
    void setScale(int scale);
    Q_INVOKABLE void setCustomScale(double scale);
    void setTransparentBackground(bool transparent);
    void setIncludeMeasurements(bool include);
    void setIncludeSectionCaps(bool include);
    Q_INVOKABLE void beginCapture();
    Q_INVOKABLE void setCaptureProgress(double progress, const QString& message);
    Q_INVOKABLE void failCapture(const QString& message);
    Q_INVOKABLE bool saveImage(const QImage& image, const QUrl& destination);
    [[nodiscard]] QString format() const { return QStringLiteral("png"); }
    [[nodiscard]] QSize resolvedSize() const;
    [[nodiscard]] int resolvedWidth() const { return resolvedSize().width(); }
    [[nodiscard]] int resolvedHeight() const { return resolvedSize().height(); }
    [[nodiscard]] double scale() const noexcept { return scale_; }
    [[nodiscard]] double minimumScale() const noexcept { return kMinimumScale; }
    [[nodiscard]] double maximumScale() const noexcept;
    [[nodiscard]] bool transparentBackground() const noexcept { return transparentBackground_; }
    [[nodiscard]] bool includeMeasurements() const noexcept { return includeMeasurements_; }
    [[nodiscard]] bool includeSectionCaps() const noexcept { return includeSectionCaps_; }
    [[nodiscard]] const QString& statusMessage() const noexcept { return statusMessage_; }
    [[nodiscard]] bool lastSaveSucceeded() const noexcept { return lastSaveSucceeded_; }
    [[nodiscard]] bool inProgress() const noexcept { return inProgress_; }
    [[nodiscard]] double progress() const noexcept { return progress_; }

signals:
    void changed();
    void statusChanged();
    void progressChanged();

private:
    static constexpr double kMinimumScale = 0.01;
    static constexpr double kMaximumScale = 16.0;
    static constexpr int kMaximumCaptureDimension = 16'384;
    static constexpr qint64 kMaximumCapturePixels = 96LL * 1024 * 1024;

    QSize viewportSize_;
    double scale_{1.0};
    bool transparentBackground_{true};
    bool includeMeasurements_{true};
    bool includeSectionCaps_{true};
    QString statusMessage_;
    bool lastSaveSucceeded_{false};
    bool inProgress_{false};
    double progress_{0.0};
};

} // namespace loupe::app::tools
