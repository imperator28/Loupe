#include "app/tools/CaptureController.h"

#include <QClipboard>
#include <QFileInfo>
#include <QGuiApplication>
#include <QSaveFile>

#include <png.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace loupe::app::tools {

namespace {

std::optional<QByteArray> encodePngBytes(const QImage& rgba)
{
    png_image png{};
    png.version = PNG_IMAGE_VERSION;
    png.width = static_cast<png_uint_32>(rgba.width());
    png.height = static_cast<png_uint_32>(rgba.height());
    png.format = PNG_FORMAT_RGBA;
    png_alloc_size_t encodedSize{};
    if (!png_image_write_to_memory(&png, nullptr, &encodedSize, 0, rgba.constBits(), 0, nullptr)) return std::nullopt;
    QByteArray encoded(static_cast<qsizetype>(encodedSize), Qt::Uninitialized);
    if (!png_image_write_to_memory(&png, encoded.data(), &encodedSize, 0, rgba.constBits(), 0, nullptr)) return std::nullopt;
    encoded.resize(static_cast<qsizetype>(encodedSize));
    return encoded;
}

#ifdef Q_OS_WIN
HGLOBAL globalFromBytes(const void* data, SIZE_T size)
{
    HGLOBAL handle = ::GlobalAlloc(GMEM_MOVEABLE, size);
    if (!handle) return nullptr;
    if (void* locked = ::GlobalLock(handle)) {
        std::memcpy(locked, data, size);
        ::GlobalUnlock(handle);
        return handle;
    }
    ::GlobalFree(handle);
    return nullptr;
}

// Publish the capture to the Windows clipboard with alpha intact, in the
// formats real apps actually read for a transparent paste:
//   - CF_PNG ("PNG"): Chromium reads this by name — covers Chrome and every
//     Electron app (Notion, Claude Code, …) plus modern PowerPoint.
//   - CF_DIBV5: an alpha-capable bitmap for apps that read it (e.g. Snipaste).
// Windows auto-synthesizes the opaque CF_DIB / CF_BITMAP from the DIBV5 for
// legacy apps. Qt's own clipboard converter refuses to emit CF_PNG at all
// (QTBUG-47656), which is exactly why a transparent paste otherwise lands as
// solid black everywhere except DIBV5-aware apps, so this is done via the
// Win32 clipboard API directly rather than through QClipboard.
bool setWindowsClipboardImageWithAlpha(const QImage& source, const QByteArray& png)
{
    const QImage image = source.convertToFormat(QImage::Format_ARGB32);
    const int width = image.width();
    const int height = image.height();
    if (width <= 0 || height <= 0) return false;

    const SIZE_T rowBytes = static_cast<SIZE_T>(width) * 4;
    const SIZE_T pixelBytes = rowBytes * static_cast<SIZE_T>(height);
    const HGLOBAL dibHandle = ::GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPV5HEADER) + pixelBytes);
    if (!dibHandle) return false;
    if (auto* base = static_cast<std::uint8_t*>(::GlobalLock(dibHandle))) {
        auto* header = reinterpret_cast<BITMAPV5HEADER*>(base);
        std::memset(header, 0, sizeof(BITMAPV5HEADER));
        header->bV5Size = sizeof(BITMAPV5HEADER);
        header->bV5Width = width;
        header->bV5Height = height; // positive => bottom-up rows
        header->bV5Planes = 1;
        header->bV5BitCount = 32;
        header->bV5Compression = BI_BITFIELDS;
        header->bV5RedMask = 0x00FF0000;
        header->bV5GreenMask = 0x0000FF00;
        header->bV5BlueMask = 0x000000FF;
        header->bV5AlphaMask = 0xFF000000;
        header->bV5CSType = LCS_WINDOWS_COLOR_SPACE;
        header->bV5Intent = LCS_GM_IMAGES;
        header->bV5SizeImage = static_cast<DWORD>(pixelBytes);
        // Format_ARGB32 scanlines are B,G,R,A bytes on little-endian Windows,
        // matching the masks above; copy rows bottom-up for the DIB.
        std::uint8_t* pixels = base + sizeof(BITMAPV5HEADER);
        for (int y = 0; y < height; ++y) {
            std::memcpy(pixels + static_cast<SIZE_T>(y) * rowBytes,
                        image.constScanLine(height - 1 - y), rowBytes);
        }
        ::GlobalUnlock(dibHandle);
    } else {
        ::GlobalFree(dibHandle);
        return false;
    }

    const UINT pngFormat = ::RegisterClipboardFormatW(L"PNG");
    HGLOBAL pngHandle = (pngFormat && !png.isEmpty())
        ? globalFromBytes(png.constData(), static_cast<SIZE_T>(png.size())) : nullptr;

    if (!::OpenClipboard(nullptr)) {
        ::GlobalFree(dibHandle);
        if (pngHandle) ::GlobalFree(pngHandle);
        return false;
    }
    ::EmptyClipboard();
    bool anySet = false;
    // Offer PNG first so format-priority-ordered readers see the alpha path.
    if (pngHandle) {
        if (::SetClipboardData(pngFormat, pngHandle)) anySet = true;
        else ::GlobalFree(pngHandle);
    }
    if (::SetClipboardData(CF_DIBV5, dibHandle)) anySet = true;
    else ::GlobalFree(dibHandle);
    ::CloseClipboard();
    return anySet;
}
#endif

} // namespace

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
    const auto encoded = encodePngBytes(rgba);
    if (!encoded) return fail(tr("Capture failed while encoding PNG."));

    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly)) return fail(tr("Capture failed: %1").arg(output.errorString()));
    if (output.write(*encoded) != encoded->size()) {
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
    const auto verified = verification.width == static_cast<png_uint_32>(rgba.width())
        && verification.height == static_cast<png_uint_32>(rgba.height())
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

bool CaptureController::copyImageToClipboard(const QImage& image)
{
    if (image.isNull()) {
        failCapture(tr("Capture failed: the viewport image was empty."));
        return false;
    }
    auto* clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        failCapture(tr("Capture failed: no system clipboard is available."));
        return false;
    }
#ifdef Q_OS_WIN
    const auto rgba = image.convertToFormat(QImage::Format_RGBA8888);
    const auto png = encodePngBytes(rgba);
    if (!setWindowsClipboardImageWithAlpha(image, png ? *png : QByteArray())) {
        // Fall back to Qt if the direct clipboard write failed for any reason;
        // better an opaque paste than nothing.
        clipboard->setImage(image.convertToFormat(QImage::Format_ARGB32));
    }
#else
    clipboard->setImage(image.convertToFormat(QImage::Format_ARGB32));
#endif
    lastSaveSucceeded_ = true;
    inProgress_ = false;
    progress_ = 1.0;
    statusMessage_ = tr("Copied to clipboard");
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
