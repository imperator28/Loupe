#include <QtTest/QTest>

#include <QSignalSpy>

#include <cmath>

#include "app/drawing/DrawingWorkspaceController.h"
#include "core/domain/AssemblyTypes.h"

using loupe::app::drawing::DrawingWorkspaceController;

namespace {

// One assembly with two parts, matching the shape of a real snapshot.
QString snapshot()
{
    const auto occurrence = static_cast<int>(loupe::domain::NodeKind::Occurrence);
    const auto root = static_cast<int>(loupe::domain::NodeKind::Root);
    return QStringLiteral(R"({"effectiveUnit":"mm","sourceToMillimeters":1.0,"nodes":[
        {"id":"root","parentId":"","name":"Enclosure","kind":%1},
        {"id":"plate","parentId":"root","name":"Base plate","kind":%2},
        {"id":"cover","parentId":"root","name":"Cover","kind":%2}]})")
        .arg(root)
        .arg(occurrence);
}

// An up direction perpendicular to the view; parallel would be a degenerate view and the
// plan refuses the whole batch, which is correct but tests nothing here.
void setStandardView(DrawingWorkspaceController& controller, const QString& label, const double x,
                     const double y, const double z)
{
    const bool alongZ = std::abs(z) > 0.9;
    controller.setCandidateStandardView(label, x, y, z, 0.0, alongZ ? 1.0 : 0.0, alongZ ? 0.0 : 1.0);
}

QString queueDrawing(DrawingWorkspaceController& controller, const QString& nodeId,
                     const QString& label, const double x, const double y, const double z)
{
    controller.setCandidateNodeId(nodeId);
    setStandardView(controller, label, x, y, z);
    return controller.addCandidateToQueue();
}

QVariantMap rowAt(const DrawingWorkspaceController& controller, const int index)
{
    return controller.planRows().at(index).toMap();
}

} // namespace

class DrawingWorkspaceControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void severalViewsOfOnePartCoexistAsSeparateRows();
    void removingOneDrawingLeavesTheOthers();
    void reorderingTheQueueReordersThePlanRows();
    void aQueuedViewIsSnapshottedNotFollowedLive();
    void anExactDuplicateIsReportedRatherThanQueuedTwice();
    void aCurvedFaceIsRefusedWithItsDeviation();
    void aFlatFaceIsAcceptedAsAView();
    void anIncompleteCandidateCannotBeAdded();
    void selectingARowLoadsItBackIntoTheCandidate();
    void perPartDrawingCountsAreReported();
    void filenameOverridesReachThePlan();
    void collidingOverridesSurfaceAsAPlanError();
    void everyMutatorIsLockedWhileExporting();
    void aMismatchedRowResultFailsTheWholeBatch();
    void rowResultsReconcileAgainstTheReviewedRows();
    void previewIsRequestedOnlyForAValidCandidateAndSupersedes();
};

void DrawingWorkspaceControllerTest::severalViewsOfOnePartCoexistAsSeparateRows()
{
    // The motivating case: a checkbox over the tree cannot express this at all.
    DrawingWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    controller.setDocumentReady(true);
    controller.setDestination(QStringLiteral("/out"));

    QVERIFY(!queueDrawing(controller, QStringLiteral("plate"), QStringLiteral("Top"), 0, 0, 1).isEmpty());
    QVERIFY(!queueDrawing(controller, QStringLiteral("plate"), QStringLiteral("Front"), 0, -1, 0).isEmpty());
    QVERIFY(!queueDrawing(controller, QStringLiteral("plate"), QStringLiteral("Right"), 1, 0, 0).isEmpty());

    QCOMPARE(controller.queueCount(), 3);
    QCOMPARE(controller.planRows().size(), 3);
    QCOMPARE(rowAt(controller, 0).value(QStringLiteral("filename")).toString(),
             QStringLiteral("Base plate-top.dxf"));
    QCOMPARE(rowAt(controller, 1).value(QStringLiteral("filename")).toString(),
             QStringLiteral("Base plate-front.dxf"));
    QCOMPARE(rowAt(controller, 2).value(QStringLiteral("filename")).toString(),
             QStringLiteral("Base plate-right.dxf"));
    QVERIFY(controller.planError().isEmpty());
    QVERIFY(controller.canExport());
}

void DrawingWorkspaceControllerTest::removingOneDrawingLeavesTheOthers()
{
    DrawingWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    controller.setDestination(QStringLiteral("/out"));
    const auto top = queueDrawing(controller, QStringLiteral("plate"), QStringLiteral("Top"), 0, 0, 1);
    const auto front = queueDrawing(controller, QStringLiteral("plate"), QStringLiteral("Front"), 0, -1, 0);

    QVERIFY(controller.removeDrawing(top));
    QCOMPARE(controller.queueCount(), 1);
    QCOMPARE(controller.queue().at(0).toMap().value(QStringLiteral("drawingId")).toString(), front);
    QVERIFY(!controller.removeDrawing(QStringLiteral("nope")));
}

