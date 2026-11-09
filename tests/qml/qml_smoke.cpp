#include "app/ApplicationController.h"
#include "app/DocumentTabs.h"
#include "app/drawing/DrawingWorkspaceController.h"
#include "app/export/ExportWorkspaceController.h"
#include "app/models/AssemblyTreeModel.h"
#include "app/models/ThemePreference.h"
#include "app/platform/FileAssociation.h"
#include "app/platform/UpdateController.h"
#include "app/platform/WindowManager.h"
#include "app/platform/WindowChrome.h"
#include "app/render/CadEdgeGeometry.h"
#include "app/render/HandleRingGeometry.h"
#include "app/render/ExplodeLineGeometry.h"
#include "app/render/MeshGeometry.h"
#include "app/tools/CaptureController.h"
#include "app/tools/MeasurementController.h"
#include "app/tools/SectionController.h"
#include "protocol/GeometryPayload.h"

#include <functional>
#include <QColor>
#include <QDeadlineTimer>
#include <QDir>
#include <QFile>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGuiApplication>
#include <QMimeData>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlExpression>
#include <QJsonDocument>
#include <QJSValue>
#include <QQuaternion>
#include <QVector4D>
#include <QJsonObject>
#include <QQmlPropertyMap>
#include <QQuickItem>
#include <QQuickWindow>
#include <QPointingDevice>
#include <QTemporaryDir>
#include <QScopeGuard>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QStandardPaths>
#include <qqml.h>

#include <algorithm>
#include <tuple>
#include <cmath>
#include <limits>
#include <numbers>

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
    // Before mainLoads: that test's synthetic drop, sent to a window it then destroys,
    // leaves pointer state that stops a later window's DragHandler activating. The same
    // drop into this test's own window does not (checked below).
    void tabsDragToReorderAndOffTheStripIntoANewWindow();
    void mainLoads();
    void sidePanelsDragWithinTheirLimitsAndRemember();
    void exportPickerCheckboxUpdatesDraft();
    void inspectionTaskPanelsLoad();
    void captureProgressStaysInCapturePanel();
    void measurementFaceHighlightLoadsTopology();
    void sectionViewActivatesWithRenderedMesh();
    void viewportCaptureUsesRequestedRenderResolution();
    void aCaptureHoldsTheMeasurementOnlyWhenAskedAndWhereItIs();
    void aRoundFaceOffersItsAxisAndACornerStillWins();
    void aMovedPartIsDrawnWithItsExplodeLine();
    void aFingerGetsBiggerTargetsAndALongPressOpensTheMenu();
    void viewportDrawsAFacingFaceInItsPickedColour();
    void viewportRightClickKeepsTheSelectionAndViewsNormalToAFace();
    void explodedViewHandlesMoveAndTurnTheSelection();
    void explodePanelTypesPositionsAndExplodesTheSelectedSubAssembly();
    void floatingToolWindowDragsMinimisesAndStaysInside();
    void updateIndicatorShowsTheDownloadAsARing();
    void whatsNewListsTheReleaseAndIsMarkedSeenWhenClosed();
    void everyWorkspaceSidePanelGoesDownToTheSameMinimum();
    void drawingWorkspaceQueuesOnePartAtSeveralViews();
    void inspectTreeRevealCallsIntoTheModelWithoutAQmlError();
    void inspectTreeTakesModifierClicksAndShiftRanges();
    void drawingPreviewBuildsDrawablePathsFromProjectedGeometry();
    void drawingPreviewOrientationAndScaleSurviveTheScreenTransform();
    void drawingPreviewZoomKeepsThePointUnderTheCursorFixed();
    void drawingQueueThumbnailFlattensTheCapturedPreview();
};

void QmlSmokeTest::initTestCase()
{
    qmlRegisterUncreatableMetaObject(loupe::app::staticMetaObject, "Loupe.App", 1, 0, "AppState", "Application state only");
    qmlRegisterType<loupe::app::ApplicationController>("Loupe.App", 1, 0, "ApplicationController");
    qmlRegisterType<loupe::app::DocumentTabs>("Loupe.App", 1, 0, "DocumentTabs");
    qmlRegisterType<loupe::app::platform::FileAssociation>("Loupe.App", 1, 0, "FileAssociation");
    static loupe::app::platform::WindowManager windowManager;
    qmlRegisterSingletonInstance("Loupe.App", 1, 0, "Windows", &windowManager);
    qmlRegisterType<loupe::app::models::ThemePreference>("Loupe.App", 1, 0, "ThemePreference");
    qmlRegisterType<loupe::app::platform::WindowChrome>("Loupe.App", 1, 0, "WindowChrome");
    qmlRegisterType<loupe::app::render::MeshGeometry>("Loupe.App", 1, 0, "MeshGeometry");
    qmlRegisterType<loupe::app::render::CadEdgeGeometry>("Loupe.App", 1, 0, "CadEdgeGeometry");
    qmlRegisterType<loupe::app::render::HandleRingGeometry>("Loupe.App", 1, 0, "HandleRingGeometry");
    qmlRegisterType<loupe::app::render::ExplodeLineGeometry>("Loupe.App", 1, 0, "ExplodeLineGeometry");
    // One per engine (the app has one). Checks GitHub for a newer release at launch (#3).
    qmlRegisterSingletonType<loupe::app::platform::UpdateController>("Loupe.App", 1, 0, "Updates",
        [](QQmlEngine*, QJSEngine*) -> QObject* { return new loupe::app::platform::UpdateController; });
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

void QmlSmokeTest::sidePanelsDragWithinTheirLimitsAndRemember()
{
    // Three zones as every workspace lays them out: a picker on the left, a centre with a
    // minimum, settings on the right. Both sides drag, within limits, and remember.
    loupe::app::models::ThemePreference preferences;
    preferences.clearPanelWidth(QStringLiteral("smokePicker"));
    preferences.clearPanelWidth(QStringLiteral("smokeSettings"));
    const auto cleanup = qScopeGuard([&] {
        preferences.clearPanelWidth(QStringLiteral("smokePicker"));
        preferences.clearPanelWidth(QStringLiteral("smokeSettings"));
    });

    QQmlEngine engine;
    QQmlComponent themeComponent(&engine, QUrl::fromLocalFile(QStringLiteral(LOUPE_QML_DIR) + QStringLiteral("/Theme.qml")));
    std::unique_ptr<QObject> theme(themeComponent.create());
    QVERIFY2(theme != nullptr, qPrintable(themeComponent.errorString()));

    QQmlComponent component(&engine);
    component.setData(QByteArrayLiteral(R"(
import QtQuick
import QtQuick.Layouts
import "inspect" as Inspect
Item {
    id: root
    property QtObject theme
    property QtObject preferences
    readonly property real sidePanelRoom: width - 4 * 12 - 420
    RowLayout {
        id: row
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12
        Rectangle { id: left; objectName: "left"; Layout.fillHeight: true; Layout.preferredWidth: leftSplitter.effectiveWidth }
        Rectangle { objectName: "centre"; Layout.fillHeight: true; Layout.fillWidth: true; Layout.minimumWidth: 420 }
        Rectangle { id: right; objectName: "right"; Layout.fillHeight: true; Layout.preferredWidth: rightSplitter.effectiveWidth }
    }
    Inspect.PanelSplitter {
        id: leftSplitter; objectName: "leftSplitter"; theme: root.theme; preferences: root.preferences
        key: "smokePicker"; panel: left; gap: row.spacing
        defaultWidth: 290; minimumWidth: 220
        maximumWidth: Math.min(560, root.sidePanelRoom - rightSplitter.staticWidth)
    }
    Inspect.PanelSplitter {
        id: rightSplitter; objectName: "rightSplitter"; theme: root.theme; preferences: root.preferences
        key: "smokeSettings"; panel: right; panelOnLeft: false; gap: row.spacing
        defaultWidth: 360; minimumWidth: 300
        maximumWidth: Math.min(640, root.sidePanelRoom - leftSplitter.effectiveWidth)
    }
}
)"), QUrl::fromLocalFile(QStringLiteral(LOUPE_QML_DIR) + QStringLiteral("/splitter-harness.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> object(component.createWithInitialProperties({
        {QStringLiteral("theme"), QVariant::fromValue(theme.get())},
        {QStringLiteral("preferences"), QVariant::fromValue(static_cast<QObject*>(&preferences))},
    }));
    QVERIFY2(object != nullptr, qPrintable(component.errorString()));
    auto* harness = qobject_cast<QQuickItem*>(object.get());
    QQuickWindow window;
    window.resize(1400, 600);
    harness->setParentItem(window.contentItem());
    harness->setSize(QSizeF(1400, 600));
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 3000);

    auto* left = findItemByObjectName(harness, QStringLiteral("left"));
    auto* right = findItemByObjectName(harness, QStringLiteral("right"));
    auto* centre = findItemByObjectName(harness, QStringLiteral("centre"));
    auto* leftSplitter = findItemByObjectName(harness, QStringLiteral("leftSplitter"));
    auto* rightSplitter = findItemByObjectName(harness, QStringLiteral("rightSplitter"));
    QVERIFY(left && right && centre && leftSplitter && rightSplitter);
    QTRY_COMPARE(left->width(), 290.0);
    QCOMPARE(right->width(), 360.0);

    const auto drag = [&](QQuickItem* splitter, const qreal dx) {
        const auto start = splitter->mapToScene(QPointF(splitter->width() / 2.0, splitter->height() / 2.0)).toPoint();
        QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, start);
        for (int step = 1; step <= 5; ++step)
            QTest::mouseMove(&window, start + QPoint(qRound(dx * step / 5.0), 0));
        QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, start + QPoint(qRound(dx), 0));
    };
    // The divider sits in the gap, not over either panel.
    QVERIFY(leftSplitter->x() >= left->x() + left->width() + 12 - leftSplitter->width());
    QVERIFY(leftSplitter->x() + leftSplitter->width() <= centre->x() + 12);

    drag(leftSplitter, 100);
    QTRY_COMPARE(left->width(), 390.0);
    QCOMPARE(preferences.panelWidths().value(QStringLiteral("smokePicker")).toInt(), 390);
    // A right-hand panel widens leftward.
    drag(rightSplitter, -80);
    QTRY_COMPARE(right->width(), 440.0);
    QCOMPARE(preferences.panelWidths().value(QStringLiteral("smokeSettings")).toInt(), 440);

    // Held to its limit, and the centre keeps its minimum.
    drag(leftSplitter, 2000);
    QTRY_COMPARE(left->width(), 1400.0 - 4 * 12 - 420 - 440);
    QCOMPARE(centre->width(), 420.0);
    drag(leftSplitter, -2000);
    QTRY_COMPARE(left->width(), 220.0);

    // A narrower window narrows the panels rather than the centre; the widths come back.
    drag(leftSplitter, 150);
    QTRY_COMPARE(left->width(), 370.0);
    harness->setWidth(1100);
    QTRY_COMPARE(left->width(), 220.0);
    QTRY_COMPARE(right->width(), 1100.0 - 4 * 12 - 420 - 220);
    QCOMPARE(centre->width(), 420.0);
    QCOMPARE(preferences.panelWidths().value(QStringLiteral("smokePicker")).toInt(), 370);
    harness->setWidth(1400);
    QTRY_COMPARE(left->width(), 370.0);
    QTRY_COMPARE(right->width(), 440.0);

    // A double click puts a panel back at its default and forgets the width.
    const auto leftCentre = leftSplitter->mapToScene(QPointF(leftSplitter->width() / 2.0, leftSplitter->height() / 2.0)).toPoint();
    QTest::mouseDClick(&window, Qt::LeftButton, Qt::NoModifier, leftCentre);
    QTRY_COMPARE(left->width(), 290.0);
    QVERIFY(!preferences.panelWidths().contains(QStringLiteral("smokePicker")));

    // Remembered: a new preferences object reads the stored width back.
    loupe::app::models::ThemePreference reloaded;
    QCOMPARE(reloaded.panelWidths().value(QStringLiteral("smokeSettings")).toInt(), 440);
}

