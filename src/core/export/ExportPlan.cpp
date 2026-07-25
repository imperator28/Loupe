#include "core/export/ExportPlan.h"
#include "core/export/OutputNaming.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>
#include <set>
#include <string_view>

#include <QByteArrayView>
#include <QString>
#include <QStringDecoder>

#include <xxhash.h>

namespace loupe::exporting {
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

// The naming rules are shared with the drawing plan; this maps their neutral error
// onto this plan's own codes so buildPlan's call sites stay unchanged.
[[nodiscard]] std::string sanitizedLeaf(const std::string_view hierarchyPath)
{
    try {
        return detail::sanitizedLeaf(hierarchyPath);
    } catch (const detail::OutputNamingError& error) {
        throw PlanError(PlanError::Code::UnsafeOutputName, error.what());
    }
}

[[nodiscard]] std::u16string windowsComparablePath(std::string path)
{
    try {
        return detail::windowsComparablePath(std::move(path));
    } catch (const detail::OutputNamingError& error) {
        throw PlanError(PlanError::Code::UnsafeOutputName, error.what());
    }
}

[[nodiscard]] bool isValid(const SelectionKind value)
{
    return value == SelectionKind::Subassembly || value == SelectionKind::Occurrence
        || value == SelectionKind::Definition || value == SelectionKind::Body;
}
[[nodiscard]] bool isValid(const Format value) { return value == Format::Stl || value == Format::Step; }
[[nodiscard]] bool isValid(const Coordinates value)
{
    return value == Coordinates::Local || value == Coordinates::Assembly;
}
[[nodiscard]] bool isValid(const Grouping value)
{
    return value == Grouping::SeparateFiles || value == Grouping::PreserveAssembly
        || value == Grouping::FlattenMultiBody;
}
[[nodiscard]] bool isValid(const StepOutputUnit value)
{
    return value == StepOutputUnit::Requested || value == StepOutputUnit::Millimeter || value == StepOutputUnit::Inch;
}

[[nodiscard]] StepOutputUnit concreteStepUnit(const PlanRequest& request)
{
    if (request.stepOutputUnit == StepOutputUnit::Millimeter || request.stepOutputUnit == StepOutputUnit::Inch) {
        return request.stepOutputUnit;
    }
    if (std::abs(request.requestedUnitToMillimeters - 1.0) < 1.0e-12) return StepOutputUnit::Millimeter;
    if (std::abs(request.requestedUnitToMillimeters - 25.4) < 1.0e-12) return StepOutputUnit::Inch;
    throw PlanError(PlanError::Code::InvalidRequestedUnit,
                    "requested STEP output unit must be millimeter or inch");
}

[[nodiscard]] double millimetersPerStepUnit(const StepOutputUnit unit)
{
    switch (unit) {
    case StepOutputUnit::Millimeter: return 1.0;
    case StepOutputUnit::Inch: return 25.4;
    case StepOutputUnit::Requested: break;
    }
    throw PlanError(PlanError::Code::InvalidRequestedUnit, "STEP output unit was not resolved");
}

[[nodiscard]] const char* extension(const Format format)
{
    return format == Format::Stl ? ".stl" : ".step";
}

void appendFingerprintField(std::string& input, const std::string_view value)
{
    input.append(std::to_string(value.size()));
    input.push_back(':');
    input.append(value);
    input.push_back('\n');
}

[[nodiscard]] std::string fingerprint(const std::vector<OutputRow>& outputs)
{
    std::string input;
    for (const auto& output : outputs) {
        appendFingerprintField(input, output.nodeId());
        appendFingerprintField(input, output.hierarchyPath());
        appendFingerprintField(input, output.finalPath());
        appendFingerprintField(input, std::to_string(static_cast<int>(output.selectionKind())));
        appendFingerprintField(input, std::to_string(static_cast<int>(output.format())));
        appendFingerprintField(input, std::to_string(static_cast<int>(output.coordinates())));
        appendFingerprintField(input, std::to_string(static_cast<int>(output.grouping())));
        appendFingerprintField(input, std::to_string(static_cast<int>(output.stepOutputUnit())));
        appendFingerprintField(input, std::format("{:.17g}", output.sourceToOutputScale()));
    }

    const auto hash = XXH3_128bits(input.data(), input.size());
    return std::format("{:016x}{:016x}", hash.high64, hash.low64);
}

} // namespace

