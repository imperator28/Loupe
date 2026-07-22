#include "app/ApplicationController.h"
#include "app/export/ExportWorkspaceController.h"
#include "app/models/ThemePreference.h"
#include "app/platform/WindowChrome.h"
#include "app/render/CadEdgeGeometry.h"
#include "app/render/MeshGeometry.h"
#include "app/tools/CaptureController.h"
#include "app/tools/SectionController.h"
#include "protocol/GeometryPayload.h"

#include <QColor>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGuiApplication>
#include <QMimeData>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlPropertyMap>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest/QTest>
#include <qqml.h>

#include <algorithm>
#include <cmath>

namespace {

QQuickItem* findItemByObjectName(QQuickItem* root, const QString& objectName)
{
    if (root->objectName() == objectName) return root;
    for (auto* child : root->childItems()) {
        if (auto* match = findItemByObjectName(child, objectName)) return match;
    }
    return nullptr;
}

QRect opaqueBounds(const QImage& source)
{
    const auto image = source.convertToFormat(QImage::Format_ARGB32);
    int left = image.width();
    int top = image.height();
    int right = -1;
    int bottom = -1;
    for (int y = 0; y < image.height(); ++y) {
        const auto* row = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(row[x]) <= 16) continue;
            left = std::min(left, x);
            top = std::min(top, y);
            right = std::max(right, x);
            bottom = std::max(bottom, y);
        }
    }
    return right >= left && bottom >= top ? QRect{QPoint{left, top}, QPoint{right, bottom}} : QRect{};
}

} // namespace

class QmlSmokeTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void mainLoads();
    void exportPickerCheckboxUpdatesDraft();
    void inspectionTaskPanelsLoad();
    void captureProgressStaysInCapturePanel();
    void measurementFaceHighlightLoadsTopology();
    void sectionViewActivatesWithRenderedMesh();
    void viewportCaptureUsesRequestedRenderResolution();
};

