#include "app/ApplicationController.h"
#include "app/DocumentTabs.h"
#include "app/platform/FileAssociation.h"
#include "app/platform/FileOpenRouter.h"
#include "app/platform/GraphicsWarmup.h"
#include "app/platform/StartupTrace.h"
#include "app/platform/TempHygiene.h"
#include "app/platform/UpdateController.h"
#include "app/platform/WindowManager.h"
#include "app/platform/WindowChrome.h"
#include "app/render/CadEdgeGeometry.h"
#include "app/render/HandleRingGeometry.h"
#include "app/render/ExplodeLineGeometry.h"
#include "app/render/MeshGeometry.h"
#include "app/worker/WorkerLaunch.h"
#include "app/models/ThemePreference.h"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScopeGuard>
#include <qqml.h>

namespace {

// With LOUPE_STARTUP_TRACE set: marks when each window is first exposed and its first frames.
class StartupFrameTrace final : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Expose) {
            if (auto* window = qobject_cast<QQuickWindow*>(watched); window && !traced_) {
                traced_ = true;
                loupe::app::platform::startupMark("window exposed");
                QObject::connect(window, &QQuickWindow::frameSwapped, window, [] {
                    static int frames = 0;
                    if (++frames <= 400) loupe::app::platform::startupMark("frame swapped");
                }, Qt::DirectConnection);
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    bool traced_{false};
};

} // namespace

