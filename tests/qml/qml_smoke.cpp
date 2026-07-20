#include "app/ApplicationController.h"
#include "app/models/ThemePreference.h"
#include "app/render/CadEdgeGeometry.h"
#include "app/render/MeshGeometry.h"
#include "app/tools/SectionController.h"
#include "protocol/GeometryPayload.h"

#include <QColor>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlPropertyMap>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest/QTest>
#include <qqml.h>

class QmlSmokeTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void mainLoads();
    void inspectionTaskPanelsLoad();
    void measurementFaceHighlightLoadsTopology();
    void sectionViewActivatesWithRenderedMesh();
};

void QmlSmokeTest::initTestCase()
{
    qmlRegisterUncreatableMetaObject(loupe::app::staticMetaObject, "Loupe.App", 1, 0, "AppState", "Application state only");
    qmlRegisterType<loupe::app::ApplicationController>("Loupe.App", 1, 0, "ApplicationController");
    qmlRegisterType<loupe::app::models::ThemePreference>("Loupe.App", 1, 0, "ThemePreference");
    qmlRegisterType<loupe::app::render::MeshGeometry>("Loupe.App", 1, 0, "MeshGeometry");
    qmlRegisterType<loupe::app::render::CadEdgeGeometry>("Loupe.App", 1, 0, "CadEdgeGeometry");
}

void QmlSmokeTest::mainLoads()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(LOUPE_MAIN_QML_PATH)));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> object(component.create());
    QVERIFY2(object != nullptr, qPrintable(component.errorString()));
}

void QmlSmokeTest::sectionViewActivatesWithRenderedMesh()
{
    std::unique_ptr<QQmlPropertyMap> theme(QQmlPropertyMap::create());
    theme->insert(QStringLiteral("dark"), true);
    theme->insert(QStringLiteral("surface"), QColor(QStringLiteral("#101418")));
    theme->insert(QStringLiteral("viewport"), QColor(QStringLiteral("#101418")));
    theme->insert(QStringLiteral("surfaceRaised"), QColor(QStringLiteral("#182027")));
    theme->insert(QStringLiteral("surfaceSubtle"), QColor(QStringLiteral("#202a33")));
    theme->insert(QStringLiteral("control"), QColor(QStringLiteral("#26323c")));
    theme->insert(QStringLiteral("border"), QColor(QStringLiteral("#34414b")));
    theme->insert(QStringLiteral("onSurface"), QColor(QStringLiteral("#e6edf3")));
    theme->insert(QStringLiteral("foreground"), QColor(QStringLiteral("#e6edf3")));
    theme->insert(QStringLiteral("muted"), QColor(QStringLiteral("#aeb8c2")));
    theme->insert(QStringLiteral("accent"), QColor(QStringLiteral("#67d5c0")));
    theme->insert(QStringLiteral("selection"), QColor(QStringLiteral("#377f76")));
    theme->insert(QStringLiteral("selectedBody"), QColor(QStringLiteral("#ffd166")));
    theme->insert(QStringLiteral("selectedEdge"), QColor(QStringLiteral("#fff0a6")));
    theme->insert(QStringLiteral("edge"), QColor(QStringLiteral("#b8c7d1")));

    loupe::app::ApplicationController controller;
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(LOUPE_INSPECT_QML_DIR) + QStringLiteral("/StepViewport.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    const QVariantMap initialProperties{
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject*>(&controller))},
        {QStringLiteral("theme"), QVariant::fromValue(static_cast<QObject*>(theme.get()))},
    };
    std::unique_ptr<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(object != nullptr, qPrintable(component.errorString()));
    auto* viewport = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(viewport != nullptr);

    QQuickWindow window;
    window.resize(640, 480);
    viewport->setParentItem(window.contentItem());
    viewport->setSize(QSizeF(640, 480));
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 3000);

    const QByteArray cube = QByteArrayLiteral(
        "{\"vertices\":[-1,-1,-1,1,-1,-1,1,1,-1,-1,1,-1,-1,-1,1,1,-1,1,1,1,1,-1,1,1],"
        "\"indices\":[0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7]}" );
    QVERIFY(QMetaObject::invokeMethod(&controller, "meshReady",
                                      Q_ARG(QString, QStringLiteral("node")),
                                      Q_ARG(QString, QStringLiteral("segment")),
                                      Q_ARG(QString, QStringLiteral("#67d5c0")),
                                      Q_ARG(QByteArray, cube)));
    QTest::qWait(100);

    auto* section = qobject_cast<loupe::app::tools::SectionController*>(controller.sectionController());
    QVERIFY(section != nullptr);
    section->setEnabled(true);
    QTest::qWait(500);
    QVERIFY(section->enabled());
}

void QmlSmokeTest::measurementFaceHighlightLoadsTopology()
{
    const loupe::protocol::MeshPayload payload{
        1, 1, QStringLiteral("definition"), QStringLiteral("node"), QStringLiteral("segment"),
        QStringLiteral("#ffffff"), 1,
        {0.0F, 0.0F, 0.0F, 2.0F, 0.0F, 0.0F, 2.0F, 2.0F, 0.0F, 0.0F, 2.0F, 0.0F},
        {0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F},
        {0, 1, 2, 0, 2, 3},
        {{17, loupe::protocol::TopologyKind::Face, 0, 6, 4.0F, 0.0F}}};
    loupe::app::render::MeshGeometry source;
    QVERIFY(source.appendWorkerMesh(loupe::protocol::encodeGeometry(payload)));

    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(LOUPE_INSPECT_QML_DIR) + QStringLiteral("/MeasurementFaceHighlight.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> object(component.create());
    QVERIFY2(object != nullptr, qPrintable(component.errorString()));
    object->setProperty("active", true);

    const bool invoked = QMetaObject::invokeMethod(
        object.get(), "setTopology",
        Q_ARG(QVariant, QVariant::fromValue(static_cast<QObject*>(&source))),
        Q_ARG(QVariant, QVariant::fromValue(17)));
    QVERIFY(invoked);
    QVERIFY(object->property("faceReady").toBool());
    QVERIFY(object->property("boundaryReady").toBool());

    auto* fill = object->findChild<QObject*>(QStringLiteral("measurementFaceFill"));
    auto* boundary = object->findChild<QObject*>(QStringLiteral("measurementFaceBoundary"));
    QVERIFY(fill != nullptr);
    QVERIFY(boundary != nullptr);
    QVERIFY(fill->property("visible").toBool());
    QVERIFY(boundary->property("visible").toBool());
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
