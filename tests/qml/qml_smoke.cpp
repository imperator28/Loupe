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
    void inspectionTaskPanelsLoad();
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

void QmlSmokeTest::inspectionTaskPanelsLoad()
{
    QQmlEngine engine;
    const QStringList panelFiles{
        QStringLiteral("MeasurementPanel.qml"),
        QStringLiteral("SectionPanel.qml"),
        QStringLiteral("CapturePanel.qml"),
    };

    for (const auto& panelFile : panelFiles) {
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(LOUPE_INSPECT_QML_DIR) + QLatin1Char('/') + panelFile));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object != nullptr, qPrintable(component.errorString()));
    }
}

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    QmlSmokeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "qml_smoke.moc"
