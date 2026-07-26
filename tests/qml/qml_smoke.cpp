#include "app/ApplicationController.h"
#include "app/drawing/DrawingWorkspaceController.h"
#include "app/export/ExportWorkspaceController.h"
#include "app/models/AssemblyTreeModel.h"
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
    void drawingWorkspaceQueuesOnePartAtSeveralViews();
    void inspectTreeRevealCallsIntoTheModelWithoutAQmlError();
    void drawingPreviewBuildsDrawablePathsFromProjectedGeometry();
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
    // The point is that the overlay traces the window frame, and the two platforms round
    // their frames differently, so a literal here would only be right on one of them.
    // Assert the platform's own expected value, and that the surface really carries it.
    const qreal expectedWindowRadius =
#if defined(Q_OS_WIN)
        8.0;
#else
        18.0;
#endif
    QCOMPARE(dropOverlay->property("cornerRadius").toReal(), expectedWindowRadius);
    auto* dropOverlaySurface = findItemByObjectName(window->contentItem(),
                                                     QStringLiteral("stepFileDropOverlaySurface"));
    QVERIFY(dropOverlaySurface != nullptr);
    QVERIFY(dropOverlaySurface->y() < 0);
    QCOMPARE(dropOverlaySurface->property("radius").toReal(), expectedWindowRadius);
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

