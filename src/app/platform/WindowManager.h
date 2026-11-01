#pragma once

#include <QObject>
#include <QPointer>
#include <QVector>

class QQuickWindow;

namespace loupe::app {
class ApplicationController;
}

namespace loupe::app::platform {

// Loupe's windows (#15): every window registers here, so a tab dragged out of one can
// become a window of its own or drop into another window's tab strip, and files the OS
// opens go to the window used last. Windows opened here are owned here and destroyed
// when closed, along with their tabs and workers.
class WindowManager final : public QObject {
    Q_OBJECT

public:
    explicit WindowManager(QObject* parent = nullptr);

    // Every Main window calls this once it exists.
    Q_INVOKABLE void registerWindow(QObject* window);
    // A new window at global (x, y) holding `document`, created in `fromWindow`'s engine.
    Q_INVOKABLE QObject* openWindowWith(loupe::app::ApplicationController* document, QObject* fromWindow, int x, int y);
    // The window, other than `excluding`, whose tab strip is under global (x, y), or null.
    Q_INVOKABLE QObject* dropTargetAt(int x, int y, QObject* excluding) const;
    // Where files from the OS go: the visible window used last.
    [[nodiscard]] QObject* preferredWindow() const;

private:
    QVector<QPointer<QQuickWindow>> windows_; // most recently active last
};

} // namespace loupe::app::platform
