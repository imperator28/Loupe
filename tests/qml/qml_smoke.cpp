#include "app/ApplicationController.h"

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QtTest/QTest>
#include <qqml.h>

class QmlSmokeTest final : public QObject
{
    Q_OBJECT

private slots:
    void mainLoads();
};

void QmlSmokeTest::mainLoads()
{
    qmlRegisterUncreatableMetaObject(loupe::app::staticMetaObject, "Loupe.App", 1, 0, "AppState", "Application state only");
    qmlRegisterType<loupe::app::ApplicationController>("Loupe.App", 1, 0, "ApplicationController");
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(LOUPE_MAIN_QML_PATH)));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> object(component.create());
    QVERIFY2(object != nullptr, qPrintable(component.errorString()));
}

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    QmlSmokeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "qml_smoke.moc"
