#include <QtTest/QTest>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QSignalSpy>
#include <QVector3D>
#include <QStandardPaths>
#include <QSettings>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QColor>
#include <QFile>
#include <QTemporaryDir>
#include <QPointer>
#include <QSet>
#include <QUrl>

#include <cstdio>

#include "app/ApplicationController.h"
#include "app/DocumentTabs.h"
#include "app/models/AssemblyTreeModel.h"
#include "core/domain/AssemblyTypes.h"
#include "fixtures/FixtureFactory.h"

namespace {
// QtTest's report is buffered when stdout is a pipe, so a process that dies before
// exit loses all of it. Unbuffered, each line reaches the CI log as it is written.
[[maybe_unused]] const int unbufferedStdout = std::setvbuf(stdout, nullptr, _IONBF, 0);
} // namespace

class ApplicationControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void inspectIsDefaultWorkspace();
    void tabsOpenAFileIntoAnEmptyTabOrANewOne();
    void exportsTheAssemblyWithoutItsHiddenParts();
    void treeRowsReportHiddenAndPartlyHidden();
    void closingTabsAlwaysLeavesOneAndKeepsANeighbourActive();
    void tabsReorderAndMoveBetweenWindowsWithoutReloading();
    void resolvesStandardViewsAgainstDocumentUpAxis();
    void rejectsUnknownUpAxisAndViewName();
    void emptyDocumentShowsDocumentInspector();
    void selectionShowsComponentInspector();
    void ownsInspectionTaskControllers();
    void formatsEstimatedMassInAppropriateUnits();
    void rejectsNonStepFilesBeforeStartingImport();
    void opensStepThroughWorkerAndRetainsSnapshot();
    void combinesMultiSelectionMassAndVisibilityActions();
    void selectAllTakesEveryPartShown();
    void reportsWorkerStartupFailure();
    void forwardsWorkerMeshToViewer();
    void replaysGeometryWithoutBlockingTheCaller();
    void fitViewNotifiesViewport();
    void renderModeIsSharedAndClamped();
    void transparencyTogglesTheSelectionAndOthers();
    void displayStatesSaveAndRestoreWhatIsHiddenOrTransparent();
    void eachOpenRecordsHowLongItTook();
    void isolatesHidesAndRestoresTheActiveComponentForViewerPresentation();
    void openingDocumentResetsTransientInspectionState();
    void reopensUnchangedStepFromLocalSnapshotCache();
    void manualUnitOverrideReimportsAtTheRequestedScale();
    void selectedComponentFeedsGeometryBackedMeasurementModes();
    void viewportSelectionDoesNotRecordMeasurementPick();
    void viewportPickRecordsPointMeasurementWithoutChangingSelection();
    void topologyPickRecordsEntityMeasurementWithoutChangingSelection();
    void appearanceOverridesMaterialAndSourceColor();
    void reopensSourceWithPersistedAppearanceOverrides();
    void uncolouredPartsCarryNoColourAndContainersShowTheirGeometry();
    void aMulticolouredPartsSwatchShowsTheColourOfMostOfItsSurface();
    void autoPartColoursGiveEachDefinitionItsOwnColour();
    void autoPartColoursTreatOneSharedFileColourAsNone();
};

void ApplicationControllerTest::tabsOpenAFileIntoAnEmptyTabOrANewOne()
{
    loupe::app::DocumentTabs tabs(QStringLiteral(LOUPE_WORKER_PATH));
    QCOMPARE(tabs.count(), 1);
    auto* first = tabs.active();
    QVERIFY(first != nullptr);
    // Nothing open: the file goes into the current tab.
    QCOMPARE(tabs.documentForOpening(), first);
    QCOMPARE(tabs.count(), 1);

    const auto fixture = loupe::tests::writeRepeatedBoxAssembly(QStringLiteral("tab-one.step").toStdString(), loupe::tests::FixtureUnit::Millimeter);
    first->openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));
    QTRY_VERIFY_WITH_TIMEOUT(first->documentState() == loupe::app::DocumentState::TreeReady, 10'000);
    QCOMPARE(tabs.data(tabs.index(0), loupe::app::DocumentTabs::NameRole).toString(), QStringLiteral("tab-one.step"));

    // Something open: a second file gets its own tab, made active, leaving the first as it was.
    QSignalSpy added(&tabs, &loupe::app::DocumentTabs::documentAdded);
    auto* second = tabs.documentForOpening();
    QVERIFY(second != first);
    QCOMPARE(tabs.count(), 2);
    QCOMPARE(tabs.activeIndex(), 1);
    QCOMPARE(tabs.active(), second);
    QCOMPARE(added.count(), 1);
    QCOMPARE(first->documentState(), loupe::app::DocumentState::TreeReady);
    QVERIFY(!tabs.isBusy(0));
}

void ApplicationControllerTest::tabsReorderAndMoveBetweenWindowsWithoutReloading()
{
    loupe::app::DocumentTabs tabs(QStringLiteral(LOUPE_WORKER_PATH));
    auto* first = tabs.active();
    auto* second = tabs.add();
    auto* third = tabs.add();
    tabs.setActiveIndex(0);

    // Reorder: dragging the active first tab to the end keeps it active.
    tabs.move(0, 2);
    QCOMPARE(tabs.documentAt(0), second);
    QCOMPARE(tabs.documentAt(2), first);
    QCOMPARE(tabs.active(), first);
    QCOMPARE(tabs.activeIndex(), 2);
    tabs.move(2, 0);
    QCOMPARE(tabs.documentAt(0), first);

    // Drag out: the document leaves this window's tabs, alive and unchanged, and another
    // window adopts it.
    const auto fixture = loupe::tests::writeRepeatedBoxAssembly(QStringLiteral("tab-move.step").toStdString(), loupe::tests::FixtureUnit::Millimeter);
    third->openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));
    QTRY_VERIFY_WITH_TIMEOUT(third->documentState() == loupe::app::DocumentState::TreeReady, 10'000);
    QPointer<loupe::app::ApplicationController> moving(third);
    auto* taken = tabs.take(2);
    QCOMPARE(taken, third);
    QCOMPARE(tabs.count(), 2);
    loupe::app::DocumentTabs otherWindow(QStringLiteral(LOUPE_WORKER_PATH));
    otherWindow.adopt(taken);
    otherWindow.close(0); // the new window's own empty tab
    QCoreApplication::processEvents();
    QVERIFY(moving);
    QCOMPARE(otherWindow.count(), 1);
    QCOMPARE(otherWindow.active(), third);
    QCOMPARE(third->parent(), &otherWindow);
    QCOMPARE(third->documentState(), loupe::app::DocumentState::TreeReady);

    // The last tab cannot be dragged out of its window.
    QVERIFY(otherWindow.take(0) == nullptr);
}

