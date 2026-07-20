#include <QtTest/QTest>
#include <QSignalSpy>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QUrl>

#include "app/ApplicationController.h"
#include "app/models/AssemblyTreeModel.h"
#include "fixtures/FixtureFactory.h"

class ApplicationControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void inspectIsDefaultWorkspace();
    void emptyDocumentShowsDocumentInspector();
    void selectionShowsComponentInspector();
    void ownsInspectionTaskControllers();
    void opensStepThroughWorkerAndRetainsSnapshot();
    void reportsWorkerStartupFailure();
    void forwardsWorkerMeshToViewer();
    void replaysGeometryWithoutBlockingTheCaller();
    void fitViewNotifiesViewport();
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
};

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
    const auto geometry = QJsonDocument::fromJson(controller.snapshotJson().toUtf8()).object().value(QStringLiteral("geometry")).toArray();
    QVERIFY(!geometry.isEmpty());
    controller.setActiveNodeId(geometry.first().toObject().value(QStringLiteral("nodeId")).toString());
    QTRY_VERIFY_WITH_TIMEOUT(controller.modelExtentMm() > 0.0, 10'000);
    QTRY_VERIFY_WITH_TIMEOUT(controller.activeVolumeMm3() > 0.0, 10'000);
    QVERIFY(controller.assignActiveMaterial(QStringLiteral("aluminum-6061")));
    QVERIFY(controller.estimatedMassKg() > 0.0);
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
    QSignalSpy meshSpy(&controller, &loupe::app::ApplicationController::meshReady);

    controller.openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));

    QTRY_VERIFY_WITH_TIMEOUT(meshSpy.count() > 0, 10'000);
    QVERIFY(!meshSpy.first().at(0).toString().isEmpty());
    QVERIFY(!meshSpy.first().at(1).toString().isEmpty());
    const auto mesh = meshSpy.first().at(3).toByteArray();
    QVERIFY(!mesh.isEmpty());
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

    QSignalSpy meshSpy(&controller, &loupe::app::ApplicationController::meshReady);
    controller.replayGeometry();

    QCOMPARE(meshSpy.count(), 0);
    QTRY_VERIFY_WITH_TIMEOUT(meshSpy.count() > 0, 3'000);
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
    QSignalSpy meshSpy(&reopened, &loupe::app::ApplicationController::meshReady);
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

QTEST_MAIN(ApplicationControllerTest)

#include "test_application_controller.moc"