void SelectionDraft::highlight(std::string nodeId)
{
    highlightedNodeId_ = std::move(nodeId);
}

const std::optional<std::string>& SelectionDraft::highlightedNodeId() const & noexcept
{
    return highlightedNodeId_;
}

void SelectionDraft::setChecked(CheckedSelection selection, const bool checked)
{
    if (!isValid(selection.kind)) {
        throw PlanError(PlanError::Code::InvalidEnumValue, "selection kind is invalid");
    }
    const auto existing = std::ranges::find(checkedSelections_, selection);
    if (checked && existing == checkedSelections_.end()) {
        checkedSelections_.push_back(std::move(selection));
    } else if (!checked && existing != checkedSelections_.end()) {
        checkedSelections_.erase(existing);
    }
}

const std::vector<CheckedSelection>& SelectionDraft::checkedSelections() const & noexcept
{
    return checkedSelections_;
}

OutputRow::OutputRow(std::string nodeId,
                     std::string hierarchyPath,
                     std::string finalPath,
                     const SelectionKind selectionKind,
                     const Format format,
                     const Coordinates coordinates,
                     const Grouping grouping,
                     const StepOutputUnit stepOutputUnit,
                     const double sourceToOutputScale)
    : nodeId_(std::move(nodeId))
    , hierarchyPath_(std::move(hierarchyPath))
    , finalPath_(std::move(finalPath))
    , selectionKind_(selectionKind)
    , format_(format)
    , coordinates_(coordinates)
    , grouping_(grouping)
    , stepOutputUnit_(stepOutputUnit)
    , sourceToOutputScale_(sourceToOutputScale)
{
}

const std::string& OutputRow::nodeId() const & noexcept { return nodeId_; }
const std::string& OutputRow::hierarchyPath() const & noexcept { return hierarchyPath_; }
const std::string& OutputRow::finalPath() const & noexcept { return finalPath_; }
SelectionKind OutputRow::selectionKind() const noexcept { return selectionKind_; }
Format OutputRow::format() const noexcept { return format_; }
Coordinates OutputRow::coordinates() const noexcept { return coordinates_; }
Grouping OutputRow::grouping() const noexcept { return grouping_; }
StepOutputUnit OutputRow::stepOutputUnit() const noexcept { return stepOutputUnit_; }
double OutputRow::sourceToOutputScale() const noexcept { return sourceToOutputScale_; }

ExportPlan::ExportPlan(std::vector<OutputRow> outputs, std::string fingerprint)
    : outputs_(std::move(outputs))
    , fingerprint_(std::move(fingerprint))
{
}

const std::vector<OutputRow>& ExportPlan::outputs() const & noexcept { return outputs_; }
const std::string& ExportPlan::fingerprint() const & noexcept { return fingerprint_; }

PlanError::PlanError(const Code code, std::string message)
    : std::runtime_error(message)
    , code_(code)
{
}

PlanError::Code PlanError::code() const noexcept { return code_; }

