#include "app/platform/WindowManager.h"

#include "app/ApplicationController.h"

#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>

namespace loupe::app::platform {

WindowManager::WindowManager(QObject* parent)
    : QObject(parent)
{
}

void WindowManager::registerWindow(QObject* object)
{
    auto* window = qobject_cast<QQuickWindow*>(object);
    if (!window) return;
    for (const auto& known : windows_) if (known == window) return;
    windows_.append(window);
    // Keep the list in activation order, most recent last.
    connect(window, &QWindow::activeChanged, this, [this, window] {
        if (!window->isActive()) return;
        windows_.removeAll(window);
        windows_.append(window);
    });
    connect(window, &QObject::destroyed, this, [this] { windows_.removeAll(nullptr); });
}

QObject* WindowManager::dropTargetAt(const int x, const int y, QObject* excluding) const
{
    // Most recently active first: the one on top where windows overlap.
    for (auto it = windows_.crbegin(); it != windows_.crend(); ++it) {
        QQuickWindow* window = *it;
        if (!window || window == excluding || !window->isVisible()) continue;
        QVariant index;
        if (QMetaObject::invokeMethod(window, "tabDropIndexAt", Q_RETURN_ARG(QVariant, index), Q_ARG(QVariant, x), Q_ARG(QVariant, y))
            && index.toInt() >= 0)
            return window;
    }
    return nullptr;
}

QObject* WindowManager::preferredWindow() const
{
    for (auto it = windows_.crbegin(); it != windows_.crend(); ++it)
        if (*it && (*it)->isVisible()) return it->data();
    return nullptr;
}

QObject* WindowManager::openWindowWith(ApplicationController* document, QObject* fromWindow, const int x, const int y)
{
    auto* engine = fromWindow ? qmlEngine(fromWindow) : nullptr;
    if (!document || !engine) return nullptr;
    // From the same QML file the dragging window was made from, however it was loaded.
    const auto* context = qmlContext(fromWindow);
    QQmlComponent component(engine);
    if (context && context->baseUrl().isValid()) component.loadUrl(context->baseUrl());
    else component.loadFromModule(QStringLiteral("Loupe.App"), QStringLiteral("Main"));
    auto* object = component.createWithInitialProperties({{QStringLiteral("adoptDocument"), QVariant::fromValue(static_cast<QObject*>(document))}});
    auto* window = qobject_cast<QQuickWindow*>(object);
    if (!window) {
        qWarning("Could not open a new Loupe window: %s", qPrintable(component.errorString()));
        delete object;
        return nullptr;
    }
    QQmlEngine::setObjectOwnership(window, QQmlEngine::CppOwnership);
    if (auto* source = qobject_cast<QWindow*>(fromWindow)) window->resize(source->size());
    // Placed so the dragged tab lands under the pointer, roughly where it was let go.
    window->setPosition(x - 120, y - 20);
    window->show();
    window->requestActivate();
    // A closed window is gone for good, with its tabs and their workers.
    connect(window, &QWindow::visibleChanged, window, [window](const bool visible) {
        if (!visible) window->deleteLater();
    });
    registerWindow(window);
    return window;
}

} // namespace loupe::app::platform