int main(int argc, char* argv[])
{
    loupe::app::platform::startupMark("main");
    // The native macOS control style does not reliably honor the dynamic QML
    // palette. Basic keeps every control on the semantic theme roles below.
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication application(argc, argv);
    loupe::app::platform::startupMark("application constructed");
    StartupFrameTrace frameTrace;
    if (loupe::app::platform::startupTraceEnabled()) application.installEventFilter(&frameTrace);
    application.setOrganizationName(QStringLiteral("imperator28"));
    application.setApplicationName(QStringLiteral("Loupe"));
    application.setWindowIcon(QIcon(QStringLiteral(":/branding/loupe-app-icon.png")));
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#182027"));
    palette.setColor(QPalette::WindowText, QColor("#e6edf3"));
    palette.setColor(QPalette::Base, QColor("#11181e"));
    palette.setColor(QPalette::AlternateBase, QColor("#202a33"));
    palette.setColor(QPalette::Text, QColor("#e6edf3"));
    palette.setColor(QPalette::Button, QColor("#26323c"));
    palette.setColor(QPalette::ButtonText, QColor("#e6edf3"));
    palette.setColor(QPalette::Highlight, QColor("#315b64"));
    palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    palette.setColor(QPalette::ToolTipBase, QColor("#26323c"));
    palette.setColor(QPalette::ToolTipText, QColor("#e6edf3"));
    // Qt's own built-in dialogs (e.g. the QML ColorDialog used for material/
    // body color pickers) style their header and footer bars from
    // QPalette::Light, and borders from QPalette::Dark. Left unset, Qt
    // auto-derives these from Button, landing on a shade visibly lighter than
    // the rest of this dark palette — a washed-out band across otherwise-dark
    // dialogs. Setting them explicitly keeps every native-QPalette-driven
    // surface consistent with the rest of the app.
    palette.setColor(QPalette::Light, QColor("#2a3540"));
    palette.setColor(QPalette::Midlight, QColor("#24303a"));
    palette.setColor(QPalette::Dark, QColor("#0d1318"));
    palette.setColor(QPalette::Mid, QColor("#1a232b"));
    palette.setColor(QPalette::Shadow, QColor("#05080a"));
    application.setPalette(palette);
    qmlRegisterUncreatableMetaObject(loupe::app::staticMetaObject, "Loupe.App", 1, 0, "AppState", "Application state only");
    qmlRegisterType<loupe::app::ApplicationController>("Loupe.App", 1, 0, "ApplicationController");
    qmlRegisterType<loupe::app::DocumentTabs>("Loupe.App", 1, 0, "DocumentTabs");
    qmlRegisterType<loupe::app::models::ThemePreference>("Loupe.App", 1, 0, "ThemePreference");
    qmlRegisterType<loupe::app::platform::WindowChrome>("Loupe.App", 1, 0, "WindowChrome");
    qmlRegisterType<loupe::app::render::MeshGeometry>("Loupe.App", 1, 0, "MeshGeometry");
    qmlRegisterType<loupe::app::render::CadEdgeGeometry>("Loupe.App", 1, 0, "CadEdgeGeometry");
    qmlRegisterType<loupe::app::render::HandleRingGeometry>("Loupe.App", 1, 0, "HandleRingGeometry");
    qmlRegisterType<loupe::app::render::ExplodeLineGeometry>("Loupe.App", 1, 0, "ExplodeLineGeometry");
    // One per engine (the app has one). Checks GitHub for a newer release at launch (#3).
    qmlRegisterSingletonType<loupe::app::platform::UpdateController>("Loupe.App", 1, 0, "Updates",
        [](QQmlEngine*, QJSEngine*) -> QObject* { return new loupe::app::platform::UpdateController; });

    qmlRegisterType<loupe::app::platform::FileAssociation>("Loupe.App", 1, 0, "FileAssociation");
    loupe::app::platform::WindowManager windowManager;
    qmlRegisterSingletonInstance("Loupe.App", 1, 0, "Windows", &windowManager);

    // STEP files from the command line (Windows "Open with", a default-app double-click),
    // from macOS Finder, or from a later launch while this one runs (#8). A later launch
    // with files hands them over and exits, so they open as tabs here.
    const bool selfCheck = QCoreApplication::arguments().contains(QStringLiteral("--self-check"));
    const auto launchFiles = loupe::app::platform::FileOpenRouter::stepFilesIn(QCoreApplication::arguments());
    if (!selfCheck && loupe::app::platform::FileOpenRouter::forwardToRunningInstance(launchFiles)) return 0;
    // Both run alongside the window's creation below, which takes a while.
    loupe::app::platform::startGraphicsWarmup();
    const auto finishWarmup = qScopeGuard([] { loupe::app::platform::finishGraphicsWarmup(); });
    // The import of the file Loupe was opened with starts now, in its own process, and runs
    // while the window below is built; the document that opens it takes the process over.
    if (!selfCheck && !launchFiles.isEmpty()) loupe::app::worker::prelaunchWorker(launchFiles.constFirst());
    const auto discardUnclaimedImport = qScopeGuard([] { loupe::app::worker::discardPrelaunchedWorker(); });
    loupe::app::platform::FileOpenRouter fileOpenRouter;
    if (!selfCheck) fileOpenRouter.listenForOtherInstances();

    // Packaged builds check for updates each launch; LOUPE_NO_UPDATE_CHECK turns it off.
    loupe::app::platform::UpdateController::setAutomaticChecks(!selfCheck && !qEnvironmentVariableIsSet("LOUPE_NO_UPDATE_CHECK"));
    // Leftovers of an earlier Loupe that crashed or was force-quit.
    if (!selfCheck) loupe::app::platform::sweepStaleTempFilesAtLaunch();
    // What's New shows once after an update; never during the packaging self-check.
    loupe::app::platform::UpdateController::setShowWhatsNewOnLaunch(!selfCheck);
    QQmlApplicationEngine engine;
    loupe::app::platform::startupMark("loading QML");
    engine.loadFromModule("Loupe.App", "Main");
    loupe::app::platform::startupMark("QML loaded");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    fileOpenRouter.setWindow(engine.rootObjects().constFirst());
    fileOpenRouter.setWindowProvider([&windowManager] { return windowManager.preferredWindow(); });
    fileOpenRouter.open(launchFiles);
    loupe::app::platform::startupMark("launch files handed over");
    // Opening is synchronous, so a pre-launched import still unclaimed was refused.
    loupe::app::worker::discardPrelaunchedWorker();
    // The window has drawn its first frame, on a device of Qt's own, by now.
    loupe::app::platform::finishGraphicsWarmup();
    // --self-check: start exactly as a user's launch does -- real platform plugin, the whole
    // QML UI -- then confirm the worker sits beside the executable, and exit. Release CI runs
    // it on the packaged folder, so a package that cannot start (a missing Qt plugin, a
    // missing worker) fails the build instead of reaching a tester. Getting this far already
    // proves the platform plugin loaded: without one, Qt aborts in the constructor above.
    if (selfCheck) {
#ifdef Q_OS_WIN
        const auto worker = QStringLiteral("loupe-worker.exe");
#else
        const auto worker = QStringLiteral("loupe-worker");
#endif
        const bool workerPresent = QFileInfo::exists(QDir(QCoreApplication::applicationDirPath()).filePath(worker));
        qInfo("Loupe self-check: platform '%s', UI loaded, worker %s",
              qPrintable(QGuiApplication::platformName()), workerPresent ? "present" : "MISSING");
        return workerPresent ? 0 : 2;
    }
    return application.exec();
}
