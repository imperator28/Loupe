#pragma once

#include <QColor>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>

namespace loupe::app::render {

// Automatic per-part colours for assemblies whose file gives the parts none (or gives
// every part the same one). The point is to tell neighbouring parts apart, not to
// decorate, so the default styles are deliberately low in contrast: colours are generated
// in OKLCH, where a fixed lightness band and a small chroma stay visually even. Vivid is
// the exception, for when telling parts apart matters more than calm.
enum class PartPaletteStyle {
    CoolShades, // one slate-blue hue, varying lightness
    WarmShades, // one sand hue, varying lightness
    Muted,      // soft hues around the wheel at an even, low chroma
    Vivid,      // high contrast: saturated hues around the wheel, two lightness levels
};

[[nodiscard]] std::optional<PartPaletteStyle> partPaletteStyleFromName(const QString& name);

// The colour at a position in the style's sequence. Positions come from a low-discrepancy
// walk, so consecutive positions -- consecutive parts in the tree -- land far apart.
[[nodiscard]] QColor partPaletteColor(PartPaletteStyle style, int position, std::uint32_t seed);

// Order in which parts take positions. Seed 0 keeps the tree order; any other seed is a
// reproducible shuffle of it, so "shuffle" is stable across sessions for the same seed.
[[nodiscard]] QStringList partPaletteOrder(const QStringList& treeOrderedKeys, std::uint32_t seed);

// Perceptual lightness (OKLab L, 0..1) and the OKLab distance between two colours, exposed
// so tests can pin the low-contrast promise numerically.
[[nodiscard]] double oklabLightness(const QColor& color);
[[nodiscard]] double oklabDistance(const QColor& first, const QColor& second);

} // namespace loupe::app::render
