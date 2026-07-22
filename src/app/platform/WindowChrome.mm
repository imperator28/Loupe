#include "app/platform/WindowChrome.h"

#import <AppKit/AppKit.h>

namespace loupe::app::platform {

void WindowChrome::applyAppearance(QQuickWindow* window, bool dark)
{
    if (!window) return;
    auto* view = reinterpret_cast<NSView*>(window->winId());
    if (!view || !view.window) return;

    view.window.appearance = [NSAppearance appearanceNamed:
        dark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua];
}

} // namespace loupe::app::platform
