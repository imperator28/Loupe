#pragma once

#include "core/drawing/DrawingWriter.h"

#include <filesystem>
#include <string>

namespace loupe::drawing {

// SVG 1.1 writer.
//
// Emits physical dimensions in millimetres together with a matching viewBox, so
// one user unit is exactly one millimetre and the scale is unambiguous per spec.
// Hand-written rather than routed through QSvgGenerator, which caps coordinates
// at six significant digits, takes an integer page size, and needs a
// QGuiApplication the headless worker does not have.
class SvgWriter final {
public:
    [[nodiscard]] DrawingWriteResult write(const Drawing& drawing, const std::filesystem::path& destination,
                                           const DrawingWriteOptions& options = {}) const;

    // Exposed for testing without touching the filesystem.
    [[nodiscard]] std::string serialize(const Drawing& drawing, const DrawingWriteOptions& options,
                                        DrawingWriteResult& result) const;
};

} // namespace loupe::drawing
