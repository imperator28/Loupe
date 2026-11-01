#include "app/models/ThemePreference.h"

#include <QSettings>

#include <algorithm>

namespace loupe::app::models {
namespace {

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
    treePanelWidth_ = std::clamp(settings.value(QStringLiteral("layout/treePanelWidth"), 248).toInt(), 160, 1200);
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

void ThemePreference::setTreePanelWidth(const int width)
{
    const auto clamped = std::clamp(width, 160, 1200);
    if (treePanelWidth_ == clamped) return;
    treePanelWidth_ = clamped;
    QSettings{}.setValue(QStringLiteral("layout/treePanelWidth"), treePanelWidth_);
    emit treePanelWidthChanged();
}

} // namespace loupe::app::models
