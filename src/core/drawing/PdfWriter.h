#pragma once

#include "core/drawing/DrawingWriter.h"

#include <filesystem>
#include <string>

namespace loupe::drawing {

// Minimal PDF 1.4 writer.
//
// PDF is the most reliably 1:1 of the three formats because its unit is fixed by
// the spec at exactly 1/72 inch, so there is no viewBox, DPI, or units-header
// ambiguity for a consumer to get wrong: printing at 100% gives true size, which
// makes this the format of choice for a print-and-tape-to-stock template.
//
// Hand-written rather than via QPdfWriter -- which is otherwise a good fit and
// lives in Qt Gui rather than the uninstalled PrintSupport module -- because it
// requires a QGuiApplication, and that would force drawing output out of the
// headless worker where all other geometry work runs.
//
// Note PDF user space has its origin at the lower left with y increasing UPWARD,
// the same as DXF model space. Only SVG is y-down. This writer therefore does not
// mirror.
class PdfWriter final {
public:
    [[nodiscard]] DrawingWriteResult write(const Drawing& drawing, const std::filesystem::path& destination,
                                           const DrawingWriteOptions& options = {}) const;

    [[nodiscard]] std::string serialize(const Drawing& drawing, const DrawingWriteOptions& options,
                                        DrawingWriteResult& result) const;
};

} // namespace loupe::drawing