void QmlSmokeTest::tabsDragToReorderAndOffTheStripIntoANewWindow()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(LOUPE_MAIN_QML_PATH)));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> object(component.create());
    QVERIFY2(object != nullptr, qPrintable(component.errorString()));
    auto* window = qobject_cast<QQuickWindow*>(object.get());
    QTRY_VERIFY_WITH_TIMEOUT(window->isExposed(), 3000);
    auto* tabs = qobject_cast<loupe::app::DocumentTabs*>(object->property("documents").value<QObject*>());
    QVERIFY(tabs != nullptr);
    auto* first = tabs->active();
    auto* second = tabs->add();
    auto* third = tabs->add();

    const auto tabItems = [&] {
        QList<QQuickItem*> items;
        std::function<void(QQuickItem*)> walk = [&](QQuickItem* item) {
            if (item->objectName() == QStringLiteral("documentTab") && item->isVisible()) items.append(item);
            for (auto* child : item->childItems()) walk(child);
        };
        walk(window->contentItem());
        std::sort(items.begin(), items.end(), [](QQuickItem* a, QQuickItem* b) { return a->mapToScene({}).x() < b->mapToScene({}).x(); });
        return items;
    };
    QTRY_COMPARE_WITH_TIMEOUT(tabItems().size(), 3, 3000);
    const auto centreOf = [](QQuickItem* item) {
        return item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
    };
    // Where every tab sits, and the window's size: a drag aimed at a tab must start once
    // these stop changing. A fixed wait is not enough on a fresh machine, where the first
    // text layout (cold font caches) and the window settling can run on past it.
    const auto tabLayout = [&] {
        QStringList parts{QStringLiteral("%1x%2").arg(window->width()).arg(window->height())};
        for (auto* item : tabItems()) {
            const auto rect = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
            parts << QStringLiteral("%1,%2 %3x%4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
        }
        return parts.join(QStringLiteral("; "));
    };
    const auto tabsLaidOut = [&] {
        // A strip not yet laid out has every tab at 0 x 0, which is also perfectly still: a
        // fresh macOS runner sat like that past the first checks, and the drag missed.
        const auto items = tabItems();
        return !items.isEmpty() && std::all_of(items.cbegin(), items.cend(), [](QQuickItem* item) {
            return item->width() > 0 && item->height() > 0;
        });
    };
    const auto waitForSettledTabs = [&] {
        QString previous;
        QDeadlineTimer deadline(8000);
        while (!deadline.hasExpired()) {
            QTest::qWait(100);
            if (!tabsLaidOut()) {
                previous.clear();
                continue;
            }
            const auto current = tabLayout();
            if (current == previous) return true;
            previous = current;
        }
        return false;
    };
    const auto tabOrder = [&] {
        QStringList order;
        for (int i = 0; i < tabs->count(); ++i) {
            const auto* document = tabs->documentAt(i);
            order << (document == first ? QStringLiteral("first") : document == second ? QStringLiteral("second")
                                                                 : document == third ? QStringLiteral("third")
                                                                                     : QStringLiteral("other"));
        }
        return order.join(QStringLiteral(", "));
    };
    const auto drag = [&](const QPoint from, const QPoint to) {
        QTest::mousePress(window, Qt::LeftButton, {}, from);
        for (int step = 1; step <= 12; ++step) {
            QTest::mouseMove(window, from + (to - from) * step / 12, 16);
        }
        QTest::mouseRelease(window, Qt::LeftButton, {}, to);
        QTest::qWait(100);
    };

    // Dropping a file must not stop tabs dragging afterwards.
    {
        QMimeData mimeData;
        mimeData.setUrls({QUrl(QStringLiteral("file:///tmp/missing-dropped-model.step"))});
        QDragEnterEvent enterEvent(QPoint{window->width() / 2, window->height() / 2}, Qt::CopyAction, &mimeData, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &enterEvent);
        QDropEvent dropEvent(QPointF{window->width() / 2.0, window->height() / 2.0}, Qt::CopyAction, &mimeData, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &dropEvent);
    }
    // Drag the first tab past the second: they swap.
    QVERIFY2(waitForSettledTabs(), qPrintable(QStringLiteral("tabs never settled: ") + tabLayout()));
    auto items = tabItems();
    const auto layoutBeforeReorder = tabLayout();
    drag(centreOf(items.at(0)), centreOf(items.at(1)) + QPoint(items.at(1)->width() / 2 - 2, 0));
    QVERIFY2(tabs->documentAt(1) == first && tabs->documentAt(0) == second,
             qPrintable(QStringLiteral("order after the drag: %1; tabs before it: %2; tabs now: %3")
                            .arg(tabOrder(), layoutBeforeReorder, tabLayout())));

    // Drag a tab well below the strip and let go: it leaves for a window of its own.
    const auto windowsBefore = QGuiApplication::topLevelWindows().size();
    items = tabItems();
    drag(centreOf(items.at(2)), centreOf(items.at(2)) + QPoint(0, 260));
    QTRY_COMPARE_WITH_TIMEOUT(tabs->count(), 2, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(QGuiApplication::topLevelWindows().size() > windowsBefore, 3000);
    QQuickWindow* opened = nullptr;
    for (auto* candidate : QGuiApplication::topLevelWindows())
        if (candidate != window && candidate->property("documents").isValid()) opened = qobject_cast<QQuickWindow*>(candidate);
    QVERIFY(opened != nullptr);
    auto* openedTabs = qobject_cast<loupe::app::DocumentTabs*>(opened->property("documents").value<QObject*>());
    QCOMPARE(openedTabs->count(), 1);
    QCOMPARE(openedTabs->active(), third);

    // Side by side, then drag a tab from the first window onto the second one's toolbar
    // and tab area: it moves there.
    window->setPosition(0, 0);
    window->resize(700, 500);
    opened->setPosition(760, 0);
    opened->resize(700, 500);
    QVERIFY2(waitForSettledTabs(), qPrintable(QStringLiteral("tabs never settled: ") + tabLayout()));
    const auto onOpened = opened->contentItem()->mapToGlobal(QPointF(300, 0)).toPoint() + QPoint(0, 40);
    items = tabItems();
    const auto moving = tabs->documentAt(0);
    const auto start = centreOf(items.at(0));
    // While the tab is over the other window, that window marks where it would land, and
    // stops once it is let go.
    auto* dropIndicator = findItemByObjectName(opened->contentItem(), QStringLiteral("tabDropIndicator"));
    QVERIFY(dropIndicator != nullptr);
    QVERIFY(!dropIndicator->isVisible());
    QTest::mousePress(window, Qt::LeftButton, {}, start);
    for (int step = 1; step <= 12; ++step)
        QTest::mouseMove(window, start + (onOpened - window->position() - start) * step / 12, 16);
    QTRY_VERIFY_WITH_TIMEOUT(dropIndicator->isVisible(), 3000);
    QTest::mouseRelease(window, Qt::LeftButton, {}, onOpened - window->position());
    QTest::qWait(100);
    QTRY_VERIFY_WITH_TIMEOUT(!dropIndicator->isVisible(), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(openedTabs->count(), 2, 3000);
    QCOMPARE(tabs->count(), 1);
    QVERIFY(openedTabs->documentAt(0) == moving || openedTabs->documentAt(1) == moving);

    // A window's last tab can go too, and its window closes behind it. A lone empty tab
    // shows no strip to drag from, so give it a file first (an empty one is enough to name
    // the tab; it is never read as geometry here).
    QTemporaryDir files;
    const auto named = QDir(files.path()).filePath(QStringLiteral("last.step"));
    { QFile file(named); QVERIFY(file.open(QIODevice::WriteOnly)); }
    tabs->documentAt(0)->openFile(QUrl::fromLocalFile(named));
    QTRY_COMPARE_WITH_TIMEOUT(tabItems().size(), 1, 3000);
    QVERIFY2(waitForSettledTabs(), qPrintable(QStringLiteral("tabs never settled: ") + tabLayout()));
    items = tabItems();
    const auto last = tabs->documentAt(0);
    drag(centreOf(items.at(0)), onOpened - window->position());
    QTRY_COMPARE_WITH_TIMEOUT(openedTabs->count(), 3, 3000);
    QVERIFY(openedTabs->documentAt(0) == last || openedTabs->documentAt(1) == last || openedTabs->documentAt(2) == last);
    QTRY_VERIFY_WITH_TIMEOUT(!window->isVisible(), 3000);
    opened->close();
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
    auto theme = std::make_unique<QQmlPropertyMap>();
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

    // View normal to section looks at the cut: from the side the section takes away, which
    // is against the normal while the positive side is kept.
    const auto normal = section->effectiveNormal();
    QVERIFY(QMetaObject::invokeMethod(viewport, "alignToSection", Q_ARG(QVariant, false)));
    auto direction = viewport->property("viewDirection").value<QVector3D>();
    QVERIFY2((direction + normal).length() < 1e-3F,
             qPrintable(QStringLiteral("%1 %2 %3").arg(direction.x()).arg(direction.y()).arg(direction.z())));
    // The opposite side keeps the other half and looks at its cut.
    QVERIFY(QMetaObject::invokeMethod(viewport, "alignToSection", Q_ARG(QVariant, true)));
    QVERIFY(section->flipped());
    direction = viewport->property("viewDirection").value<QVector3D>();
    QVERIFY((direction - normal).length() < 1e-3F);
}

void QmlSmokeTest::viewportDrawsAFacingFaceInItsPickedColour()
{
    // The inspector swatch shows the colour a part is given; a face pointed at the viewer
    // must render that colour. It did not: the head-on key light and fill put ~1.5x the
    // colour on such a face and a specular hot spot washed it toward white (#3070C0 drew
    // as #9BC0FF). A sphere's centre always faces the camera, whatever the view.
    auto theme = std::make_unique<QQmlPropertyMap>();
    for (const auto* key : {"surface", "surfaceRaised", "surfaceSubtle", "control", "border", "onSurface", "foreground",
                            "muted", "accent", "selection", "selectedBody", "selectedEdge", "edge", "neutralBody"})
        theme->insert(QString::fromLatin1(key), QColor(QStringLiteral("#808080")));
    theme->insert(QStringLiteral("viewport"), QColor(QStringLiteral("#000000")));
    theme->insert(QStringLiteral("dark"), false);

    loupe::app::ApplicationController controller;
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(LOUPE_INSPECT_QML_DIR) + QStringLiteral("/StepViewport.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    const QVariantMap initialProperties{
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject*>(&controller))},
        {QStringLiteral("theme"), QVariant::fromValue(static_cast<QObject*>(theme.get()))},
        {QStringLiteral("presentationOnly"), true},
    };
    std::unique_ptr<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(object != nullptr, qPrintable(component.errorString()));
    auto* viewport = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(viewport != nullptr);
    QQuickWindow window;
    window.resize(400, 400);
    viewport->setParentItem(window.contentItem());
    viewport->setSize(QSizeF(400, 400));
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 3000);

    loupe::protocol::MeshPayload sphere;
    sphere.requestId = 1;
    sphere.nodeId = sphere.definitionId = QStringLiteral("sphere");
    sphere.segmentKey = QStringLiteral("sphere:0");
    constexpr int rings = 48;
    constexpr int sectors = 96;
    for (int ring = 0; ring <= rings; ++ring) {
        for (int sector = 0; sector <= sectors; ++sector) {
            const double theta = std::numbers::pi * ring / rings;
            const double phi = 2.0 * std::numbers::pi * sector / sectors;
            const auto x = static_cast<float>(std::sin(theta) * std::cos(phi));
            const auto y = static_cast<float>(std::cos(theta));
            const auto z = static_cast<float>(std::sin(theta) * std::sin(phi));
            sphere.vertices << x * 10.0F << y * 10.0F << z * 10.0F;
            sphere.normals << x << y << z;
        }
    }
    for (quint32 ring = 0; ring < rings; ++ring) {
        for (quint32 sector = 0; sector < sectors; ++sector) {
            const quint32 first = ring * (sectors + 1) + sector;
            const quint32 below = first + sectors + 1;
            sphere.indices << first << first + 1 << below << below << first + 1 << below + 1;
        }
    }
    const auto payload = loupe::protocol::encodeGeometry(sphere);

    for (const auto& picked : {QStringLiteral("#3070C0"), QStringLiteral("#9AA2B2"), QStringLiteral("#E0C040"),
                               QStringLiteral("#202020")}) {
        QVERIFY(QMetaObject::invokeMethod(&controller, "meshReady", Q_ARG(QString, sphere.nodeId),
                                          Q_ARG(QString, sphere.segmentKey), Q_ARG(QString, picked),
                                          Q_ARG(QByteArray, payload)));
        // Polled rather than read after a fixed wait: a slow runner may not have drawn the new
        // colour yet, and a fixed wait made this fail intermittently on Windows CI.
        const QColor expected(picked);
        QColor rendered;
        const auto difference = [&] {
            const auto image = window.grabWindow();
            rendered = image.pixelColor(image.width() / 2, image.height() / 2);
            return std::max({std::abs(rendered.red() - expected.red()), std::abs(rendered.green() - expected.green()),
                             std::abs(rendered.blue() - expected.blue())});
        };
        QTRY_VERIFY_WITH_TIMEOUT(difference() <= 8, 5000);
        QVERIFY2(difference() <= 8, qPrintable(QStringLiteral("%1 rendered as %2").arg(picked, rendered.name())));
    }
}

