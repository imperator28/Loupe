#include "app/models/ThemePreference.h"

#include <QSettings>

namespace loupe::app::models {
namespace {

[[nodiscard]] bool isPartColorMode(const QString& mode)
{
    return mode == QStringLiteral("file") || mode == QStringLiteral("shades") || mode == QStringLiteral("warm")
        || mode == QStringLiteral("muted");
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

} // namespace loupe::app::models