void QmlSmokeTest::drawingWorkspaceQueuesOnePartAtSeveralViews()
{
    // The case the whole queue design exists for, driven through the real QML: pick a part
    // in the picker, choose two different views, and end up with two independent rows.
    loupe::app::drawing::DrawingWorkspaceController draft;
    draft.replaceSnapshot(QStringLiteral(R"({"effectiveUnit":"mm","sourceToMillimeters":1,"nodes":[
        {"id":"root","name":"Assembly","kind":0,"parentId":""},
        {"id":"plate","name":"Base plate","kind":2,"parentId":"root"}
    ]})"));
    draft.setDocumentReady(true);
    draft.setDestination(QStringLiteral("/out"));

    QQmlEngine engine;
    QQmlComponent themeComponent(&engine, QUrl::fromLocalFile(
        QStringLiteral(LOUPE_QML_DIR) + QStringLiteral("/Theme.qml")));
    QVERIFY2(themeComponent.isReady(), qPrintable(themeComponent.errorString()));
    std::unique_ptr<QObject> theme(themeComponent.create());
    QVERIFY2(theme != nullptr, qPrintable(themeComponent.errorString()));

    // The picker chooses the part.
    QQmlComponent pickerComponent(&engine, QUrl::fromLocalFile(
        QStringLiteral(LOUPE_QML_DIR) + QStringLiteral("/drawing/DrawingComponentPicker.qml")));
    QVERIFY2(pickerComponent.isReady(), qPrintable(pickerComponent.errorString()));
    std::unique_ptr<QObject> pickerObject(pickerComponent.createWithInitialProperties({
        {QStringLiteral("draft"), QVariant::fromValue(static_cast<QObject*>(&draft))},
        {QStringLiteral("theme"), QVariant::fromValue(theme.get())},
        {QStringLiteral("width"), 300},
        {QStringLiteral("height"), 400},
    }));
    QVERIFY2(pickerObject != nullptr, qPrintable(pickerComponent.errorString()));
    auto* picker = qobject_cast<QQuickItem*>(pickerObject.get());
    QVERIFY(picker != nullptr);

    QQuickWindow window;
    window.resize(360, 760);
    picker->setParentItem(window.contentItem());

    // The setup panel chooses the view and adds to the queue. The controller resolves
    // standard views, so it stands in as the view resolver.
    loupe::app::ApplicationController controller;
    QQmlComponent setupComponent(&engine, QUrl::fromLocalFile(
        QStringLiteral(LOUPE_QML_DIR) + QStringLiteral("/drawing/DrawingSetupPanel.qml")));
    QVERIFY2(setupComponent.isReady(), qPrintable(setupComponent.errorString()));
    std::unique_ptr<QObject> setupObject(setupComponent.createWithInitialProperties({
        {QStringLiteral("draft"), QVariant::fromValue(static_cast<QObject*>(&draft))},
        {QStringLiteral("theme"), QVariant::fromValue(theme.get())},
        {QStringLiteral("viewResolver"), QVariant::fromValue(static_cast<QObject*>(&controller))},
        {QStringLiteral("width"), 340},
    }));
    QVERIFY2(setupObject != nullptr, qPrintable(setupComponent.errorString()));
    auto* setup = qobject_cast<QQuickItem*>(setupObject.get());
    QVERIFY(setup != nullptr);
    setup->setParentItem(window.contentItem());
    setup->setY(410);

    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 3000);

    auto* partRow = findItemByObjectName(window.contentItem(), QStringLiteral("drawingComponentRow-plate"));
    QVERIFY(partRow != nullptr);
    const auto partCenter = partRow->mapToScene(QPointF(partRow->width() / 2.0, 16.0));
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, partCenter.toPoint());
    QTRY_COMPARE(draft.candidateNodeId(), QStringLiteral("plate"));

    auto* addButton = findItemByObjectName(window.contentItem(), QStringLiteral("drawingAddToQueue"));
    QVERIFY(addButton != nullptr);
    // Nothing can be queued before a view is chosen.
    QVERIFY(!addButton->property("enabled").toBool());

    auto* topButton = findItemByObjectName(window.contentItem(), QStringLiteral("drawingStandardView-Top"));
    auto* frontButton = findItemByObjectName(window.contentItem(), QStringLiteral("drawingStandardView-Front"));
    QVERIFY(topButton != nullptr);
    QVERIFY(frontButton != nullptr);

    const auto clickCentre = [&window](QQuickItem* item) {
        const auto centre = item->mapToScene(QPointF(item->width() / 2.0, item->height() / 2.0));
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, centre.toPoint());
    };

    clickCentre(topButton);
    QTRY_COMPARE(draft.candidateViewLabel(), QStringLiteral("Top"));
    QTRY_VERIFY(addButton->property("enabled").toBool());
    clickCentre(addButton);
    QTRY_COMPARE(draft.queueCount(), 1);

    clickCentre(frontButton);
    QTRY_COMPARE(draft.candidateViewLabel(), QStringLiteral("Front"));
    clickCentre(addButton);
    QTRY_COMPARE(draft.queueCount(), 2);

    // Two rows for one part, with different views and non-colliding filenames.
    QCOMPARE(draft.drawingCountForNode(QStringLiteral("plate")), 2);
    QCOMPARE(draft.planRows().size(), 2);
    const auto firstPath = draft.planRows().at(0).toMap().value(QStringLiteral("path")).toString();
    const auto secondPath = draft.planRows().at(1).toMap().value(QStringLiteral("path")).toString();
    QVERIFY(firstPath != secondPath);
    QVERIFY(draft.planError().isEmpty());
}

namespace {

// Collects QML warnings so a test can assert a specific one is absent.
//
// Needed because the defect this guards against is invisible by construction: a call to a
// non-Q_INVOKABLE method fails as a caught QML TypeError. Nothing crashes, nothing returns
// an error, and the only evidence is a warning on stderr that no assertion was reading.
QStringList g_capturedMessages;
QtMessageHandler g_previousHandler = nullptr;

void capturingMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    g_capturedMessages.append(message);
    if (g_previousHandler) g_previousHandler(type, context, message);
}

} // namespace

