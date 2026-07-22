#include "app/platform/WindowChrome.h"

namespace loupe::app::platform {

void WindowChrome::applyAppearance(QQuickWindow*, bool)
{
    // No native chrome to adjust on this platform; the OS window already
    // follows the system/app color scheme on its own.
}

} // namespace loupe::app::platform