void ApplicationControllerTest::closingTabsAlwaysLeavesOneAndKeepsANeighbourActive()
{
    loupe::app::DocumentTabs tabs(QStringLiteral(LOUPE_WORKER_PATH));
    auto* first = tabs.active();
    auto* second = tabs.add();
    auto* third = tabs.add();
    QCOMPARE(tabs.count(), 3);
    QCOMPARE(tabs.active(), third);

    // Closing a tab before the active one keeps the same document active.
    tabs.setActiveIndex(2);
    tabs.close(0);
    QCOMPARE(tabs.count(), 2);
    QCOMPARE(tabs.active(), third);
    QCOMPARE(tabs.documentAt(0), second);
    // Closing the active tab moves to its neighbour.
    tabs.close(1);
    QCOMPARE(tabs.active(), second);
    // Closing the last tab leaves a fresh empty one.
    tabs.close(0);
    QCOMPARE(tabs.count(), 1);
    QVERIFY(tabs.active() != nullptr);
    QVERIFY(tabs.active() != second);
    QCOMPARE(tabs.active()->documentState(), loupe::app::DocumentState::Empty);
    Q_UNUSED(first);
}

void ApplicationControllerTest::inspectIsDefaultWorkspace()
{
    loupe::app::ApplicationController controller;

    QCOMPARE(controller.workspace(), loupe::app::Workspace::Inspect);
}

void ApplicationControllerTest::emptyDocumentShowsDocumentInspector()
{
    loupe::app::ApplicationController controller;

    QCOMPARE(controller.inspectorMode(), loupe::app::InspectorMode::Document);
    QVERIFY(controller.activeNodeId().isEmpty());
}

void ApplicationControllerTest::selectionShowsComponentInspector()
{
    loupe::app::ApplicationController controller;
    controller.setActiveNodeId(QStringLiteral("occ-front-cover"));

    QCOMPARE(controller.inspectorMode(), loupe::app::InspectorMode::Component);
}

void ApplicationControllerTest::ownsInspectionTaskControllers()
{
    loupe::app::ApplicationController controller;

    QVERIFY(controller.measurementController() != nullptr);
    QVERIFY(controller.sectionController() != nullptr);
    QVERIFY(controller.captureController() != nullptr);
}

void ApplicationControllerTest::formatsEstimatedMassInAppropriateUnits()
{
    const auto grams = loupe::app::ApplicationController::formatMassKg(0.125);
    const auto kilograms = loupe::app::ApplicationController::formatMassKg(1.5);

    QCOMPARE(grams, QLocale{}.toString(125.0, 'f', 2) + QStringLiteral(" g"));
    QCOMPARE(kilograms, QLocale{}.toString(1.5, 'f', 2) + QStringLiteral(" kg"));
}

void ApplicationControllerTest::rejectsNonStepFilesBeforeStartingImport()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("notes.txt"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write("not a STEP file") > 0);
    file.close();

    loupe::app::ApplicationController controller;
    controller.openFile(QUrl::fromLocalFile(path));

    QCOMPARE(controller.documentState(), loupe::app::DocumentState::Invalid);
    QVERIFY(controller.errorMessage().contains(QStringLiteral(".step"), Qt::CaseInsensitive));
    QVERIFY(!controller.importInProgress());
}

void ApplicationControllerTest::opensStepThroughWorkerAndRetainsSnapshot()
{
    const auto fixture = loupe::tests::writeRepeatedBoxAssembly(QStringLiteral("controller-worker.step").toStdString(), loupe::tests::FixtureUnit::Millimeter);
    loupe::app::ApplicationController controller(QStringLiteral(LOUPE_WORKER_PATH));
    controller.openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));

    QTRY_VERIFY_WITH_TIMEOUT(controller.documentState() != loupe::app::DocumentState::Opening, 10'000);
    QVERIFY2(controller.documentState() == loupe::app::DocumentState::TreeReady,
             qPrintable(controller.errorMessage()));
    QVERIFY(!controller.snapshotJson().isEmpty());
    QCOMPARE(controller.documentState(), loupe::app::DocumentState::TreeReady);
    QVERIFY(controller.assemblyTreeModel()->rowCount() > 0);
    QCOMPARE(controller.effectiveUnit(), QStringLiteral("mm"));
    // Output defaults to folders beside the file, named after it (#3).
    const QFileInfo source(QString::fromStdString(fixture.string()));
    const auto* exportWorkspace = controller.property("exportWorkspace").value<QObject*>();
    QCOMPARE(exportWorkspace->property("destination").toString(),
             QDir(source.absolutePath()).filePath(QStringLiteral("controller-worker export")));
    QVERIFY(exportWorkspace->property("destinationIsDefault").toBool());
    const auto geometry = QJsonDocument::fromJson(controller.snapshotJson().toUtf8()).object().value(QStringLiteral("geometry")).toArray();
    QVERIFY(!geometry.isEmpty());
    controller.setActiveNodeId(geometry.first().toObject().value(QStringLiteral("nodeId")).toString());
    QTRY_VERIFY_WITH_TIMEOUT(controller.modelExtentMm() > 0.0, 10'000);
    QTRY_VERIFY_WITH_TIMEOUT(controller.activeVolumeMm3() > 0.0, 10'000);
    QVERIFY(controller.assignActiveMaterial(QStringLiteral("aluminum-6061")));
    QVERIFY(controller.estimatedMassKg() > 0.0);
}

void ApplicationControllerTest::combinesMultiSelectionMassAndVisibilityActions()
{
    const auto fixture = loupe::tests::writeRepeatedBoxAssembly(QStringLiteral("controller-multiselect.step").toStdString(), loupe::tests::FixtureUnit::Millimeter);
    loupe::app::ApplicationController controller(QStringLiteral(LOUPE_WORKER_PATH));
    controller.openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.importInProgress(), 10'000);

    const auto snapshot = QJsonDocument::fromJson(controller.snapshotJson().toUtf8()).object();
    const auto geometry = snapshot.value(QStringLiteral("geometry")).toArray();
    QVERIFY(geometry.size() >= 2);
    const auto first = geometry.at(0).toObject().value(QStringLiteral("nodeId")).toString();
    const auto second = geometry.at(1).toObject().value(QStringLiteral("nodeId")).toString();
    controller.setActiveNodeId(first);
    QVERIFY(controller.assignActiveMaterial(QStringLiteral("aluminum-6061")));
    const auto firstMass = controller.estimatedMassKg();
    controller.setActiveNodeId(second);
    QVERIFY(controller.assignActiveMaterial(QStringLiteral("aluminum-6061")));

    controller.setActiveNodeId(first);
    controller.selectNode(second, true);
    QCOMPARE(controller.selectedNodeCount(), 2);
    QVERIFY(controller.isNodeSelected(first));
    QVERIFY(controller.isNodeSelected(second));
    const auto combinedMass = controller.estimatedMassKg();
    QVERIFY(combinedMass > firstMass);

    const auto rootId = snapshot.value(QStringLiteral("nodes")).toArray().first().toObject().value(QStringLiteral("id")).toString();
    controller.setActiveNodeId(rootId);
    QCOMPARE(controller.estimatedMassKg(), combinedMass);
    controller.hideActiveNode();
    QVERIFY(!controller.isNodeVisible(first));
    QVERIFY(!controller.isNodeVisible(second));
}

void ApplicationControllerTest::selectAllTakesEveryPartShown()
{
    // Cmd/Ctrl+A in Inspect: every part with geometry, not the ones hidden, the active one kept.
    const auto fixture = loupe::tests::writeRepeatedBoxAssembly(QStringLiteral("controller-select-all.step").toStdString(), loupe::tests::FixtureUnit::Millimeter);
    loupe::app::ApplicationController controller(QStringLiteral(LOUPE_WORKER_PATH));
    controller.openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.importInProgress(), 10'000);
    const auto snapshot = QJsonDocument::fromJson(controller.snapshotJson().toUtf8()).object();
    const auto geometry = snapshot.value(QStringLiteral("geometry")).toArray();
    QVERIFY(geometry.size() >= 2);
    const auto first = geometry.at(0).toObject().value(QStringLiteral("nodeId")).toString();
    const auto second = geometry.at(1).toObject().value(QStringLiteral("nodeId")).toString();

    controller.setActiveNodeId(second);
    controller.selectAllParts();
    QCOMPARE(controller.selectedNodeCount(), geometry.size());
    QVERIFY(controller.isNodeSelected(first) && controller.isNodeSelected(second));
    QCOMPARE(controller.activeNodeId(), second);

    controller.setActiveNodeId(first);
    controller.hideActiveNode();
    controller.selectAllParts();
    QCOMPARE(controller.selectedNodeCount(), geometry.size() - 1);
    QVERIFY(!controller.isNodeSelected(first));
}

void ApplicationControllerTest::reportsWorkerStartupFailure()
{
    const auto fixture = loupe::tests::writeRepeatedBoxAssembly(QStringLiteral("controller-missing-worker.step").toStdString(), loupe::tests::FixtureUnit::Millimeter);
    QTemporaryDir cacheDirectory;
    QVERIFY(cacheDirectory.isValid());
    loupe::app::ApplicationController controller(QStringLiteral("/missing/loupe-worker"), cacheDirectory.path());

    controller.openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));

    QTRY_COMPARE_WITH_TIMEOUT(controller.documentState(), loupe::app::DocumentState::WorkerFailed, 2'000);
    QVERIFY(!controller.errorMessage().isEmpty());
}

void ApplicationControllerTest::forwardsWorkerMeshToViewer()
{
    const auto fixture = loupe::tests::writeRepeatedBoxAssembly(QStringLiteral("controller-mesh.step").toStdString(), loupe::tests::FixtureUnit::Millimeter);
    loupe::app::ApplicationController controller(QStringLiteral(LOUPE_WORKER_PATH));
    QSignalSpy sharedSpy(&controller, &loupe::app::ApplicationController::sharedMeshReady);
    QSignalSpy meshSpy(&controller, &loupe::app::ApplicationController::meshInstanceReady);

    controller.openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));

    QTRY_VERIFY_WITH_TIMEOUT(!controller.importInProgress() && controller.documentState() == loupe::app::DocumentState::TreeReady, 10'000);
    // Two copies of one box: its mesh is sent once per pass, and each copy is placed.
    QCOMPARE(sharedSpy.count(), 2);
    QVERIFY(!sharedSpy.first().at(2).toByteArray().isEmpty());
    QCOMPARE(meshSpy.count(), 4);
    QSet<QString> nodes;
    QSet<QString> sources;
    QList<QVector3D> positions;
    for (const auto& arguments : meshSpy) {
        QVERIFY(!arguments.at(0).toString().isEmpty());
        QVERIFY(!arguments.at(1).toString().isEmpty());
        nodes.insert(arguments.at(0).toString());
        sources.insert(arguments.at(2).toString());
        positions.append(arguments.at(4).value<QVector3D>());
    }
    QCOMPARE(nodes.size(), 2);
    QCOMPARE(sources.size(), 1);
    QCOMPARE(sources.values().first(), sharedSpy.first().at(0).toString());
    // The copies sit 25 mm apart, as the fixture places them.
    QVERIFY(qAbs(qAbs(positions.at(0).x() - positions.at(1).x()) - 25.0F) < 1.0e-4F);
}

