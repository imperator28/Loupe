#include "app/ApplicationController.h"
#include "app/platform/WindowChrome.h"
#include "app/render/CadEdgeGeometry.h"
#include "app/render/MeshGeometry.h"
#include "app/models/ThemePreference.h"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <qqml.h>

int main(int argc, char* argv[])
{
    // The native macOS control style does not reliably honor the dynamic QML
    // palette. Basic keeps every control on the semantic theme roles below.
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication application(argc, argv);
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
    qmlRegisterType<loupe::app::models::ThemePreference>("Loupe.App", 1, 0, "ThemePreference");
    qmlRegisterType<loupe::app::platform::WindowChrome>("Loupe.App", 1, 0, "WindowChrome");
    qmlRegisterType<loupe::app::render::MeshGeometry>("Loupe.App", 1, 0, "MeshGeometry");
    qmlRegisterType<loupe::app::render::CadEdgeGeometry>("Loupe.App", 1, 0, "CadEdgeGeometry");

    QQmlApplicationEngine engine;
    engine.loadFromModule("Loupe.App", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    // --self-check: start exactly as a user's launch does -- real platform plugin, the whole
    // QML UI -- then confirm the worker sits beside the executable, and exit. Release CI runs
    // it on the packaged folder, so a package that cannot start (a missing Qt plugin, a
    // missing worker) fails the build instead of reaching a tester. Getting this far already
    // proves the platform plugin loaded: without one, Qt aborts in the constructor above.
    if (QCoreApplication::arguments().contains(QStringLiteral("--self-check"))) {
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