void QmlSmokeTest::inspectTreeRevealCallsIntoTheModelWithoutAQmlError()
{
    // Exercises the real reveal path: InspectWorkspace.revealActiveNode() calls
    // assemblyTree.indexForStableId(), then hands the result to TreeView.expandToIndex and
    // TableView.positionViewAtIndex. All three have to accept a QModelIndex from QML.
    loupe::app::ApplicationController controller;
    auto* treeModel = qobject_cast<loupe::app::models::AssemblyTreeModel*>(
        controller.property("assemblyTree").value<QObject*>());
    QVERIFY(treeModel != nullptr);
    treeModel->replaceSnapshot({
        {QStringLiteral("occ-root"), {}, QStringLiteral("assembly"), QStringLiteral("Housing"), 1},
        {QStringLiteral("occ-carrier"), QStringLiteral("occ-root"), QStringLiteral("assembly"), QStringLiteral("Carrier"), 1},
        {QStringLiteral("occ-cover"), QStringLiteral("occ-carrier"), QStringLiteral("part"), QStringLiteral("Cover"), 1},
    });

    QQmlEngine engine;
    QQmlComponent themeComponent(&engine, QUrl::fromLocalFile(
        QStringLiteral(LOUPE_QML_DIR) + QStringLiteral("/Theme.qml")));
    QVERIFY2(themeComponent.isReady(), qPrintable(themeComponent.errorString()));
    std::unique_ptr<QObject> theme(themeComponent.create());
    QVERIFY2(theme != nullptr, qPrintable(themeComponent.errorString()));

    QQmlComponent workspaceComponent(&engine, QUrl::fromLocalFile(
        QStringLiteral(LOUPE_INSPECT_QML_DIR) + QStringLiteral("/InspectWorkspace.qml")));
    QVERIFY2(workspaceComponent.isReady(), qPrintable(workspaceComponent.errorString()));
    std::unique_ptr<QObject> workspaceObject(workspaceComponent.createWithInitialProperties({
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject*>(&controller))},
        {QStringLiteral("theme"), QVariant::fromValue(theme.get())},
        {QStringLiteral("width"), 900},
        {QStringLiteral("height"), 600},
    }));
    QVERIFY2(workspaceObject != nullptr, qPrintable(workspaceComponent.errorString()));
    auto* workspace = qobject_cast<QQuickItem*>(workspaceObject.get());
    QVERIFY(workspace != nullptr);

    QQuickWindow window;
    window.resize(900, 600);
    workspace->setParentItem(window.contentItem());
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 3000);

    // Capture only around the reveal, so unrelated Quick3D warnings from construction under
    // the offscreen platform are not in scope.
    g_capturedMessages.clear();
    g_previousHandler = qInstallMessageHandler(capturingMessageHandler);

    // The workspace reveals on activeNodeIdChanged, via Qt.callLater.
    controller.setActiveNodeId(QStringLiteral("occ-cover"));
    QTest::qWait(200);
    // Called directly as well, so the assertion does not depend on callLater having fired.
    QVERIFY(QMetaObject::invokeMethod(workspaceObject.get(), "revealActiveNode"));
    QTest::qWait(50);

    qInstallMessageHandler(g_previousHandler);
    g_previousHandler = nullptr;

    const auto offending = g_capturedMessages.filter(QStringLiteral("is not a function"))
        + g_capturedMessages.filter(QStringLiteral("indexForStableId"));
    QVERIFY2(offending.isEmpty(), qPrintable(QStringLiteral("QML reveal path reported: ")
                                             + offending.join(QStringLiteral(" | "))));

    // And the reveal genuinely resolved the node rather than bailing out at the valid check.
    QVERIFY(treeModel->indexForStableId(QStringLiteral("occ-cover")).isValid());
}

