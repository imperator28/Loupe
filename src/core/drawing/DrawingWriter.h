#pragma once

#include "core/drawing/DrawingContour.h"

#include <filesystem>
#include <stdexcept>
#include <string>

// Shared contract for the vector writers. Each writer takes the same
// millimetre-space Drawing and is responsible for exactly one thing beyond
// serialising it: getting its format's page conventions right, above all the
// Y-axis direction. DXF model space is Y-up; SVG and PDF are Y-down.
namespace loupe::drawing {

struct DrawingWriteOptions final {
    // Blank border around the geometry. Purely cosmetic; it never scales.
    double marginMm{2.0};
    // Emit a known-length reference line on its own non-cut layer. Cheap
    // insurance: if the operator measures it and gets the stated length, the
    // whole file's scale is right. Worth switching on for DXF R12 in
    // particular, which has no way to declare its units.
    bool includeScaleFiducial{false};
    double fiducialLengthMm{50.0};
};

struct DrawingWriteResult final {
    bool written{};
    double pageWidthMm{};
    double pageHeightMm{};
    int contoursWritten{};
};

class DrawingWriteError : public std::runtime_error {
public:
    enum class Code { EmptyDrawing, UnwritableDestination, EncodingFailed };

    DrawingWriteError(Code code, std::string message);
    [[nodiscard]] Code code() const noexcept { return code_; }

private:
    Code code_;
};

// Normalise a drawing for output: optionally append the fiducial, then shift the
// whole thing so its lower-left corner sits at the margin. Returns the prepared
// drawing and the resulting page size, so every writer derives its page the same
// way and cannot disagree about scale.
struct PreparedDrawing final {
    Drawing drawing;
    double pageWidthMm{};
    double pageHeightMm{};
};

[[nodiscard]] PreparedDrawing prepareForOutput(const Drawing& drawing, const DrawingWriteOptions& options);

// Locale-independent fixed-notation number, trailing zeros trimmed.
//
// Locale matters more than it looks: a comma-decimal locale would turn every
// coordinate into "1,5", which silently corrupts every output file. std::format
// is locale-independent by contract, unlike printf or an imbued stream.
[[nodiscard]] std::string formatNumber(double value, int decimals = 6);

} // namespace loupe::drawing
