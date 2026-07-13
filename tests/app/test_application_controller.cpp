#include <QtTest/QTest>
#include <QSignalSpy>
#include <QUrl>

#include "app/ApplicationController.h"
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
}

QTEST_MAIN(ApplicationControllerTest)

#include "test_application_controller.moc"
