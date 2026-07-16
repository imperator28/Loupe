#include "app/models/ThemePreference.h"

#include <QSettings>

namespace loupe::app::models {

ThemePreference::ThemePreference(QObject* parent)
    : QObject(parent)
{
    const auto stored = QSettings{}.value(QStringLiteral("appearance/themeMode"), mode_).toString();
    if (stored == QStringLiteral("System") || stored == QStringLiteral("Light") || stored == QStringLiteral("Dark")) {
        mode_ = stored;
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

} // namespace loupe::app::models
