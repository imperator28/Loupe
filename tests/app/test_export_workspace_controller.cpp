#include <QtTest/QTest>
#include <QtTest/QSignalSpy>

#include "app/export/ExportWorkspaceController.h"

class ExportWorkspaceControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void checkedSelectionIsIndependentFromFocus();
    void checkingDoesNotReplaceComponentModel();
    void hoverFallsBackToPinnedPreviewWithoutChangingBucket();
    void bucketOrderDrivesSequentialNames();
    void manualFilenameOverrideStaysWithComponent();
    void nodeIdReorderUsesCurrentBucketIndex();
    void sceneNodeFocusResolvesVisibleAncestorWithoutChecking();
    void hidesRedundantRawBodyRows();
    void reportsVisibleHierarchyMetadataForPicker();
    void createsDeterministicReviewedPlan();
    void reportsUnsupportedLocalOccurrencePlan();
    void selectedFolderUrlUpdatesDestination();
    void invalidFolderUrlDoesNotReplaceDestination();
    void reviewedExportLocksPlanAndTracksResults();
    void exportWaitsForNativeDocument();
    void selectAllChecksEveryExportableComponent();
    void deselectAllClearsSelection();
};

namespace {

QString snapshot()
{
    return QStringLiteral(R"({"effectiveUnit":"mm","sourceToMillimeters":1,"nodes":[
        {"id":"root","name":"Assembly","kind":0,"parentId":""},
        {"id":"sub","name":"Carrier","kind":1,"parentId":"root"},
        {"id":"cover","name":"Cover","kind":2,"parentId":"sub"},
        {"id":"insert","name":"Insert","kind":2,"parentId":"sub"}
    ]})");
}

QString snapshotWithRawBodies()
{
    return QStringLiteral(R"({"effectiveUnit":"mm","sourceToMillimeters":1,"nodes":[
        {"id":"root","name":"Assembly","kind":0,"parentId":""},
        {"id":"cover","name":"Cover","kind":2,"parentId":"root"},
        {"id":"solid","name":"SOLID","kind":4,"parentId":"cover"},
        {"id":"namedBody","name":"Heat sink","kind":4,"parentId":"cover"}
    ]})");
}

} // namespace

void ExportWorkspaceControllerTest::checkedSelectionIsIndependentFromFocus()
{
    loupe::app::exporting::ExportWorkspaceController controller;
    controller.replaceSnapshot(snapshot());

    controller.setChecked(QStringLiteral("cover"), true);
    controller.setFocusedNodeId(QStringLiteral("insert"));

    QVERIFY(controller.isChecked(QStringLiteral("cover")));
    QVERIFY(!controller.isChecked(QStringLiteral("insert")));
    QCOMPARE(controller.focusedNodeId(), QStringLiteral("insert"));
    QCOMPARE(controller.checkedCount(), 1);
}

void ExportWorkspaceControllerTest::checkingDoesNotReplaceComponentModel()
{
    loupe::app::exporting::ExportWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    QSignalSpy componentsChanged(&controller, &loupe::app::exporting::ExportWorkspaceController::componentsChanged);
    QSignalSpy selectionChanged(&controller, &loupe::app::exporting::ExportWorkspaceController::selectionChanged);
    const auto revision = controller.selectionRevision();

    controller.setChecked(QStringLiteral("cover"), true);

    QCOMPARE(componentsChanged.count(), 0);
    QCOMPARE(selectionChanged.count(), 1);
    QCOMPARE(controller.selectionRevision(), revision + 1);
    QVERIFY(controller.isChecked(QStringLiteral("cover")));

    controller.moveBucketItemById(QStringLiteral("cover"), 0);
    QCOMPARE(componentsChanged.count(), 0);
}

void ExportWorkspaceControllerTest::hoverFallsBackToPinnedPreviewWithoutChangingBucket()
{
    loupe::app::exporting::ExportWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    controller.setChecked(QStringLiteral("cover"), true);
    controller.setFocusedNodeId(QStringLiteral("cover"));
    controller.setHoveredNodeId(QStringLiteral("insert"));

    QCOMPARE(controller.previewNodeId(), QStringLiteral("insert"));
    QCOMPARE(controller.checkedCount(), 1);

    controller.setHoveredNodeId({});
    QCOMPARE(controller.previewNodeId(), QStringLiteral("cover"));
    QVERIFY(!controller.isChecked(QStringLiteral("insert")));
}

