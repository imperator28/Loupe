#include "core/drawing/DrawingValidator.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <optional>
#include <regex>
#include <sstream>

namespace loupe::drawing {
namespace {

constexpr double millimetresPerPoint = 25.4 / 72.0;

[[nodiscard]] std::optional<DrawingFormat> formatFor(const std::filesystem::path& path)
{
    auto extension = path.extension().string();
    std::ranges::transform(extension, extension.begin(),
                           [](const unsigned char value) { return static_cast<char>(std::tolower(value)); });
    if (extension == ".dxf") return DrawingFormat::Dxf;
    if (extension == ".svg") return DrawingFormat::Svg;
    if (extension == ".pdf") return DrawingFormat::Pdf;
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> readFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) return std::nullopt;
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

// Read a DXF header variable's paired code value, e.g. $EXTMIN's code 10 and 20.
[[nodiscard]] std::optional<double> dxfHeaderValue(const std::string& text, const std::string& variable,
                                                  const std::string& code)
{
    const auto variablePosition = text.find(variable);
    if (variablePosition == std::string::npos) return std::nullopt;
    const std::string needle = "\r\n" + code + "\r\n";
    const auto codePosition = text.find(needle, variablePosition);
    if (codePosition == std::string::npos) return std::nullopt;
    const auto valueStart = codePosition + needle.size();
    const auto valueEnd = text.find("\r\n", valueStart);
    if (valueEnd == std::string::npos) return std::nullopt;
    try {
        return std::stod(text.substr(valueStart, valueEnd - valueStart));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

void validateDxf(const std::string& text, DrawingValidationResult& result)
{
    if (text.find("AC1009") == std::string::npos) {
        result.warnings.push_back({"unexpected_dxf_version", "the file does not declare DXF R12 (AC1009)"});
    }
    if (text.find("\r\nEOF\r\n") == std::string::npos) {
        result.errors.push_back({"truncated_file", "the DXF has no EOF marker"});
    }
    const auto minX = dxfHeaderValue(text, "$EXTMIN", "10");
    const auto minY = dxfHeaderValue(text, "$EXTMIN", "20");
    const auto maxX = dxfHeaderValue(text, "$EXTMAX", "10");
    const auto maxY = dxfHeaderValue(text, "$EXTMAX", "20");
    if (!minX || !minY || !maxX || !maxY) {
        result.errors.push_back({"missing_extents", "the DXF does not declare $EXTMIN and $EXTMAX"});
        return;
    }
    // R12 has no units header, so the extents are unitless by construction and
    // "one drawing unit is one millimetre" is the documented convention. The
    // width and height are still exactly checkable.
    result.declaredWidthMm = *maxX - *minX;
    result.declaredHeightMm = *maxY - *minY;
}

void validateSvg(const std::string& text, DrawingValidationResult& result)
{
    const std::regex widthPattern(R"(width="([\d.]+)mm")");
    const std::regex heightPattern(R"(height="([\d.]+)mm")");
    // Custom delimiter: the default R"( ... )" would end early here, because the
    // pattern itself contains the )" sequence.
    const std::regex viewBoxPattern(R"re(viewBox="0 0 ([\d.]+) ([\d.]+)")re");
    std::smatch width;
    std::smatch height;
    std::smatch viewBox;

    const bool hasWidth = std::regex_search(text, width, widthPattern);
    const bool hasHeight = std::regex_search(text, height, heightPattern);
    if (!hasWidth || !hasHeight) {
        result.errors.push_back({"missing_physical_size",
                                 "the SVG does not declare width and height in millimetres"});
        return;
    }
    result.declaredWidthMm = std::stod(width[1]);
    result.declaredHeightMm = std::stod(height[1]);

    if (!std::regex_search(text, viewBox, viewBoxPattern)) {
        // Without a viewBox the millimetre size is still declared, but consumers
        // that key off user units will silently assume 96 dpi.
        result.errors.push_back({"missing_viewbox", "the SVG has no viewBox to anchor its user units"});
        return;
    }
    // The two must agree or one user unit is not one millimetre.
    if (std::abs(std::stod(viewBox[1]) - result.declaredWidthMm) > 1.0e-6
        || std::abs(std::stod(viewBox[2]) - result.declaredHeightMm) > 1.0e-6) {
        result.errors.push_back({"viewbox_scale_mismatch",
                                 "the SVG viewBox does not match its millimetre size, so one user "
                                 "unit is not one millimetre"});
    }
}

void validatePdf(const std::string& text, DrawingValidationResult& result)
{
    if (!text.starts_with("%PDF-")) {
        result.errors.push_back({"not_a_pdf", "the file does not begin with a PDF header"});
        return;
    }
    if (text.find("%%EOF") == std::string::npos) {
        result.errors.push_back({"truncated_file", "the PDF has no EOF marker"});
    }
    const std::regex boxPattern(R"(/MediaBox \[0 0 ([\d.]+) ([\d.]+)\])");
    std::smatch box;
    if (!std::regex_search(text, box, boxPattern)) {
        result.errors.push_back({"missing_mediabox", "the PDF does not declare a MediaBox"});
        return;
    }
    // PDF's unit is exactly 1/72 inch by specification, so this conversion is
    // exact rather than a convention.
    result.declaredWidthMm = std::stod(box[1]) * millimetresPerPoint;
    result.declaredHeightMm = std::stod(box[2]) * millimetresPerPoint;

    const std::regex lengthPattern(R"(/Length (\d+) >>\s*stream)");
    std::smatch length;
    if (std::regex_search(text, length, lengthPattern)) {
        const auto declared = static_cast<std::size_t>(std::stoul(length[1]));
        const std::string marker = "stream\n";
        const auto streamStart = text.find(marker, static_cast<std::size_t>(length.position(0)));
        const auto streamEnd = text.find("endstream", streamStart);
        if (streamStart != std::string::npos && streamEnd != std::string::npos
            && streamEnd - (streamStart + marker.size()) != declared) {
            result.errors.push_back({"stream_length_mismatch",
                                     "the PDF content stream length does not match its declaration"});
        }
    }
}

} // namespace

DrawingValidationResult validateDrawing(const ExpectedDrawing& expected)
{
    DrawingValidationResult result;
    result.path = expected.path;

    if (!std::isfinite(expected.toleranceMm) || expected.toleranceMm <= 0.0) {
        result.errors.push_back({"invalid_tolerance", "the validation tolerance must be positive"});
        return result;
    }
    const auto format = formatFor(expected.path);
    if (!format) {
        result.errors.push_back({"unsupported_format", "the output extension is not dxf, svg or pdf"});
        return result;
    }
    result.format = *format;

    const auto text = readFile(expected.path);
    if (!text) {
        result.errors.push_back({"missing_file", "the written drawing could not be reopened"});
        return result;
    }
    result.reopened = true;

    switch (*format) {
    case DrawingFormat::Dxf: validateDxf(*text, result); break;
    case DrawingFormat::Svg: validateSvg(*text, result); break;
    case DrawingFormat::Pdf: validatePdf(*text, result); break;
    }

    if (expected.widthMm > 0.0 && expected.heightMm > 0.0) {
        if (std::abs(result.declaredWidthMm - expected.widthMm) > expected.toleranceMm
            || std::abs(result.declaredHeightMm - expected.heightMm) > expected.toleranceMm) {
            result.errors.push_back({"size_mismatch",
                                     "the written drawing does not declare the expected millimetre size"});
        }
    }
    result.passed = result.reopened && result.errors.empty();
    return result;
}

} // namespace loupe::drawing