void QmlSmokeTest::viewportRightClickKeepsTheSelectionAndViewsNormalToAFace()
{
    auto theme = std::make_unique<QQmlPropertyMap>();
    for (const auto* key : {"surface", "surfaceRaised", "surfaceSubtle", "control", "border", "onSurface", "foreground",
                            "muted", "accent", "selection", "selectedBody", "selectedEdge", "edge", "neutralBody"})
        theme->insert(QString::fromLatin1(key), QColor(QStringLiteral("#808080")));
    theme->insert(QStringLiteral("viewport"), QColor(QStringLiteral("#000000")));
    theme->insert(QStringLiteral("dark"), false);

    loupe::app::ApplicationController controller;
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(LOUPE_INSPECT_QML_DIR) + QStringLiteral("/StepViewport.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> object(component.createWithInitialProperties({
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject*>(&controller))},
        {QStringLiteral("theme"), QVariant::fromValue(static_cast<QObject*>(theme.get()))}}));
    QVERIFY2(object != nullptr, qPrintable(component.errorString()));
    auto* viewport = qobject_cast<QQuickItem*>(object.get());
    QQuickWindow window;
    window.resize(400, 400);
    viewport->setParentItem(window.contentItem());
    viewport->setSize(QSizeF(400, 400));
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 3000);

    // A 20 mm square plate tilted 30 degrees about x, centred on the origin, as one face.
    const auto tilt = std::numbers::pi / 6.0;
    const auto c = static_cast<float>(std::cos(tilt));
    const auto sn = static_cast<float>(std::sin(tilt));
    loupe::protocol::MeshPayload plate;
    plate.requestId = 1;
    plate.nodeId = plate.definitionId = QStringLiteral("plate");
    plate.segmentKey = QStringLiteral("plate:0");
    for (const auto& [u, v] : {std::pair{-10.0F, -10.0F}, {10.0F, -10.0F}, {10.0F, 10.0F}, {-10.0F, 10.0F}}) {
        plate.vertices << u << v * c << v * sn;
        plate.normals << 0.0F << -sn << c;
    }
    plate.indices = {0, 1, 2, 0, 2, 3};
    plate.topology = {{5, loupe::protocol::TopologyKind::Face, 0, 6, 400.0F, 0.0F}};
    QVERIFY(QMetaObject::invokeMethod(&controller, "meshReady", Q_ARG(QString, plate.nodeId), Q_ARG(QString, plate.segmentKey),
                                      Q_ARG(QString, QStringLiteral("#3070C0")), Q_ARG(QByteArray, loupe::protocol::encodeGeometry(plate))));
    QVERIFY(QMetaObject::invokeMethod(viewport, "fitCamera"));
    QTRY_VERIFY_WITH_TIMEOUT(viewport->property("hasSceneBounds").toBool(), 3000);

    const auto contextPickAt = [&](const double x, const double y) {
        QVariant hit;
        QMetaObject::invokeMethod(viewport, "contextPickAt", Q_RETURN_ARG(QVariant, hit), Q_ARG(QVariant, x), Q_ARG(QVariant, y));
        return hit.toBool();
    };
    // Right-clicking a part that is in a multi-selection keeps the whole selection.
    controller.selectNode(QStringLiteral("plate"), false);
    controller.selectNode(QStringLiteral("other"), true);
    QTRY_VERIFY_WITH_TIMEOUT(contextPickAt(200, 200), 3000);
    QCOMPARE(controller.selectedNodeIds(), (QStringList{QStringLiteral("plate"), QStringLiteral("other")}));
    // Empty space leaves it alone too.
    QVERIFY(!contextPickAt(2, 2));
    QCOMPARE(controller.selectedNodeIds().size(), 2);
    // A part outside the selection replaces it, as before.
    controller.selectNode(QStringLiteral("other"), false);
    QVERIFY(contextPickAt(200, 200));
    QCOMPARE(controller.selectedNodeIds(), QStringList{QStringLiteral("plate")});

    // Normal to face: straight down the plate's normal, then its other side.
    const QVector3D normal(0.0F, -sn, c);
    QVariant done;
    QVERIFY(QMetaObject::invokeMethod(viewport, "viewNormalToFaceAt", Q_RETURN_ARG(QVariant, done), Q_ARG(QVariant, 200.0), Q_ARG(QVariant, 200.0)));
    QVERIFY(done.toBool());
    auto direction = viewport->property("viewDirection").value<QVector3D>();
    QVERIFY2((direction - normal).length() < 1.0e-4F, qPrintable(QStringLiteral("%1 %2 %3").arg(direction.x()).arg(direction.y()).arg(direction.z())));
    QVERIFY(QMetaObject::invokeMethod(viewport, "viewNormalToFaceAt", Q_RETURN_ARG(QVariant, done), Q_ARG(QVariant, 200.0), Q_ARG(QVariant, 200.0)));
    direction = viewport->property("viewDirection").value<QVector3D>();
    QVERIFY((direction + normal).length() < 1.0e-4F);

    // A saved camera view: going back to it after turning away glides the camera home, and
    // it stays marked as current until the camera moves again.
    QStandardPaths::setTestModeEnabled(true);
    controller.savedViews().setDocument(QStringLiteral("/tmp/loupe-smoke-views.step"));
    auto* navigation = viewport->findChild<QObject*>(QStringLiteral("viewportNavigation"));
    QVERIFY(navigation != nullptr);
    QVariant savedId;
    QVERIFY(QMetaObject::invokeMethod(viewport, "saveCameraView", Q_RETURN_ARG(QVariant, savedId), Q_ARG(QVariant, QStringLiteral("Head on"))));
    QVERIFY(!savedId.toString().isEmpty());
    QCOMPARE(viewport->property("activeCameraViewId").toString(), savedId.toString());
    const auto savedOrientation = navigation->property("orientation").value<QQuaternion>();
    QVERIFY(QMetaObject::invokeMethod(navigation, "orbit", Q_ARG(QVariant, 120.0), Q_ARG(QVariant, 40.0)));
    QVERIFY(viewport->property("activeCameraViewId").toString().isEmpty());
    QVERIFY(qAbs(QQuaternion::dotProduct(navigation->property("orientation").value<QQuaternion>(), savedOrientation)) < 0.999F);
    QVariant went;
    QVERIFY(QMetaObject::invokeMethod(viewport, "goToCameraView", Q_RETURN_ARG(QVariant, went), Q_ARG(QVariant, savedId)));
    QVERIFY(went.toBool());
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(QQuaternion::dotProduct(navigation->property("orientation").value<QQuaternion>(), savedOrientation)) > 0.99999F, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(viewport->property("activeCameraViewId").toString(), savedId.toString(), 3000);
    QCOMPARE(controller.savedViews().cameraViews().constFirst().toMap().value(QStringLiteral("summary")).toString(), QStringLiteral("Orthographic"));
    controller.savedViews().remove(savedId.toString());

    // The empty-space menu is exactly as tall as the entries it shows (the exploded-view
    // entry is hidden here), and as wide as the longest one, so nothing is cut short.
    auto* menu = viewport->findChild<QObject*>(QStringLiteral("viewportContextMenu"));
    QVERIFY(menu != nullptr);
    QVERIFY(QMetaObject::invokeMethod(menu, "popup", Q_ARG(QPointF, QPointF(20, 20))));
    QTRY_VERIFY_WITH_TIMEOUT(menu->property("opened").toBool(), 3000);
    const auto count = menu->property("count").toInt();
    double shownHeight = 0;
    int shown = 0;
    for (int i = 0; i < count; ++i) {
        QQuickItem* item = nullptr;
        QVERIFY(QMetaObject::invokeMethod(menu, "itemAt", Q_RETURN_ARG(QQuickItem*, item), Q_ARG(int, i)));
        QVERIFY(item != nullptr);
        if (!item->isVisible()) {
            QCOMPARE(item->height(), 0.0);
            continue;
        }
        ++shown;
        shownHeight += item->height();
        QVERIFY2(item->width() >= item->implicitWidth(), qPrintable(item->property("text").toString()));
    }
    QCOMPARE(shown, count - 1);
    QTRY_COMPARE_WITH_TIMEOUT(menu->property("height").toDouble(),
                              shownHeight + menu->property("topPadding").toDouble() + menu->property("bottomPadding").toDouble(), 3000);
    QVERIFY(QMetaObject::invokeMethod(menu, "close"));
}

