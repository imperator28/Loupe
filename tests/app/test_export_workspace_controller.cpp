#include <QtTest/QTest>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
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
    void suggestedFolderIsCreatedOnlyByTheFirstExport();
    void copiesOfOnePartExportOnce();
    void differentPartsSharingANameGetNumberedFiles();
    void numberingDoesNotDependOnCheckOrder();
    void sameFileGivesSameNamesEveryRun();
    void reviewedPlanSendsOneSelectionPerFile();
    void removingARowUnchecksEveryCopy();
    void movingARowMovesEveryCopy();
    void copiesNamedOneByOneUseThePartName();
    void typedNamesWinAndOnlyTypedClashesBlock();
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

// Shaped like the benchmark stage: a sub-assembly placed twice, parts placed several times,
// a second sub-assembly that only shares the first one's name, and a different part that
// only shares a name with a repeated one.
QString snapshotWithRepeats()
{
    return QStringLiteral(R"({"effectiveUnit":"mm","sourceToMillimeters":1,"nodes":[
        {"id":"root","name":"Stage","kind":0,"parentId":""},
        {"id":"dBolt","name":"M5 Screw","kind":3,"parentId":""},
        {"id":"dBush","name":"Bushing","kind":3,"parentId":""},
        {"id":"dOther","name":"M5 Screw","kind":3,"parentId":""},
        {"id":"dPlate","name":"Top Plate","kind":3,"parentId":""},
        {"id":"c1","name":"Carriage","kind":1,"parentId":"root"},
        {"id":"b1","name":"M5 Screw","kind":2,"parentId":"c1","definitionId":"dBolt"},
        {"id":"b2","name":"M5 Screw","kind":2,"parentId":"c1","definitionId":"dBolt"},
        {"id":"u1","name":"Bushing","kind":2,"parentId":"c1","definitionId":"dBush"},
        {"id":"c2","name":"Carriage","kind":1,"parentId":"root"},
        {"id":"b3","name":"M5 Screw","kind":2,"parentId":"c2","definitionId":"dBolt"},
        {"id":"b4","name":"M5 Screw","kind":2,"parentId":"c2","definitionId":"dBolt"},
        {"id":"u2","name":"Bushing","kind":2,"parentId":"c2","definitionId":"dBush"},
        {"id":"c3","name":"Carriage","kind":1,"parentId":"root"},
        {"id":"p1","name":"Top Plate","kind":2,"parentId":"c3","definitionId":"dPlate"},
        {"id":"s1","name":"M5 Screw","kind":2,"parentId":"root","definitionId":"dOther"}
    ]})");
}

QHash<QString, QString> filenamesByNode(const loupe::app::exporting::ExportWorkspaceController& controller)
{
    QHash<QString, QString> result;
    for (const auto& value : controller.bucketRows()) {
        const auto row = value.toMap();
        result.insert(row.value(QStringLiteral("nodeId")).toString(), row.value(QStringLiteral("filename")).toString());
    }
    return result;
}

QStringList filenames(const loupe::app::exporting::ExportWorkspaceController& controller)
{
    QStringList result;
    for (const auto& value : controller.bucketRows()) result.append(value.toMap().value(QStringLiteral("filename")).toString());
    return result;
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

void ExportWorkspaceControllerTest::suggestedFolderIsCreatedOnlyByTheFirstExport()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto suggested = QDir(root.path()).filePath(QStringLiteral("Drawer export"));
    loupe::app::exporting::ExportWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    controller.setChecked(QStringLiteral("cover"), true);
    controller.setDefaultDestination(suggested);
    controller.setDocumentReady(true);
    QCOMPARE(controller.destination(), suggested);
    QVERIFY(controller.destinationIsDefault());
    QVERIFY(controller.canExport());
    QVERIFY(!QDir(suggested).exists()); // not made just for opening the file

    QVERIFY(controller.exportReviewedPlan());
    QVERIFY(QDir(suggested).exists());

    // A folder picked by hand is the user's, and stops being the suggestion.
    loupe::app::exporting::ExportWorkspaceController picked;
    picked.setDefaultDestination(suggested);
    picked.setDestination(root.path());
    QVERIFY(!picked.destinationIsDefault());
}

