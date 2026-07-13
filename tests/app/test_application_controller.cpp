#include <QtTest/QTest>

#include "app/ApplicationController.h"

class ApplicationControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void inspectIsDefaultWorkspace();
    void emptyDocumentShowsDocumentInspector();
    void selectionShowsComponentInspector();
    void ownsInspectionTaskControllers();
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

QTEST_MAIN(ApplicationControllerTest)

#include "test_application_controller.moc"