void QmlSmokeTest::explodedViewHandlesMoveAndTurnTheSelection()
{
    auto theme = std::make_unique<QQmlPropertyMap>();
    for (const auto* key : {"surface", "surfaceRaised", "surfaceSubtle", "control", "border", "onSurface", "foreground",
                            "muted", "accent", "selection", "selectedBody", "selectedEdge", "edge", "neutralBody"})
        theme->insert(QString::fromLatin1(key), QColor(QStringLiteral("#808080")));
    theme->insert(QStringLiteral("axisX"), QColor(QStringLiteral("#D92D20")));
    theme->insert(QStringLiteral("axisY"), QColor(QStringLiteral("#079455")));
    theme->insert(QStringLiteral("axisZ"), QColor(QStringLiteral("#4F46E5")));
    theme->insert(QStringLiteral("handleActive"), QColor(QStringLiteral("#EAAA08")));
    theme->insert(QStringLiteral("viewport"), QColor(QStringLiteral("#000000")));
    theme->insert(QStringLiteral("dark"), false);
    theme->insert(QStringLiteral("durSpatial"), 0);

    loupe::app::ApplicationController controller;
    auto& explode = controller.explodeController();
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(LOUPE_INSPECT_QML_DIR) + QStringLiteral("/StepViewport.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> object(component.createWithInitialProperties({
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject*>(&controller))},
        {QStringLiteral("theme"), QVariant::fromValue(static_cast<QObject*>(theme.get()))},
        {QStringLiteral("explodedLayoutEnabled"), true},
        {QStringLiteral("explodeToolActive"), true}}));
    QVERIFY2(object != nullptr, qPrintable(component.errorString()));
    auto* viewport = qobject_cast<QQuickItem*>(object.get());
    QQuickWindow window;
    window.resize(600, 600);
    viewport->setParentItem(window.contentItem());
    viewport->setSize(QSizeF(600, 600));
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 3000);

    // Two 20 mm plates in the XY plane, facing +Z: "a" at the origin, "b" 40 mm along X.
    const auto plate = [&](const QString& id, const float centreX) {
        loupe::protocol::MeshPayload mesh;
        mesh.requestId = 1;
        mesh.nodeId = mesh.definitionId = id;
        mesh.segmentKey = id + QStringLiteral(":0");
        for (const auto& [u, v] : {std::pair{-10.0F, -10.0F}, {10.0F, -10.0F}, {10.0F, 10.0F}, {-10.0F, 10.0F}}) {
            mesh.vertices << centreX + u << v << 0.0F;
            mesh.normals << 0.0F << 0.0F << 1.0F;
        }
        mesh.indices = {0, 1, 2, 0, 2, 3};
        mesh.topology = {{5, loupe::protocol::TopologyKind::Face, 0, 6, 400.0F, 0.0F}};
        explode.includePartBounds(id, {centreX - 10, -10, 0}, {centreX + 10, 10, 0});
        QVERIFY(QMetaObject::invokeMethod(&controller, "meshReady", Q_ARG(QString, id), Q_ARG(QString, mesh.segmentKey),
                                          Q_ARG(QString, QStringLiteral("#3070C0")), Q_ARG(QByteArray, loupe::protocol::encodeGeometry(mesh))));
    };
    explode.setHierarchy({{QStringLiteral("a"), QString{}}, {QStringLiteral("b"), QString{}}});
    plate(QStringLiteral("a"), 0.0F);
    plate(QStringLiteral("b"), 40.0F);
    QTRY_VERIFY_WITH_TIMEOUT(viewport->property("hasSceneBounds").toBool(), 3000);
    QVERIFY(QMetaObject::invokeMethod(viewport, "setStandardView", Q_ARG(QVariant, QVariant::fromValue(QVector3D(0, 0, 1)))));
    QVERIFY(QMetaObject::invokeMethod(viewport, "fitCamera"));

    const auto partNode = [&](const QString& id) {
        return viewport->property("partNodeByNode").value<QJSValue>().property(id).toQObject();
    };
    QVERIFY(partNode(QStringLiteral("a")) != nullptr);

    // The handles show on the selection, and nowhere without one.
    QVERIFY(!viewport->property("handlesShown").toBool());
    controller.selectNode(QStringLiteral("a"), false);
    QTRY_VERIFY_WITH_TIMEOUT(viewport->property("handlesShown").toBool(), 3000);
    QTest::qWait(100);

    const auto layout = [&] {
        QVariant value;
        QMetaObject::invokeMethod(viewport, "handleLayout", Q_RETURN_ARG(QVariant, value));
        return value.toMap();
    };
    const auto point = [](const QVariant& value) {
        const auto map = value.toMap();
        return QPointF(map.value(QStringLiteral("x")).toDouble(), map.value(QStringLiteral("y")).toDouble());
    };
    auto handles = layout();
    QVERIFY(!handles.isEmpty());
    const auto centre = point(handles.value(QStringLiteral("centre")));
    const auto xTip = point(handles.value(QStringLiteral("tips")).toList().at(0));
    QVERIFY2((xTip - centre).manhattanLength() > 40, "the X arrow is drawn across the screen");

    // Drag the X arrow by its own length on screen: the plate moves along X, in whole steps.
    const auto grab = (centre + (xTip - centre) * 0.6).toPoint();
    const auto drop = (centre + (xTip - centre) * 1.6).toPoint();
    QTest::mousePress(&window, Qt::LeftButton, {}, grab);
    for (int step = 1; step <= 10; ++step) QTest::mouseMove(&window, grab + (drop - grab) * step / 10, 10);
    QVERIFY(explode.interacting());
    QTest::mouseRelease(&window, Qt::LeftButton, {}, drop);
    QVERIFY(!explode.interacting());
    const auto offset = explode.offsetOf(QStringLiteral("a"));
    QVERIFY2(offset.x() > 4.0F && std::abs(offset.y()) < 1e-3F && std::abs(offset.z()) < 1e-3F,
             qPrintable(QStringLiteral("%1 %2 %3").arg(offset.x()).arg(offset.y()).arg(offset.z())));
    QCOMPARE(std::fmod(offset.x(), 5.0F), 0.0F); // snapped to the 5 mm step
    QVERIFY(!explode.isMoved(QStringLiteral("b")));
    // Grabbing a handle is not a click: the selection stays as it was.
    QCOMPARE(controller.selectedNodeIds(), QStringList{QStringLiteral("a")});
    // The part's node is where the controller says.
    QTRY_VERIFY_WITH_TIMEOUT((partNode(QStringLiteral("a"))->property("position").value<QVector3D>()
                              - explode.displayedPosition(QStringLiteral("a"))).length() < 1e-3F, 3000);

    // Turn it on the Z ring, seen face-on: a quarter of the way round, from 45 degrees (clear
    // of the arrows, which take a press where they cross the ring) to 135.
    handles = layout();
    const auto zRing = handles.value(QStringLiteral("rings")).toList().at(2).toList();
    QCOMPARE(zRing.size(), 48);
    QTest::mousePress(&window, Qt::LeftButton, {}, point(zRing.at(6)).toPoint());
    QVERIFY(explode.interacting());
    for (int index = 7; index <= 18; ++index) QTest::mouseMove(&window, point(zRing.at(index)).toPoint(), 10);
    QTest::mouseRelease(&window, Qt::LeftButton, {}, point(zRing.at(18)).toPoint());
    const auto angles = explode.anglesOf(QStringLiteral("a"));
    QVERIFY2(std::abs(angles.z() - 90.0F) < 1e-2F,
             qPrintable(QStringLiteral("%1 %2 %3").arg(angles.x()).arg(angles.y()).arg(angles.z())));
    // Undo takes the turn back, one drag at a time.
    explode.undo();
    QVERIFY(std::abs(explode.anglesOf(QStringLiteral("a")).z()) < 1e-2F);
    QVERIFY(std::abs(explode.offsetOf(QStringLiteral("a")).x() - offset.x()) < 1e-3F);

    // The ball in the middle drags freely in the screen's plane, here X and Y (seen down Z),
    // each axis snapped on its own.
    handles = layout();
    const auto ball = point(handles.value(QStringLiteral("centre"))).toPoint();
    const auto beforeFree = explode.offsetOf(QStringLiteral("a"));
    QTest::mousePress(&window, Qt::LeftButton, {}, ball);
    QVERIFY(explode.interacting());
    for (int step = 1; step <= 8; ++step) QTest::mouseMove(&window, ball + QPoint(60, -40) * step / 8, 10);
    QTest::mouseRelease(&window, Qt::LeftButton, {}, ball + QPoint(60, -40));
    const auto freeMove = explode.offsetOf(QStringLiteral("a")) - beforeFree;
    QVERIFY2(freeMove.x() > 4.0F && freeMove.y() > 4.0F && std::abs(freeMove.z()) < 1e-3F,
             qPrintable(QStringLiteral("%1 %2 %3").arg(freeMove.x()).arg(freeMove.y()).arg(freeMove.z())));
    QCOMPARE(std::fmod(std::round(freeMove.x() * 1000.0F) / 1000.0F, 5.0F), 0.0F);
    QCOMPARE(std::fmod(std::round(freeMove.y() * 1000.0F) / 1000.0F, 5.0F), 0.0F);
    QCOMPARE(controller.selectedNodeIds(), QStringList{QStringLiteral("a")});
    explode.undo();

    // View normal to face follows the turned part: tipped a quarter about X, its face now
    // looks along -Y, and that is the way the view turns.
    explode.setAngles(QStringLiteral("a"), {90, 0, 0});
    QVERIFY(QMetaObject::invokeMethod(viewport, "setStandardView", Q_ARG(QVariant, QVariant::fromValue(QVector3D(0.4F, -0.8F, 0.45F).normalized()))));
    QTest::qWait(150);
    handles = layout();
    const auto plateCentre = point(handles.value(QStringLiteral("centre")));
    // Once only: asked again for the face it is already square to, it shows the back.
    QVariant done;
    QVERIFY(QMetaObject::invokeMethod(viewport, "viewNormalToFaceAt", Q_RETURN_ARG(QVariant, done),
                                      Q_ARG(QVariant, plateCentre.x()), Q_ARG(QVariant, plateCentre.y())));
    QVERIFY(done.toBool());
    const auto direction = viewport->property("viewDirection").value<QVector3D>();
    QVERIFY2((direction - QVector3D(0, -1, 0)).length() < 1e-3F,
             qPrintable(QStringLiteral("%1 %2 %3").arg(direction.x()).arg(direction.y()).arg(direction.z())));

    // A section of the moved part is cut against its own frame.
    QVariant plane;
    QVERIFY(QMetaObject::invokeMethod(viewport, "sectionPlaneFor", Q_RETURN_ARG(QVariant, plane), Q_ARG(QVariant, QStringLiteral("a")),
                                      Q_ARG(QVariant, QVariant::fromValue(QVector3D(1, 0, 0))), Q_ARG(QVariant, 2.0)));
    QCOMPARE(plane.value<QVector4D>(), explode.localPlane(QStringLiteral("a"), {1, 0, 0}, 2.0));

    // With a section on, moving parts holds their caps back until they stop, then re-cuts
    // them where the parts now are.
    auto* section = qobject_cast<loupe::app::tools::SectionController*>(controller.sectionController());
    section->setEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(!viewport->property("sectionFinalizing").toBool(), 3000);
    explode.setOffset(QStringLiteral("a"), {0, 0, 12});
    QVERIFY(viewport->property("explodeSettling").toBool());
    QTRY_VERIFY_WITH_TIMEOUT(!viewport->property("explodeSettling").toBool(), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(!viewport->property("sectionFinalizing").toBool(), 3000);
    section->setEnabled(false);

    // Several parts selected move together, by the same amount.
    explode.resetAll();
    QVERIFY(QMetaObject::invokeMethod(viewport, "setStandardView", Q_ARG(QVariant, QVariant::fromValue(QVector3D(0, 0, 1)))));
    QVERIFY(QMetaObject::invokeMethod(viewport, "fitCamera"));
    controller.selectNode(QStringLiteral("a"), false);
    controller.selectNode(QStringLiteral("b"), true);
    QCOMPARE(controller.selectedNodeIds().size(), 2);
    QTRY_VERIFY_WITH_TIMEOUT(viewport->property("handlesShown").toBool(), 3000);
    QTest::qWait(100);
    handles = layout();
    const auto pairCentre = point(handles.value(QStringLiteral("centre")));
    const auto pairTip = point(handles.value(QStringLiteral("tips")).toList().at(1)); // Y arrow
    const auto pairGrab = (pairCentre + (pairTip - pairCentre) * 0.6).toPoint();
    QTest::mousePress(&window, Qt::LeftButton, {}, pairGrab);
    for (int step = 1; step <= 8; ++step) QTest::mouseMove(&window, pairGrab + QPoint(0, -60) * step / 8, 10);
    QTest::mouseRelease(&window, Qt::LeftButton, {}, pairGrab + QPoint(0, -60));
    const auto pairA = explode.offsetOf(QStringLiteral("a"));
    QVERIFY2(pairA.y() > 4.0F, qPrintable(QString::number(pairA.y())));
    QVERIFY((explode.offsetOf(QStringLiteral("b")) - pairA).length() < 1e-3F);

    // A picked pivot: the handle goes to the point clicked on the part, and stays on it as
    // the part moves.
    controller.selectNode(QStringLiteral("b"), false);
    QVERIFY(QMetaObject::invokeMethod(viewport, "beginPivotPick"));
    QVERIFY(viewport->property("pickingPivot").toBool());
    QVERIFY(!viewport->property("handlesShown").toBool());
    QVariant bCentre;
    QMetaObject::invokeMethod(viewport, "screenPoint", Q_RETURN_ARG(QVariant, bCentre),
                              Q_ARG(QVariant, QVariant::fromValue(QVector3D(45.0F, 5.0F, 0.0F) + explode.offsetOf(QStringLiteral("b")))));
    const auto pivotClick = point(bCentre).toPoint();
    // A pick sees the scene as last drawn: let the moved parts be drawn where they now are.
    QTest::qWait(150);
    QTest::mouseClick(&window, Qt::LeftButton, {}, pivotClick);
    QTRY_VERIFY_WITH_TIMEOUT(!viewport->property("pickingPivot").toBool(), 3000);
    QVERIFY(!viewport->property("handlePivot").isNull());
    QCOMPARE(controller.selectedNodeIds(), QStringList{QStringLiteral("b")}); // the click selected nothing
    const auto pivotAt = viewport->property("handleCentre").value<QVector3D>();
    QVERIFY2((pivotAt - (QVector3D(45, 5, 0) + explode.offsetOf(QStringLiteral("b")))).length() < 0.5F,
             qPrintable(QStringLiteral("%1 %2 %3").arg(pivotAt.x()).arg(pivotAt.y()).arg(pivotAt.z())));
    explode.translateBy({QStringLiteral("b")}, {0, 0, 7});
    QTRY_VERIFY_WITH_TIMEOUT((viewport->property("handleCentre").value<QVector3D>() - (pivotAt + QVector3D(0, 0, 7))).length() < 1e-2F, 3000);
    // Lined up with the picked face: the plate faces +Z, so the handle's Z is the face normal.
    viewport->setProperty("handleAlignToFace", true);
    const auto frame = viewport->property("handleFrame").value<QQuaternion>();
    QVERIFY((frame.rotatedVector({0, 0, 1}) - QVector3D(0, 0, 1)).length() < 1e-3F);
    // The pivot is kept for that part: selecting another part uses its centre, and selecting
    // this one again brings the pivot back, until Use centre clears it.
    controller.selectNode(QStringLiteral("a"), false);
    QVERIFY(viewport->property("handlePivot").isNull());
    controller.selectNode(QStringLiteral("b"), false);
    QVERIFY(!viewport->property("handlePivot").isNull());
    QTRY_VERIFY_WITH_TIMEOUT((viewport->property("handleCentre").value<QVector3D>() - (pivotAt + QVector3D(0, 0, 7))).length() < 1e-2F, 3000);
    QVERIFY(QMetaObject::invokeMethod(viewport, "clearPivot"));
    QVERIFY(viewport->property("handlePivot").isNull());
    controller.selectNode(QStringLiteral("a"), false);
    controller.selectNode(QStringLiteral("b"), false);
    QVERIFY(viewport->property("handlePivot").isNull());

    // Assembled and Exploded: the view eases between the assembly and the layout, keeping it.
    QVERIFY(QMetaObject::invokeMethod(viewport, "showAssembled"));
    QTRY_VERIFY_WITH_TIMEOUT(explode.blend() < 1e-3, 3000);
    QVERIFY(explode.exploded());
    QTRY_VERIFY_WITH_TIMEOUT(partNode(QStringLiteral("b"))->property("position").value<QVector3D>().length() < 1e-3F, 3000);
    QVERIFY(QMetaObject::invokeMethod(viewport, "showExploded"));
    QTRY_VERIFY_WITH_TIMEOUT(explode.blend() > 0.999, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(viewport->property("handlesShown").toBool(), 3000);

    // On the assembly the handles stay: a move there starts a new layout from where the parts
    // are assembled, and the layout kept aside is saved first rather than lost.
    QVERIFY(QMetaObject::invokeMethod(viewport, "showAssembled"));
    QTRY_VERIFY_WITH_TIMEOUT(explode.blend() < 1e-3, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(viewport->property("handlesShown").toBool(), 3000);
    QVERIFY(controller.savedViews().explodeLayouts().isEmpty());
    explode.translateBy({QStringLiteral("b")}, {0, 0, 3});
    QCOMPARE(explode.blend(), 1.0);
    QVERIFY((explode.offsetOf(QStringLiteral("b")) - QVector3D(0, 0, 3)).length() < 1e-3F);
    QVERIFY(!explode.isMoved(QStringLiteral("a")));
    QCOMPARE(controller.savedViews().explodeLayouts().size(), 1);
    // The kept layout comes back from Saved layouts, easing out of the assembly.
    const auto keptId = controller.savedViews().explodeLayouts().constFirst().toMap().value(QStringLiteral("id")).toString();
    QVariant wentBack;
    QVERIFY(QMetaObject::invokeMethod(viewport, "goToExplodeLayout", Q_RETURN_ARG(QVariant, wentBack), Q_ARG(QVariant, keptId)));
    QVERIFY(wentBack.toBool());
    QTRY_VERIFY_WITH_TIMEOUT(explode.blend() > 0.999, 3000);
    QVERIFY(explode.isMoved(QStringLiteral("a")));
    QTRY_COMPARE_WITH_TIMEOUT(viewport->property("activeExplodeLayoutId").toString(), keptId, 3000);

    // Reset brings everything home.
    QVERIFY(QMetaObject::invokeMethod(viewport, "animateExplodeReset"));
    QTRY_VERIFY_WITH_TIMEOUT(!explode.exploded(), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(partNode(QStringLiteral("a"))->property("position").value<QVector3D>().length() < 1e-3F, 3000);
}

void QmlSmokeTest::explodePanelTypesPositionsAndExplodesTheSelectedSubAssembly()
{
    loupe::app::ApplicationController controller;
    auto& explode = controller.explodeController();
    explode.setHierarchy({
        {QStringLiteral("root"), QString{}},
        {QStringLiteral("sub"), QStringLiteral("root")},
        {QStringLiteral("left"), QStringLiteral("sub")},
        {QStringLiteral("right"), QStringLiteral("sub")},
    });
    explode.includePartBounds(QStringLiteral("left"), {-11, -1, -1}, {-9, 1, 1});
    explode.includePartBounds(QStringLiteral("right"), {9, -1, -1}, {11, 1, 1});

    QQmlEngine engine;
    QQmlComponent themeComponent(&engine, QUrl::fromLocalFile(QStringLiteral(LOUPE_QML_DIR) + QStringLiteral("/Theme.qml")));
    QVERIFY2(themeComponent.isReady(), qPrintable(themeComponent.errorString()));
    std::unique_ptr<QObject> theme(themeComponent.create());
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(LOUPE_INSPECT_QML_DIR) + QStringLiteral("/ExplodePanel.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> panel(component.createWithInitialProperties({
        {QStringLiteral("theme"), QVariant::fromValue(theme.get())},
        {QStringLiteral("taskController"), QVariant::fromValue(controller.explodeControllerObject())},
        {QStringLiteral("documentController"), QVariant::fromValue(static_cast<QObject*>(&controller))}}));
    QVERIFY2(panel != nullptr, qPrintable(component.errorString()));
    auto* scopeLabel = panel->findChild<QObject*>(QStringLiteral("explodeScopeLabel"));
    QVERIFY(scopeLabel != nullptr);
    QVERIFY(scopeLabel->property("text").toString().startsWith(QStringLiteral("Whole model")));

    // A part selected: auto-explode takes apart the sub-assembly it is in.
    controller.selectNode(QStringLiteral("left"), false);
    QCOMPARE(panel->property("scopeId").toString(), QStringLiteral("sub"));
    QVERIFY(scopeLabel->property("text").toString().startsWith(QStringLiteral("Sub-assembly")));
    QSignalSpy autoExplode(panel.get(), SIGNAL(autoExplodeRequested(QString,double)));
    auto* stateSwitch = panel->findChild<QObject*>(QStringLiteral("explodeStateSwitch"));
    QVERIFY(stateSwitch != nullptr);
    QCOMPARE(stateSwitch->property("currentIndex").toInt(), 0);
    QVERIFY(QMetaObject::invokeMethod(stateSwitch, "activated", Q_ARG(int, 1)));
    QCOMPARE(autoExplode.count(), 1);
    QCOMPARE(autoExplode.at(0).at(0).toString(), QStringLiteral("sub"));
    QVERIFY(autoExplode.at(0).at(1).toDouble() >= 1.0);

    // Typing a position moves the part there, and the field shows where it is.
    // Repeater delegates are children in the item tree, not of the panel object.
    const std::function<QQuickItem*(QQuickItem*, const QString&)> findItem = [&](QQuickItem* item, const QString& name) -> QQuickItem* {
        if (item->objectName() == name) return item;
        for (auto* child : item->childItems()) {
            if (auto* found = findItem(child, name)) return found;
        }
        return nullptr;
    };
    auto* panelItem = qobject_cast<QQuickItem*>(panel.get());
    auto* offsetX = findItem(panelItem, QStringLiteral("explodeOffsetField0"));
    QVERIFY(offsetX != nullptr);
    offsetX->setProperty("text", QStringLiteral("15"));
    QVERIFY(QMetaObject::invokeMethod(offsetX, "editingFinished"));
    QVERIFY(std::abs(explode.offsetOf(QStringLiteral("left")).x() - 15.0F) < 1e-3F);
    auto* angleZ = findItem(panelItem, QStringLiteral("explodeAngleField2"));
    QVERIFY(angleZ != nullptr);
    angleZ->setProperty("text", QStringLiteral("45"));
    QVERIFY(QMetaObject::invokeMethod(angleZ, "editingFinished"));
    QVERIFY(std::abs(explode.anglesOf(QStringLiteral("left")).z() - 45.0F) < 1e-2F);
    QVERIFY(std::abs(explode.offsetOf(QStringLiteral("left")).x() - 15.0F) < 1e-3F);
    QCOMPARE(angleZ->property("text").toString(), QStringLiteral("45"));
    QVERIFY(panel->findChild<QObject*>(QStringLiteral("explodeResetAllButton"))->property("enabled").toBool());

    // "Move by" adds an exact distance to where the selection is; an empty field is 0.
    const auto before = explode.offsetOf(QStringLiteral("left"));
    findItem(panelItem, QStringLiteral("explodeMoveByField0"))->setProperty("text", QStringLiteral("2.5"));
    findItem(panelItem, QStringLiteral("explodeMoveByField2"))->setProperty("text", QStringLiteral("-10"));
    QVERIFY(QMetaObject::invokeMethod(findItem(panelItem, QStringLiteral("explodeMoveByButton")), "clicked"));
    const auto moved = explode.offsetOf(QStringLiteral("left")) - before;
    QVERIFY2((moved - QVector3D(2.5F, 0, -10)).length() < 1e-3F,
             qPrintable(QStringLiteral("%1 %2 %3").arg(moved.x()).arg(moved.y()).arg(moved.z())));
    QVERIFY(explode.canUndo());

    // Export… asks for a file; the format follows its suffix.
    QSignalSpy exportRequested(panel.get(), SIGNAL(exportRequested()));
    QVERIFY(QMetaObject::invokeMethod(panel->findChild<QObject*>(QStringLiteral("explodeExportButton")), "clicked"));
    QCOMPARE(exportRequested.count(), 1);
    using loupe::app::ApplicationController;
    QCOMPARE(ApplicationController::explodedExportFormat(QUrl::fromLocalFile(QStringLiteral("/x/a.STP"))), QStringLiteral("step"));
    QCOMPARE(ApplicationController::explodedExportFormat(QUrl::fromLocalFile(QStringLiteral("/x/a.stl"))), QStringLiteral("stl"));
    QCOMPARE(ApplicationController::explodedExportFormat(QUrl::fromLocalFile(QStringLiteral("/x/a.pdf"))), QStringLiteral("pdf"));
    QCOMPARE(ApplicationController::explodedExportFormat(QUrl::fromLocalFile(QStringLiteral("/x/a.obj"))), QString{});
    // Nothing to export from an empty document.
    QVERIFY(!controller.exportExplodedAssembly(QUrl::fromLocalFile(QStringLiteral("/x/a.step")), {0, 0, 1}, {0, 1, 0}));
}

void QmlSmokeTest::floatingToolWindowDragsMinimisesAndStaysInside()
{
    QQmlEngine engine;
    QQmlComponent themeComponent(&engine, QUrl::fromLocalFile(QStringLiteral(LOUPE_QML_DIR) + QStringLiteral("/Theme.qml")));
    std::unique_ptr<QObject> theme(themeComponent.create());
    QQmlComponent component(&engine);
    component.setData(QByteArrayLiteral(R"(
import QtQuick
Item {
    width: 800; height: 600
    property alias window: tool
    FloatingToolWindow {
        id: tool
        objectName: "tool"
        title: "Section"
        shown: true
        defaultRightMargin: 40
        defaultBottomMargin: 40
        Rectangle { implicitWidth: 300; implicitHeight: 400 }
    }
}
)"), QUrl::fromLocalFile(QStringLiteral(LOUPE_INSPECT_QML_DIR) + QStringLiteral("/harness.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> object(component.createWithInitialProperties({}));
    QVERIFY2(object != nullptr, qPrintable(component.errorString()));
    auto* harness = qobject_cast<QQuickItem*>(object.get());
    auto* tool = harness->property("window").value<QQuickItem*>();
    tool->setProperty("theme", QVariant::fromValue(theme.get()));
    QQuickWindow window;
    window.resize(800, 600);
    harness->setParentItem(window.contentItem());
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 3000);

    // Opens at its corner, sized to the panel under a title bar.
    const auto titleHeight = tool->property("titleHeight").toDouble();
    QCOMPARE(tool->width(), 300.0);
    QCOMPARE(tool->height(), 400.0 + titleHeight);
    QCOMPARE(tool->x(), 800.0 - 40 - 300);
    QCOMPARE(tool->y(), 600.0 - 40 - tool->height());

    // Dragged by its title bar.
    const QPoint grab(qRound(tool->x() + 150), qRound(tool->y() + titleHeight / 2));
    QTest::mousePress(&window, Qt::LeftButton, {}, grab);
    for (int step = 1; step <= 6; ++step) QTest::mouseMove(&window, grab + QPoint(-200, -60) * step / 6, 10);
    QTest::mouseRelease(&window, Qt::LeftButton, {}, grab + QPoint(-200, -60));
    QCOMPARE(tool->x(), 800.0 - 40 - 300 - 200);
    QCOMPARE(tool->y(), 600.0 - 40 - (400.0 + titleHeight) - 60);

    // Never off the workspace, however far it is dragged.
    const QPoint again(qRound(tool->x() + 150), qRound(tool->y() + titleHeight / 2));
    QTest::mousePress(&window, Qt::LeftButton, {}, again);
    for (int step = 1; step <= 6; ++step) QTest::mouseMove(&window, again + QPoint(-2000, -2000) * step / 6, 10);
    QTest::mouseRelease(&window, Qt::LeftButton, {}, again + QPoint(-2000, -2000));
    QCOMPARE(tool->x(), 8.0);
    QCOMPARE(tool->y(), 8.0);

    // Minimised to its title bar, which stays where it is; a double-click restores it.
    QVERIFY(QMetaObject::invokeMethod(tool, "toggleMinimized"));
    QCOMPARE(tool->height(), titleHeight);
    QCOMPARE(tool->y(), 8.0);
    QTest::mouseDClick(&window, Qt::LeftButton, {}, QPoint(qRound(tool->x() + 150), qRound(tool->y() + titleHeight / 2)));
    QTRY_COMPARE(tool->height(), 400.0 + titleHeight);

    // A workspace too short for the panel scrolls it instead of pushing it off screen.
    harness->setHeight(300);
    QTRY_VERIFY(tool->height() <= 300.0 - 16);
}

void QmlSmokeTest::updateIndicatorShowsTheDownloadAsARing()
{
    auto theme = std::make_unique<QQmlPropertyMap>();
    for (const auto* key : {"foreground", "muted", "accent", "error", "border", "control", "surfaceRaised", "surface3", "accentTint", "selection"})
        theme->insert(QString::fromLatin1(key), QColor(QStringLiteral("#808080")));
    for (const auto* key : {"spacing1", "spacing2", "spacing4", "radius1", "radius2", "radius3", "fontCaption"})
        theme->insert(QString::fromLatin1(key), 4);
    theme->insert(QStringLiteral("durIndeterminateTravel"), 1200);

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(QByteArrayLiteral(
        "import QtQuick\n"
        "import Loupe.App\n"
        "Item {\n"
        "  property QtObject theme\n"
        "  property alias indicator: indicator\n"
        "  property QtObject updates: QtObject {\n"
        "    property int state: Updates.Idle\n"
        "    property string availableVersion: \"0.2.0\"\n"
        "    property string currentVersion: \"0.1.5\"\n"
        "    property double progress: 0\n"
        "    property real bytesReceived: 0\n"
        "    property real bytesTotal: 0\n"
        "    property string message: \"\"\n"
        "    property string releaseUrl: \"\"\n"
        "    property bool dismissed: false\n"
        "    property bool userInitiated: false\n"
        "    signal changed()\n"
        "    signal quitRequested()\n"
        "  }\n"
        "  UpdateIndicator { id: indicator; theme: parent.theme; updates: parent.updates }\n"
        "}\n"), QUrl::fromLocalFile(QStringLiteral(LOUPE_INSPECT_QML_DIR) + QStringLiteral("/UpdateIndicatorHost.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> host(component.createWithInitialProperties(
        {{QStringLiteral("theme"), QVariant::fromValue(static_cast<QObject*>(theme.get()))}}));
    QVERIFY2(host != nullptr, qPrintable(component.errorString()));
    auto* indicator = host->property("indicator").value<QObject*>();
    QVERIFY(indicator != nullptr);
    auto* updates = host->property("updates").value<QObject*>();

    // Nothing to say: not in the toolbar at all. An automatic check stays quiet too.
    QVERIFY(!indicator->property("visible").toBool());
    updates->setProperty("state", static_cast<int>(loupe::app::platform::UpdateController::State::Checking));
    QVERIFY(!indicator->property("visible").toBool());
    // A newer version: the icon, with its dot.
    updates->setProperty("state", static_cast<int>(loupe::app::platform::UpdateController::State::Available));
    QVERIFY(indicator->property("visible").toBool());
    // Downloading: the ring fills with the download, its percentage beside it.
    updates->setProperty("state", static_cast<int>(loupe::app::platform::UpdateController::State::Downloading));
    updates->setProperty("progress", 0.42);
    QCOMPARE(indicator->property("percent").toInt(), 42);
    auto* ring = indicator->findChild<QObject*>(QStringLiteral("updateProgressRing"));
    QVERIFY(ring != nullptr);
    QVERIFY(ring->property("visible").toBool());
    QCOMPARE(ring->property("sweep").toDouble(), 360.0 * 42 / 100);
    auto* percent = indicator->findChild<QObject*>(QStringLiteral("updatePercent"));
    QVERIFY(percent != nullptr);
    QCOMPARE(percent->property("text").toString(), QStringLiteral("42%"));
    // Ready: a full ring. Later hides it until the next launch.
    updates->setProperty("state", static_cast<int>(loupe::app::platform::UpdateController::State::Ready));
    QCOMPARE(ring->property("sweep").toDouble(), 360.0);
    updates->setProperty("dismissed", true);
    QVERIFY(!indicator->property("visible").toBool());
}

void QmlSmokeTest::whatsNewListsTheReleaseAndIsMarkedSeenWhenClosed()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(QByteArrayLiteral(
        "import QtQuick\n"
        "import Loupe.App\n"
        "import \"..\"\n"
        "Window {\n"
        "  id: host\n"
        "  width: 800; height: 700; visible: true\n"
        "  property alias dialog: dialog\n"
        "  property int seen: 0\n"
        "  Theme { id: theme }\n"
        "  property QtObject updates: QtObject {\n"
        "    property var whatsNewItems: [{icon: \"tabs\", title: \"Tabs and windows\", body: \"Several files.\"},\n"
        "                                 {icon: \"explode\", title: \"Exploded view\", body: \"Pull it apart.\"}]\n"
        "    property string whatsNewVersion: \"0.1.5\"\n"
        "    function markWhatsNewSeen() { seen += 1 }\n"
        "  }\n"
        "  WhatsNewDialog { id: dialog; theme: theme; updates: host.updates }\n"
        "}\n"), QUrl::fromLocalFile(QStringLiteral(LOUPE_INSPECT_QML_DIR) + QStringLiteral("/WhatsNewHost.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> host(component.create());
    QVERIFY2(host != nullptr, qPrintable(component.errorString()));
    auto* window = qobject_cast<QQuickWindow*>(host.get());
    QVERIFY(window != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(window->isExposed(), 3000);
    auto* dialog = host->property("dialog").value<QObject*>();
    QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
    QTRY_VERIFY_WITH_TIMEOUT(dialog->property("opened").toBool(), 3000);
    QCOMPARE(dialog->property("items").toList().size(), 2);
    auto* title = findItemByObjectName(window->contentItem(), QStringLiteral("whatsNewTitle"));
    QVERIFY(title == nullptr || title->property("text").toString().contains(QStringLiteral("What's New")));
    QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
    QTRY_VERIFY_WITH_TIMEOUT(!dialog->property("visible").toBool(), 3000);
    QCOMPARE(host->property("seen").toInt(), 1);
}

void QmlSmokeTest::everyWorkspaceSidePanelGoesDownToTheSameMinimum()
{
    // The panels are as wide as their dividers say, down to the same minimums in every
    // workspace: Export's and Drawing's settings column used to fill half the spare width as
    // well, standing at 525 px when its divider said 260.
    const QList<std::tuple<const char*, const char*, double>> expected{
        {"/inspect/InspectWorkspace.qml", "inspectTreeSplitter", 180.0},
        {"/inspect/InspectWorkspace.qml", "inspectPropertiesSplitter", 260.0},
        {"/export/ExportWorkspace.qml", "pickerSplitter", 180.0},
        {"/export/ExportWorkspace.qml", "settingsSplitter", 260.0},
        {"/drawing/DrawingWorkspace.qml", "pickerSplitter", 180.0},
        {"/drawing/DrawingWorkspace.qml", "settingsSplitter", 260.0},
    };
    loupe::app::models::ThemePreference preferences;
    const QStringList keys{QStringLiteral("outputPicker"), QStringLiteral("outputSettings"),
                           QStringLiteral("inspectTree"), QStringLiteral("inspectProperties")};
    for (const auto& key : keys) preferences.setPanelWidth(key, 120); // under every minimum
    const auto restore = qScopeGuard([&] { for (const auto& key : keys) preferences.clearPanelWidth(key); });
    for (const auto& [file, splitterName, minimum] : expected) {
        loupe::app::ApplicationController controller;
        QQmlEngine engine;
        QQmlComponent themeComponent(&engine, QUrl::fromLocalFile(QStringLiteral(LOUPE_QML_DIR) + QStringLiteral("/Theme.qml")));
        std::unique_ptr<QObject> theme(themeComponent.create());
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(LOUPE_QML_DIR) + QString::fromLatin1(file)));
        std::unique_ptr<QObject> object(component.createWithInitialProperties({
            {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject*>(&controller))},
            {QStringLiteral("theme"), QVariant::fromValue(theme.get())},
            {QStringLiteral("preferences"), QVariant::fromValue(static_cast<QObject*>(&preferences))}}));
        QVERIFY2(object != nullptr, qPrintable(component.errorString()));
        auto* workspace = qobject_cast<QQuickItem*>(object.get());
        QQuickWindow window;
        window.resize(1600, 900);
        workspace->setParentItem(window.contentItem());
        workspace->setSize({1600, 900});
        window.show();
        QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 3000);
        auto* splitter = workspace->findChild<QQuickItem*>(QString::fromLatin1(splitterName));
        QVERIFY2(splitter != nullptr, splitterName);
        auto* panel = splitter->property("panel").value<QQuickItem*>();
        QCOMPARE(splitter->property("effectiveWidth").toDouble(), minimum);
        QTRY_VERIFY2_WITH_TIMEOUT(std::abs(panel->width() - minimum) < 0.5,
                                  qPrintable(QStringLiteral("%1 %2: %3").arg(QString::fromLatin1(file), QString::fromLatin1(splitterName))
                                                 .arg(panel->width())), 3000);
    }
}

void QmlSmokeTest::aFingerGetsBiggerTargetsAndALongPressOpensTheMenu()
{
    loupe::app::ApplicationController controller;
    QQmlEngine engine;
    QQmlComponent themeComponent(&engine, QUrl::fromLocalFile(QStringLiteral(LOUPE_QML_DIR) + QStringLiteral("/Theme.qml")));
    std::unique_ptr<QObject> theme(themeComponent.create());
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(LOUPE_INSPECT_QML_DIR) + QStringLiteral("/StepViewport.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> object(component.createWithInitialProperties({
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject*>(&controller))},
        {QStringLiteral("theme"), QVariant::fromValue(theme.get())}}));
    QVERIFY2(object != nullptr, qPrintable(component.errorString()));
    auto* viewport = qobject_cast<QQuickItem*>(object.get());
    QQuickWindow window;
    window.resize(400, 300);
    viewport->setParentItem(window.contentItem());
    viewport->setSize(QSizeF(400, 300));
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 3000);
    QVERIFY(!viewport->property("touchInput").toBool());
    const auto mouseVertexPixels = viewport->property("measureVertexPixels").toDouble();

    // A finger on the touchscreen, held still on empty space: touch is noticed, the snap radius
    // grows, and the view's menu comes up after the hold.
    std::unique_ptr<QPointingDevice> screen(QTest::createTouchDevice(QInputDevice::DeviceType::TouchScreen));
    QTest::touchEvent(&window, screen.get()).press(0, QPoint(200, 150));
    QTRY_VERIFY_WITH_TIMEOUT(viewport->property("touchInput").toBool(), 1000);
    QVERIFY(viewport->property("measureVertexPixels").toDouble() > mouseVertexPixels);
    auto* menu = viewport->findChild<QObject*>(QStringLiteral("viewportContextMenu"));
    QVERIFY(menu != nullptr);
    QTRY_VERIFY2_WITH_TIMEOUT(menu->property("opened").toBool() || menu->property("visible").toBool(),
                              qPrintable(QStringLiteral("touchInput %1").arg(viewport->property("touchInput").toBool())), 2000);
    QTest::touchEvent(&window, screen.get()).release(0, QPoint(200, 150));
    QMetaObject::invokeMethod(menu, "close");
    QTRY_VERIFY_WITH_TIMEOUT(!menu->property("visible").toBool(), 2000);

    // A quick tap does not open it.
    QTest::touchEvent(&window, screen.get()).press(0, QPoint(150, 120));
    QTest::qWait(80);
    QTest::touchEvent(&window, screen.get()).release(0, QPoint(150, 120));
    QTest::qWait(600);
    QVERIFY(!menu->property("visible").toBool());

    // A mouse moving over the view (hovering) is a mouse again.
    QTest::mouseMove(&window, QPoint(100, 100));
    QTest::mouseMove(&window, QPoint(120, 110));
    QTRY_VERIFY_WITH_TIMEOUT(!viewport->property("touchInput").toBool(), 1000);
    QCOMPARE(viewport->property("measureVertexPixels").toDouble(), mouseVertexPixels);
}