void ExportWorkspaceControllerTest::copiesOfOnePartExportOnce()
{
    loupe::app::exporting::ExportWorkspaceController controller;
    controller.replaceSnapshot(snapshotWithRepeats());
    controller.setDestination(QStringLiteral("/tmp/loupe-export-review"));
    controller.setDocumentReady(true);

    controller.setAllChecked(true);

    QCOMPARE(controller.checkedCount(), 11);
    QCOMPARE(controller.fileCount(), 6);
    QVERIFY(controller.canExport());
    QVERIFY(controller.planError().isEmpty());
    const auto rows = controller.bucketRows();
    QCOMPARE(rows.size(), 6);
    // Four screws in two copies of the carriage: one file, standing for all four.
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("nodeId")).toString(), QStringLiteral("b1"));
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("copyCount")).toInt(), 4);
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("nodeId")).toString(), QStringLiteral("c1"));
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("copyCount")).toInt(), 2);
    QCOMPARE(rows.at(2).toMap().value(QStringLiteral("copyCount")).toInt(), 2);
}

void ExportWorkspaceControllerTest::differentPartsSharingANameGetNumberedFiles()
{
    loupe::app::exporting::ExportWorkspaceController controller;
    controller.replaceSnapshot(snapshotWithRepeats());
    controller.setDestination(QStringLiteral("/tmp/loupe-export-review"));
    controller.setDocumentReady(true);

    controller.setAllChecked(true);

    QCOMPARE(filenames(controller), (QStringList{QStringLiteral("Carriage.step"), QStringLiteral("M5 Screw.step"),
                                                 QStringLiteral("Bushing.step"), QStringLiteral("Carriage-2.step"),
                                                 QStringLiteral("Top Plate.step"), QStringLiteral("M5 Screw-2.step")}));
    QVERIFY(controller.canExport());
}

void ExportWorkspaceControllerTest::numberingDoesNotDependOnCheckOrder()
{
    loupe::app::exporting::ExportWorkspaceController controller;
    controller.replaceSnapshot(snapshotWithRepeats());
    controller.setDestination(QStringLiteral("/tmp/loupe-export-review"));
    // The part that only shares the name is checked first, and the copies backwards; the
    // part earlier in the assembly tree still gets the plain name.
    for (const auto* nodeId : {"s1", "c3", "b4", "b3", "c2", "b1"}) controller.setChecked(QString::fromLatin1(nodeId), true);

    const auto names = filenamesByNode(controller);
    QCOMPARE(names.value(QStringLiteral("s1")), QStringLiteral("M5 Screw-2.step"));
    QCOMPARE(names.value(QStringLiteral("b4")), QStringLiteral("M5 Screw.step"));
    QCOMPARE(names.value(QStringLiteral("c2")), QStringLiteral("Carriage.step"));
    QCOMPARE(names.value(QStringLiteral("c3")), QStringLiteral("Carriage-2.step"));
    QCOMPARE(controller.fileCount(), 4);

    // Reordering the bucket changes the order files are written, not their names.
    controller.moveBucketItemById(QStringLiteral("s1"), 3);
    QCOMPARE(filenamesByNode(controller), names);
}

void ExportWorkspaceControllerTest::sameFileGivesSameNamesEveryRun()
{
    QString fingerprint;
    QStringList names;
    for (int run = 0; run < 3; ++run) {
        loupe::app::exporting::ExportWorkspaceController controller;
        controller.replaceSnapshot(snapshotWithRepeats());
        controller.setDestination(QStringLiteral("/tmp/loupe-export-review"));
        controller.setAllChecked(true);
        if (run == 0) {
            fingerprint = controller.planFingerprint();
            names = filenames(controller);
            QVERIFY(!fingerprint.isEmpty());
        }
        QCOMPARE(controller.planFingerprint(), fingerprint);
        QCOMPARE(filenames(controller), names);
    }
}

