#include "core/units/UnitPolicy.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace loupe::units {
namespace {

// One known unit across every representation context. The XCAF unit is deliberately not
// compared: OCCT normalises transferred geometry into its own working unit (millimetres
// by default), so an inch file always reads back with a millimetre XCAF unit. Treating
// that as a conflict sent every inch file down the unresolved path with a 1.0 factor,
// which the viewer happened to survive and export did not -- parts came out 25.4x small.
[[nodiscard]] std::optional<LengthUnit> coherentDeclaredUnit(const std::vector<LengthUnit>& declaredUnits)
{
    if (declaredUnits.empty()) return std::nullopt;
    const auto first = declaredUnits.front();
    if (first != LengthUnit::Millimeter && first != LengthUnit::Inch) return std::nullopt;
    if (!std::ranges::all_of(declaredUnits, [first](const LengthUnit unit) { return unit == first; })) return std::nullopt;
    return first;
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

    const auto declared = coherentDeclaredUnit(evidence.declaredRepresentationUnits);
    if (!declared || evidence.internallyInconsistent) {
        // sourceUnit() is millimetres here, so a 1.0 factor keeps OCCT's per-context
        // conversion -- the best available reading -- while review is still required.
        return {LengthUnit::Millimeter,
                UnitConfidence::MissingOrMixed,
                1.0,
                "missing, mixed, or inconsistent STEP unit evidence"};
    }

    const bool implausibleExtent = !std::isfinite(evidence.normalizedLongestExtentMm)
        || evidence.normalizedLongestExtentMm < 10.0
        || evidence.normalizedLongestExtentMm > 100000.0;
    return {*declared,
            implausibleExtent ? UnitConfidence::Suspicious : UnitConfidence::Confirmed,
            millimetersPerUnit(*declared),
            implausibleExtent ? "normalized extent crossed conservative unit-review threshold"
                             : "coherent declared STEP units"};
}

LengthUnit sourceUnit(const UnitEvidence& evidence) noexcept
{
    if (evidence.internallyInconsistent) return LengthUnit::Millimeter;
    return coherentDeclaredUnit(evidence.declaredRepresentationUnits).value_or(LengthUnit::Millimeter);
}

} // namespace loupe::units