void QmlSmokeTest::aMovedPartIsDrawnWithItsExplodeLine()
{
    loupe::app::ApplicationController controller;
    QQmlEngine engine;
    QQmlComponent themeComponent(&engine, QUrl::fromLocalFile(QStringLiteral(LOUPE_QML_DIR) + QStringLiteral("/Theme.qml")));
    std::unique_ptr<QObject> theme(themeComponent.create());
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(LOUPE_INSPECT_QML_DIR) + QStringLiteral("/StepViewport.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> object(component.createWithInitialProperties({
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject*>(&controller))},
        {QStringLiteral("theme"), QVariant::fromValue(theme.get())},
        {QStringLiteral("explodedLayoutEnabled"), true}}));
    QVERIFY2(object != nullptr, qPrintable(component.errorString()));
    auto* viewport = qobject_cast<QQuickItem*>(object.get());
    QQuickWindow window;
    window.resize(320, 240);
    viewport->setParentItem(window.contentItem());
    viewport->setSize(QSizeF(320, 240));
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 3000);
    const QByteArray cube = QByteArrayLiteral(
        "{\"vertices\":[-1,-1,-1,1,-1,-1,1,1,-1,-1,1,-1,-1,-1,1,1,-1,1,1,1,1,-1,1,1],"
        "\"indices\":[0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7]}" );
    // The explode controller learns the parts from the document; here, from the test.
    controller.explodeController().setHierarchy({{QStringLiteral("moved"), QString{}}, {QStringLiteral("still"), QString{}}});
    for (const auto& node : {QStringLiteral("moved"), QStringLiteral("still")}) {
        controller.explodeController().includePartBounds(node, {-1, -1, -1}, {1, 1, 1});
        QVERIFY(QMetaObject::invokeMethod(&controller, "meshReady", Q_ARG(QString, node), Q_ARG(QString, node + QStringLiteral("-segment")),
                                          Q_ARG(QString, QStringLiteral("#67d5c0")), Q_ARG(QByteArray, cube)));
    }
    QTest::qWait(200);
    auto* lines = viewport->findChild<QObject*>(QStringLiteral("explodeLines"));
    QVERIFY(lines != nullptr);
    auto* geometry = lines->property("geometry").value<QObject*>();
    QVERIFY(geometry != nullptr);
    QCOMPARE(geometry->property("segmentCount").toInt(), 0);

    auto* explode = controller.property("explode").value<QObject*>();
    QVERIFY(explode != nullptr);
    QVERIFY(QMetaObject::invokeMethod(explode, "translateBy", Q_ARG(QStringList, QStringList{QStringLiteral("moved")}),
                                      Q_ARG(QVector3D, QVector3D(0, 0, 30))));
    QTRY_VERIFY_WITH_TIMEOUT(geometry->property("segmentCount").toInt() > 0, 2000);
    QVERIFY(lines->property("visible").toBool());
    // Only the moved part has a line; putting it back takes the line away.
    QVariantList list;
    QVERIFY(QMetaObject::invokeMethod(explode, "explodeLines", Q_RETURN_ARG(QVariantList, list)));
    QCOMPARE(list.size(), 1);
    QCOMPARE(list.front().toMap().value(QStringLiteral("nodeId")).toString(), QStringLiteral("moved"));
    QVERIFY(QMetaObject::invokeMethod(explode, "resetNode", Q_ARG(QString, QStringLiteral("moved"))));
    QTRY_COMPARE_WITH_TIMEOUT(geometry->property("segmentCount").toInt(), 0, 2000);
    QVERIFY(!lines->property("visible").toBool());

    // A pin in a bore, both round about the same line along z: pulled out, the pin's line runs
    // along that shared axis, as a centerline.
    const auto roundPart = [&](const QString& id, const float radius, const float z0, const float z1) {
        QVector<float> vertices;
        QVector<float> normals;
        QVector<quint32> indices;
        for (int column = 0; column <= 8; ++column) {
            const auto angle = static_cast<float>(column) / 8.0F * static_cast<float>(std::numbers::pi / 2.0);
            for (const auto z : {z0, z1}) {
                vertices << 20.0F + radius * std::cos(angle) << radius * std::sin(angle) << z;
                normals << std::cos(angle) << std::sin(angle) << 0.0F;
            }
        }
        for (quint32 column = 0; column < 8; ++column)
            indices << column * 2 << column * 2 + 1 << column * 2 + 3 << column * 2 << column * 2 + 3 << column * 2 + 2;
        loupe::protocol::TopologyRange range{9, loupe::protocol::TopologyKind::Face, 0, static_cast<quint32>(indices.size()), 10.0F, radius};
        range.axisOrigin = {20.0F, 0.0F, 0.0F};
        range.axisDirection = {0.0F, 0.0F, 1.0F};
        const loupe::protocol::MeshPayload mesh{1, 1, id, id, id + QStringLiteral("-segment"), QStringLiteral("#67d5c0"), 1,
                                                vertices, normals, indices, {range}};
        controller.explodeController().includePartBounds(id, {20.0F - radius, -radius, z0}, {20.0F + radius, radius, z1});
        QVERIFY(QMetaObject::invokeMethod(&controller, "meshReady", Q_ARG(QString, id), Q_ARG(QString, id + QStringLiteral("-segment")),
                                          Q_ARG(QString, QStringLiteral("#67d5c0")), Q_ARG(QByteArray, loupe::protocol::encodeGeometry(mesh))));
    };
    controller.explodeController().setHierarchy({{QStringLiteral("pin"), QString{}}, {QStringLiteral("bore"), QString{}}});
    roundPart(QStringLiteral("pin"), 2.0F, 0.0F, 10.0F);
    roundPart(QStringLiteral("bore"), 2.05F, 2.0F, 8.0F);
    QVERIFY(QMetaObject::invokeMethod(explode, "translateBy", Q_ARG(QStringList, QStringList{QStringLiteral("pin")}),
                                      Q_ARG(QVector3D, QVector3D(0, 0, 25))));
    const auto pinLineIsACenterline = [&]() {
        QVariantList now;
        QMetaObject::invokeMethod(explode, "explodeLines", Q_RETURN_ARG(QVariantList, now));
        if (now.size() != 1) return false;
        const auto segments = now.front().toMap().value(QStringLiteral("segments")).toList();
        return segments.size() == 1 && segments.front().toMap().value(QStringLiteral("centerline")).toBool();
    };
    QTRY_VERIFY_WITH_TIMEOUT(pinLineIsACenterline(), 3000);
}