void DrawingWorkspaceControllerTest::reorderingTheQueueReordersThePlanRows()
{
    // Rows must follow the queue rather than the plan's canonical order, because a
    // worker event's rowIndex indexes what the user is looking at.
    DrawingWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    controller.setDestination(QStringLiteral("/out"));
    const auto top = queueDrawing(controller, QStringLiteral("plate"), QStringLiteral("Top"), 0, 0, 1);
    queueDrawing(controller, QStringLiteral("cover"), QStringLiteral("Front"), 0, -1, 0);

    QVERIFY(controller.moveDrawing(top, 1));
    QCOMPARE(rowAt(controller, 0).value(QStringLiteral("filename")).toString(),
             QStringLiteral("Cover-front.dxf"));
    QCOMPARE(rowAt(controller, 1).value(QStringLiteral("filename")).toString(),
             QStringLiteral("Base plate-top.dxf"));
    QVERIFY(!controller.moveDrawing(top, 9));
}

void DrawingWorkspaceControllerTest::aQueuedViewIsSnapshottedNotFollowedLive()
{
    // Orbiting after queueing must not change what was queued. This is the reason a
    // queued drawing stores a resolved vector instead of referring to camera state.
    DrawingWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    controller.setDestination(QStringLiteral("/out"));
    const auto queued = queueDrawing(controller, QStringLiteral("plate"), QStringLiteral("Top"), 0, 0, 1);
    const auto fingerprintWhenQueued = controller.planFingerprint();

    setStandardView(controller, QStringLiteral("Bottom"), 0, 0, -1);
    controller.setCandidateScale(1, 2);
    controller.rebuildPlan();

    QCOMPARE(controller.queueCount(), 1);
    QCOMPARE(controller.queue().at(0).toMap().value(QStringLiteral("viewLabel")).toString(),
             QStringLiteral("Top"));
    QCOMPARE(rowAt(controller, 0).value(QStringLiteral("filename")).toString(),
             QStringLiteral("Base plate-top.dxf"));
    QCOMPARE(controller.planFingerprint(), fingerprintWhenQueued);
    Q_UNUSED(queued)
}

void DrawingWorkspaceControllerTest::anExactDuplicateIsReportedRatherThanQueuedTwice()
{
    DrawingWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    controller.setDestination(QStringLiteral("/out"));
    const auto first = queueDrawing(controller, QStringLiteral("plate"), QStringLiteral("Top"), 0, 0, 1);

    setStandardView(controller, QStringLiteral("Top"), 0, 0, 1);
    QVERIFY(controller.addCandidateToQueue().isEmpty());
    QCOMPARE(controller.queueCount(), 1);
    QVERIFY(controller.candidateStatus().contains(QStringLiteral("already in the queue")));
    // The existing row is selected, so the user is shown the drawing they asked for.
    QCOMPARE(controller.selectedDrawingId(), first);

    // A different scale of the same view is not a duplicate.
    controller.setCandidateScale(1, 2);
    QVERIFY(!controller.addCandidateToQueue().isEmpty());
    QCOMPARE(controller.queueCount(), 2);
    QCOMPARE(rowAt(controller, 1).value(QStringLiteral("filename")).toString(),
             QStringLiteral("Base plate-top-1to2.dxf"));
}

void DrawingWorkspaceControllerTest::aCurvedFaceIsRefusedWithItsDeviation()
{
    DrawingWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    controller.setCandidateNodeId(QStringLiteral("plate"));
    controller.setCandidateFaceNormal(0.0, 0.0, 1.0, 39.4);

    QVERIFY(!controller.candidateValid());
    QVERIFY(controller.candidateStatus().contains(QStringLiteral("curved")));
    QVERIFY(controller.candidateStatus().contains(QStringLiteral("39.4")));
    QVERIFY(controller.addCandidateToQueue().isEmpty());
}

void DrawingWorkspaceControllerTest::aFlatFaceIsAcceptedAsAView()
{
    DrawingWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    controller.setDestination(QStringLiteral("/out"));
    controller.setCandidateNodeId(QStringLiteral("plate"));
    controller.setCandidateFaceNormal(0.0, 0.0, 1.0, 0.002);

    QVERIFY(controller.candidateValid());
    QCOMPARE(controller.candidateViewKind(), QStringLiteral("FaceNormal"));
    QVERIFY(!controller.addCandidateToQueue().isEmpty());
    // The up direction has to be usable, not merely present: parallel to the normal is
    // a degenerate view and the plan would refuse the whole batch.
    QVERIFY(controller.planError().isEmpty());
}

