#pragma once

#include <filesystem>
#include <string>
#include <vector>

// Reopens a written drawing and checks that the file itself declares the size it
// was supposed to. Mirrors the role OutputValidator plays for 3D export: nothing
// is reported as succeeded until it has been read back.
//
// This is the last line of defence for the one failure that costs real money --
// a file that is the wrong physical size. It deliberately re-parses the file
// rather than trusting the writer's own return value, because a writer that
// miscomputed its page would also miscompute its report.
namespace loupe::drawing {

enum class DrawingFormat { Dxf, Svg, Pdf };

struct DrawingValidationIssue final {
    std::string code;
    std::string message;
};

struct DrawingValidationResult final {
    std::filesystem::path path;
    bool reopened{};
    bool passed{};
    DrawingFormat format{DrawingFormat::Dxf};
    double declaredWidthMm{};
    double declaredHeightMm{};
    std::vector<DrawingValidationIssue> errors;
    std::vector<DrawingValidationIssue> warnings;
};

struct ExpectedDrawing final {
    std::filesystem::path path;
    double widthMm{};
    double heightMm{};
    // Absolute tolerance in millimetres. Generous enough to absorb the decimal
    // rounding each text format applies, tight enough that a real scale error
    // -- the smallest of which would be a percent or more -- cannot hide.
    double toleranceMm{0.01};
};

[[nodiscard]] DrawingValidationResult validateDrawing(const ExpectedDrawing& expected);

} // namespace loupe::drawing