void QmlSmokeTest::aRoundFaceOffersItsAxisAndACornerStillWins()
{
    // The shared picker (measure, pivot, section): over a round face it takes the face's axis,
    // but a corner inside the snap radius still wins; Alt takes the point on the face.
    loupe::app::ApplicationController controller;
    QQmlEngine engine;
    QQmlComponent themeComponent(&engine, QUrl::fromLocalFile(QStringLiteral(LOUPE_QML_DIR) + QStringLiteral("/Theme.qml")));
    std::unique_ptr<QObject> theme(themeComponent.create());
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(LOUPE_INSPECT_QML_DIR) + QStringLiteral("/StepViewport.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> object(component.createWithInitialProperties({
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject*>(&controller))},
        {QStringLiteral("theme"), QVariant::fromValue(theme.get())}}));
    QVERIFY2(object != nullptr, qPrintable(component.errorString()));
    auto* viewport = qobject_cast<QQuickItem*>(object.get());
    QQuickWindow window;
    window.resize(640, 480);
    viewport->setParentItem(window.contentItem());
    viewport->setSize(QSizeF(640, 480));
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 3000);

    // A quarter of a hole's wall, radius 10 about z, 20 deep, and the straight edge along one side.
    QVector<float> vertices;
    QVector<float> normals;
    QVector<quint32> indices;
    for (int column = 0; column <= 8; ++column) {
        const auto angle = static_cast<float>(column) / 8.0F * static_cast<float>(std::numbers::pi / 2.0);
        for (const auto z : {0.0F, 20.0F}) {
            vertices << 10.0F * std::cos(angle) << 10.0F * std::sin(angle) << z;
            normals << -std::cos(angle) << -std::sin(angle) << 0.0F;
        }
    }
    for (quint32 column = 0; column < 8; ++column)
        indices << column * 2 << column * 2 + 1 << column * 2 + 3 << column * 2 << column * 2 + 3 << column * 2 + 2;
    loupe::protocol::TopologyRange wall{41, loupe::protocol::TopologyKind::Face, 0, static_cast<quint32>(indices.size()), 314.0F, 10.0F};
    wall.axisOrigin = {0.0F, 0.0F, 0.0F};
    wall.axisDirection = {0.0F, 0.0F, 1.0F};
    const loupe::protocol::MeshPayload mesh{1, 1, QStringLiteral("definition"), QStringLiteral("node"), QStringLiteral("segment"),
                                            QStringLiteral("#67d5c0"), 1, vertices, normals, indices, {wall}};
    const loupe::protocol::EdgePayload edge{1, 1, QStringLiteral("definition"), QStringLiteral("node"), 1,
                                            {10.0F, 0.0F, 0.0F, 10.0F, 0.0F, 20.0F}, {0, 1},
                                            {{7, loupe::protocol::TopologyKind::Edge, 0, 2, 20.0F, 0.0F}}};
    QVERIFY(QMetaObject::invokeMethod(&controller, "meshReady", Q_ARG(QString, QStringLiteral("node")),
                                      Q_ARG(QString, QStringLiteral("segment")), Q_ARG(QString, QStringLiteral("#67d5c0")),
                                      Q_ARG(QByteArray, loupe::protocol::encodeGeometry(mesh))));
    QVERIFY(QMetaObject::invokeMethod(&controller, "edgeReady", Q_ARG(QString, QStringLiteral("node")),
                                      Q_ARG(QByteArray, loupe::protocol::encodeGeometry(edge))));
    QTest::qWait(300);

    // What the picker takes for a hit at `at`, with Alt held or not, as point to point does.
    const auto pickKind = [&](const QVector3D& at, const QVector3D& normal, bool alternate) {
        viewport->setProperty("pickAlternate", alternate);
        QQmlExpression expression(qmlContext(viewport), viewport,
            QStringLiteral("(function() { const pick = measureCandidateAt({ objectHit: modelByNode['segment'], "
                           "scenePosition: Qt.vector3d(%1, %2, %3), sceneNormal: Qt.vector3d(%4, %5, %6) }, false, 0); "
                           "return pick ? pick.info.kind + '|' + (pick.info.hint || '') + '|' + pick.info.point.z : 'none' })()")
                .arg(at.x()).arg(at.y()).arg(at.z()).arg(normal.x()).arg(normal.y()).arg(normal.z()));
        const auto value = expression.evaluate().toString();
        if (expression.hasError()) return expression.error().toString();
        return value;
    };
    // Snap radius in scene units at this zoom: the middle of the wall must be well outside it.
    QQmlExpression tolerance(qmlContext(viewport), viewport, QStringLiteral("worldPerPixelAt(Qt.vector3d(7, 7, 10)) * measureVertexPixels"));
    const auto radius = tolerance.evaluate().toDouble();
    QVERIFY2(radius > 0.0 && radius < 3.0, qPrintable(QString::number(radius)));

    const auto middle = pickKind({7.071F, 7.071F, 10.0F}, {-0.7071F, -0.7071F, 0.0F}, false);
    QVERIFY2(middle.startsWith(QStringLiteral("axis|")), qPrintable(middle));
    QVERIFY2(middle.endsWith(QStringLiteral("|10")), qPrintable(middle)); // its point: on the axis, level with the hit
    const auto surface = pickKind({7.071F, 7.071F, 10.0F}, {-0.7071F, -0.7071F, 0.0F}, true);
    QVERIFY2(surface.startsWith(QStringLiteral("surfacePoint|")), qPrintable(surface));
    const auto corner = pickKind({10.0F, static_cast<float>(radius * 0.2), 20.0F}, {-1.0F, 0.0F, 0.0F}, false);
    QVERIFY2(corner.startsWith(QStringLiteral("vertex|")), qPrintable(corner));
    viewport->setProperty("pickAlternate", false);
}

