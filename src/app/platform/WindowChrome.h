#pragma once

#include <QObject>
#include <QQuickWindow>

namespace loupe::app::platform {

// Keeps native window chrome (currently: the macOS title bar) in sync with
// Loupe's own theme, since the OS does not do this automatically for a
// window whose in-app theme differs from the system appearance. A no-op on
// platforms without matching native chrome to adjust.
class WindowChrome : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    Q_INVOKABLE void applyAppearance(QQuickWindow* window, bool dark);
};

} // namespace loupe::app::platform
