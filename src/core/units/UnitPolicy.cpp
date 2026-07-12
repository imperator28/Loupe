#include "core/units/UnitPolicy.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace loupe::units {
namespace {

[[nodiscard]] bool hasInvalidDeclaredUnits(
    const std::vector<LengthUnit>& declaredUnits,
    const LengthUnit xcafUnit)
{
    return std::ranges::any_of(declaredUnits, [xcafUnit](const LengthUnit unit) {
        return unit == LengthUnit::Unknown || unit == LengthUnit::Mixed || unit != xcafUnit;
    });
}

} // namespace

double millimetersPerUnit(const LengthUnit unit)
{
    switch (unit) {
    case LengthUnit::Millimeter:
        return 1.0;
    case LengthUnit::Inch:
        return 25.4;
    case LengthUnit::Unknown:
    case LengthUnit::Mixed:
        throw std::invalid_argument("unit has no single scale");
    }

    throw std::invalid_argument("unhandled unit");
}

UnitDecision decide(const UnitEvidence& evidence, const std::optional<UnitOverride>& overrideValue)
{
    if (overrideValue.has_value()) {
        if (!std::isfinite(overrideValue->customFactor) || overrideValue->customFactor <= 0.0) {
            throw std::invalid_argument("unit override factor must be finite and positive");
        }

        return {overrideValue->interpretAs,
                UnitConfidence::UserOverride,
                millimetersPerUnit(overrideValue->interpretAs) * overrideValue->customFactor,
                overrideValue->reason};
    }

    if (evidence.xcafUnit == LengthUnit::Unknown || evidence.xcafUnit == LengthUnit::Mixed
        || evidence.declaredRepresentationUnits.empty()
        || hasInvalidDeclaredUnits(evidence.declaredRepresentationUnits, evidence.xcafUnit)
        || evidence.internallyInconsistent) {
        return {evidence.xcafUnit,
                UnitConfidence::MissingOrMixed,
                1.0,
                "missing, mixed, or inconsistent STEP unit evidence"};
    }

    const bool implausibleExtent = !std::isfinite(evidence.normalizedLongestExtentMm)
        || evidence.normalizedLongestExtentMm < 10.0
        || evidence.normalizedLongestExtentMm > 100000.0;
    return {evidence.xcafUnit,
            implausibleExtent ? UnitConfidence::Suspicious : UnitConfidence::Confirmed,
            millimetersPerUnit(evidence.xcafUnit),
            implausibleExtent ? "normalized extent crossed conservative unit-review threshold"
                             : "coherent STEP and XCAF unit evidence"};
}

} // namespace loupe::units
