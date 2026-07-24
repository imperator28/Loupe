#include "app/platform/WindowChrome.h"

#ifdef Q_OS_WIN
#include <windows.h>

#include <dwmapi.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#endif

namespace loupe::app::platform {

void WindowChrome::applyAppearance(QQuickWindow* window, bool dark)
{
#ifdef Q_OS_WIN
    if (!window) return;
    const auto hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd) return;
    const BOOL useDarkMode = dark ? TRUE : FALSE;
    ::DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
    // DWM only repaints the non-client frame (title bar) on a frame-changed
    // notification, not merely because the attribute was set.
    ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
#else
    // No native chrome to adjust on this platform; the OS window already
    // follows the system/app color scheme on its own.
    Q_UNUSED(window);
    Q_UNUSED(dark);
#endif
}

} // namespace loupe::app::platform