void ApplicationControllerTest::replaysGeometryWithoutBlockingTheCaller()
{
    const auto fixture = loupe::tests::writeRepeatedBoxAssembly(
        QStringLiteral("controller-replay.step").toStdString(), loupe::tests::FixtureUnit::Millimeter);
    QTemporaryDir cacheDirectory;
    QVERIFY(cacheDirectory.isValid());
    loupe::app::ApplicationController controller(QStringLiteral(LOUPE_WORKER_PATH), cacheDirectory.path());
    controller.openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.importInProgress(), 10'000);

    QSignalSpy meshSpy(&controller, &loupe::app::ApplicationController::meshInstanceReady);
    controller.replayGeometry();

    QCOMPARE(meshSpy.count(), 0);
    QTRY_VERIFY_WITH_TIMEOUT(meshSpy.count() > 0, 3'000);
}

void ApplicationControllerTest::renderModeIsSharedAndClamped()
{
    // One mode for every viewport, so switching workspace keeps it.
    loupe::app::ApplicationController controller;
    QSignalSpy spy(&controller, &loupe::app::ApplicationController::renderModeChanged);
    // Solid + Edges by default (#13).
    QCOMPARE(controller.renderMode(), 1);
    controller.setRenderMode(0);
    controller.setRenderMode(0);
    QCOMPARE(controller.renderMode(), 0);
    QCOMPARE(spy.count(), 1);
    controller.setRenderMode(7);
    QCOMPARE(controller.renderMode(), 2);
}

namespace {
QStringList geometryNodeIds(const loupe::app::ApplicationController& controller);
} // namespace

void ApplicationControllerTest::transparencyTogglesTheSelectionAndOthers()
{
    const auto fixture = loupe::tests::writeTwoDefinitionAssembly(QStringLiteral("controller-transparent.step").toStdString());
    loupe::app::ApplicationController controller(QStringLiteral(LOUPE_WORKER_PATH));
    controller.openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.importInProgress() && controller.documentState() == loupe::app::DocumentState::TreeReady, 10'000);
    const auto parts = geometryNodeIds(controller);
    QCOMPARE(parts.size(), 2);
    QSignalSpy visibility(&controller, &loupe::app::ApplicationController::visibilityChanged);

    controller.setActiveNodeId(parts.at(0));
    QVERIFY(!controller.isSelectionTransparent());
    QCOMPARE(controller.nodeOpacity(parts.at(0)), 1.0);
    controller.toggleTransparentSelection();
    QVERIFY(controller.isNodeTransparent(parts.at(0)));
    QVERIFY(!controller.isNodeTransparent(parts.at(1)));
    QVERIFY(controller.isNodeVisible(parts.at(0)));
    QVERIFY(visibility.count() > 0);
    // The menus now offer Make opaque; the part draws at the default opacity.
    QVERIFY(controller.isSelectionTransparent());
    QCOMPARE(controller.nodeTransparencyState(parts.at(0)), 2);
    QCOMPARE(controller.nodeTransparencyState(parts.at(1)), 0);
    QCOMPARE(controller.nodeOpacity(parts.at(0)), loupe::app::ApplicationController::kDefaultTransparentOpacity);
    QCOMPARE(controller.selectionOpacity(), loupe::app::ApplicationController::kDefaultTransparentOpacity);
    // Opacity is adjustable, and says so to the viewport even though the part stays transparent.
    const auto before = visibility.count();
    controller.setSelectionOpacity(0.2);
    QVERIFY(visibility.count() > before);
    QCOMPARE(controller.nodeOpacity(parts.at(0)), 0.2);
    // It is kept across a toggle off and on.
    controller.toggleTransparentSelection();
    QVERIFY(!controller.isSelectionTransparent());
    QCOMPARE(controller.nodeOpacity(parts.at(0)), 1.0);
    controller.toggleTransparentSelection();
    QCOMPARE(controller.nodeOpacity(parts.at(0)), 0.2);
    // All the way up makes it opaque and forgets the setting.
    controller.setSelectionOpacity(1.0);
    QVERIFY(!controller.isNodeTransparent(parts.at(0)));
    // The slider on an opaque part makes it transparent at that opacity.
    controller.setSelectionOpacity(0.6);
    QVERIFY(controller.isNodeTransparent(parts.at(0)));
    QCOMPARE(controller.nodeOpacity(parts.at(0)), 0.6);
    controller.setSelectionOpacity(1.0);
    controller.toggleTransparentSelection();
    QCOMPARE(controller.nodeOpacity(parts.at(0)), loupe::app::ApplicationController::kDefaultTransparentOpacity);
    controller.toggleTransparentSelection();
    QVERIFY(!controller.isNodeTransparent(parts.at(0)));
    // Edges stay on a transparent part by default, and can be turned off for it.
    controller.toggleTransparentSelection();
    QVERIFY(controller.nodeShowsTransparentEdges(parts.at(0)));
    QVERIFY(controller.selectionShowsTransparentEdges());
    const auto beforeEdges = visibility.count();
    controller.setSelectionTransparentEdges(false);
    QVERIFY(visibility.count() > beforeEdges);
    QVERIFY(!controller.nodeShowsTransparentEdges(parts.at(0)));
    QVERIFY(controller.nodeShowsTransparentEdges(parts.at(1)));
    controller.setSelectionTransparentEdges(true);
    QVERIFY(controller.nodeShowsTransparentEdges(parts.at(0)));
    controller.toggleTransparentSelection();
    // The tree row's own toggle.
    controller.setNodeTransparent(parts.at(1), true);
    QVERIFY(controller.isNodeTransparent(parts.at(1)));
    controller.setNodeTransparent(parts.at(1), false);
    QVERIFY(!controller.isNodeTransparent(parts.at(1)));

    controller.makeOthersTransparent();
    QVERIFY(!controller.isNodeTransparent(parts.at(0)));
    QVERIFY(controller.isNodeTransparent(parts.at(1)));
    controller.showAllNodes();
    QVERIFY(!controller.isNodeTransparent(parts.at(1)));
}

void ApplicationControllerTest::eachOpenRecordsHowLongItTook()
{
    // Outlives the test: later opens still write the shared history to this path.
    static QTemporaryDir storage;
    auto& history = loupe::app::models::OpenHistory::shared();
    history.setStoragePath(QDir(storage.path()).filePath(QStringLiteral("open-history.json")));
    const auto fixture = loupe::tests::writeTwoDefinitionAssembly(QStringLiteral("controller-open-time.step").toStdString());
    loupe::app::ApplicationController controller(QStringLiteral(LOUPE_WORKER_PATH));
    controller.openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));
    QVERIFY(controller.openTiming().value(QStringLiteral("opening")).toBool());
    QTRY_COMPARE_WITH_TIMEOUT(history.records().size(), 1, 15'000);

    const auto record = history.records().constFirst().toMap();
    QCOMPARE(record.value(QStringLiteral("file")).toString(), QStringLiteral("controller-open-time.step"));
    QCOMPARE(record.value(QStringLiteral("parts")).toInt(), 2);
    QVERIFY(record.value(QStringLiteral("sizeBytes")).toLongLong() > 0);
    // In order: first geometry on screen, every part shown, fully detailed.
    const auto view = record.value(QStringLiteral("viewMs")).toLongLong();
    const auto preview = record.value(QStringLiteral("previewMs")).toLongLong();
    const auto full = record.value(QStringLiteral("fullMs")).toLongLong();
    QVERIFY2(view >= 0 && view <= full, qPrintable(QStringLiteral("%1 %2 %3").arg(view).arg(preview).arg(full)));
    QVERIFY2(preview < 0 || (preview >= view && preview <= full), qPrintable(QStringLiteral("%1 %2 %3").arg(view).arg(preview).arg(full)));
    QVERIFY(!controller.openTiming().value(QStringLiteral("opening")).toBool());
    QCOMPARE(controller.openTiming().value(QStringLiteral("fullMs")).toLongLong(), full);

    // As CSV, for setting against another program's times.
    const auto csv = history.csv();
    QVERIFY(csv.startsWith(QStringLiteral("opened_at,file,size_mb,parts,from_cache,ready_to_view_s")));
    QVERIFY(csv.contains(QStringLiteral("controller-open-time.step")));
    QCOMPARE(csv.count(QLatin1Char('\n')), 2);
    history.clear();
    QVERIFY(history.records().isEmpty());
}

