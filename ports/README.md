# vcpkg overlay ports

Ports here replace the same-named port from the vcpkg baseline in
`vcpkg-configuration.json`. The macOS presets in `CMakePresets.json` point
`VCPKG_OVERLAY_PORTS` here; Windows builds use the baseline ports unchanged.

## qtbase

vcpkg's `qtbase` 6.11.1 port at baseline `db4723bd0a99eab031f1a3dee4336dca43049c87`,
copied unchanged except for one added patch, `QTBUG-147602.patch`, applied on
Apple targets only.

Qt 6.11.0 and 6.11.1 pass `CGImageCreate` a colour space whose owner was already
destroyed. Every `QImage` to `CGImage` conversion uses it: setting the app icon,
cursors, the pasteboard. macOS used to keep that colour space alive in a cache, but
newer releases do not, so a Qt app crashes on launch (in Loupe,
`QGuiApplication::setWindowIcon` crashed in `CGColorSpaceGetType` on macOS 15.8).
The patch is the fix from Qt 6.11.2, qtbase commit
[08e464f85597](https://github.com/qt/qtbase/commit/08e464f85597), for the
three files it changes in `src/gui`.

Remove this overlay when the baseline moves to Qt 6.11.2 or later.
