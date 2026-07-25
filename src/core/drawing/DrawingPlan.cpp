#include "core/drawing/DrawingPlan.h"
#include "core/export/OutputNaming.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>
#include <set>
#include <string_view>
#include <utility>

#include <xxhash.h>

namespace loupe::drawing {
namespace {

[[nodiscard]] bool isBlank(const std::string_view value)
{
    return std::ranges::all_of(value, [](const unsigned char character) { return std::isspace(character) != 0; });
}

[[nodiscard]] std::string normalizedDestination(std::string destination)
{
    while (!destination.empty() && (destination.back() == '/' || destination.back() == '\\')) {
        destination.pop_back();
    }
    return destination;
}

[[nodiscard]] bool isValid(const DrawingFormat value)
{
    return value == DrawingFormat::Dxf || value == DrawingFormat::Svg || value == DrawingFormat::Pdf;
}

[[nodiscard]] bool isValid(const DrawingContent value)
{
    return value == DrawingContent::OuterContourOnly || value == DrawingContent::CutContours
        || value == DrawingContent::TechnicalView;
}

[[nodiscard]] std::string_view extensionFor(const DrawingFormat format)
{
    switch (format) {
    case DrawingFormat::Dxf: return ".dxf";
    case DrawingFormat::Svg: return ".svg";
    case DrawingFormat::Pdf: return ".pdf";
    }
    return ".dxf";
}

[[nodiscard]] double magnitude(const Vector3& value)
{
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

[[nodiscard]] double dot(const Vector3& a, const Vector3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Slugify a view label so it is safe inside a filename and reads predictably.
// "Normal to face" becomes "normal-to-face".
[[nodiscard]] std::string slug(const std::string_view label)
{
    std::string result;
    result.reserve(label.size());
    bool pendingSeparator = false;
    for (const unsigned char character : label) {
        if (std::isalnum(character) != 0) {
            if (pendingSeparator && !result.empty()) result.push_back('-');
            pendingSeparator = false;
            result.push_back(static_cast<char>(std::tolower(character)));
        } else {
            pendingSeparator = true;
        }
    }
    return result;
}

void appendFingerprintField(std::string& target, const std::string_view value)
{
    // Length-prefixed, so no combination of field contents can imitate a boundary.
    target += std::format("{}:{}\n", value.size(), value);
}

void appendFingerprintNumber(std::string& target, const double value)
{
    appendFingerprintField(target, std::format("{:.17g}", value));
}

[[nodiscard]] std::string fingerprintOf(const std::vector<DrawingOutputRow>& outputs)
{
    std::string input;
    for (const auto& row : outputs) {
        appendFingerprintField(input, row.drawingId());
        appendFingerprintField(input, row.nodeId());
        appendFingerprintField(input, row.hierarchyPath());
        appendFingerprintField(input, row.finalPath());
        appendFingerprintField(input, row.viewLabel());
        appendFingerprintField(input, std::to_string(static_cast<int>(row.content())));
        appendFingerprintField(input, std::to_string(static_cast<int>(row.format())));
        // Every field that changes the geometry has to be covered, or the worker
        // would accept a plan whose output differs from the one that was reviewed.
        appendFingerprintNumber(input, row.viewDirection().x);
        appendFingerprintNumber(input, row.viewDirection().y);
        appendFingerprintNumber(input, row.viewDirection().z);
        appendFingerprintNumber(input, row.upDirection().x);
        appendFingerprintNumber(input, row.upDirection().y);
        appendFingerprintNumber(input, row.upDirection().z);
        appendFingerprintNumber(input, row.scale());
        appendFingerprintNumber(input, row.sourceToMillimeters());
    }
    const auto digest = XXH3_128bits(input.data(), input.size());
    return std::format("{:016x}{:016x}", digest.high64, digest.low64);
}

[[nodiscard]] std::string sanitize(const std::string_view value)
{
    try {
        return exporting::detail::sanitizedLeaf(value);
    } catch (const exporting::detail::OutputNamingError& error) {
        throw DrawingPlanError(DrawingPlanError::Code::UnsafeOutputName, error.what());
    }
}

} // namespace

DrawingPlanError::DrawingPlanError(const Code code, std::string message)
    : std::runtime_error(std::move(message))
    , code_(code)
{
}

std::string drawingLeafName(const DrawingSelection& selection, const std::string_view hierarchyPath)
{
    const auto separator = hierarchyPath.find_last_of("/\\");
    const std::string_view base =
        separator == std::string_view::npos ? hierarchyPath : hierarchyPath.substr(separator + 1);

    std::string name(base);
    // Disambiguate by view: the same part queued at several angles would otherwise
    // collide, and one part at many angles is the normal case rather than an edge one.
    const auto viewSlug = slug(selection.viewLabel);
    if (!viewSlug.empty()) name += "-" + viewSlug;
    // A non-1:1 drawing must say so in its own filename; discovering it at the cutter
    // is too late.
    if (selection.scaleNumerator != selection.scaleDenominator) {
        name += std::format("-{}to{}", selection.scaleNumerator, selection.scaleDenominator);
    }
    return name;
}

DrawingPlan buildDrawingPlan(const DrawingPlanRequest& request)
{
    if (!isValid(request.format)) {
        throw DrawingPlanError(DrawingPlanError::Code::InvalidEnumValue,
                               "drawing request contains an invalid format");
    }
    if (request.selections.empty()) {
        throw DrawingPlanError(DrawingPlanError::Code::EmptySelection, "queue at least one drawing");
    }
    if (isBlank(request.destination)) {
        throw DrawingPlanError(DrawingPlanError::Code::BlankDestination, "choose an export destination");
    }
    if (request.unitDecision.blocksExport()) {
        throw DrawingPlanError(DrawingPlanError::Code::UnitDecisionBlocksExport,
                               "resolve the reviewed unit decision before exporting a drawing");
    }
    if (!std::isfinite(request.unitDecision.sourceToMillimeters)
        || request.unitDecision.sourceToMillimeters <= 0.0) {
        throw DrawingPlanError(DrawingPlanError::Code::InvalidSourceScale,
                               "source-to-millimetre scale must be positive");
    }
    if (!std::isfinite(request.deflectionMm) || request.deflectionMm <= 0.0) {
        throw DrawingPlanError(DrawingPlanError::Code::InvalidTolerance,
                               "the curve tolerance must be a positive number of millimetres");
    }

    struct Resolved final {
        const DrawingSelection* selection{};
        std::string hierarchyPath;
    };
    std::vector<Resolved> resolved;
    resolved.reserve(request.selections.size());
    std::set<std::string> seenIds;

    for (const auto& selection : request.selections) {
        if (!isValid(selection.content)) {
            throw DrawingPlanError(DrawingPlanError::Code::InvalidEnumValue,
                                   "drawing selection contains an invalid content mode");
        }
        if (selection.drawingId.empty() || !seenIds.insert(selection.drawingId).second) {
            // Rows are addressed by drawing ID when results come back, so a duplicate
            // would make a result ambiguous.
            throw DrawingPlanError(DrawingPlanError::Code::DuplicateDrawingId,
                                   "each queued drawing needs its own identifier");
        }
        if (selection.scaleNumerator <= 0 || selection.scaleDenominator <= 0) {
            throw DrawingPlanError(DrawingPlanError::Code::InvalidScale,
                                   "drawing scale must be a positive ratio");
        }
        const double viewLength = magnitude(selection.viewDirection);
        const double upLength = magnitude(selection.upDirection);
        if (!std::isfinite(viewLength) || !std::isfinite(upLength) || viewLength <= 0.0 || upLength <= 0.0) {
            throw DrawingPlanError(DrawingPlanError::Code::DegenerateView,
                                   "the view and up directions must be non-zero");
        }
        // Parallel directions leave the in-plane axis undefined, and the failure
        // surfaces deep inside the geometry kernel rather than as something a user
        // could act on, so it is rejected here.
        if (std::abs(dot(selection.viewDirection, selection.upDirection) / (viewLength * upLength))
            > 1.0 - 1.0e-9) {
            throw DrawingPlanError(DrawingPlanError::Code::DegenerateView,
                                   "the view direction and the up direction must not be parallel");
        }
        const auto path = request.hierarchyPaths.find(selection.nodeId);
        if (path == request.hierarchyPaths.end() || path->second.empty()) {
            throw DrawingPlanError(DrawingPlanError::Code::MissingHierarchyPath,
                                   "every drawing needs a hierarchy path for its node");
        }
        resolved.push_back({&selection, path->second});
    }

    // Sorted by hierarchy path then drawing ID, so the plan is canonical and its
    // fingerprint does not depend on the order the queue happened to be built in.
    std::ranges::sort(resolved, [](const Resolved& left, const Resolved& right) {
        if (left.hierarchyPath != right.hierarchyPath) return left.hierarchyPath < right.hierarchyPath;
        return left.selection->drawingId < right.selection->drawingId;
    });

    const auto destination = normalizedDestination(request.destination);
    const auto extension = extensionFor(request.format);

    DrawingPlan plan;
    plan.outputs_.reserve(resolved.size());
    std::set<std::u16string> finalPaths;

    for (const auto& item : resolved) {
        const auto& selection = *item.selection;
        const auto reviewed = request.outputLeafNames.find(selection.drawingId);
        const auto leaf = reviewed != request.outputLeafNames.end() && !reviewed->second.empty()
            ? sanitize(reviewed->second)
            : sanitize(drawingLeafName(selection, item.hierarchyPath));

        DrawingOutputRow row;
        row.drawingId_ = selection.drawingId;
        row.nodeId_ = selection.nodeId;
        row.hierarchyPath_ = item.hierarchyPath;
        row.finalPath_ = destination + "/" + leaf + std::string(extension);
        row.viewLabel_ = selection.viewLabel;
        row.viewDirection_ = selection.viewDirection;
        row.upDirection_ = selection.upDirection;
        row.content_ = selection.content;
        row.format_ = request.format;
        row.scale_ = static_cast<double>(selection.scaleNumerator)
            / static_cast<double>(selection.scaleDenominator);
        row.sourceToMillimeters_ = request.unitDecision.sourceToMillimeters;

        std::u16string comparable;
        try {
            comparable = exporting::detail::windowsComparablePath(row.finalPath_);
        } catch (const exporting::detail::OutputNamingError& error) {
            throw DrawingPlanError(DrawingPlanError::Code::UnsafeOutputName, error.what());
        }
        if (!finalPaths.insert(std::move(comparable)).second) {
            throw DrawingPlanError(DrawingPlanError::Code::OutputPathCollision,
                                   "two queued drawings would write to the same file");
        }
        plan.outputs_.push_back(std::move(row));
    }

    plan.fingerprint_ = fingerprintOf(plan.outputs_);
    return plan;
}

} // namespace loupe::drawing