void QmlSmokeTest::drawingPreviewBuildsDrawablePathsFromProjectedGeometry()
{
    // Guards the failure this was shipped with: the preview reported its extents and contour
    // counts correctly while drawing nothing at all, because the geometry was built with a
    // Repeater over ShapePath. Repeater requires an Item delegate and ShapePath is not one,
    // so the Shape stayed empty and no warning was ever emitted. Asserting on the point
    // lists the Shape consumes is what makes that visible to a test.
    loupe::app::drawing::DrawingWorkspaceController draft;
    draft.replaceSnapshot(QStringLiteral(R"({"effectiveUnit":"mm","sourceToMillimeters":1,"nodes":[
        {"id":"root","name":"Assembly","kind":0,"parentId":""},
        {"id":"plate","name":"Base plate","kind":2,"parentId":"root"}
    ]})"));
    draft.setCandidateNodeId(QStringLiteral("plate"));
    draft.setCandidateStandardView(QStringLiteral("Top"), 0.0, 0.0, 1.0, 0.0, 1.0, 0.0);
    QVERIFY(draft.candidateValid());
    const auto revision = draft.previewRevision();
    QVERIFY(revision > 0);

    QQmlEngine engine;
    QQmlComponent themeComponent(&engine, QUrl::fromLocalFile(
        QStringLiteral(LOUPE_QML_DIR) + QStringLiteral("/Theme.qml")));
    QVERIFY2(themeComponent.isReady(), qPrintable(themeComponent.errorString()));
    std::unique_ptr<QObject> theme(themeComponent.create());
    QVERIFY2(theme != nullptr, qPrintable(themeComponent.errorString()));

    QQmlComponent previewComponent(&engine, QUrl::fromLocalFile(
        QStringLiteral(LOUPE_QML_DIR) + QStringLiteral("/drawing/DrawingPreview2D.qml")));
    QVERIFY2(previewComponent.isReady(), qPrintable(previewComponent.errorString()));
    std::unique_ptr<QObject> previewObject(previewComponent.createWithInitialProperties({
        {QStringLiteral("draft"), QVariant::fromValue(static_cast<QObject*>(&draft))},
        {QStringLiteral("theme"), QVariant::fromValue(theme.get())},
        {QStringLiteral("width"), 360},
        {QStringLiteral("height"), 420},
    }));
    QVERIFY2(previewObject != nullptr, qPrintable(previewComponent.errorString()));
    auto* previewItem = qobject_cast<QQuickItem*>(previewObject.get());
    QVERIFY(previewItem != nullptr);

    QQuickWindow window;
    window.resize(400, 460);
    previewItem->setParentItem(window.contentItem());
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 3000);

    // One closed 10x20 rectangle and one open two-segment contour, in the shape the worker
    // sends back.
    const auto previewJson = QStringLiteral(R"({"schemaVersion":1,"widthMm":10,"heightMm":20,
        "minX":0,"minY":0,"empty":false,"closedContours":1,"openContours":1,
        "layers":[{"name":"cut","role":"cut","contours":[
            {"closed":true,"points":[0,0,10,0,10,20,0,20,0,0]},
            {"closed":false,"points":[2,2,8,2,8,10]}]}],
        "warnings":[{"code":"open_contour","count":1}]})");
    QVERIFY(QMetaObject::invokeMethod(previewObject.get(), "applyPreview",
                                      Q_ARG(QVariant, previewJson),
                                      Q_ARG(QVariant, revision),
                                      Q_ARG(QVariant, false)));

    QTRY_VERIFY(previewObject->property("hasGeometry").toBool());
    QCOMPARE(previewObject->property("previewRevision").toInt(), revision);

    // The two stroke styles are separate, so an open contour can be drawn as a warning
    // rather than being indistinguishable from a cuttable one.
    const auto closedPaths = previewObject->property("closedPaths").toList();
    const auto openPaths = previewObject->property("openPaths").toList();
    QCOMPARE(closedPaths.size(), 1);
    QCOMPARE(openPaths.size(), 1);
    // Five points for the rectangle, three for the open chain: the polylines actually carry
    // geometry rather than being empty placeholders.
    QCOMPARE(closedPaths.at(0).toList().size(), 5);
    QCOMPARE(openPaths.at(0).toList().size(), 3);

    // Extents are stated, since they are what a user checks a 1:1 drawing against.
    auto* extents = findItemByObjectName(window.contentItem(), QStringLiteral("drawingPreviewExtents"));
    QVERIFY(extents != nullptr);
    QVERIFY(extents->property("text").toString().contains(QStringLiteral("10.00")));
    QVERIFY(extents->property("text").toString().contains(QStringLiteral("20.00")));

    // A superseded candidate must blank the geometry rather than leave it looking current.
    draft.setCandidateStandardView(QStringLiteral("Front"), 0.0, -1.0, 0.0, 0.0, 0.0, 1.0);
    QTRY_VERIFY(!previewObject->property("hasGeometry").toBool());
    QVERIFY(previewObject->property("closedPaths").toList().isEmpty());
}

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    QmlSmokeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "qml_smoke.moc"