void ExportWorkspaceControllerTest::reviewedPlanSendsOneSelectionPerFile()
{
    loupe::app::exporting::ExportWorkspaceController controller;
    controller.replaceSnapshot(snapshotWithRepeats());
    controller.setDestination(QStringLiteral("/tmp/loupe-export-review"));
    controller.setDocumentReady(true);
    controller.setAllChecked(true);
    QSignalSpy executeSpy(&controller, &loupe::app::exporting::ExportWorkspaceController::executeRequested);

    QVERIFY(controller.exportReviewedPlan());

    QCOMPARE(executeSpy.count(), 1);
    const auto plan = QJsonDocument::fromJson(executeSpy.first().at(0).toByteArray()).object();
    const auto selections = plan.value(QStringLiteral("selections")).toArray();
    QCOMPARE(selections.size(), 6);
    QStringList leafNames;
    for (const auto& value : selections) leafNames.append(value.toObject().value(QStringLiteral("leafName")).toString());
    QCOMPARE(leafNames, (QStringList{QStringLiteral("Carriage"), QStringLiteral("M5 Screw"), QStringLiteral("Bushing"),
                                     QStringLiteral("Carriage-2"), QStringLiteral("Top Plate"), QStringLiteral("M5 Screw-2")}));

    // Results come back per file row.
    for (int row = 0; row < 6; ++row) {
        const auto map = controller.planRows().at(row).toMap();
        controller.handleExportRowResult(row, map.value(QStringLiteral("nodeId")).toString(),
                                         map.value(QStringLiteral("path")).toString(), true, QString{});
    }
    controller.handleExportCompleted(6, 0);
    QVERIFY(controller.exportSucceeded());
}

void ExportWorkspaceControllerTest::removingARowUnchecksEveryCopy()
{
    loupe::app::exporting::ExportWorkspaceController controller;
    controller.replaceSnapshot(snapshotWithRepeats());
    controller.setAllChecked(true);

    controller.removeBucketRow(QStringLiteral("b1"));

    for (const auto* nodeId : {"b1", "b2", "b3", "b4"}) QVERIFY(!controller.isChecked(QString::fromLatin1(nodeId)));
    QVERIFY(controller.isChecked(QStringLiteral("s1")));
    QCOMPARE(controller.fileCount(), 5);
}

void ExportWorkspaceControllerTest::movingARowMovesEveryCopy()
{
    loupe::app::exporting::ExportWorkspaceController controller;
    controller.replaceSnapshot(snapshotWithRepeats());
    controller.setAllChecked(true);

    controller.moveBucketItemById(QStringLiteral("b3"), 5);

    const auto rows = controller.bucketRows();
    QCOMPARE(rows.size(), 6);
    QCOMPARE(rows.at(5).toMap().value(QStringLiteral("nodeId")).toString(), QStringLiteral("b1"));
    QCOMPARE(rows.at(5).toMap().value(QStringLiteral("copyCount")).toInt(), 4);
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("nodeId")).toString(), QStringLiteral("u1"));
}

void ExportWorkspaceControllerTest::copiesNamedOneByOneUseThePartName()
{
    loupe::app::exporting::ExportWorkspaceController controller;
    controller.replaceSnapshot(QStringLiteral(R"({"effectiveUnit":"mm","sourceToMillimeters":1,"nodes":[
        {"id":"root","name":"Assembly","kind":0,"parentId":""},
        {"id":"dBolt","name":"Bolt","kind":3,"parentId":""},
        {"id":"o1","name":"Bolt:1","kind":2,"parentId":"root","definitionId":"dBolt"},
        {"id":"o2","name":"Bolt:2","kind":2,"parentId":"root","definitionId":"dBolt"},
        {"id":"o3","name":"Washer:1","kind":2,"parentId":"root","definitionId":"dWasher"}
    ]})"));
    controller.setAllChecked(true);

    QCOMPARE(filenames(controller), (QStringList{QStringLiteral("Bolt.step"), QStringLiteral("Washer_1.step")}));
}

void ExportWorkspaceControllerTest::typedNamesWinAndOnlyTypedClashesBlock()
{
    loupe::app::exporting::ExportWorkspaceController controller;
    controller.replaceSnapshot(snapshotWithRepeats());
    controller.setDestination(QStringLiteral("/tmp/loupe-export-review"));
    controller.setDocumentReady(true);
    controller.setAllChecked(true);

    // Typing the bushing's name onto the top plate moves the bushing, not the typed name.
    controller.setFilenameOverride(QStringLiteral("p1"), QStringLiteral("Bushing"));
    auto names = filenamesByNode(controller);
    QCOMPARE(names.value(QStringLiteral("p1")), QStringLiteral("Bushing.step"));
    QCOMPARE(names.value(QStringLiteral("u1")), QStringLiteral("Bushing-2.step"));
    QVERIFY(controller.canExport());

    // Two typed names that clash are the user's to resolve.
    controller.setFilenameOverride(QStringLiteral("u1"), QStringLiteral("bushing.step"));
    QVERIFY(!controller.canExport());
    QVERIFY(!controller.planError().isEmpty());
}

QTEST_MAIN(ExportWorkspaceControllerTest)
#include "test_export_workspace_controller.moc"
