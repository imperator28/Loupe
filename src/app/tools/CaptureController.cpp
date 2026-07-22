#include "app/tools/CaptureController.h"

#include <QFileInfo>
#include <QSaveFile>

#include <png.h>

#include <algorithm>
#include <cmath>

namespace loupe::app::tools {

CaptureController::CaptureController(QObject* parent)
    : QObject(parent)
{
}

void CaptureController::setViewportSize(const QSize& size)
{
    const auto normalized = size.expandedTo({0, 0});
    if (viewportSize_ == normalized) return;
    viewportSize_ = normalized;
    scale_ = std::clamp(scale_, minimumScale(), maximumScale());
    emit changed();
}

void CaptureController::setViewportSize(const int width, const int height)
{
    setViewportSize(QSize{width, height});
}

void CaptureController::setScale(const int scale)
{
    setCustomScale(static_cast<double>(scale));
}

void CaptureController::setCustomScale(const double scale)
{
    const auto clamped = std::clamp(scale, minimumScale(), maximumScale());
    if (qFuzzyCompare(scale_ + 1.0, clamped + 1.0)) return;
    scale_ = clamped;
    emit changed();
}

void CaptureController::setTransparentBackground(const bool transparent)
{
    if (transparentBackground_ == transparent) return;
    transparentBackground_ = transparent;
    emit changed();
}

void CaptureController::setIncludeMeasurements(const bool include)
{
    if (includeMeasurements_ == include) return;
    includeMeasurements_ = include;
    emit changed();
}

void CaptureController::setIncludeSectionCaps(const bool include)
{
    if (includeSectionCaps_ == include) return;
    includeSectionCaps_ = include;
    emit changed();
}

void CaptureController::beginCapture()
{
    inProgress_ = true;
    progress_ = 0.04;
    lastSaveSucceeded_ = false;
    statusMessage_ = tr("Preparing capture…");
    emit progressChanged();
    emit statusChanged();
}

void CaptureController::setCaptureProgress(const double progress, const QString& message)
{
    const auto normalized = std::clamp(progress, 0.0, 1.0);
    const auto progressDidChange = !qFuzzyCompare(progress_ + 1.0, normalized + 1.0);
    const auto messageDidChange = statusMessage_ != message;
    progress_ = normalized;
    statusMessage_ = message;
    if (progressDidChange) emit progressChanged();
    if (messageDidChange) emit statusChanged();
}

void CaptureController::failCapture(const QString& message)
{
    inProgress_ = false;
    lastSaveSucceeded_ = false;
    statusMessage_ = message;
    emit progressChanged();
    emit statusChanged();
}

bool CaptureController::saveImage(const QImage& image, const QUrl& destination)
{
    const auto path = destination.isLocalFile() ? destination.toLocalFile() : QString{};
    auto fail = [this](const QString& message) {
        failCapture(message);
        return false;
    };
    if (image.isNull()) return fail(tr("Capture failed: the viewport image was empty."));
    if (path.isEmpty()) return fail(tr("Capture failed: choose a local PNG file."));

    const auto rgba = image.convertToFormat(QImage::Format_RGBA8888);
    png_image png{};
    png.version = PNG_IMAGE_VERSION;
    png.width = static_cast<png_uint_32>(rgba.width());
    png.height = static_cast<png_uint_32>(rgba.height());
    png.format = PNG_FORMAT_RGBA;
    png_alloc_size_t encodedSize{};
    if (!png_image_write_to_memory(&png, nullptr, &encodedSize, 0, rgba.constBits(), 0, nullptr)) {
        return fail(tr("Capture failed while sizing PNG output."));
    }
    QByteArray encoded(static_cast<qsizetype>(encodedSize), Qt::Uninitialized);
    if (!png_image_write_to_memory(&png, encoded.data(), &encodedSize, 0, rgba.constBits(), 0, nullptr)) {
        return fail(tr("Capture failed while encoding PNG."));
    }
    encoded.resize(static_cast<qsizetype>(encodedSize));

    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly)) return fail(tr("Capture failed: %1").arg(output.errorString()));
    if (output.write(encoded) != encoded.size()) {
        output.cancelWriting();
        return fail(tr("Capture failed while writing PNG."));
    }
    if (!output.commit()) return fail(tr("Capture failed: %1").arg(output.errorString()));

    png_image verification{};
    verification.version = PNG_IMAGE_VERSION;
    const auto nativePath = QFile::encodeName(path);
    if (!png_image_begin_read_from_file(&verification, nativePath.constData())) {
        return fail(tr("Capture was written but could not be verified."));
    }
    verification.format = PNG_FORMAT_RGBA;
    QByteArray decoded(static_cast<qsizetype>(PNG_IMAGE_SIZE(verification)), Qt::Uninitialized);
    const auto verified = verification.width == png.width && verification.height == png.height
        && png_image_finish_read(&verification, nullptr, decoded.data(), 0, nullptr);
    png_image_free(&verification);
    if (!verified) return fail(tr("Capture was written but could not be verified."));
    lastSaveSucceeded_ = true;
    inProgress_ = false;
    progress_ = 1.0;
    statusMessage_ = tr("Saved %1").arg(QFileInfo(path).fileName());
    emit progressChanged();
    emit statusChanged();
    return true;
}

QSize CaptureController::resolvedSize() const
{
    return {qRound(viewportSize_.width() * scale_), qRound(viewportSize_.height() * scale_)};
}

double CaptureController::maximumScale() const noexcept
{
    if (viewportSize_.isEmpty()) return kMaximumScale;

    const auto width = static_cast<double>(viewportSize_.width());
    const auto height = static_cast<double>(viewportSize_.height());
    const auto byPixels = std::sqrt(static_cast<double>(kMaximumCapturePixels) / (width * height));
    const auto byDimension = std::min(static_cast<double>(kMaximumCaptureDimension) / width,
                                      static_cast<double>(kMaximumCaptureDimension) / height);
    return std::max(kMinimumScale, std::min({kMaximumScale, byPixels, byDimension}));
}

} // namespace loupe::app::tools
