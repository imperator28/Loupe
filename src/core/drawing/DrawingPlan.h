#pragma once

#include "core/drawing/DrawingFormat.h"
#include "core/units/UnitPolicy.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// Reviewed plan for a batch of 2D drawings.
//
// A deliberate parallel to ExportPlan rather than an extension of it. A drawing row
// carries a view direction, a content mode and a scale, and needs none of STEP's
// unit rebasing or assembly-versus-local coordinates; folding both into one type
// would force each validator to reason about the other's concerns. The naming and
// collision rules that genuinely are common live in export/OutputNaming.h and are
// shared, not copied.
//
// Kept free of OpenCASCADE types, as ExportPlan is, so plan construction and
// validation are testable without any geometry kernel. The projector converts these
// plain vectors into gp_Dir at its own boundary.
namespace loupe::drawing {

struct Vector3 final {
    double x{};
    double y{};
    double z{};

    bool operator==(const Vector3&) const = default;
};

// Mirrors the projector's ContentMode. Duplicated deliberately: the projector's
// header pulls in OpenCASCADE, and the plan layer stays free of it.
enum class DrawingContent { OuterContourOnly, CutContours, TechnicalView };

// One drawing the user has configured and queued. The view direction is stored as a
// resolved vector rather than a reference to camera state, so a queued drawing
// cannot change meaning when the camera later moves.
struct DrawingSelection final {
    std::string drawingId;
    std::string nodeId;
    std::string viewLabel;
    Vector3 viewDirection{0.0, 0.0, 1.0};
    Vector3 upDirection{0.0, 1.0, 0.0};
    DrawingContent content{DrawingContent::CutContours};
    // Expressed as a ratio so 1:1 is exact rather than a floating-point 1.0.
    int scaleNumerator{1};
    int scaleDenominator{1};

    bool operator==(const DrawingSelection&) const = default;
};

struct DrawingPlanRequest final {
    std::vector<DrawingSelection> selections;
    // Required: node IDs are opaque, so a caller-supplied path is the only source of
    // a meaningful output name.
    std::unordered_map<std::string, std::string> hierarchyPaths;
    // Optional reviewed leaf names, without extension, keyed by drawing ID.
    std::unordered_map<std::string, std::string> outputLeafNames;
    std::string destination;
    DrawingFormat format{DrawingFormat::Dxf};
    double deflectionMm{0.01};
    bool includeScaleFiducial{false};
    units::UnitDecision unitDecision{units::LengthUnit::Millimeter,
                                     units::UnitConfidence::Confirmed,
                                     1.0,
                                     "default millimeter drawing"};
};

class DrawingPlan;

class DrawingOutputRow final {
public:
    [[nodiscard]] const std::string& drawingId() const& noexcept { return drawingId_; }
    [[nodiscard]] const std::string& nodeId() const& noexcept { return nodeId_; }
    [[nodiscard]] const std::string& hierarchyPath() const& noexcept { return hierarchyPath_; }
    [[nodiscard]] const std::string& finalPath() const& noexcept { return finalPath_; }
    [[nodiscard]] const std::string& viewLabel() const& noexcept { return viewLabel_; }
    [[nodiscard]] Vector3 viewDirection() const noexcept { return viewDirection_; }
    [[nodiscard]] Vector3 upDirection() const noexcept { return upDirection_; }
    [[nodiscard]] DrawingContent content() const noexcept { return content_; }
    [[nodiscard]] DrawingFormat format() const noexcept { return format_; }
    [[nodiscard]] double scale() const noexcept { return scale_; }
    [[nodiscard]] double sourceToMillimeters() const noexcept { return sourceToMillimeters_; }
    // True when the generated name collided and a sequence number was appended. Surfaced so
    // the workspace can ask the user to check it: an auto-numbered name is a guess about
    // which drawing is which, and only the user knows if it reads right.
    [[nodiscard]] bool autoNumbered() const noexcept { return autoNumbered_; }

    // Rvalue overloads deleted so a reference cannot outlive a temporary plan.
    [[nodiscard]] const std::string& drawingId() const&& = delete;
    [[nodiscard]] const std::string& nodeId() const&& = delete;
    [[nodiscard]] const std::string& hierarchyPath() const&& = delete;
    [[nodiscard]] const std::string& finalPath() const&& = delete;
    [[nodiscard]] const std::string& viewLabel() const&& = delete;

    bool operator==(const DrawingOutputRow&) const = default;

private:
    DrawingOutputRow() = default;

    std::string drawingId_;
    std::string nodeId_;
    std::string hierarchyPath_;
    std::string finalPath_;
    std::string viewLabel_;
    Vector3 viewDirection_{};
    Vector3 upDirection_{};
    DrawingContent content_{};
    DrawingFormat format_{};
    double scale_{1.0};
    double sourceToMillimeters_{1.0};
    bool autoNumbered_{};

    // Only the builder may produce a row, so an unvalidated one cannot exist.
    friend DrawingPlan buildDrawingPlan(const DrawingPlanRequest& request);
};

class DrawingPlan final {
public:
    [[nodiscard]] const std::vector<DrawingOutputRow>& outputs() const& noexcept { return outputs_; }
    [[nodiscard]] const std::vector<DrawingOutputRow>& outputs() const&& = delete;
    [[nodiscard]] const std::string& fingerprint() const& noexcept { return fingerprint_; }
    [[nodiscard]] const std::string& fingerprint() const&& = delete;

private:
    DrawingPlan() = default;

    std::vector<DrawingOutputRow> outputs_;
    std::string fingerprint_;

    friend DrawingPlan buildDrawingPlan(const DrawingPlanRequest& request);
};

class DrawingPlanError : public std::runtime_error {
public:
    enum class Code {
        EmptySelection,
        BlankDestination,
        MissingHierarchyPath,
        OutputPathCollision,
        UnsafeOutputName,
        UnitDecisionBlocksExport,
        InvalidScale,
        InvalidSourceScale,
        InvalidTolerance,
        DegenerateView,
        DuplicateDrawingId,
        InvalidEnumValue,
    };

    DrawingPlanError(Code code, std::string message);
    [[nodiscard]] Code code() const noexcept { return code_; }

private:
    Code code_;
};

// Throws DrawingPlanError when the requested batch cannot be reviewed safely.
[[nodiscard]] DrawingPlan buildDrawingPlan(const DrawingPlanRequest& request);

// Exposed for the workspace, which shows the generated name before export.
[[nodiscard]] std::string drawingLeafName(const DrawingSelection& selection,
                                          std::string_view hierarchyPath);

} // namespace loupe::drawing
