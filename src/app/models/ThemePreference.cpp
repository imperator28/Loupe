#include "app/models/ThemePreference.h"

#include <QSettings>

#include <algorithm>

namespace loupe::app::models {
namespace {

constexpr int kMinimumPanelWidth = 120;
constexpr int kMaximumPanelWidth = 2000;

// A settings key segment: letters and digits only, so a key cannot reach another group.
[[nodiscard]] bool isPanelKey(const QString& key)
{
    if (key.isEmpty() || key.size() > 40) return false;
    return std::all_of(key.cbegin(), key.cend(), [](const QChar character) {
        return character.isLetterOrNumber() && character.unicode() < 128;
    });
}

[[nodiscard]] bool isPartColorMode(const QString& mode)
{
    return mode == QStringLiteral("file") || mode == QStringLiteral("shades") || mode == QStringLiteral("warm")
        || mode == QStringLiteral("muted") || mode == QStringLiteral("vivid");
}

} // namespace

ThemePreference::ThemePreference(QObject* parent)
    : QObject(parent)
{
    const auto stored = QSettings{}.value(QStringLiteral("appearance/themeMode"), mode_).toString();
    if (stored == QStringLiteral("System") || stored == QStringLiteral("Light") || stored == QStringLiteral("Dark")) {
        mode_ = stored;
    }
    const QSettings settings;
    const auto partColorMode = settings.value(QStringLiteral("appearance/partColorMode"), partColorMode_).toString();
    if (isPartColorMode(partColorMode)) partColorMode_ = partColorMode;
    recolorFileColors_ = settings.value(QStringLiteral("appearance/recolorFileColors"), false).toBool();
    partColorSeed_ = settings.value(QStringLiteral("appearance/partColorSeed"), 0).toInt();
    renderMode_ = std::clamp(settings.value(QStringLiteral("appearance/renderMode"), 1).toInt(), 0, 2);
    const auto panelGroup = QStringLiteral("layout/panels/");
    for (const auto& key : settings.allKeys()) {
        if (!key.startsWith(panelGroup)) continue;
        const auto name = key.mid(panelGroup.size());
        bool valid = false;
        const auto width = settings.value(key).toInt(&valid);
        if (isPanelKey(name) && valid) panelWidths_.insert(name, std::clamp(width, kMinimumPanelWidth, kMaximumPanelWidth));
    }
    // 0.1.5-beta's tree width, from before every panel could be resized.
    if (!panelWidths_.contains(QStringLiteral("inspectTree")) && settings.contains(QStringLiteral("layout/treePanelWidth"))) {
        panelWidths_.insert(QStringLiteral("inspectTree"),
                            std::clamp(settings.value(QStringLiteral("layout/treePanelWidth")).toInt(), kMinimumPanelWidth, kMaximumPanelWidth));
    }
}

void ThemePreference::setMode(const QString& mode)
{
    if (mode != QStringLiteral("System") && mode != QStringLiteral("Light") && mode != QStringLiteral("Dark")) return;
    if (mode_ == mode) return;
    mode_ = mode;
    QSettings{}.setValue(QStringLiteral("appearance/themeMode"), mode_);
    emit modeChanged();
}

void ThemePreference::setPartColorMode(const QString& mode)
{
    if (!isPartColorMode(mode) || partColorMode_ == mode) return;
    partColorMode_ = mode;
    QSettings{}.setValue(QStringLiteral("appearance/partColorMode"), partColorMode_);
    emit partColorsChanged();
}

void ThemePreference::setRecolorFileColors(const bool recolor)
{
    if (recolorFileColors_ == recolor) return;
    recolorFileColors_ = recolor;
    QSettings{}.setValue(QStringLiteral("appearance/recolorFileColors"), recolorFileColors_);
    emit partColorsChanged();
}

void ThemePreference::setPartColorSeed(const int seed)
{
    if (partColorSeed_ == seed) return;
    partColorSeed_ = seed;
    QSettings{}.setValue(QStringLiteral("appearance/partColorSeed"), partColorSeed_);
    emit partColorsChanged();
}

void ThemePreference::setRenderMode(const int mode)
{
    const auto clamped = std::clamp(mode, 0, 2);
    if (renderMode_ == clamped) return;
    renderMode_ = clamped;
    QSettings{}.setValue(QStringLiteral("appearance/renderMode"), renderMode_);
    emit renderModeChanged();
}

void ThemePreference::setPanelWidth(const QString& key, const int width)
{
    if (!isPanelKey(key)) return;
    const auto clamped = std::clamp(width, kMinimumPanelWidth, kMaximumPanelWidth);
    if (panelWidths_.value(key).toInt() == clamped && panelWidths_.contains(key)) return;
    panelWidths_.insert(key, clamped);
    QSettings{}.setValue(QStringLiteral("layout/panels/") + key, clamped);
    emit panelWidthsChanged();
}

void ThemePreference::clearPanelWidth(const QString& key)
{
    if (!isPanelKey(key) || !panelWidths_.contains(key)) return;
    panelWidths_.remove(key);
    QSettings settings;
    settings.remove(QStringLiteral("layout/panels/") + key);
    if (key == QStringLiteral("inspectTree")) settings.remove(QStringLiteral("layout/treePanelWidth"));
    emit panelWidthsChanged();
}

} // namespace loupe::app::models