void QmlSmokeTest::aCaptureHoldsTheMeasurementOnlyWhenAskedAndWhereItIs()
{
    // The measurements are drawn over the 3D view, not in it: a capture that asks for them has
    // them, at the capture's size and where they are on the model; one that does not, has none.
    loupe::app::ApplicationController controller;
    auto* capture = qobject_cast<loupe::app::tools::CaptureController*>(controller.captureController());
    auto* measurement = qobject_cast<loupe::app::tools::MeasurementController*>(controller.measurementController());
    QVERIFY(capture != nullptr && measurement != nullptr);
    capture->setTransparentBackground(true);

    QQmlEngine engine;
    QQmlComponent themeComponent(&engine, QUrl::fromLocalFile(QStringLiteral(LOUPE_QML_DIR) + QStringLiteral("/Theme.qml")));
    std::unique_ptr<QObject> theme(themeComponent.create());
    QVERIFY2(theme != nullptr, qPrintable(themeComponent.errorString()));
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(LOUPE_INSPECT_QML_DIR) + QStringLiteral("/StepViewport.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> object(component.createWithInitialProperties({
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject*>(&controller))},
        {QStringLiteral("theme"), QVariant::fromValue(theme.get())}}));
    QVERIFY2(object != nullptr, qPrintable(component.errorString()));
    auto* viewport = qobject_cast<QQuickItem*>(object.get());
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
    QVERIFY(QMetaObject::invokeMethod(&controller, "meshReady", Q_ARG(QString, QStringLiteral("node")),
                                      Q_ARG(QString, QStringLiteral("segment")), Q_ARG(QString, QStringLiteral("#67d5c0")),
                                      Q_ARG(QByteArray, cube)));
    QTest::qWait(500);

    // A distance across the cube's top face, front corner to front corner.
    measurement->setModeName(QStringLiteral("point"));
    QVERIFY(measurement->acceptEntity(loupe::app::tools::MeasurementController::surfacePointEntity({-1, 1, 1}, {0, 0, 1}), QStringLiteral("Cube")));
    QVERIFY(measurement->acceptEntity(loupe::app::tools::MeasurementController::surfacePointEntity({1, 1, 1}, {0, 0, 1}), QStringLiteral("Cube")));
    QCOMPARE(measurement->pickedPoints().size(), 2);
    QTest::qWait(200);

    QVariant amberValue;
    QVERIFY(QMetaObject::invokeMethod(theme.get(), "measureColor", Q_RETURN_ARG(QVariant, amberValue), Q_ARG(QVariant, QStringLiteral("amber"))));
    const QColor amber = amberValue.value<QColor>();
    QVERIFY(amber.isValid());
    // Where the measurement's own colour is in an image: its bounds, normalised to the image.
    const auto amberBounds = [&](const QImage& image, int* count) {
        QRect bounds;
        *count = 0;
        const QImage argb = image.convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < argb.height(); ++y) {
            for (int x = 0; x < argb.width(); ++x) {
                const QColor pixel = QColor::fromRgba(argb.pixel(x, y));
                if (pixel.alpha() < 200) continue;
                if (std::abs(pixel.red() - amber.red()) + std::abs(pixel.green() - amber.green()) + std::abs(pixel.blue() - amber.blue()) > 40) continue;
                ++*count;
                bounds |= QRect(x, y, 1, 1);
            }
        }
        return QRectF(double(bounds.x()) / argb.width(), double(bounds.y()) / argb.height(),
                      double(bounds.width()) / argb.width(), double(bounds.height()) / argb.height());
    };

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto captureTo = [&](double scale, bool include, const QString& name, QImage* result) {
        capture->setCustomScale(scale);
        capture->setIncludeMeasurements(include);
        const auto expectedSize = capture->resolvedSize();
        const auto path = directory.filePath(name);
        QVERIFY(QMetaObject::invokeMethod(viewport, "captureToFile", Q_ARG(QVariant, QVariant::fromValue(QUrl::fromLocalFile(path)))));
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(path), 10'000);
        QTRY_VERIFY_WITH_TIMEOUT(!capture->inProgress(), 10'000);
        *result = QImage(path);
        QVERIFY(!result->isNull());
        QCOMPARE(result->size(), expectedSize);
    };

    QImage without;
    QImage with2x;
    QImage with3x;
    captureTo(2.0, false, QStringLiteral("without.png"), &without);
    captureTo(2.0, true, QStringLiteral("with-2x.png"), &with2x);
    captureTo(3.0, true, QStringLiteral("with-3x.png"), &with3x);
    int withoutCount = 0;
    int with2xCount = 0;
    int with3xCount = 0;
    amberBounds(without, &withoutCount);
    const auto bounds2x = amberBounds(with2x, &with2xCount);
    const auto bounds3x = amberBounds(with3x, &with3xCount);
    QVERIFY2(withoutCount < 20, qPrintable(QStringLiteral("unasked, the capture still has %1 measurement pixels").arg(withoutCount)));
    QVERIFY2(with2xCount > 100, qPrintable(QStringLiteral("asked, the capture has only %1 measurement pixels").arg(with2xCount)));
    QVERIFY(with3xCount > with2xCount);
    // The same place in both, whatever the capture's size: laid out for it, not for the window.
    QVERIFY2(std::abs(bounds2x.center().x() - bounds3x.center().x()) < 0.03 && std::abs(bounds2x.center().y() - bounds3x.center().y()) < 0.03,
             qPrintable(QStringLiteral("2x at (%1, %2), 3x at (%3, %4)").arg(bounds2x.center().x()).arg(bounds2x.center().y())
                            .arg(bounds3x.center().x()).arg(bounds3x.center().y())));
    // And where the picks are on the model: the points as the view projects them on screen.
    QVariant fromScreen;
    QVERIFY(QMetaObject::invokeMethod(viewport->findChild<QObject*>(QStringLiteral("measurementOverlay")), "project",
                                      Q_RETURN_ARG(QVariant, fromScreen), Q_ARG(QVariant, QVariant::fromValue(QVector3D(-1, 1, 1)))));
    QVariant toScreen;
    QVERIFY(QMetaObject::invokeMethod(viewport->findChild<QObject*>(QStringLiteral("measurementOverlay")), "project",
                                      Q_RETURN_ARG(QVariant, toScreen), Q_ARG(QVariant, QVariant::fromValue(QVector3D(1, 1, 1)))));
    const QPointF middle = (fromScreen.toPointF() + toScreen.toPointF()) / 2.0;
    const QPointF expected(middle.x() / viewport->width(), middle.y() / viewport->height());
    QVERIFY2(std::abs(bounds2x.center().x() - expected.x()) < 0.08 && std::abs(bounds2x.center().y() - expected.y()) < 0.12,
             qPrintable(QStringLiteral("measurement at (%1, %2), picks around (%3, %4)").arg(bounds2x.center().x()).arg(bounds2x.center().y())
                            .arg(expected.x()).arg(expected.y())));
}

void QmlSmokeTest::viewportCaptureUsesRequestedRenderResolution()
{
    auto theme = std::make_unique<QQmlPropertyMap>();
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
        QStringLiteral("ViewsPanel.qml"),
        QStringLiteral("OpenHistoryDialog.qml"),
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

    // The add action now lives in the workspace beside the review action, not in this panel,
    // and the workspace needs a live 3D viewport that the offscreen platform cannot give us.
    // So the button's own binding is asserted instead: it is enabled on candidateValid, and
    // nothing can be queued before a view is chosen.
    QVERIFY(!draft.candidateValid());

    auto* topButton = findItemByObjectName(window.contentItem(), QStringLiteral("drawingStandardView-Top"));
    auto* frontButton = findItemByObjectName(window.contentItem(), QStringLiteral("drawingStandardView-Front"));
    QVERIFY(topButton != nullptr);
    QVERIFY(frontButton != nullptr);

    const auto clickCentre = [&window](QQuickItem* item) {
        const auto centre = item->mapToScene(QPointF(item->width() / 2.0, item->height() / 2.0));
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, centre.toPoint());
    };

    // Views are still chosen by real clicks on the real panel; only the add itself is invoked
    // directly, since its button is in the workspace.
    clickCentre(topButton);
    QTRY_COMPARE(draft.candidateViewLabel(), QStringLiteral("Top"));
    QVERIFY(draft.candidateValid());
    QVERIFY(!draft.addCandidateToQueue().isEmpty());
    QTRY_COMPARE(draft.queueCount(), 1);

    clickCentre(frontButton);
    QTRY_COMPARE(draft.candidateViewLabel(), QStringLiteral("Front"));
    QVERIFY(!draft.addCandidateToQueue().isEmpty());
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

void QmlSmokeTest::inspectTreeTakesModifierClicksAndShiftRanges()
{
    loupe::app::ApplicationController controller;
    auto* treeModel = qobject_cast<loupe::app::models::AssemblyTreeModel*>(
        controller.property("assemblyTree").value<QObject*>());
    QVERIFY(treeModel != nullptr);
    treeModel->replaceSnapshot({
        {QStringLiteral("root"), {}, QStringLiteral("assembly"), QStringLiteral("Housing"), 1},
        {QStringLiteral("p1"), QStringLiteral("root"), QStringLiteral("part"), QStringLiteral("One"), 1},
        {QStringLiteral("p2"), QStringLiteral("root"), QStringLiteral("part"), QStringLiteral("Two"), 1},
        {QStringLiteral("p3"), QStringLiteral("root"), QStringLiteral("part"), QStringLiteral("Three"), 1},
        {QStringLiteral("p4"), QStringLiteral("root"), QStringLiteral("part"), QStringLiteral("Four"), 1},
    });
    QQmlEngine engine;
    QQmlComponent themeComponent(&engine, QUrl::fromLocalFile(QStringLiteral(LOUPE_QML_DIR) + QStringLiteral("/Theme.qml")));
    std::unique_ptr<QObject> theme(themeComponent.create());
    QVERIFY(theme != nullptr);
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
    QQuickWindow window;
    window.resize(900, 600);
    workspace->setParentItem(window.contentItem());
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.isExposed(), 3000);
    const auto rowCentre = [&](const char* id) {
        auto* row = findItemByObjectName(window.contentItem(), QStringLiteral("treeRow-") + QLatin1String(id));
        return row ? row->mapToScene(QPointF(row->width() * 0.3, row->height() / 2)).toPoint() : QPoint(-1, -1);
    };
    QTRY_VERIFY_WITH_TIMEOUT(rowCentre("p4").x() >= 0, 3000);
    const auto click = [&](const char* id, Qt::KeyboardModifiers modifiers) {
        QTest::mouseClick(&window, Qt::LeftButton, modifiers, rowCentre(id));
        QTest::qWait(50);
    };
    const auto selected = [&] { auto ids = controller.selectedNodeIds(); ids.sort(); return ids.join(QLatin1Char(',')); };

    click("p1", Qt::NoModifier);
    QCOMPARE(selected(), QStringLiteral("p1"));
    // ⌘ (Qt's Control on a Mac) or Ctrl: add a row, and again to take it out.
    click("p3", Qt::ControlModifier);
    QCOMPARE(selected(), QStringLiteral("p1,p3"));
    click("p3", Qt::ControlModifier);
    QCOMPARE(selected(), QStringLiteral("p1"));
    // Shift: every row from the anchor to the one clicked, either way.
    click("p2", Qt::NoModifier);
    click("p4", Qt::ShiftModifier);
    QCOMPARE(selected(), QStringLiteral("p2,p3,p4"));
    click("p1", Qt::ShiftModifier);
    QCOMPARE(selected(), QStringLiteral("p1,p2"));
    // Shift with ⌘: the range is added to what is selected.
    click("p4", Qt::NoModifier);
    click("p1", Qt::ControlModifier);
    click("p3", Qt::ShiftModifier | Qt::ControlModifier);
    QCOMPARE(selected(), QStringLiteral("p1,p2,p3,p4"));
}

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

    // Tree search selects the first match, and stepping walks the rest in tree order.
    QVERIFY(QMetaObject::invokeMethod(workspaceObject.get(), "runTreeSearch", Q_ARG(QVariant, QStringLiteral("r"))));
    QCOMPARE(workspaceObject->property("treeMatches").toStringList(),
             (QStringList{QStringLiteral("occ-carrier"), QStringLiteral("occ-cover")}));
    QCOMPARE(controller.selectedNodeIds(), QStringList{QStringLiteral("occ-carrier")});
    QVERIFY(QMetaObject::invokeMethod(workspaceObject.get(), "stepTreeMatch", Q_ARG(QVariant, 1)));
    QCOMPARE(controller.selectedNodeIds(), QStringList{QStringLiteral("occ-cover")});
    QVERIFY(QMetaObject::invokeMethod(workspaceObject.get(), "stepTreeMatch", Q_ARG(QVariant, 1)));
    QCOMPARE(controller.selectedNodeIds(), QStringList{QStringLiteral("occ-carrier")});
    QVERIFY(QMetaObject::invokeMethod(workspaceObject.get(), "runTreeSearch", Q_ARG(QVariant, QStringLiteral("gasket"))));
    QVERIFY(workspaceObject->property("treeMatches").toStringList().isEmpty());
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

namespace {

// The preview JSON for an L-shaped part: 10 mm wide, 20 mm tall, tall on the left and short
// on the right. Deliberately asymmetric in both axes, because a symmetric outline cannot
// detect a flip and a square one cannot detect a transposed scale.
QString lShapedPreviewJson()
{
    return QStringLiteral(R"({"schemaVersion":1,"widthMm":10,"heightMm":20,
        "minX":0,"minY":0,"empty":false,"closedContours":1,"openContours":0,
        "layers":[{"name":"cut","role":"cut","contours":[
            {"closed":true,"points":[0,0,10,0,10,5,4,5,4,20,0,20,0,0]}]}],
        "warnings":[]})");
}

