#pragma once

#include "core/units/UnitPolicy.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace loupe::exporting {

enum class SelectionKind { Subassembly, Occurrence, Definition, Body };
enum class Format { Stl, Step };
enum class Coordinates { Local, Assembly };
enum class Grouping { SeparateFiles, PreserveAssembly, FlattenMultiBody };
// Requested is accepted only on PlanRequest; OutputRow always stores Millimeter or Inch.
enum class StepOutputUnit { Requested, Millimeter, Inch };

struct PlanRequest;
class ExportPlan;

struct CheckedSelection {
    std::string nodeId;
    SelectionKind kind{};

    bool operator==(const CheckedSelection&) const = default;
};

class SelectionDraft {
public:
    void highlight(std::string nodeId);
    [[nodiscard]] const std::optional<std::string>& highlightedNodeId() const & noexcept;
    [[nodiscard]] const std::optional<std::string>& highlightedNodeId() const && = delete;

    void setChecked(CheckedSelection selection, bool checked);
    [[nodiscard]] const std::vector<CheckedSelection>& checkedSelections() const & noexcept;
    [[nodiscard]] const std::vector<CheckedSelection>& checkedSelections() const && = delete;

private:
    std::optional<std::string> highlightedNodeId_;
    std::vector<CheckedSelection> checkedSelections_;
};

class OutputRow {
public:
    [[nodiscard]] const std::string& nodeId() const & noexcept;
    [[nodiscard]] const std::string& nodeId() const && = delete;
    [[nodiscard]] const std::string& hierarchyPath() const & noexcept;
    [[nodiscard]] const std::string& hierarchyPath() const && = delete;
    [[nodiscard]] const std::string& finalPath() const & noexcept;
    [[nodiscard]] const std::string& finalPath() const && = delete;
    [[nodiscard]] SelectionKind selectionKind() const noexcept;
    [[nodiscard]] Format format() const noexcept;
    [[nodiscard]] Coordinates coordinates() const noexcept;
    [[nodiscard]] Grouping grouping() const noexcept;
    [[nodiscard]] StepOutputUnit stepOutputUnit() const noexcept;
    [[nodiscard]] double sourceToOutputScale() const noexcept;

    bool operator==(const OutputRow&) const = default;

private:
    OutputRow(std::string nodeId,
              std::string hierarchyPath,
              std::string finalPath,
              SelectionKind selectionKind,
              Format format,
              Coordinates coordinates,
              Grouping grouping,
              StepOutputUnit stepOutputUnit,
              double sourceToOutputScale);

    std::string nodeId_;
    std::string hierarchyPath_;
    std::string finalPath_;
    SelectionKind selectionKind_{};
    Format format_{};
    Coordinates coordinates_{};
    Grouping grouping_{};
    StepOutputUnit stepOutputUnit_{};
    double sourceToOutputScale_{};

    friend ExportPlan buildPlan(const PlanRequest& request);
};

struct PlanRequest {
    std::vector<CheckedSelection> selections;
    // Canonical paths are required because node IDs are opaque and are never used as hierarchy order.
    std::unordered_map<std::string, std::string> hierarchyPaths;
    std::string destination;
    Format format{Format::Step};
    Coordinates coordinates{};
    Grouping grouping{};
    StepOutputUnit stepOutputUnit{};
    double requestedUnitToMillimeters{1.0};
    units::UnitDecision unitDecision{units::LengthUnit::Millimeter,
                                     units::UnitConfidence::Confirmed,
                                     1.0,
                                     "default millimeter export"};
};

class ExportPlan {
public:
    [[nodiscard]] const std::vector<OutputRow>& outputs() const & noexcept;
    [[nodiscard]] const std::vector<OutputRow>& outputs() const && = delete;
    [[nodiscard]] const std::string& fingerprint() const & noexcept;
    [[nodiscard]] const std::string& fingerprint() const && = delete;

private:
    ExportPlan(std::vector<OutputRow> outputs, std::string fingerprint);

    std::vector<OutputRow> outputs_;
    std::string fingerprint_;

    friend ExportPlan buildPlan(const PlanRequest& request);
};

class PlanError : public std::runtime_error {
public:
    enum class Code {
        EmptySelection,
        BlankDestination,
        MissingHierarchyPath,
        OutputPathCollision,
        UnsafeOutputName,
        AmbiguousOccurrenceCoordinates,
        IncompatibleGrouping,
        UnitDecisionBlocksExport,
        InvalidRequestedUnit,
        InvalidSourceScale,
        InvalidEnumValue,
    };

    PlanError(Code code, std::string message);
    [[nodiscard]] Code code() const noexcept;

private:
    Code code_;
};

// Throws PlanError when the requested export cannot be reviewed safely.
[[nodiscard]] ExportPlan buildPlan(const PlanRequest& request);

} // namespace loupe::exporting