void DrawingWorkspaceControllerTest::anIncompleteCandidateCannotBeAdded()
{
    DrawingWorkspaceController controller;
    controller.replaceSnapshot(snapshot());

    QVERIFY(!controller.candidateValid());
    QVERIFY(controller.addCandidateToQueue().isEmpty());

    controller.setCandidateNodeId(QStringLiteral("plate"));
    QVERIFY(!controller.candidateValid());
    QVERIFY(controller.candidateStatus().contains(QStringLiteral("view")));
    QVERIFY(controller.addCandidateToQueue().isEmpty());

    setStandardView(controller, QStringLiteral("Top"), 0, 0, 1);
    QVERIFY(controller.candidateValid());

    // An unknown part is refused rather than accepted and failed later.
    controller.setCandidateNodeId(QStringLiteral("ghost"));
    QCOMPARE(controller.candidateNodeId(), QStringLiteral("plate"));
}

void DrawingWorkspaceControllerTest::selectingARowLoadsItBackIntoTheCandidate()
{
    DrawingWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    controller.setDestination(QStringLiteral("/out"));
    const auto top = queueDrawing(controller, QStringLiteral("plate"), QStringLiteral("Top"), 0, 0, 1);
    queueDrawing(controller, QStringLiteral("cover"), QStringLiteral("Front"), 0, -1, 0);

    QSignalSpy candidateSpy(&controller, &DrawingWorkspaceController::candidateChanged);
    QVERIFY(controller.selectDrawing(top));
    QCOMPARE(controller.selectedDrawingId(), top);
    QCOMPARE(controller.candidateNodeId(), QStringLiteral("plate"));
    QCOMPARE(controller.candidateViewLabel(), QStringLiteral("Top"));
    QVERIFY(candidateSpy.count() > 0);
    QVERIFY(!controller.selectDrawing(QStringLiteral("nope")));
}

void DrawingWorkspaceControllerTest::perPartDrawingCountsAreReported()
{
    DrawingWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    controller.setDestination(QStringLiteral("/out"));
    queueDrawing(controller, QStringLiteral("plate"), QStringLiteral("Top"), 0, 0, 1);
    queueDrawing(controller, QStringLiteral("plate"), QStringLiteral("Front"), 0, -1, 0);

    QCOMPARE(controller.drawingCountForNode(QStringLiteral("plate")), 2);
    QCOMPARE(controller.drawingCountForNode(QStringLiteral("cover")), 0);
    for (const auto& value : controller.components()) {
        const auto component = value.toMap();
        if (component.value(QStringLiteral("nodeId")).toString() == QStringLiteral("plate")) {
            QCOMPARE(component.value(QStringLiteral("drawingCount")).toInt(), 2);
        }
    }
}

void DrawingWorkspaceControllerTest::filenameOverridesReachThePlan()
{
    DrawingWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    controller.setDestination(QStringLiteral("/out"));
    const auto top = queueDrawing(controller, QStringLiteral("plate"), QStringLiteral("Top"), 0, 0, 1);

    QVERIFY(controller.setFilenameOverride(top, QStringLiteral("blank-outline")));
    QCOMPARE(rowAt(controller, 0).value(QStringLiteral("filename")).toString(),
             QStringLiteral("blank-outline.dxf"));
    QVERIFY(rowAt(controller, 0).value(QStringLiteral("filenameOverridden")).toBool());
    QVERIFY(!controller.setFilenameOverride(QStringLiteral("nope"), QStringLiteral("x")));
}

void DrawingWorkspaceControllerTest::collidingOverridesSurfaceAsAPlanError()
{
    // A plan error must still leave a row per queued drawing, so the user can see which
    // ones need attention rather than an empty list.
    DrawingWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    controller.setDocumentReady(true);
    controller.setDestination(QStringLiteral("/out"));
    const auto top = queueDrawing(controller, QStringLiteral("plate"), QStringLiteral("Top"), 0, 0, 1);
    const auto front = queueDrawing(controller, QStringLiteral("plate"), QStringLiteral("Front"), 0, -1, 0);
    controller.setFilenameOverride(top, QStringLiteral("same"));
    controller.setFilenameOverride(front, QStringLiteral("same"));

    QVERIFY(!controller.planError().isEmpty());
    QCOMPARE(controller.planRows().size(), 2);
    QCOMPARE(rowAt(controller, 0).value(QStringLiteral("status")).toString(),
             QStringLiteral("Needs attention"));
    QVERIFY(!controller.canExport());
}

