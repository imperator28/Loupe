#include "app/ApplicationController.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <qqml.h>

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    qmlRegisterUncreatableMetaObject(loupe::app::staticMetaObject, "Loupe.App", 1, 0, "AppState", "Application state only");
    qmlRegisterType<loupe::app::ApplicationController>("Loupe.App", 1, 0, "ApplicationController");

    QQmlApplicationEngine engine;
    engine.loadFromModule("Loupe.App", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    return application.exec();
}