void QmlSmokeTest::initTestCase()
{
    qmlRegisterUncreatableMetaObject(loupe::app::staticMetaObject, "Loupe.App", 1, 0, "AppState", "Application state only");
    qmlRegisterType<loupe::app::ApplicationController>("Loupe.App", 1, 0, "ApplicationController");
    qmlRegisterType<loupe::app::models::ThemePreference>("Loupe.App", 1, 0, "ThemePreference");
    qmlRegisterType<loupe::app::platform::WindowChrome>("Loupe.App", 1, 0, "WindowChrome");
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

    const auto acceptsUrls = [&](const QVariantList& urls) {
        QVariant accepted;
        const auto invoked = QMetaObject::invokeMethod(object.get(), "canOpenDroppedUrls",
                                                       Q_RETURN_ARG(QVariant, accepted),
                                                       Q_ARG(QVariant, QVariant::fromValue(urls)));
        return qMakePair(invoked, accepted.toBool());
    };
    const auto upperStep = acceptsUrls({QUrl(QStringLiteral("file:///tmp/model.STEP"))});
    const auto shortStep = acceptsUrls({QUrl(QStringLiteral("file:///tmp/model.stp"))});
    const auto unsupported = acceptsUrls({QUrl(QStringLiteral("file:///tmp/model.obj"))});
    const auto multiple = acceptsUrls({QUrl(QStringLiteral("file:///tmp/a.step")),
                                       QUrl(QStringLiteral("file:///tmp/b.step"))});
    QVERIFY(upperStep.first && upperStep.second);
    QVERIFY(shortStep.first && shortStep.second);
    QVERIFY(unsupported.first && !unsupported.second);
    QVERIFY(multiple.first && !multiple.second);

    auto* window = qobject_cast<QQuickWindow*>(object.get());
    QVERIFY(window != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(window->isExposed(), 3000);
    QVERIFY(object->findChild<QObject*>(QStringLiteral("interactionGuidePopup")) != nullptr);
    auto* dropOverlay = findItemByObjectName(window->contentItem(), QStringLiteral("stepFileDropOverlay"));
    QVERIFY(dropOverlay != nullptr);
    QCOMPARE(dropOverlay->property("cornerRadius").toReal(), 18.0);
    auto* dropOverlaySurface = findItemByObjectName(window->contentItem(),
                                                     QStringLiteral("stepFileDropOverlaySurface"));
    QVERIFY(dropOverlaySurface != nullptr);
    QVERIFY(dropOverlaySurface->y() < 0);
    auto* controller = qobject_cast<loupe::app::ApplicationController*>(
        object->property("controller").value<QObject*>());
    QVERIFY(controller != nullptr);

    QMimeData mimeData;
    mimeData.setUrls({QUrl(QStringLiteral("file:///tmp/missing-dropped-model.step"))});
    QDragEnterEvent enterEvent(QPoint{window->width() / 2, window->height() / 2},
                               Qt::CopyAction, &mimeData, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(window, &enterEvent);
    QVERIFY(enterEvent.isAccepted());
    QDropEvent dropEvent(QPointF{window->width() / 2.0, window->height() / 2.0},
                         Qt::CopyAction, &mimeData, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(window, &dropEvent);
    QVERIFY(dropEvent.isAccepted());
    QTRY_COMPARE(controller->documentState(), loupe::app::DocumentState::Invalid);
    QVERIFY(controller->errorMessage().contains(QStringLiteral("does not exist"), Qt::CaseInsensitive));
}

void QmlSmokeTest::exportPickerCheckboxUpdatesDraft()
{
    loupe::app::exporting::ExportWorkspaceController draft;
    draft.replaceSnapshot(QStringLiteral(R"({"effectiveUnit":"mm","sourceToMillimeters":1,"nodes":[
        {"id":"root","name":"Assembly","kind":0,"parentId":""},
        {"id":"sub","name":"Carrier","kind":1,"parentId":"root"},
        {"id":"cover","name":"Cover","kind":2,"parentId":"sub"}
    ]})"));

    QQmlEngine engine;
    QQmlComponent themeComponent(&engine, QUrl::fromLocalFile(
        QStringLiteral(LOUPE_QML_DIR) + QStringLiteral("/Theme.qml")));
    QVERIFY2(themeComponent.isReady(), qPrintable(themeComponent.errorString()));
    std::unique_ptr<QObject> theme(themeComponent.create());
    QVERIFY2(theme != nullptr, qPrintable(themeComponent.errorString()));

    QQmlComponent pickerComponent(&engine, QUrl::fromLocalFile(
        QStringLiteral(LOUPE_EXPORT_QML_DIR) + QStringLiteral("/ExportComponentPicker.qml")));
    QVERIFY2(pickerComponent.isReady(), qPrintable(pickerComponent.errorString()));
    const QVariantMap initialProperties{
        {QStringLiteral("draft"), QVariant::fromValue(static_cast<QObject*>(&draft))},
        {QStringLiteral("theme"), QVariant::fromValue(theme.get())},
        {QStringLiteral("width"), 320},
        {QStringLiteral("height"), 480},
    };
    std::unique_ptr<QObject> pickerObject(pickerComponent.createWithInitialProperties(initialProperties));
    QVERIFY2(pickerObject != nullptr, qPrintable(pickerComponent.errorString()));
    auto* picker = qobject_cast<QQuickItem*>(pickerObject.get());
    QVERIFY(picker != nullptr);

    QQuickWindow window;
    window.resize(320, 480);
    picker->setParentItem(window.contentItem());
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 3000);

    auto* disclosure = findItemByObjectName(window.contentItem(), QStringLiteral("exportComponentDisclosure-sub"));
    auto* coverRow = findItemByObjectName(window.contentItem(), QStringLiteral("exportComponentRow-cover"));
    QVERIFY(disclosure != nullptr);
    QVERIFY(coverRow != nullptr);
    const auto disclosureCenter = disclosure->mapToScene(QPointF(disclosure->width() / 2.0, disclosure->height() / 2.0));
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, disclosureCenter.toPoint());
    QTRY_VERIFY(!coverRow->property("visible").toBool());
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, disclosureCenter.toPoint());
    QTRY_VERIFY(coverRow->property("visible").toBool());

    auto* checkbox = findItemByObjectName(window.contentItem(), QStringLiteral("exportComponentCheckBox-cover"));
    QTRY_VERIFY_WITH_TIMEOUT(checkbox != nullptr, 3000);
    const auto checkboxCenter = checkbox->mapToScene(QPointF(checkbox->width() / 2.0, checkbox->height() / 2.0));
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, checkboxCenter.toPoint());
    QTRY_COMPARE(draft.checkedCount(), 1);
    QVERIFY(draft.isChecked(QStringLiteral("cover")));
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
    theme->insert(QStringLiteral("edge"), QColor(QStringLiteral("#eef2ff")));

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

