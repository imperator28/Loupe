#pragma once

#include <optional>
#include <string>
#include <vector>

namespace loupe::units {

enum class LengthUnit { Millimeter, Inch, Unknown, Mixed };
enum class UnitConfidence { Confirmed, Suspicious, MissingOrMixed, UserOverride };

struct UnitEvidence {
    std::vector<LengthUnit> declaredRepresentationUnits;
    LengthUnit xcafUnit{LengthUnit::Unknown};
    double normalizedLongestExtentMm{};
    bool internallyInconsistent{};
};

struct UnitOverride {
    LengthUnit interpretAs{LengthUnit::Unknown};
    double customFactor{1.0};
    std::string reason;
};

struct UnitDecision {
    LengthUnit effectiveUnit{LengthUnit::Unknown};
    UnitConfidence confidence{UnitConfidence::MissingOrMixed};
    double sourceToMillimeters{1.0};
    std::string reason;

    [[nodiscard]] bool blocksExport() const noexcept
    {
        return confidence == UnitConfidence::MissingOrMixed || confidence == UnitConfidence::Suspicious;
    }
};

// Throws std::invalid_argument when an override does not specify a concrete unit or a finite positive factor.
[[nodiscard]] UnitDecision decide(
    const UnitEvidence& evidence,
    const std::optional<UnitOverride>& overrideValue);
[[nodiscard]] double millimetersPerUnit(LengthUnit unit);

// The unit the raw numbers in the STEP file are written in: the one unit every
// representation context declares, or millimetres when the declarations are missing
// or disagree. OCCT converts each context into its own working unit while reading, so
// this -- not the XCAF document unit -- is what a reviewed source factor applies to.
[[nodiscard]] LengthUnit sourceUnit(const UnitEvidence& evidence) noexcept;

} // namespace loupe::units