ExportPlan buildPlan(const PlanRequest& request)
{
    if (!isValid(request.format) || !isValid(request.coordinates) || !isValid(request.grouping)
        || !isValid(request.stepOutputUnit)) {
        throw PlanError(PlanError::Code::InvalidEnumValue, "export request contains an invalid enum value");
    }
    if (request.selections.empty()) {
        throw PlanError(PlanError::Code::EmptySelection, "choose at least one export selection");
    }
    if (isBlank(request.destination)) {
        throw PlanError(PlanError::Code::BlankDestination, "choose an export destination");
    }
    if (request.unitDecision.blocksExport()) {
        throw PlanError(PlanError::Code::UnitDecisionBlocksExport, "resolve the reviewed unit decision before export");
    }
    if (!std::isfinite(request.requestedUnitToMillimeters) || request.requestedUnitToMillimeters <= 0.0) {
        throw PlanError(PlanError::Code::InvalidRequestedUnit, "requested output unit must be positive");
    }
    if (!std::isfinite(request.unitDecision.sourceToMillimeters) || request.unitDecision.sourceToMillimeters <= 0.0) {
        throw PlanError(PlanError::Code::InvalidSourceScale, "source-to-millimeter scale must be positive");
    }
    if (request.coordinates == Coordinates::Local
        && std::ranges::any_of(request.selections, [](const CheckedSelection& selection) {
               return selection.kind == SelectionKind::Occurrence;
           })) {
        throw PlanError(PlanError::Code::AmbiguousOccurrenceCoordinates,
                        "local coordinates are ambiguous for occurrences");
    }
    if (request.grouping != Grouping::SeparateFiles) {
        throw PlanError(PlanError::Code::IncompatibleGrouping,
                        "selected exporters currently support separate-files grouping only");
    }
    if (request.grouping == Grouping::PreserveAssembly
        && std::ranges::any_of(request.selections, [](const CheckedSelection& selection) {
               return selection.kind != SelectionKind::Subassembly;
           })) {
        throw PlanError(PlanError::Code::IncompatibleGrouping,
                        "preserve-assembly requires subassembly selections");
    }
    if (request.grouping == Grouping::FlattenMultiBody
        && std::ranges::any_of(request.selections, [](const CheckedSelection& selection) {
               return selection.kind != SelectionKind::Body;
           })) {
        throw PlanError(PlanError::Code::IncompatibleGrouping,
                        "flatten-multi-body requires body selections");
    }

    struct ResolvedSelection {
        CheckedSelection selection;
        std::string hierarchyPath;
    };
    std::vector<ResolvedSelection> resolved;
    resolved.reserve(request.selections.size());
    for (const auto& selection : request.selections) {
        if (!isValid(selection.kind)) {
            throw PlanError(PlanError::Code::InvalidEnumValue, "selection kind is invalid");
        }
        const auto path = request.hierarchyPaths.find(selection.nodeId);
        if (path == request.hierarchyPaths.end() || path->second.empty()) {
            throw PlanError(PlanError::Code::MissingHierarchyPath,
                            "every selection needs a canonical hierarchy path");
        }
        resolved.push_back({selection, path->second});
    }
    std::ranges::sort(resolved, {}, &ResolvedSelection::hierarchyPath);

    const std::string destination = normalizedDestination(request.destination);
    const StepOutputUnit effectiveUnit = request.format == Format::Stl ? StepOutputUnit::Millimeter
                                                                         : concreteStepUnit(request);
    const double scale = request.format == Format::Stl
        ? request.unitDecision.sourceToMillimeters
        : request.unitDecision.sourceToMillimeters / millimetersPerStepUnit(effectiveUnit);
    std::set<std::u16string> finalPaths;
    std::vector<OutputRow> outputs;
    outputs.reserve(resolved.size());
    for (const auto& item : resolved) {
        const auto reviewedName = request.outputLeafNames.find(item.selection.nodeId);
        const auto outputLeaf = reviewedName == request.outputLeafNames.end()
            ? sanitizedLeaf(item.hierarchyPath)
            : sanitizedLeaf(reviewedName->second);
        OutputRow row(item.selection.nodeId,
                      item.hierarchyPath,
                      destination + "/" + outputLeaf + extension(request.format),
                      item.selection.kind,
                      request.format,
                      request.coordinates,
                      request.grouping,
                      effectiveUnit,
                      scale);
        if (!finalPaths.insert(windowsComparablePath(row.finalPath())).second) {
            throw PlanError(PlanError::Code::OutputPathCollision,
                            "multiple selections resolve to the same output path");
        }
        outputs.push_back(std::move(row));
    }
    const std::string planFingerprint = fingerprint(outputs);
    return ExportPlan(std::move(outputs), planFingerprint);
}

} // namespace loupe::exporting