void ExportWorkspaceControllerTest::bucketOrderDrivesSequentialNames()
{
    loupe::app::exporting::ExportWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    controller.setChecked(QStringLiteral("cover"), true);
    controller.setChecked(QStringLiteral("insert"), true);
    controller.setNamingStrategy(QStringLiteral("sequence"));
    controller.setNamingValue(QStringLiteral("Bracket"));

    QCOMPARE(controller.bucketRows().at(0).toMap().value(QStringLiteral("filename")).toString(),
             QStringLiteral("Bracket-001.step"));
    QCOMPARE(controller.bucketRows().at(1).toMap().value(QStringLiteral("filename")).toString(),
             QStringLiteral("Bracket-002.step"));

    controller.moveBucketItem(1, 0);
    QCOMPARE(controller.bucketRows().at(0).toMap().value(QStringLiteral("nodeId")).toString(),
             QStringLiteral("insert"));
    QCOMPARE(controller.bucketRows().at(0).toMap().value(QStringLiteral("filename")).toString(),
             QStringLiteral("Bracket-001.step"));
    QCOMPARE(controller.bucketRows().at(0).toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Insert"));
    QCOMPARE(controller.bucketRows().at(1).toMap().value(QStringLiteral("filename")).toString(),
             QStringLiteral("Bracket-002.step"));
}

void ExportWorkspaceControllerTest::manualFilenameOverrideStaysWithComponent()
{
    loupe::app::exporting::ExportWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    controller.setChecked(QStringLiteral("cover"), true);
    controller.setChecked(QStringLiteral("insert"), true);
    controller.setNamingStrategy(QStringLiteral("sequence"));
    controller.setNamingValue(QStringLiteral("Bracket"));
    controller.setFilenameOverride(QStringLiteral("cover"), QStringLiteral("Custom-cover.step"));

    controller.moveBucketItemById(QStringLiteral("cover"), 1);

    QCOMPARE(controller.bucketRows().at(0).toMap().value(QStringLiteral("filename")).toString(),
             QStringLiteral("Bracket-001.step"));
    QCOMPARE(controller.bucketRows().at(1).toMap().value(QStringLiteral("filename")).toString(),
             QStringLiteral("Custom-cover.step"));
    QVERIFY(controller.bucketRows().at(1).toMap().value(QStringLiteral("filenameOverridden")).toBool());
}

void ExportWorkspaceControllerTest::nodeIdReorderUsesCurrentBucketIndex()
{
    loupe::app::exporting::ExportWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    controller.setChecked(QStringLiteral("cover"), true);
    controller.setChecked(QStringLiteral("insert"), true);

    controller.moveBucketItemById(QStringLiteral("cover"), 1);
    controller.moveBucketItemById(QStringLiteral("cover"), 0);

    QCOMPARE(controller.bucketRows().at(0).toMap().value(QStringLiteral("nodeId")).toString(),
             QStringLiteral("cover"));
    controller.moveBucketItemById(QStringLiteral("missing"), 1);
    QCOMPARE(controller.bucketRows().at(0).toMap().value(QStringLiteral("nodeId")).toString(),
             QStringLiteral("cover"));
}

void ExportWorkspaceControllerTest::sceneNodeFocusResolvesVisibleAncestorWithoutChecking()
{
    loupe::app::exporting::ExportWorkspaceController controller;
    controller.replaceSnapshot(snapshotWithRawBodies());

    QCOMPARE(controller.focusSceneNode(QStringLiteral("solid")), QStringLiteral("cover"));
    QCOMPARE(controller.focusedNodeId(), QStringLiteral("cover"));
    QCOMPARE(controller.checkedCount(), 0);

    QCOMPARE(controller.focusSceneNode(QStringLiteral("namedBody")), QStringLiteral("namedBody"));
    QCOMPARE(controller.checkedCount(), 0);
}

void ExportWorkspaceControllerTest::hidesRedundantRawBodyRows()
{
    loupe::app::exporting::ExportWorkspaceController controller;
    controller.replaceSnapshot(snapshotWithRawBodies());

    const auto components = controller.components();
    QCOMPARE(components.size(), 3);
    for (const auto& value : components) {
        QVERIFY(value.toMap().value(QStringLiteral("name")).toString() != QStringLiteral("SOLID"));
    }
}

void ExportWorkspaceControllerTest::reportsVisibleHierarchyMetadataForPicker()
{
    loupe::app::exporting::ExportWorkspaceController controller;
    controller.replaceSnapshot(snapshotWithRawBodies());

    QVariantMap cover;
    QVariantMap namedBody;
    for (const auto& value : controller.components()) {
        const auto component = value.toMap();
        if (component.value(QStringLiteral("nodeId")).toString() == QStringLiteral("cover")) cover = component;
        if (component.value(QStringLiteral("nodeId")).toString() == QStringLiteral("namedBody")) namedBody = component;
    }

    QCOMPARE(cover.value(QStringLiteral("parentId")).toString(), QStringLiteral("root"));
    QVERIFY(cover.value(QStringLiteral("hasChildren")).toBool());
    QCOMPARE(namedBody.value(QStringLiteral("parentId")).toString(), QStringLiteral("cover"));
    QVERIFY(!namedBody.value(QStringLiteral("hasChildren")).toBool());
}

void ExportWorkspaceControllerTest::createsDeterministicReviewedPlan()
{
    loupe::app::exporting::ExportWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    controller.setChecked(QStringLiteral("cover"), true);
    controller.setDestination(QStringLiteral("/tmp/loupe-export-review"));
    controller.setDocumentReady(true);

    QVERIFY(controller.rebuildPlan());
    QCOMPARE(controller.planRows().size(), 1);
    QCOMPARE(controller.planRows().first().toMap().value(QStringLiteral("path")).toString(),
             QStringLiteral("/tmp/loupe-export-review/Cover.step"));
    QVERIFY(!controller.planFingerprint().isEmpty());
}

void ExportWorkspaceControllerTest::reportsUnsupportedLocalOccurrencePlan()
{
    loupe::app::exporting::ExportWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    controller.setChecked(QStringLiteral("cover"), true);
    controller.setDestination(QStringLiteral("/tmp/loupe-export-review"));
    controller.setCoordinates(QStringLiteral("Local"));

    QVERIFY(!controller.rebuildPlan());
    QCOMPARE(controller.planRows().size(), 1);
    QCOMPARE(controller.planRows().first().toMap().value(QStringLiteral("status")).toString(),
             QStringLiteral("Needs attention"));
    QVERIFY(controller.planError().contains(QStringLiteral("local coordinates")));
}

void ExportWorkspaceControllerTest::selectedFolderUrlUpdatesDestination()
{
    loupe::app::exporting::ExportWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    controller.setChecked(QStringLiteral("cover"), true);
    controller.setDocumentReady(true);

    controller.setDestinationUrl(QUrl::fromEncoded("file:///tmp/Loupe%20Export/Bracket%20Parts"));

    QCOMPARE(controller.destination(), QStringLiteral("/tmp/Loupe Export/Bracket Parts"));
    QVERIFY(controller.canExport());
    QCOMPARE(controller.bucketRows().first().toMap().value(QStringLiteral("status")).toString(),
             QStringLiteral("Ready"));
}

void ExportWorkspaceControllerTest::invalidFolderUrlDoesNotReplaceDestination()
{
    loupe::app::exporting::ExportWorkspaceController controller;
    controller.setDestination(QStringLiteral("/tmp/existing"));

    controller.setDestinationUrl(QUrl(QStringLiteral("https://example.com/export")));
    QCOMPARE(controller.destination(), QStringLiteral("/tmp/existing"));

    controller.setDestinationUrl(QUrl {});
    QCOMPARE(controller.destination(), QStringLiteral("/tmp/existing"));
}

void ExportWorkspaceControllerTest::reviewedExportLocksPlanAndTracksResults()
{
    loupe::app::exporting::ExportWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    controller.setChecked(QStringLiteral("cover"), true);
    controller.setDestination(QStringLiteral("/tmp/loupe-export-review"));
    controller.setDocumentReady(true);
    QSignalSpy executeSpy(&controller, &loupe::app::exporting::ExportWorkspaceController::executeRequested);

    QVERIFY(controller.exportReviewedPlan());
    QCOMPARE(executeSpy.count(), 1);
    QVERIFY(controller.exporting());
    QVERIFY(!controller.canExport());
    const auto reviewedPath = controller.planRows().first().toMap().value(QStringLiteral("path")).toString();
    controller.setNamingStrategy(QStringLiteral("sequence"));
    QCOMPARE(controller.planRows().first().toMap().value(QStringLiteral("path")).toString(), reviewedPath);

    controller.setExportRequestId(31);
    controller.handleExportProgress(0, 1, QStringLiteral("Validating Cover.step"), 0.75);
    controller.handleExportRowResult(0, QStringLiteral("cover"), reviewedPath, true,
                                     QStringLiteral("Exported and validated"));
    controller.handleExportCompleted(1, 0);
    QVERIFY(controller.exportSucceeded());
    QVERIFY(controller.exportSummary().contains(QStringLiteral("1 files")));
}

void ExportWorkspaceControllerTest::exportWaitsForNativeDocument()
{
    loupe::app::exporting::ExportWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    controller.setChecked(QStringLiteral("cover"), true);
    controller.setDestination(QStringLiteral("/tmp/loupe-export-review"));

    QVERIFY(!controller.canExport());
    QVERIFY(!controller.exportReviewedPlan());
    controller.setDocumentReady(true);
    QVERIFY(controller.canExport());
}

void ExportWorkspaceControllerTest::selectAllChecksEveryExportableComponent()
{
    loupe::app::exporting::ExportWorkspaceController controller;
    controller.replaceSnapshot(snapshot());

    controller.setAllChecked(true);

    QCOMPARE(controller.checkedCount(), 3);
    QVERIFY(controller.isChecked(QStringLiteral("sub")));
    QVERIFY(controller.isChecked(QStringLiteral("cover")));
    QVERIFY(controller.isChecked(QStringLiteral("insert")));
    QVERIFY(!controller.isChecked(QStringLiteral("root")));
}

void ExportWorkspaceControllerTest::deselectAllClearsSelection()
{
    loupe::app::exporting::ExportWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    controller.setAllChecked(true);
    QCOMPARE(controller.checkedCount(), 3);

    controller.setAllChecked(false);

    QCOMPARE(controller.checkedCount(), 0);
}

QTEST_MAIN(ExportWorkspaceControllerTest)
#include "test_export_workspace_controller.moc"
