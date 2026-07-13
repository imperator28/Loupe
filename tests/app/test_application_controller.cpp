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
    void forwardsWorkerMeshToViewer();
    void fitViewNotifiesViewport();
    void isolatesAndGhostsTheActiveComponentForViewerPresentation();
    void reopensUnchangedStepFromLocalSnapshotCache();
    void manualUnitOverrideReimportsAtTheRequestedScale();
    void selectedComponentFeedsGeometryBackedMeasurementModes();
    void viewportPickSelectsNodeAndRecordsPointMeasurement();
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

    QTRY_COMPARE_WITH_TIMEOUT(controller.documentState(), loupe::app::DocumentState::TreeReady, 10'000);
    QVERIFY(!controller.snapshotJson().isEmpty());
    QCOMPARE(controller.documentState(), loupe::app::DocumentState::TreeReady);
    QVERIFY(controller.assemblyTreeModel()->rowCount() > 0);
    QVERIFY(controller.modelExtentMm() > 0.0);
    QCOMPARE(controller.effectiveUnit(), QStringLiteral("mm"));
    const auto geometry = QJsonDocument::fromJson(controller.snapshotJson().toUtf8()).object().value(QStringLiteral("geometry")).toArray();
    QVERIFY(!geometry.isEmpty());
    controller.setActiveNodeId(geometry.first().toObject().value(QStringLiteral("nodeId")).toString());
    QVERIFY(controller.assignActiveMaterial(QStringLiteral("aluminum-6061")));
    QVERIFY(controller.activeVolumeMm3() > 0.0);
    QVERIFY(controller.estimatedMassKg() > 0.0);
}

void ApplicationControllerTest::forwardsWorkerMeshToViewer()
{
    const auto fixture = loupe::tests::writeRepeatedBoxAssembly(QStringLiteral("controller-mesh.step").toStdString(), loupe::tests::FixtureUnit::Millimeter);
    loupe::app::ApplicationController controller(QStringLiteral(LOUPE_WORKER_PATH));
    QSignalSpy meshSpy(&controller, &loupe::app::ApplicationController::meshReady);

    controller.openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));

    QTRY_VERIFY_WITH_TIMEOUT(meshSpy.count() > 0, 10'000);
    QVERIFY(!meshSpy.first().at(0).toString().isEmpty());
    const auto mesh = meshSpy.first().at(1).toByteArray();
    QVERIFY(!mesh.isEmpty());
}

void ApplicationControllerTest::fitViewNotifiesViewport()
{
    loupe::app::ApplicationController controller;
    QSignalSpy fitSpy(&controller, &loupe::app::ApplicationController::fitRequested);

    controller.fitView();

    QCOMPARE(fitSpy.count(), 1);
}

void ApplicationControllerTest::isolatesAndGhostsTheActiveComponentForViewerPresentation()
{
    loupe::app::ApplicationController controller;
    controller.setActiveNodeId(QStringLiteral("occ-1"));

    controller.isolateActiveNode();
    QCOMPARE(controller.viewerPresentation(), loupe::app::ViewerPresentation::Isolate);
    controller.ghostActiveNode();
    QCOMPARE(controller.viewerPresentation(), loupe::app::ViewerPresentation::Ghost);
    controller.restoreFullAssembly();
    QCOMPARE(controller.viewerPresentation(), loupe::app::ViewerPresentation::Full);
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
    reopened.openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));

    QTRY_COMPARE_WITH_TIMEOUT(reopened.documentState(), loupe::app::DocumentState::TreeReady, 10'000);
    QVERIFY(reopened.cacheHit());
    QTRY_VERIFY_WITH_TIMEOUT(meshSpy.count() > 0, 10'000);
}

void ApplicationControllerTest::manualUnitOverrideReimportsAtTheRequestedScale()
{
    const auto fixture = loupe::tests::writeRepeatedBoxAssembly(QStringLiteral("controller-unit-override.step").toStdString(), loupe::tests::FixtureUnit::Millimeter);
    QTemporaryDir cacheDirectory;
    QVERIFY(cacheDirectory.isValid());
    loupe::app::ApplicationController controller(QStringLiteral(LOUPE_WORKER_PATH), cacheDirectory.path());

    controller.openFile(QUrl::fromLocalFile(QString::fromStdString(fixture.string())));
    QTRY_COMPARE_WITH_TIMEOUT(controller.documentState(), loupe::app::DocumentState::TreeReady, 10'000);
    const auto millimeterExtent = controller.modelExtentMm();
    QCOMPARE(controller.effectiveUnit(), QStringLiteral("mm"));

    QVERIFY(controller.setUnitOverride(QStringLiteral("in")));
    QTRY_COMPARE_WITH_TIMEOUT(controller.documentState(), loupe::app::DocumentState::TreeReady, 10'000);
    QCOMPARE(controller.effectiveUnit(), QStringLiteral("in"));
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
    auto* measurement = qobject_cast<loupe::app::tools::MeasurementController*>(controller.measurementController());
    QVERIFY(measurement != nullptr);
    measurement->setMode(loupe::app::tools::MeasurementMode::Volume);
    QVERIFY(measurement->resultValue() > 0.0);
    QCOMPARE(measurement->resultUnit(), QStringLiteral("mm³"));
}

void ApplicationControllerTest::viewportPickSelectsNodeAndRecordsPointMeasurement()
{
    loupe::app::ApplicationController controller;
    controller.acceptViewPick(QStringLiteral("occ-picked"), 0.0, 0.0, 0.0, 0.0, 0.0, 1.0);
    controller.acceptViewPick(QStringLiteral("occ-picked"), 25.4, 0.0, 0.0, 0.0, 0.0, 1.0);

    QCOMPARE(controller.activeNodeId(), QStringLiteral("occ-picked"));
    auto* measurement = qobject_cast<loupe::app::tools::MeasurementController*>(controller.measurementController());
    QVERIFY(measurement != nullptr);
    QCOMPARE(measurement->resultLabel(), QStringLiteral("25.4 mm"));
}

QTEST_MAIN(ApplicationControllerTest)

#include "test_application_controller.moc"
