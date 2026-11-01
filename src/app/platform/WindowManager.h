#pragma once

#include <QObject>
#include <QPointer>
#include <QVector>

class QQuickWindow;

namespace loupe::app {
class ApplicationController;
}

namespace loupe::app::platform {

// Opens further Loupe windows (#15): a tab dragged off the tab strip becomes a window of
// its own, taking its document with it. Each window is another instance of Main, owned
// here and destroyed when it is closed, along with its tabs and their workers.
class WindowManager final : public QObject {
    Q_OBJECT

public:
    explicit WindowManager(QObject* parent = nullptr);

    // A new window at global (x, y) holding `document`, created in `fromWindow`'s engine.
    Q_INVOKABLE QObject* openWindowWith(loupe::app::ApplicationController* document, QObject* fromWindow, int x, int y);

private:
    QVector<QPointer<QQuickWindow>> windows_;
};

} // namespace loupe::app::platform