void ApplicationControllerTest::displayStatesSaveAndRestoreWhatIsHiddenOrTransparent()
{
    QStandardPaths::setTestModeEnabled(true);
    // As the app does in main: without an organisation, QSettings on Windows (the
    // registry) neither reads nor writes, so nothing would be kept with the file.
    QCoreApplication::setOrganizationName(QStringLiteral("Loupe"));
    QCoreApplication::setApplicationName(QStringLiteral("LoupeApplicationControllerTests"));
    const auto fixture = loupe::tests::writeTwoDefinitionAssembly(QStringLiteral("controller-display-states.step").toStdString());
    QSettings().remove(loupe::app::models::SavedViews::storageKeyFor(QFileInfo(QString::fromStdString(fixture.string())).absoluteFilePath()));
    loupe::app::ApplicationController controller(QStringLiteral(LOUPE_WORKER_PATH));
    controller.openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.importInProgress() && controller.documentState() == loupe::app::DocumentState::TreeReady, 10'000);
    const auto parts = geometryNodeIds(controller);
    QCOMPARE(parts.size(), 2);
    auto& views = controller.savedViews();
    QVERIFY(views.displayStates().isEmpty());

    // Part 0 hidden, part 1 transparent at 30% with its edges off: saved under a name.
    controller.setActiveNodeId(parts.at(0));
    controller.hideActiveNode();
    controller.setActiveNodeId(parts.at(1));
    controller.setSelectionOpacity(0.3);
    controller.setSelectionTransparentEdges(false);
    const auto id = controller.saveDisplayState(QStringLiteral("Cover off"));
    QVERIFY(!id.isEmpty());
    QCOMPARE(views.displayStates().size(), 1);
    const auto entry = views.displayStates().constFirst().toMap();
    QCOMPARE(entry.value(QStringLiteral("name")).toString(), QStringLiteral("Cover off"));
    QCOMPARE(entry.value(QStringLiteral("summary")).toString(), QStringLiteral("1 hidden · 1 transparent"));

    // Reset to default: everything shown and opaque; then the saved state comes back whole.
    controller.resetDisplayState();
    QVERIFY(controller.isNodeVisible(parts.at(0)));
    QVERIFY(!controller.isNodeTransparent(parts.at(1)));
    QVERIFY(controller.applyDisplayState(id));
    QVERIFY(!controller.isNodeVisible(parts.at(0)));
    QVERIFY(controller.isNodeTransparent(parts.at(1)));
    QCOMPARE(controller.nodeOpacity(parts.at(1)), 0.3);
    QVERIFY(!controller.nodeShowsTransparentEdges(parts.at(1)));
    // Undoable as one step.
    QVERIFY(controller.canRestoreVisibility());

    // Default names count up; rename and delete.
    controller.resetDisplayState();
    const auto second = controller.saveDisplayState({});
    QCOMPARE(views.displayStates().constFirst().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Display 1"));
    QCOMPARE(views.displayStates().constFirst().toMap().value(QStringLiteral("summary")).toString(), QStringLiteral("Everything shown"));
    views.rename(second, QStringLiteral("All parts"));
    QCOMPARE(views.displayStates().constFirst().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("All parts"));

    // Kept with the file: another controller opening it sees both.
    {
        loupe::app::models::SavedViews reopened;
        reopened.setDocument(QFileInfo(QString::fromStdString(fixture.string())).absoluteFilePath());
        QCOMPARE(reopened.displayStates().size(), 2);
        QCOMPARE(reopened.data(id).value(QStringLiteral("hidden")).toList().size(), 1);
    }
    views.remove(second);
    views.remove(id);
    QVERIFY(views.displayStates().isEmpty());

    // Camera views are stored as given.
    const auto camera = controller.saveCameraView(QStringLiteral("Front"), {{QStringLiteral("distance"), 42.0}}, QStringLiteral("Orthographic · Front"));
    QCOMPARE(controller.cameraView(camera).value(QStringLiteral("distance")).toDouble(), 42.0);
    QCOMPARE(views.cameraViews().size(), 1);
    views.remove(camera);
}

void ApplicationControllerTest::exportsTheAssemblyWithoutItsHiddenParts()
{
    const auto fixture = loupe::tests::writeTwoDefinitionAssembly(QStringLiteral("controller-strip.step").toStdString());
    loupe::app::ApplicationController controller(QStringLiteral(LOUPE_WORKER_PATH));
    controller.openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.importInProgress() && controller.documentState() == loupe::app::DocumentState::TreeReady, 10'000);
    const auto parts = geometryNodeIds(controller);
    QCOMPARE(parts.size(), 2);
    QCOMPARE(controller.hiddenPartCount(), 0);
    controller.setActiveNodeId(parts.at(0));
    controller.hideActiveNode();
    QCOMPARE(controller.hiddenPartCount(), 1);

    QTemporaryDir output;
    QVERIFY(output.isValid());
    const auto destination = QDir(output.path()).filePath(QStringLiteral("stripped.step"));
    QVERIFY(controller.exportAssemblyWithoutHiddenParts(QUrl::fromLocalFile(destination)));
    QVERIFY(controller.assemblyExportInProgress());
    QTRY_VERIFY_WITH_TIMEOUT(!controller.assemblyExportInProgress(), 10'000);
    QVERIFY2(controller.assemblyExportSucceeded(), qPrintable(controller.assemblyExportMessage()));
    QVERIFY(QFileInfo::exists(destination));

    // Read back: one of the two parts is left.
    loupe::app::ApplicationController reread(QStringLiteral(LOUPE_WORKER_PATH));
    reread.openFile(QUrl::fromLocalFile(destination));
    QTRY_VERIFY_WITH_TIMEOUT(!reread.importInProgress() && reread.documentState() == loupe::app::DocumentState::TreeReady, 10'000);
    QCOMPARE(geometryNodeIds(reread).size(), 1);
}

void ApplicationControllerTest::treeRowsReportHiddenAndPartlyHidden()
{
    const auto fixture = loupe::tests::writeTwoDefinitionAssembly(QStringLiteral("controller-tree-visibility.step").toStdString());
    loupe::app::ApplicationController controller(QStringLiteral(LOUPE_WORKER_PATH));
    controller.openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.importInProgress() && controller.documentState() == loupe::app::DocumentState::TreeReady, 10'000);
    const auto parts = geometryNodeIds(controller);
    QCOMPARE(parts.size(), 2);
    const auto root = controller.assemblyTreeModel()->data(controller.assemblyTreeModel()->index(0, 0),
                                                           loupe::app::models::AssemblyTreeModel::StableIdRole).toString();
    QCOMPARE(controller.nodeVisibilityState(root), 0);

    const auto revision = controller.visibilityRevision();
    controller.setNodeHidden(parts.at(0), true);
    QVERIFY(controller.visibilityRevision() > revision);
    QCOMPARE(controller.nodeVisibilityState(parts.at(0)), 2);
    QCOMPARE(controller.nodeVisibilityState(parts.at(1)), 0);
    QCOMPARE(controller.nodeVisibilityState(root), 1);

    controller.setNodeHidden(root, true);
    QCOMPARE(controller.nodeVisibilityState(root), 2);
    controller.setNodeHidden(root, false);
    QCOMPARE(controller.nodeVisibilityState(root), 0);
    // Showing the assembly is one undoable step.
    controller.restoreVisibility();
    QCOMPARE(controller.nodeVisibilityState(root), 2);
}

void ApplicationControllerTest::fitViewNotifiesViewport()
{
    loupe::app::ApplicationController controller;
    QSignalSpy fitSpy(&controller, &loupe::app::ApplicationController::fitRequested);

    controller.fitView();

    QCOMPARE(fitSpy.count(), 1);
}

void ApplicationControllerTest::isolatesHidesAndRestoresTheActiveComponentForViewerPresentation()
{
    loupe::app::ApplicationController controller;
    controller.setActiveNodeId(QStringLiteral("occ-1"));

    controller.isolateActiveNode();
    QCOMPARE(controller.viewerPresentation(), loupe::app::ViewerPresentation::Isolate);
    controller.hideActiveNode();
    QCOMPARE(controller.viewerPresentation(), loupe::app::ViewerPresentation::Isolate);
    controller.restoreFullAssembly();
    QCOMPARE(controller.viewerPresentation(), loupe::app::ViewerPresentation::Full);
}

void ApplicationControllerTest::openingDocumentResetsTransientInspectionState()
{
    const auto fixture = loupe::tests::writeRepeatedBoxAssembly(QStringLiteral("controller-reset.step").toStdString(), loupe::tests::FixtureUnit::Millimeter);
    loupe::app::ApplicationController controller(QStringLiteral(LOUPE_WORKER_PATH));
    controller.setActiveNodeId(QStringLiteral("occ-1"));
    controller.isolateActiveNode();
    auto* section = qobject_cast<loupe::app::tools::SectionController*>(controller.sectionController());
    QVERIFY(section != nullptr);
    section->setEnabled(true);

    controller.openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));

    QCOMPARE(controller.viewerPresentation(), loupe::app::ViewerPresentation::Full);
    QVERIFY(!section->enabled());
}

void ApplicationControllerTest::reopensUnchangedStepFromLocalSnapshotCache()
{
    const auto fixture = loupe::tests::writeRepeatedBoxAssembly(QStringLiteral("controller-cache.step").toStdString(), loupe::tests::FixtureUnit::Millimeter);
    QTemporaryDir cacheDirectory;
    QVERIFY(cacheDirectory.isValid());
    {
        loupe::app::ApplicationController initial(QStringLiteral(LOUPE_WORKER_PATH), cacheDirectory.path());
        initial.openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));
        QTRY_COMPARE_WITH_TIMEOUT(initial.documentState(), loupe::app::DocumentState::TreeReady, 10'000);
        QVERIFY(!initial.cacheHit());
    }

    loupe::app::ApplicationController reopened(QStringLiteral(LOUPE_WORKER_PATH), cacheDirectory.path());
    QSignalSpy meshSpy(&reopened, &loupe::app::ApplicationController::meshInstanceReady);
    QSignalSpy snapshotSpy(&reopened, &loupe::app::ApplicationController::snapshotChanged);
    reopened.openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));

    QTRY_COMPARE_WITH_TIMEOUT(reopened.documentState(), loupe::app::DocumentState::TreeReady, 10'000);
    QVERIFY(reopened.cacheHit());
    QTRY_VERIFY_WITH_TIMEOUT(meshSpy.count() > 0, 10'000);
    QTRY_VERIFY_WITH_TIMEOUT(!reopened.importInProgress(), 10'000);
    // Opening emits one reset before the cached snapshot; the background worker must not add a third reset.
    QCOMPARE(snapshotSpy.count(), 2);
}

void ApplicationControllerTest::manualUnitOverrideReimportsAtTheRequestedScale()
{
    const auto fixture = loupe::tests::writeRepeatedBoxAssembly(QStringLiteral("controller-unit-override.step").toStdString(), loupe::tests::FixtureUnit::Millimeter);
    QTemporaryDir cacheDirectory;
    QVERIFY(cacheDirectory.isValid());
    loupe::app::ApplicationController controller(QStringLiteral(LOUPE_WORKER_PATH), cacheDirectory.path());

    controller.openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));
    QTRY_COMPARE_WITH_TIMEOUT(controller.documentState(), loupe::app::DocumentState::TreeReady, 10'000);
    QTRY_VERIFY_WITH_TIMEOUT(controller.modelExtentMm() > 0.0, 10'000);
    const auto millimeterExtent = controller.modelExtentMm();
    QCOMPARE(controller.effectiveUnit(), QStringLiteral("mm"));

    QVERIFY(controller.setUnitOverride(QStringLiteral("in")));
    QTRY_COMPARE_WITH_TIMEOUT(controller.documentState(), loupe::app::DocumentState::TreeReady, 10'000);
    QCOMPARE(controller.effectiveUnit(), QStringLiteral("in"));
    QTRY_VERIFY_WITH_TIMEOUT(controller.modelExtentMm() > 0.0, 10'000);
    QVERIFY(qAbs(controller.modelExtentMm() - millimeterExtent * 25.4) < 0.01);
}

void ApplicationControllerTest::selectedComponentFeedsGeometryBackedMeasurementModes()
{
    const auto fixture = loupe::tests::writeRepeatedBoxAssembly(QStringLiteral("controller-measurement.step").toStdString(), loupe::tests::FixtureUnit::Millimeter);
    loupe::app::ApplicationController controller(QStringLiteral(LOUPE_WORKER_PATH));
    controller.openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));
    QTRY_COMPARE_WITH_TIMEOUT(controller.documentState(), loupe::app::DocumentState::TreeReady, 10'000);

    const auto geometry = QJsonDocument::fromJson(controller.snapshotJson().toUtf8()).object().value(QStringLiteral("geometry")).toArray();
    QVERIFY(!geometry.isEmpty());
    controller.setActiveNodeId(geometry.first().toObject().value(QStringLiteral("nodeId")).toString());
    QTRY_VERIFY_WITH_TIMEOUT(controller.activeVolumeMm3() > 0.0, 10'000);
    auto* measurement = qobject_cast<loupe::app::tools::MeasurementController*>(controller.measurementController());
    QVERIFY(measurement != nullptr);
    measurement->setMode(loupe::app::tools::MeasurementMode::Volume);
    QVERIFY(measurement->resultValue() > 0.0);
    QCOMPARE(measurement->resultUnit(), QStringLiteral("mm³"));
}

void ApplicationControllerTest::viewportPickRecordsPointMeasurementWithoutChangingSelection()
{
    loupe::app::ApplicationController controller;
    controller.acceptViewPick(QStringLiteral("occ-picked"), 0.0, 0.0, 0.0, 0.0, 0.0, 1.0);
    controller.acceptViewPick(QStringLiteral("occ-picked"), 25.4, 0.0, 0.0, 0.0, 0.0, 1.0);

    QVERIFY(controller.activeNodeId().isEmpty());
    auto* measurement = qobject_cast<loupe::app::tools::MeasurementController*>(controller.measurementController());
    QVERIFY(measurement != nullptr);
    QCOMPARE(measurement->resultLabel(), QStringLiteral("25.4 mm"));
}

void ApplicationControllerTest::topologyPickRecordsEntityMeasurementWithoutChangingSelection()
{
    loupe::app::ApplicationController controller;
    auto* measurement = qobject_cast<loupe::app::tools::MeasurementController*>(controller.measurementController());
    QVERIFY(measurement != nullptr);
    measurement->setMode(loupe::app::tools::MeasurementMode::EdgeLength);

    QVERIFY(controller.acceptTopologyPick(QStringLiteral("occ-picked"), QStringLiteral("edge"), 6,
                                          1.0, 2.0, 3.0, 0.0, 0.0, 1.0, 431.0, 0.0));

    QVERIFY(controller.activeNodeId().isEmpty());
    QCOMPARE(measurement->resultLabel(), QStringLiteral("Edge length: 431 mm"));
}

void ApplicationControllerTest::viewportSelectionDoesNotRecordMeasurementPick()
{
    loupe::app::ApplicationController controller;
    auto* measurement = qobject_cast<loupe::app::tools::MeasurementController*>(controller.measurementController());
    QVERIFY(measurement != nullptr);

    controller.acceptViewSelection(QStringLiteral("occ-selected"), 12.0, 4.0, 2.0, 0.0, 0.0, 1.0);

    QCOMPARE(controller.activeNodeId(), QStringLiteral("occ-selected"));
    QVERIFY(measurement->pickedPoints().isEmpty());
}

void ApplicationControllerTest::appearanceOverridesMaterialAndSourceColor()
{
    const auto fixture = loupe::tests::writeRepeatedBoxAssembly(QStringLiteral("controller-appearance.step").toStdString(), loupe::tests::FixtureUnit::Millimeter);
    loupe::app::ApplicationController controller(QStringLiteral(LOUPE_WORKER_PATH));
    controller.openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));
    QTRY_COMPARE_WITH_TIMEOUT(controller.documentState(), loupe::app::DocumentState::TreeReady, 10'000);

    const auto geometry = QJsonDocument::fromJson(controller.snapshotJson().toUtf8()).object().value(QStringLiteral("geometry")).toArray();
    QVERIFY(!geometry.isEmpty());
    controller.setActiveNodeId(geometry.first().toObject().value(QStringLiteral("nodeId")).toString());
    QVERIFY(controller.assignActiveMaterial(QStringLiteral("aluminum-6061")));
    QCOMPARE(controller.resolvedAppearanceColor(controller.activeNodeId(), QStringLiteral("#112233")), QStringLiteral("#B8C2CC"));
    QVERIFY(controller.setActiveAppearanceColor(QStringLiteral("#FF0055"), QStringLiteral("occurrence")));
    QCOMPARE(controller.resolvedAppearanceColor(controller.activeNodeId(), QStringLiteral("#112233")), QStringLiteral("#FF0055"));
    QVERIFY(controller.clearActiveAppearanceColor(QStringLiteral("occurrence")));
    QCOMPARE(controller.resolvedAppearanceColor(controller.activeNodeId(), QStringLiteral("#112233")), QStringLiteral("#B8C2CC"));
}

void ApplicationControllerTest::uncolouredPartsCarryNoColourAndContainersShowTheirGeometry()
{
    // The inspector swatch and the viewport must resolve a part's colour the same way. An
    // uncoloured part used to arrive painted with a teal placeholder while the swatch fell
    // back on its own, and a subassembly's swatch showed a colour none of its parts had.
    const auto fixture = loupe::tests::writeRepeatedBoxAssembly(QStringLiteral("controller-uncoloured.step").toStdString(), loupe::tests::FixtureUnit::Millimeter);
    loupe::app::ApplicationController controller(QStringLiteral(LOUPE_WORKER_PATH));
    QSignalSpy meshSpy(&controller, &loupe::app::ApplicationController::meshInstanceReady);
    controller.openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.importInProgress() && controller.documentState() == loupe::app::DocumentState::TreeReady, 10'000);
    QVERIFY(meshSpy.count() > 0);
    for (const auto& arguments : meshSpy) QVERIFY(arguments.at(3).toString().isEmpty());

    const auto document = QJsonDocument::fromJson(controller.snapshotJson().toUtf8()).object();
    const auto geometry = document.value(QStringLiteral("geometry")).toArray();
    QVERIFY(geometry.size() >= 2);
    QString rootId;
    for (const auto& value : document.value(QStringLiteral("nodes")).toArray()) {
        if (value.toObject().value(QStringLiteral("kind")).toInt() == static_cast<int>(loupe::domain::NodeKind::Root))
            rootId = value.toObject().value(QStringLiteral("id")).toString();
    }
    QVERIFY(!rootId.isEmpty());

    const auto partId = geometry.first().toObject().value(QStringLiteral("nodeId")).toString();
    controller.setActiveNodeId(partId);
    QCOMPARE(controller.activeResolvedAppearanceColor(), QString{});
    controller.setActiveNodeId(rootId);
    QCOMPARE(controller.activeResolvedAppearanceColor(), QString{});

    controller.setActiveNodeId(partId);
    QVERIFY(controller.setActiveAppearanceColor(QStringLiteral("#FF0055"), QStringLiteral("occurrence")));
    controller.setActiveNodeId(rootId);
    QCOMPARE(controller.activeResolvedAppearanceColor(), QStringLiteral("#FF0055"));
}

void ApplicationControllerTest::aMulticolouredPartsSwatchShowsTheColourOfMostOfItsSurface()
{
    // A part arrives as one segment per face colour. The swatch took whichever segment came
    // last, so an orange part with one green face showed a green swatch.
    const auto fixture = loupe::tests::writeMostlyOrangeBoxWithAGreenFace(QStringLiteral("controller-two-colour.step").toStdString());
    loupe::app::ApplicationController controller(QStringLiteral(LOUPE_WORKER_PATH));
    QSignalSpy meshSpy(&controller, &loupe::app::ApplicationController::meshInstanceReady);
    controller.openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.importInProgress() && controller.documentState() == loupe::app::DocumentState::TreeReady, 10'000);
    QSet<QString> colours;
    for (const auto& arguments : meshSpy) colours.insert(arguments.at(3).toString());
    QCOMPARE(colours, (QSet<QString>{QStringLiteral("#D95900"), QStringLiteral("#00FF00")}));

    const auto geometry = QJsonDocument::fromJson(controller.snapshotJson().toUtf8()).object().value(QStringLiteral("geometry")).toArray();
    QCOMPARE(geometry.size(), 1);
    controller.setActiveNodeId(geometry.first().toObject().value(QStringLiteral("nodeId")).toString());
    QCOMPARE(controller.activeResolvedAppearanceColor(), QStringLiteral("#D95900"));

    // The same after the geometry is replayed from the session's cache, as a workspace
    // switch or a reopened tab does.
    const auto beforeReplay = meshSpy.count();
    controller.replayGeometry();
    QTRY_VERIFY_WITH_TIMEOUT(meshSpy.count() >= beforeReplay + 2, 5'000);
    QCOMPARE(controller.activeResolvedAppearanceColor(), QStringLiteral("#D95900"));
}

namespace {

QStringList geometryNodeIds(const loupe::app::ApplicationController& controller)
{
    QStringList nodeIds;
    for (const auto& value : QJsonDocument::fromJson(controller.snapshotJson().toUtf8()).object().value(QStringLiteral("geometry")).toArray())
        nodeIds.append(value.toObject().value(QStringLiteral("nodeId")).toString());
    return nodeIds;
}

} // namespace

void ApplicationControllerTest::autoPartColoursGiveEachDefinitionItsOwnColour()
{
    const auto fixture = loupe::tests::writeTwoDefinitionAssembly(QStringLiteral("controller-auto-colour.step").toStdString());
    loupe::app::ApplicationController controller(QStringLiteral(LOUPE_WORKER_PATH));
    controller.openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.importInProgress() && controller.documentState() == loupe::app::DocumentState::TreeReady, 10'000);
    const auto parts = geometryNodeIds(controller);
    QCOMPARE(parts.size(), 2);

    // A fresh controller keeps the file's colours; these parts have none.
    QCOMPARE(controller.partColorMode(), QStringLiteral("file"));
    QCOMPARE(controller.resolvedAppearanceColor(parts.at(0), {}), QString{});

    QSignalSpy appearanceSpy(&controller, &loupe::app::ApplicationController::componentPropertiesChanged);
    controller.setPartColorMode(QStringLiteral("shades"));
    QCOMPARE(appearanceSpy.count(), 1);
    const auto first = controller.resolvedAppearanceColor(parts.at(0), {});
    const auto second = controller.resolvedAppearanceColor(parts.at(1), {});
    QVERIFY(QColor(first).isValid() && QColor(second).isValid());
    QVERIFY(first != second);

    // A user's own colour and a material still win over an automatic one.
    controller.setActiveNodeId(parts.at(0));
    QVERIFY(controller.setActiveAppearanceColor(QStringLiteral("#FF0055"), QStringLiteral("occurrence")));
    QCOMPARE(controller.resolvedAppearanceColor(parts.at(0), {}), QStringLiteral("#FF0055"));

    controller.shufflePartColors();
    QVERIFY(controller.resolvedAppearanceColor(parts.at(1), {}) != second);
    controller.setPartColorMode(QStringLiteral("unknown"));
    QCOMPARE(controller.partColorMode(), QStringLiteral("file"));
    QCOMPARE(controller.resolvedAppearanceColor(parts.at(1), {}), QString{});
}

void ApplicationControllerTest::autoPartColoursTreatOneSharedFileColourAsNone()
{
    // The supplied sample gives all twelve parts SolidWorks' default #CAD1EE. A colour every
    // part shares tells them apart no better than none, so auto colour must still apply.
    const auto fixture = loupe::tests::writeTwoDefinitionAssembly(QStringLiteral("controller-uniform-colour.step").toStdString(),
                                                                  Quantity_Color(0.792, 0.820, 0.933, Quantity_TOC_sRGB));
    loupe::app::ApplicationController controller(QStringLiteral(LOUPE_WORKER_PATH));
    QSignalSpy meshSpy(&controller, &loupe::app::ApplicationController::meshInstanceReady);
    controller.openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.importInProgress() && controller.documentState() == loupe::app::DocumentState::TreeReady, 10'000);
    const auto parts = geometryNodeIds(controller);
    QCOMPARE(parts.size(), 2);
    QVERIFY(controller.fileColorsUniform());
    // The colour arrives as the file wrote it (sRGB), not as OCCT's linear value in hex,
    // which turned #CAD1EE into #979FDA.
    QVERIFY(meshSpy.count() > 0);
    for (const auto& arguments : meshSpy) QCOMPARE(arguments.at(3).toString(), QStringLiteral("#CAD1EE"));
    const auto fileColor = QStringLiteral("#CAD1EE");
    QCOMPARE(controller.resolvedAppearanceColor(parts.at(0), fileColor), fileColor);

    controller.setPartColorMode(QStringLiteral("muted"));
    const auto first = controller.resolvedAppearanceColor(parts.at(0), fileColor);
    const auto second = controller.resolvedAppearanceColor(parts.at(1), fileColor);
    QVERIFY(first != fileColor && second != fileColor && first != second);
}

void ApplicationControllerTest::reopensSourceWithPersistedAppearanceOverrides()
{
    const auto fixture = loupe::tests::writeRepeatedBoxAssembly(QStringLiteral("controller-persisted-appearance.step").toStdString(), loupe::tests::FixtureUnit::Millimeter);
    QTemporaryDir cacheDirectory;
    QVERIFY(cacheDirectory.isValid());
    const auto file = QUrl::fromLocalFile(QString::fromStdString(fixture.string()));
    QString selectedNodeId;

    {
        loupe::app::ApplicationController initial(QStringLiteral(LOUPE_WORKER_PATH), cacheDirectory.path());
        initial.openFile(file);
        QTRY_COMPARE_WITH_TIMEOUT(initial.documentState(), loupe::app::DocumentState::TreeReady, 10'000);
        const auto geometry = QJsonDocument::fromJson(initial.snapshotJson().toUtf8()).object().value(QStringLiteral("geometry")).toArray();
        QVERIFY(!geometry.isEmpty());
        selectedNodeId = geometry.first().toObject().value(QStringLiteral("nodeId")).toString();
        initial.setActiveNodeId(selectedNodeId);
        QVERIFY(initial.assignActiveMaterial(QStringLiteral("aluminum-6061"), QStringLiteral("occurrence")));
        QVERIFY(initial.setActiveAppearanceColor(QStringLiteral("#FF0055"), QStringLiteral("occurrence")));
    }

    loupe::app::ApplicationController reopened(QStringLiteral(LOUPE_WORKER_PATH), cacheDirectory.path());
    reopened.openFile(file);
    QTRY_COMPARE_WITH_TIMEOUT(reopened.documentState(), loupe::app::DocumentState::TreeReady, 10'000);
    reopened.setActiveNodeId(selectedNodeId);
    QCOMPARE(reopened.activeMaterialId(), QStringLiteral("aluminum-6061"));
    QCOMPARE(reopened.activeAppearanceColor(), QStringLiteral("#FF0055"));
}


// The renderer is Y-up while mechanical CAD is overwhelmingly Z-up, so a named
// view has to resolve against the document's own convention. Getting this wrong
// makes "Top" produce a front elevation, which in a 1:1 drawing is a wrong part
// rather than a cosmetic annoyance.
void ApplicationControllerTest::resolvesStandardViewsAgainstDocumentUpAxis()
{
    loupe::app::ApplicationController controller;

    QCOMPARE(controller.upAxis(), QStringLiteral("Z"));
    QCOMPARE(controller.directionForStandardView(QStringLiteral("Top")), QVector3D(0, 0, 1));
    QCOMPARE(controller.directionForStandardView(QStringLiteral("Bottom")), QVector3D(0, 0, -1));
    QCOMPARE(controller.directionForStandardView(QStringLiteral("Front")), QVector3D(0, -1, 0));
    QCOMPARE(controller.directionForStandardView(QStringLiteral("Back")), QVector3D(0, 1, 0));

    QSignalSpy spy(&controller, &loupe::app::ApplicationController::upAxisChanged);
    QVERIFY(controller.setUpAxis(QStringLiteral("Y")));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(controller.upAxis(), QStringLiteral("Y"));
    QCOMPARE(controller.directionForStandardView(QStringLiteral("Top")), QVector3D(0, 1, 0));
    QCOMPARE(controller.directionForStandardView(QStringLiteral("Front")), QVector3D(0, 0, 1));

    // Left and right are the same axis under either convention.
    QCOMPARE(controller.directionForStandardView(QStringLiteral("Right")), QVector3D(1, 0, 0));
    QCOMPARE(controller.directionForStandardView(QStringLiteral("Left")), QVector3D(-1, 0, 0));

    // Setting the same axis again must not emit a spurious change.
    QVERIFY(controller.setUpAxis(QStringLiteral("Y")));
    QCOMPARE(spy.count(), 1);

    // Every named view must be a unit direction, or the camera alignment would
    // scale as well as rotate.
    for (const auto* name : {"Top", "Bottom", "Front", "Back", "Left", "Right"}) {
        const auto direction = controller.directionForStandardView(QString::fromLatin1(name));
        QVERIFY(qFuzzyCompare(direction.length(), 1.0F));
    }
}

void ApplicationControllerTest::rejectsUnknownUpAxisAndViewName()
{
    loupe::app::ApplicationController controller;

    QVERIFY(!controller.setUpAxis(QStringLiteral("X")));
    QVERIFY(!controller.setUpAxis(QString()));
    QCOMPARE(controller.upAxis(), QStringLiteral("Z"));
    // An unknown name yields a zero vector so a caller can reject it rather than
    // silently aiming the camera somewhere arbitrary.
    QCOMPARE(controller.directionForStandardView(QStringLiteral("Isometric")), QVector3D());
}

QTEST_MAIN(ApplicationControllerTest)

#include "test_application_controller.moc"