void DrawingWorkspaceControllerTest::everyMutatorIsLockedWhileExporting()
{
    DrawingWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    controller.setDocumentReady(true);
    controller.setDestination(QStringLiteral("/out"));
    const auto top = queueDrawing(controller, QStringLiteral("plate"), QStringLiteral("Top"), 0, 0, 1);
    QVERIFY(controller.exportReviewedPlan());
    QVERIFY(controller.exporting());

    QVERIFY(queueDrawing(controller, QStringLiteral("cover"), QStringLiteral("Top"), 0, 0, 1).isEmpty());
    QVERIFY(!controller.removeDrawing(top));
    QVERIFY(!controller.moveDrawing(top, 0));
    QVERIFY(!controller.setFilenameOverride(top, QStringLiteral("other")));
    controller.setFormat(QStringLiteral("SVG"));
    QCOMPARE(controller.format(), QStringLiteral("DXF"));
    controller.setDestination(QStringLiteral("/elsewhere"));
    QCOMPARE(controller.destination(), QStringLiteral("/out"));
    controller.setIncludeScaleFiducial(true);
    QVERIFY(!controller.includeScaleFiducial());
    controller.replaceSnapshot(snapshot());
    QCOMPARE(controller.queueCount(), 1);

    controller.handleDrawingCompleted(1, 0);
    QVERIFY(!controller.exporting());
    QVERIFY(controller.exportSucceeded());
}

void DrawingWorkspaceControllerTest::aMismatchedRowResultFailsTheWholeBatch()
{
    // A result that does not match the reviewed row means the worker and the review have
    // diverged, which is not a per-row problem.
    DrawingWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    controller.setDocumentReady(true);
    controller.setDestination(QStringLiteral("/out"));
    queueDrawing(controller, QStringLiteral("plate"), QStringLiteral("Top"), 0, 0, 1);
    QVERIFY(controller.exportReviewedPlan());

    controller.handleDrawingRowResult(0, QStringLiteral("d1"), QStringLiteral("/out/wrong.dxf"), true, {});
    QVERIFY(!controller.exporting());
    QVERIFY(!controller.exportSucceeded());
    QVERIFY(controller.exportSummary().contains(QStringLiteral("did not match")));
}

void DrawingWorkspaceControllerTest::rowResultsReconcileAgainstTheReviewedRows()
{
    DrawingWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    controller.setDocumentReady(true);
    controller.setDestination(QStringLiteral("/out"));
    const auto top = queueDrawing(controller, QStringLiteral("plate"), QStringLiteral("Top"), 0, 0, 1);
    queueDrawing(controller, QStringLiteral("cover"), QStringLiteral("Front"), 0, -1, 0);
    QVERIFY(controller.exportReviewedPlan());

    controller.handleDrawingProgress(0, 2, QStringLiteral("Projecting"), 0.25);
    QCOMPARE(rowAt(controller, 0).value(QStringLiteral("status")).toString(), QStringLiteral("Projecting"));
    QCOMPARE(controller.exportProgress(), 0.25);

    controller.handleDrawingRowResult(0, top, rowAt(controller, 0).value(QStringLiteral("path")).toString(),
                                     true, {});
    QCOMPARE(rowAt(controller, 0).value(QStringLiteral("status")).toString(),
             QStringLiteral("Written and validated"));

    const auto second = controller.queue().at(1).toMap().value(QStringLiteral("drawingId")).toString();
    controller.handleDrawingRowResult(1, second, rowAt(controller, 1).value(QStringLiteral("path")).toString(),
                                      false, QStringLiteral("empty outline"));
    QCOMPARE(rowAt(controller, 1).value(QStringLiteral("error")).toString(), QStringLiteral("empty outline"));

    // One failed row must not abort the batch.
    QVERIFY(controller.exporting());
    controller.handleDrawingCompleted(1, 1);
    QVERIFY(!controller.exportSucceeded());
    QVERIFY(controller.exportSummary().contains(QStringLiteral("1 failed")));
}

void DrawingWorkspaceControllerTest::previewIsRequestedOnlyForAValidCandidateAndSupersedes()
{
    DrawingWorkspaceController controller;
    controller.replaceSnapshot(snapshot());
    QSignalSpy previewSpy(&controller, &DrawingWorkspaceController::previewRequested);
    QSignalSpy canceledSpy(&controller, &DrawingWorkspaceController::previewCanceled);

    controller.setCandidateNodeId(QStringLiteral("plate"));
    QCOMPARE(previewSpy.count(), 0); // no view yet

    setStandardView(controller, QStringLiteral("Top"), 0, 0, 1);
    QCOMPARE(previewSpy.count(), 1);
    const auto firstRevision = previewSpy.at(0).at(1).toInt();

    controller.setCandidateContentMode(QStringLiteral("Outer contour only"));
    QCOMPARE(previewSpy.count(), 2);
    // The superseded request is canceled by revision, so a late reply can be dropped.
    QCOMPARE(canceledSpy.count(), 1);
    QCOMPARE(canceledSpy.at(0).at(0).toInt(), firstRevision);
    QVERIFY(previewSpy.at(1).at(1).toInt() > firstRevision);
}

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    DrawingWorkspaceControllerTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_drawing_workspace_controller.moc"