void QmlSmokeTest::viewportCaptureUsesRequestedRenderResolution()
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
    theme->insert(QStringLiteral("edge"), QColor(QStringLiteral("#eef2ff")));

    loupe::app::ApplicationController controller;
    auto* capture = qobject_cast<loupe::app::tools::CaptureController*>(controller.captureController());
    QVERIFY(capture != nullptr);
    capture->setCustomScale(2.0);

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
    window.resize(320, 240);
    viewport->setParentItem(window.contentItem());
    viewport->setSize(QSizeF(320, 240));
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(capture->resolvedWidth() > 0 && capture->resolvedHeight() > 0, 3000);

    const QByteArray cube = QByteArrayLiteral(
        "{\"vertices\":[-1,-1,-1,1,-1,-1,1,1,-1,-1,1,-1,-1,-1,1,1,-1,1,1,1,1,-1,1,1],"
        "\"indices\":[0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7]}" );
    QVERIFY(QMetaObject::invokeMethod(&controller, "meshReady",
                                      Q_ARG(QString, QStringLiteral("node")),
                                      Q_ARG(QString, QStringLiteral("segment")),
                                      Q_ARG(QString, QStringLiteral("#67d5c0")),
                                      Q_ARG(QByteArray, cube)));
    QTest::qWait(500);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto captureAtScale = [&](double scale, const QString& fileName, QImage* result) {
        capture->setCustomScale(scale);
        const auto expectedSize = capture->resolvedSize();
        const auto path = directory.filePath(fileName);
        const auto url = QUrl::fromLocalFile(path);
        QVERIFY(QMetaObject::invokeMethod(viewport, "captureToFile", Q_ARG(QVariant, QVariant::fromValue(url))));
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(path), 10'000);
        QTRY_VERIFY_WITH_TIMEOUT(!capture->inProgress(), 10'000);
        const QImage image(path);
        QVERIFY(!image.isNull());
        QCOMPARE(image.size(), expectedSize);
        *result = image;
    };

    QImage image2x;
    QImage image3x;
    QImage imageCustom;
    captureAtScale(2.0, QStringLiteral("viewport-capture-2x.png"), &image2x);
    captureAtScale(3.0, QStringLiteral("viewport-capture-3x.png"), &image3x);
    captureAtScale(2.35, QStringLiteral("viewport-capture-2.35x.png"), &imageCustom);
    const auto bounds2x = opaqueBounds(image2x);
    const auto bounds3x = opaqueBounds(image3x);
    const auto boundsCustom = opaqueBounds(imageCustom);
    QVERIFY(!bounds2x.isEmpty());
    QVERIFY(!bounds3x.isEmpty());
    QVERIFY(!boundsCustom.isEmpty());
    const auto normalizedWidth2x = static_cast<double>(bounds2x.width()) / image2x.width();
    const auto normalizedWidth3x = static_cast<double>(bounds3x.width()) / image3x.width();
    const auto normalizedHeight2x = static_cast<double>(bounds2x.height()) / image2x.height();
    const auto normalizedHeight3x = static_cast<double>(bounds3x.height()) / image3x.height();
    const auto normalizedWidthCustom = static_cast<double>(boundsCustom.width()) / imageCustom.width();
    const auto normalizedHeightCustom = static_cast<double>(boundsCustom.height()) / imageCustom.height();
    QVERIFY2(std::abs(normalizedWidth2x - normalizedWidth3x) < 0.02,
             qPrintable(QStringLiteral("Capture width framing changed: %1 vs %2")
                            .arg(normalizedWidth2x).arg(normalizedWidth3x)));
    QVERIFY2(std::abs(normalizedHeight2x - normalizedHeight3x) < 0.02,
             qPrintable(QStringLiteral("Capture height framing changed: %1 vs %2")
                            .arg(normalizedHeight2x).arg(normalizedHeight3x)));
    QVERIFY2(std::abs(normalizedWidth2x - normalizedWidthCustom) < 0.02,
             qPrintable(QStringLiteral("Custom capture width framing changed: %1 vs %2")
                            .arg(normalizedWidth2x).arg(normalizedWidthCustom)));
    QVERIFY2(std::abs(normalizedHeight2x - normalizedHeightCustom) < 0.02,
             qPrintable(QStringLiteral("Custom capture height framing changed: %1 vs %2")
                            .arg(normalizedHeight2x).arg(normalizedHeightCustom)));
    QCOMPARE(capture->progress(), 1.0);
    QVERIFY(!capture->inProgress());
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

    QVERIFY(QMetaObject::invokeMethod(object.get(), "clearTopology"));
    QVERIFY(!object->property("faceReady").toBool());
    QVERIFY(!object->property("boundaryReady").toBool());
    QVERIFY(!fill->property("visible").toBool());
    QVERIFY(!boundary->property("visible").toBool());
}

void QmlSmokeTest::captureProgressStaysInCapturePanel()
{
    loupe::app::tools::CaptureController capture;
    QQmlEngine engine;
    QQmlComponent themeComponent(&engine, QUrl::fromLocalFile(
        QStringLiteral(LOUPE_QML_DIR) + QStringLiteral("/Theme.qml")));
    QVERIFY2(themeComponent.isReady(), qPrintable(themeComponent.errorString()));
    std::unique_ptr<QObject> theme(themeComponent.create());
    QVERIFY2(theme != nullptr, qPrintable(themeComponent.errorString()));

    QQmlComponent panelComponent(&engine, QUrl::fromLocalFile(
        QStringLiteral(LOUPE_INSPECT_QML_DIR) + QStringLiteral("/CapturePanel.qml")));
    QVERIFY2(panelComponent.isReady(), qPrintable(panelComponent.errorString()));
    const QVariantMap initialProperties{
        {QStringLiteral("taskController"), QVariant::fromValue(static_cast<QObject*>(&capture))},
        {QStringLiteral("theme"), QVariant::fromValue(theme.get())},
    };
    std::unique_ptr<QObject> panel(panelComponent.createWithInitialProperties(initialProperties));
    QVERIFY2(panel != nullptr, qPrintable(panelComponent.errorString()));
    auto* progressBar = panel->findChild<QObject*>(QStringLiteral("captureProgressBar"));
    QVERIFY(progressBar != nullptr);
    QVERIFY(!progressBar->property("visible").toBool());

    capture.beginCapture();
    QTRY_VERIFY(progressBar->property("visible").toBool());
    QCOMPARE(progressBar->property("value").toDouble(), capture.progress());
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
