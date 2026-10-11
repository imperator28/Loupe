#include "app/render/PartPalette.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>
#include <vector>

namespace loupe::app::render {
namespace {

struct Oklab final {
    double l{};
    double a{};
    double b{};
};

struct StyleBand final {
    double lightnessLow{};
    double lightnessHigh{};
    double chroma{};
    double hueDegrees{};
    double hueSpreadDegrees{}; // 360 walks the whole wheel; a few degrees only nudges a shade
};

// Bands chosen to sit around the neutral body colour (OKLab L ~0.70) so an auto-coloured
// assembly reads as calm in both themes, with no part far darker or more saturated than
// its neighbours.
constexpr StyleBand bandFor(const PartPaletteStyle style)
{
    switch (style) {
    case PartPaletteStyle::CoolShades: return {0.60, 0.84, 0.035, 255.0, 16.0};
    case PartPaletteStyle::WarmShades: return {0.62, 0.86, 0.040, 75.0, 16.0};
    case PartPaletteStyle::Muted: return {0.68, 0.79, 0.060, 0.0, 360.0};
    }
    return {0.60, 0.84, 0.035, 255.0, 16.0};
}

constexpr double kGoldenFraction = 0.6180339887498949;

double fractional(const double value) { return value - std::floor(value); }

double linearToSrgb(const double value)
{
    const auto clamped = std::clamp(value, 0.0, 1.0);
    return clamped <= 0.0031308 ? 12.92 * clamped : 1.055 * std::pow(clamped, 1.0 / 2.4) - 0.055;
}

double srgbToLinear(const double value)
{
    return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
}

QColor fromOklab(const Oklab& lab)
{
    const auto l = std::pow(lab.l + 0.3963377774 * lab.a + 0.2158037573 * lab.b, 3.0);
    const auto m = std::pow(lab.l - 0.1055613458 * lab.a - 0.0638541728 * lab.b, 3.0);
    const auto s = std::pow(lab.l - 0.0894841775 * lab.a - 1.2914855480 * lab.b, 3.0);
    return QColor::fromRgbF(static_cast<float>(linearToSrgb(4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s)),
                            static_cast<float>(linearToSrgb(-1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s)),
                            static_cast<float>(linearToSrgb(-0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s)));
}

Oklab toOklab(const QColor& color)
{
    const auto r = srgbToLinear(color.redF());
    const auto g = srgbToLinear(color.greenF());
    const auto b = srgbToLinear(color.blueF());
    const auto l = std::cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
    const auto m = std::cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
    const auto s = std::cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
    return {0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
            1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
            0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s};
}

// FNV-1a over the UTF-16 code units, mixed with the seed. Deliberately not qHash, whose
// output is only stable within one Qt build: a persisted shuffle seed should give the
// same colours after an upgrade.
std::uint64_t stableHash(const QString& key, const std::uint32_t seed)
{
    std::uint64_t hash = 1469598103934665603ULL ^ (static_cast<std::uint64_t>(seed) * 0x9E3779B97F4A7C15ULL);
    for (const auto unit : key) {
        hash ^= unit.unicode();
        hash *= 1099511628211ULL;
    }
    return hash;
}

} // namespace

std::optional<PartPaletteStyle> partPaletteStyleFromName(const QString& name)
{
    if (name == QStringLiteral("shades")) return PartPaletteStyle::CoolShades;
    if (name == QStringLiteral("warm")) return PartPaletteStyle::WarmShades;
    if (name == QStringLiteral("muted")) return PartPaletteStyle::Muted;
    return std::nullopt;
}

QColor partPaletteColor(const PartPaletteStyle style, const int position, const std::uint32_t seed)
{
    const auto band = bandFor(style);
    // A seed-dependent phase, so a shuffle moves colours along the walk as well as
    // reordering who gets which.
    const auto phase = seed == 0 ? 0.5 : fractional(static_cast<double>(stableHash(QStringLiteral("phase"), seed) % 10007) / 10007.0);
    const auto lightnessStep = fractional(phase + position * kGoldenFraction);
    // A second, independent walk for hue. In the shades styles it only nudges the hue a
    // few degrees, which separates two parts that happen to land at similar lightness.
    const auto hueStep = fractional(phase * 0.5 + position * 0.7548776662466927);
    const auto lightness = band.lightnessLow + lightnessStep * (band.lightnessHigh - band.lightnessLow);
    const auto hueDegrees = band.hueSpreadDegrees >= 360.0
        ? 360.0 * fractional(phase + position * kGoldenFraction)
        : band.hueDegrees + (hueStep - 0.5) * band.hueSpreadDegrees;
    const auto hue = hueDegrees * std::numbers::pi / 180.0;
    return fromOklab({lightness, band.chroma * std::cos(hue), band.chroma * std::sin(hue)});
}

QStringList partPaletteOrder(const QStringList& treeOrderedKeys, const std::uint32_t seed)
{
    if (seed == 0) return treeOrderedKeys;
    std::vector<std::pair<std::uint64_t, int>> ranked;
    ranked.reserve(static_cast<std::size_t>(treeOrderedKeys.size()));
    for (int index = 0; index < treeOrderedKeys.size(); ++index)
        ranked.emplace_back(stableHash(treeOrderedKeys.at(index), seed), index);
    std::ranges::sort(ranked);
    QStringList ordered;
    ordered.reserve(treeOrderedKeys.size());
    for (const auto& [hash, index] : ranked) ordered.append(treeOrderedKeys.at(index));
    return ordered;
}

double oklabLightness(const QColor& color) { return toOklab(color).l; }

double oklabDistance(const QColor& first, const QColor& second)
{
    const auto a = toOklab(first);
    const auto b = toOklab(second);
    return std::sqrt((a.l - b.l) * (a.l - b.l) + (a.a - b.a) * (a.a - b.a) + (a.b - b.b) * (a.b - b.b));
}

} // namespace loupe::app::render