// Builds a DrawingPreview2D holding that drawing, parented into `window`.
// Returns the preview object; the controller and theme are owned by the caller.
QObject* buildPreviewWithLShape(QQmlEngine& engine, QQuickWindow& window,
                                loupe::app::drawing::DrawingWorkspaceController& draft,
                                QObject* theme, std::unique_ptr<QObject>& keepAlive)
{
    draft.replaceSnapshot(QStringLiteral(R"({"effectiveUnit":"mm","sourceToMillimeters":1,"nodes":[
        {"id":"root","name":"Assembly","kind":0,"parentId":""},
        {"id":"plate","name":"Base plate","kind":2,"parentId":"root"}
    ]})"));
    draft.setCandidateNodeId(QStringLiteral("plate"));
    draft.setCandidateStandardView(QStringLiteral("Top"), 0.0, 0.0, 1.0, 0.0, 1.0, 0.0);
    if (!draft.candidateValid()) return nullptr;

    QQmlComponent previewComponent(&engine, QUrl::fromLocalFile(
        QStringLiteral(LOUPE_QML_DIR) + QStringLiteral("/drawing/DrawingPreview2D.qml")));
    if (!previewComponent.isReady()) return nullptr;
    keepAlive.reset(previewComponent.createWithInitialProperties({
        {QStringLiteral("draft"), QVariant::fromValue(static_cast<QObject*>(&draft))},
        {QStringLiteral("theme"), QVariant::fromValue(theme)},
        {QStringLiteral("width"), 360},
        {QStringLiteral("height"), 460},
    }));
    if (!keepAlive) return nullptr;
    auto* item = qobject_cast<QQuickItem*>(keepAlive.get());
    if (!item) return nullptr;
    item->setParentItem(window.contentItem());
    window.show();
    if (!QTest::qWaitForWindowExposed(&window, 3000)) return nullptr;

    if (!QMetaObject::invokeMethod(keepAlive.get(), "applyPreview",
                                   Q_ARG(QVariant, lShapedPreviewJson()),
                                   Q_ARG(QVariant, draft.previewRevision()),
                                   Q_ARG(QVariant, false))) {
        return nullptr;
    }
    return keepAlive.get();
}

} // namespace

void QmlSmokeTest::drawingPreviewOrientationAndScaleSurviveTheScreenTransform()
{
    // Orientation and scale are the two properties of the preview a test can check but a
    // glance cannot reliably confirm -- an upside-down drawing of a nearly-symmetric part
    // looks fine. The IR is Y-up and the scene is Y-down, so exactly one flip must happen
    // here, and the extents must map proportionally.
    std::unique_ptr<QObject> theme;
    QQmlEngine engine;
    QQmlComponent themeComponent(&engine, QUrl::fromLocalFile(
        QStringLiteral(LOUPE_QML_DIR) + QStringLiteral("/Theme.qml")));
    QVERIFY2(themeComponent.isReady(), qPrintable(themeComponent.errorString()));
    theme.reset(themeComponent.create());
    QVERIFY(theme != nullptr);

    loupe::app::drawing::DrawingWorkspaceController draft;
    QQuickWindow window;
    window.resize(400, 500);
    std::unique_ptr<QObject> previewKeepAlive;
    auto* preview = buildPreviewWithLShape(engine, window, draft, theme.get(), previewKeepAlive);
    QVERIFY(preview != nullptr);
    QTRY_VERIFY(preview->property("hasGeometry").toBool());

    const auto paths = preview->property("closedPaths").toList();
    QCOMPARE(paths.size(), 1);
    const auto polyline = paths.at(0).toList();
    QCOMPARE(polyline.size(), 7);

    const auto surfaceHeight = preview->property("surfaceHeight").toReal();
    const auto surfaceWidth = preview->property("surfaceWidth").toReal();
    QVERIFY(surfaceWidth > 1.0);
    QVERIFY(surfaceHeight > 1.0);
    // 10 x 20 mm must stay twice as tall as it is wide. A transposed scale would still fill
    // the frame and still report the right extents in the label.
    QVERIFY2(std::abs(surfaceHeight / surfaceWidth - 2.0) < 0.01,
             qPrintable(QStringLiteral("aspect was %1").arg(surfaceHeight / surfaceWidth)));

    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();
    double minX = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    for (const auto& value : polyline) {
        const auto point = value.toPointF();
        minX = std::min(minX, point.x());
        maxX = std::max(maxX, point.x());
        minY = std::min(minY, point.y());
        maxY = std::max(maxY, point.y());
    }
    // The drawing fills its surface exactly, so the transform neither crops nor pads.
    QVERIFY(std::abs(maxX - minX - surfaceWidth) < 0.5);
    QVERIFY(std::abs(maxY - minY - surfaceHeight) < 0.5);
    QVERIFY(std::abs(minX) < 0.5);
    QVERIFY(std::abs(minY) < 0.5);

    // The flip itself: mm (0,20) is the top of the part, so it must land at the TOP of the
    // scene (y near 0), and mm (0,0) at the bottom. Getting this backwards is the classic
    // mirrored-output failure the requirements call out as highest severity.
    const auto firstPoint = polyline.at(0).toPointF();   // mm (0,0)   -> bottom-left
    const auto fifthPoint = polyline.at(4).toPointF();   // mm (4,20)  -> top
    QVERIFY2(firstPoint.y() > surfaceHeight - 0.5,
             qPrintable(QStringLiteral("mm y=0 mapped to scene y=%1, expected near %2")
                            .arg(firstPoint.y()).arg(surfaceHeight)));
    QVERIFY2(fifthPoint.y() < 0.5,
             qPrintable(QStringLiteral("mm y=20 mapped to scene y=%1, expected near 0")
                            .arg(fifthPoint.y())));

    // And the short-on-the-right asymmetry is preserved rather than mirrored in X: mm x=10
    // is the right edge, and it belongs to the short (lower) part of the L.
    const auto secondPoint = polyline.at(1).toPointF();  // mm (10,0)
    QVERIFY(secondPoint.x() > surfaceWidth - 0.5);
}

void QmlSmokeTest::drawingPreviewZoomKeepsThePointUnderTheCursorFixed()
{
    // Zoom-to-cursor is the one bit of the canvas navigation with real arithmetic in it, and
    // getting it wrong is not obviously wrong -- the drawing still zooms, it just drifts.
    std::unique_ptr<QObject> theme;
    QQmlEngine engine;
    QQmlComponent themeComponent(&engine, QUrl::fromLocalFile(
        QStringLiteral(LOUPE_QML_DIR) + QStringLiteral("/Theme.qml")));
    QVERIFY2(themeComponent.isReady(), qPrintable(themeComponent.errorString()));
    theme.reset(themeComponent.create());
    QVERIFY(theme != nullptr);

    loupe::app::drawing::DrawingWorkspaceController draft;
    QQuickWindow window;
    window.resize(400, 500);
    std::unique_ptr<QObject> previewKeepAlive;
    auto* preview = buildPreviewWithLShape(engine, window, draft, theme.get(), previewKeepAlive);
    QVERIFY(preview != nullptr);
    QTRY_VERIFY(preview->property("hasGeometry").toBool());

    auto* frame = findItemByObjectName(window.contentItem(), QStringLiteral("drawingPreviewSurface"));
    QVERIFY(frame != nullptr);
    auto* canvas = frame->parentItem();
    QVERIFY(canvas != nullptr);

    // A drawing opens fitted.
    QCOMPARE(preview->property("viewScale").toReal(), 1.0);
    QCOMPARE(preview->property("panX").toReal(), 0.0);

    // Where a given canvas point sits in surface-local space, before and after zooming.
    const auto surfaceLocalOf = [&](double canvasX, double canvasY) {
        const auto scale = preview->property("viewScale").toReal();
        const auto panX = preview->property("panX").toReal();
        const auto panY = preview->property("panY").toReal();
        const auto centreX = canvasX - canvas->width() / 2.0;
        const auto centreY = canvasY - canvas->height() / 2.0;
        return QPointF((centreX - panX) / scale, (centreY - panY) / scale);
    };

    const double pointX = canvas->width() * 0.72;
    const double pointY = canvas->height() * 0.31;
    const auto before = surfaceLocalOf(pointX, pointY);

    QVERIFY(QMetaObject::invokeMethod(preview, "zoomAt", Q_ARG(QVariant, 2.5),
                                      Q_ARG(QVariant, pointX), Q_ARG(QVariant, pointY)));
    QCOMPARE(preview->property("viewScale").toReal(), 2.5);
    const auto after = surfaceLocalOf(pointX, pointY);
    QVERIFY2(std::abs(after.x() - before.x()) < 0.01 && std::abs(after.y() - before.y()) < 0.01,
             qPrintable(QStringLiteral("cursor point moved from (%1,%2) to (%3,%4) in the drawing")
                            .arg(before.x()).arg(before.y()).arg(after.x()).arg(after.y())));

    // Zoom is clamped, so a wheel held down cannot leave the drawing unreachable.
    QVERIFY(QMetaObject::invokeMethod(preview, "zoomAt", Q_ARG(QVariant, 1000.0),
                                      Q_ARG(QVariant, pointX), Q_ARG(QVariant, pointY)));
    QVERIFY(preview->property("viewScale").toReal() <= 40.0);

    QVERIFY(QMetaObject::invokeMethod(preview, "resetView"));
    QCOMPARE(preview->property("viewScale").toReal(), 1.0);
    QCOMPARE(preview->property("panX").toReal(), 0.0);
    QCOMPARE(preview->property("panY").toReal(), 0.0);
    QCOMPARE(preview->property("viewRotation").toReal(), 0.0);

    // And navigation is display-only: the geometry handed to the Shape is unchanged by it.
    const auto pathsBefore = preview->property("closedPaths").toList();
    QVERIFY(QMetaObject::invokeMethod(preview, "zoomAt", Q_ARG(QVariant, 3.0),
                                      Q_ARG(QVariant, pointX), Q_ARG(QVariant, pointY)));
    QCOMPARE(preview->property("closedPaths").toList(), pathsBefore);
}

void QmlSmokeTest::drawingQueueThumbnailFlattensTheCapturedPreview()
{
    // The queue thumbnail is fed a preview captured when the drawing was queued. This covers
    // the consumer: given that data it produces drawable polylines, and given none it produces
    // an empty list rather than a broken path.
    loupe::app::drawing::DrawingWorkspaceController draft;
    draft.replaceSnapshot(QStringLiteral(R"({"effectiveUnit":"mm","sourceToMillimeters":1,"nodes":[
        {"id":"root","name":"Assembly","kind":0,"parentId":""},
        {"id":"plate","name":"Base plate","kind":2,"parentId":"root"}
    ]})"));
    draft.setDestination(QStringLiteral("/out"));
    draft.setCandidateNodeId(QStringLiteral("plate"));
    draft.setCandidateStandardView(QStringLiteral("Top"), 0.0, 0.0, 1.0, 0.0, 1.0, 0.0);
    const auto drawingId = draft.addCandidateToQueue();
    QVERIFY(!drawingId.isEmpty());

    QQmlEngine engine;
    QQmlComponent themeComponent(&engine, QUrl::fromLocalFile(
        QStringLiteral(LOUPE_QML_DIR) + QStringLiteral("/Theme.qml")));
    QVERIFY2(themeComponent.isReady(), qPrintable(themeComponent.errorString()));
    std::unique_ptr<QObject> theme(themeComponent.create());
    QVERIFY(theme != nullptr);

    QQmlComponent queueComponent(&engine, QUrl::fromLocalFile(
        QStringLiteral(LOUPE_QML_DIR) + QStringLiteral("/drawing/DrawingQueue.qml")));
    QVERIFY2(queueComponent.isReady(), qPrintable(queueComponent.errorString()));
    std::unique_ptr<QObject> queueObject(queueComponent.createWithInitialProperties({
        {QStringLiteral("draft"), QVariant::fromValue(static_cast<QObject*>(&draft))},
        {QStringLiteral("theme"), QVariant::fromValue(theme.get())},
        {QStringLiteral("width"), 360},
        {QStringLiteral("height"), 300},
    }));
    QVERIFY2(queueObject != nullptr, qPrintable(queueComponent.errorString()));

    // No capture yet: an empty list, not a malformed one.
    QVariant emptyPaths;
    QVERIFY(QMetaObject::invokeMethod(queueObject.get(), "thumbnailPaths",
                                      Q_RETURN_ARG(QVariant, emptyPaths),
                                      Q_ARG(QVariant, drawingId), Q_ARG(QVariant, 1.0),
                                      Q_ARG(QVariant, 40.0)));
    QVERIFY(emptyPaths.toList().isEmpty());

    // A fresh map, exactly as the workspace must assign it: mutating the existing object and
    // assigning the same reference back emits no change signal, which is what left every
    // thumbnail on its placeholder.
    QVariantMap previews;
    previews.insert(drawingId, QJsonDocument::fromJson(QByteArrayLiteral(
        R"({"widthMm":10,"heightMm":20,"minX":0,"minY":0,"empty":false,
            "closedContours":1,"openContours":0,
            "layers":[{"name":"cut","role":"cut","contours":[
                {"closed":true,"points":[0,0,10,0,10,20,0,20,0,0]}]}],"warnings":[]})")).object().toVariantMap());
    queueObject->setProperty("previews", previews);

    QVariant paths;
    QVERIFY(QMetaObject::invokeMethod(queueObject.get(), "thumbnailPaths",
                                      Q_RETURN_ARG(QVariant, paths),
                                      Q_ARG(QVariant, drawingId), Q_ARG(QVariant, 2.0),
                                      Q_ARG(QVariant, 40.0)));
    const auto polylines = paths.toList();
    QCOMPARE(polylines.size(), 1);
    QCOMPARE(polylines.at(0).toList().size(), 5);
}

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    // Settings of their own, never the app's, and somewhere every platform reads back from.
    QCoreApplication::setOrganizationName(QStringLiteral("Loupe"));
    QCoreApplication::setApplicationName(QStringLiteral("LoupeQmlSmokeTests"));
    QmlSmokeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "qml_smoke.moc"
