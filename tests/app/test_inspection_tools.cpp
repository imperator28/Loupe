#include <QtTest/QTest>
#include <QJsonObject>
#include <QtTest/QSignalSpy>
#include <QtCore/qmath.h>
#include <QFileInfo>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QUrl>

#include "app/tools/MeasurementController.h"
#include "app/tools/CaptureController.h"
#include "app/tools/SectionController.h"
#include "app/tools/ExplodeController.h"

class InspectionToolsTest final : public QObject
{
    Q_OBJECT

private slots:
    void pointDistanceUsesEffectiveUnit();
    void measurementModeTracksTheRequestedOperation();
    void componentMeasurementsUseNormalizedGeometryMetrics();
    void viewportPicksProducePointToPointMeasurement();
    void viewportPicksIdentifyTheSelectedSurface();
    void pickedSurfaceNormalsProduceAngleMeasurement();
    void surfaceToSurfaceMeasuresTheGapSquareToParallelFaces();
    void surfaceToSurfaceFindsTheTrueMinimumBetweenFacesAtAnAngle();
    void angleIsBetweenFlatFacesAndStraightEdgesOnly();
    void closestPointsBetweenSkewEdgesAndAnEdgeThroughAFace();
    void circularEdgesKnowTheirCentreAndRadius();
    void pointPicksMeasureWithComponentsAndRefuseTheWrongKind();
    void sectionStateNeverMutatesExportShape();
    void sectionSupportsFlipPositionCapAndSliceOnly();
    void sectionCanUseASelectedFacePlane();
    void sectionOffsetsASelectedFacePlaneAndResetsWhenAxisChanges();
    void sectionInteractionCanCommitOrCancelPreviewPosition();
    void captureSettingsResolveTransparentPngDimensions();
    void captureSettingsBoundCustomScaleAndInclusionOptions();
    void captureReportsProgressUntilThePngIsSaved();
    void captureWritesAndVerifiesPngAtomically();
    void explodeMovesASubAssemblyWithEverythingInIt();
    void explodeHandlesWorkInWorldAxesInsideATurnedSubAssembly();
    void explodeRotationTurnsAboutTheGivenPivot();
    void explodeMovedCentreStaysPutAsASelectionTurns();
    void explodeEditOnTheAssemblyStartsFromTheAssembly();
    void explodeLayoutSnapshotsRoundTrip();
    void explodeAutoComesApartLevelByLevel();
    void explodeAutoOnlyMovesTheChosenSubAssembly();
    void explodeAutoTakesStackedLayersApartInOrder();
    void explodeResetUndoAndRedo();
    void explodeCancelRestoresWhereTheDragBegan();
    void explodeBlendEasesPartsHome();
    void explodeLocalPlaneFollowsTheMovedPart();
    void explodeTypedOffsetsAndAnglesAndSnapping();
};

void InspectionToolsTest::pointDistanceUsesEffectiveUnit()
{
    loupe::app::tools::MeasurementController controller;
    controller.setEffectiveUnit(QStringLiteral("in"));

    const auto measurement = controller.pointDistance({0.0F, 0.0F, 0.0F}, {25.4F, 0.0F, 0.0F});
    QVERIFY(qAbs(measurement.value - 1.0) < 0.00001);
    QCOMPARE(measurement.unitLabel, QStringLiteral("in"));
}

void InspectionToolsTest::measurementModeTracksTheRequestedOperation()
{
    loupe::app::tools::MeasurementController controller;
    controller.setMode(loupe::app::tools::MeasurementMode::SurfaceArea);

    QCOMPARE(controller.mode(), loupe::app::tools::MeasurementMode::SurfaceArea);
    QCOMPARE(controller.pickInstruction(), QStringLiteral("Select a surface"));
}

void InspectionToolsTest::componentMeasurementsUseNormalizedGeometryMetrics()
{
    loupe::app::tools::MeasurementController controller;
    controller.setSelectedGeometry(600.0, 1'000.0, {10.0, 20.0, 30.0});
    controller.setSelectedTopology(20.0, 5.0, 2);

    controller.setMode(loupe::app::tools::MeasurementMode::SurfaceArea);
    QVERIFY(controller.resultLabel().isEmpty());
    QVERIFY(controller.recordTopologyPick(QStringLiteral("face"), 1, {}, {0.0F, 0.0F, 1.0F}, 600.0, 0.0,
                                          QStringLiteral("Housing")));
    QCOMPARE(controller.resultLabel(), QStringLiteral("600 mm²"));
    controller.setMode(loupe::app::tools::MeasurementMode::Volume);
    QCOMPARE(controller.resultLabel(), QStringLiteral("1000 mm³"));
    controller.setMode(loupe::app::tools::MeasurementMode::Bounds);
    QCOMPARE(controller.resultLabel(), QStringLiteral("10 × 20 × 30 mm"));
    controller.setMode(loupe::app::tools::MeasurementMode::EdgeLength);
    QVERIFY(controller.recordTopologyPick(QStringLiteral("edge"), 2, {}, {}, 20.0, 0.0,
                                          QStringLiteral("Housing")));
    QCOMPARE(controller.resultLabel(), QStringLiteral("Edge length: 20 mm"));
    controller.setMode(loupe::app::tools::MeasurementMode::RadiusDiameter);
    QVERIFY(controller.recordTopologyPick(QStringLiteral("edge"), 3, {}, {}, 31.4, 5.0,
                                          QStringLiteral("Housing")));
    QCOMPARE(controller.resultLabel(), QStringLiteral("Radius: 5 mm · Diameter: 10 mm"));

    controller.setEffectiveUnit(QStringLiteral("in"));
    controller.setMode(loupe::app::tools::MeasurementMode::Volume);
    QCOMPARE(controller.resultLabel(), QStringLiteral("0.061023744 in³"));
}

void InspectionToolsTest::viewportPicksProducePointToPointMeasurement()
{
    loupe::app::tools::MeasurementController controller;
    QSignalSpy resultChanged(&controller, &loupe::app::tools::MeasurementController::resultChanged);
    controller.recordPoint({0.0F, 0.0F, 0.0F});
    controller.recordPoint({25.4F, 0.0F, 0.0F});

    QCOMPARE(controller.resultLabel(), QStringLiteral("25.4 mm"));
    controller.setEffectiveUnit(QStringLiteral("in"));
    QCOMPARE(controller.resultLabel(), QStringLiteral("1 in"));
    const auto signalsBeforeClear = resultChanged.count();
    controller.clearPicks();
    QVERIFY(controller.resultLabel().isEmpty());
    QCOMPARE(resultChanged.count(), signalsBeforeClear + 1);

    const auto signalsBeforeEmptyClear = resultChanged.count();
    controller.clearPicks();
    QCOMPARE(resultChanged.count(), signalsBeforeEmptyClear + 1);
}

void InspectionToolsTest::viewportPicksIdentifyTheSelectedSurface()
{
    loupe::app::tools::MeasurementController controller;
    controller.recordPick({1.0F, 2.0F, 3.0F}, {0.0F, 0.0F, 1.0F}, QStringLiteral("Housing"), QStringLiteral("Surface point"));

    QCOMPARE(controller.firstPickDescription(), QStringLiteral("Surface point on Housing at (1, 2, 3) mm"));
    QVERIFY(controller.secondPickDescription().isEmpty());
    const auto picks = controller.pickedPoints();
    QCOMPARE(picks.size(), 1);
    QCOMPARE(picks.first().value<QVector3D>(), QVector3D(1.0F, 2.0F, 3.0F));
}

void InspectionToolsTest::pickedSurfaceNormalsProduceAngleMeasurement()
{
    loupe::app::tools::MeasurementController controller;
    controller.setMode(loupe::app::tools::MeasurementMode::Angle);
    controller.recordPick({0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F});
    controller.recordPick({0.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F});

    QCOMPARE(controller.resultLabel(), QStringLiteral("90 °"));
    controller.setEffectiveUnit(QStringLiteral("in"));
    controller.setMode(loupe::app::tools::MeasurementMode::SurfaceToSurface);
    controller.recordPick({0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F});
    controller.recordPick({0.0F, 25.4F, 0.0F}, {0.0F, 0.0F, 1.0F});
    QCOMPARE(controller.resultLabel(), QStringLiteral("1 in"));
}

void InspectionToolsTest::sectionStateNeverMutatesExportShape()
{
    loupe::app::tools::SectionController section;
    section.setEnabled(true);
    section.setAxis(loupe::app::tools::SectionAxis::X);

    QCOMPARE(section.exportMutationCount(), 0);
}

void InspectionToolsTest::sectionSupportsFlipPositionCapAndSliceOnly()
{
    loupe::app::tools::SectionController section;
    section.setEnabled(true);
    section.setAxis(loupe::app::tools::SectionAxis::Y);
    section.setPosition(12.5);
    section.setFlipped(true);
    section.setCapEnabled(false);
    section.setSliceOnly(true);
    section.setSliceDisplay(QStringLiteral("filled"));
    section.setSliceBorderColor(QStringLiteral("#234567"));
    section.setSliceBorderWidth(3.25);

    QCOMPARE(section.axis(), loupe::app::tools::SectionAxis::Y);
    QCOMPARE(section.position(), 12.5);
    QVERIFY(section.flipped());
    QVERIFY(!section.capEnabled());
    QVERIFY(section.sliceOnly());
    QCOMPARE(section.sliceDisplay(), QStringLiteral("filled"));
    QCOMPARE(section.sliceBorderColor(), QStringLiteral("#234567"));
    QCOMPARE(section.sliceBorderWidth(), 3.25);

    section.setSliceBorderWidth(100.0);
    QCOMPARE(section.sliceBorderWidth(), 8.0);
    section.setSliceBorderWidth(0.1);
    QCOMPARE(section.sliceBorderWidth(), 0.5);
    section.setSliceBorderColor(QStringLiteral("not-a-color"));
    QCOMPARE(section.sliceBorderColor(), QStringLiteral("#234567"));
}

void InspectionToolsTest::sectionCanUseASelectedFacePlane()
{
    loupe::app::tools::SectionController section;
    section.setCandidatePlane({0.0F, 1.0F, 0.0F}, {0.0F, 12.5F, 0.0F});
    QVERIFY(section.hasSelectedPlane());
    section.useSelectedPlane();

    QVERIFY(section.usingSelectedPlane());
    QCOMPARE(section.planeOffset(), 12.5);
    QCOMPARE(section.effectiveNormal(), QVector3D(0.0F, 1.0F, 0.0F));
    QCOMPARE(section.effectiveOffset(), 12.5);
}

void InspectionToolsTest::sectionOffsetsASelectedFacePlaneAndResetsWhenAxisChanges()
{
    loupe::app::tools::SectionController section;
    section.setCandidatePlane({0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 8.0F});
    section.useSelectedPlane();
    section.setPosition(2.5);

    QCOMPARE(section.effectiveOffset(), 10.5);

    section.setAxis(loupe::app::tools::SectionAxis::X);
    QCOMPARE(section.position(), 0.0);
    QCOMPARE(section.effectiveNormal(), QVector3D(1.0F, 0.0F, 0.0F));
    QCOMPARE(section.effectiveOffset(), 0.0);
}

void InspectionToolsTest::sectionInteractionCanCommitOrCancelPreviewPosition()
{
    loupe::app::tools::SectionController section;
    section.setPosition(4.0);
    section.beginInteraction();
    QVERIFY(section.interacting());
    section.previewPosition(9.0);
    QCOMPARE(section.position(), 9.0);
    section.cancelInteraction();
    QVERIFY(!section.interacting());
    QCOMPARE(section.position(), 4.0);

    section.beginInteraction();
    section.previewPosition(7.5);
    section.commitInteraction();
    QVERIFY(!section.interacting());
    QCOMPARE(section.position(), 7.5);
}

void InspectionToolsTest::captureSettingsResolveTransparentPngDimensions()
{
    loupe::app::tools::CaptureController capture;
    capture.setViewportSize({800, 600});
    capture.setScale(2);
    capture.setTransparentBackground(true);

    QCOMPARE(capture.format(), QStringLiteral("png"));
    QCOMPARE(capture.resolvedSize(), QSize(1600, 1200));
    QCOMPARE(capture.resolvedWidth(), 1600);
    QCOMPARE(capture.resolvedHeight(), 1200);
    QVERIFY(capture.transparentBackground());
}

void InspectionToolsTest::captureSettingsBoundCustomScaleAndInclusionOptions()
{
    loupe::app::tools::CaptureController capture;
    capture.setViewportSize({400, 300});
    capture.setCustomScale(3.17);
    capture.setIncludeMeasurements(false);
    capture.setIncludeSectionCaps(false);

    QCOMPARE(capture.scale(), 3.17);
    QCOMPARE(capture.resolvedSize(), QSize(1268, 951));
    capture.setCustomScale(9.0);
    capture.setViewportSize({4096, 2160});
    QVERIFY(capture.scale() <= capture.maximumScale());
    QVERIFY(!capture.includeMeasurements());
    QVERIFY(!capture.includeSectionCaps());
}

void InspectionToolsTest::captureReportsProgressUntilThePngIsSaved()
{
    loupe::app::tools::CaptureController capture;
    capture.beginCapture();
    QVERIFY(capture.inProgress());
    QVERIFY(capture.progress() > 0.0);
    capture.setCaptureProgress(0.65, QStringLiteral("Rendering at 3×…"));
    QCOMPARE(capture.progress(), 0.65);
    QCOMPARE(capture.statusMessage(), QStringLiteral("Rendering at 3×…"));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QImage image(4, 4, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);
    QVERIFY(capture.saveImage(image, QUrl::fromLocalFile(directory.filePath(QStringLiteral("progress.png")))));
    QVERIFY(!capture.inProgress());
    QCOMPARE(capture.progress(), 1.0);
}

void InspectionToolsTest::captureWritesAndVerifiesPngAtomically()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("capture.png"));
    QImage image(32, 24, QImage::Format_RGBA8888);
    image.fill(QColor(0x33, 0x66, 0x99, 0x60));
    loupe::app::tools::CaptureController capture;

    QVERIFY(capture.saveImage(image, QUrl::fromLocalFile(path)));
    QVERIFY(capture.lastSaveSucceeded());
    QVERIFY(capture.statusMessage().contains(QStringLiteral("capture.png")));
    QVERIFY(QFileInfo(path).size() > 0);
    QFile output(path);
    QVERIFY(output.open(QIODevice::ReadOnly));
    QCOMPARE(output.read(8), QByteArray::fromHex("89504e470d0a1a0a"));
    const QImage decoded(path);
    QVERIFY(!decoded.isNull());
    QCOMPARE(decoded.pixelColor(0, 0).alpha(), 0x60);
}

namespace {

// root
// +-- sub (a sub-assembly)
// |   +-- left   centre (-10, 0, 0)
// |   +-- right  centre (10, 0, 0)
// +-- far        centre (100, 0, 0)
void buildExplodeFixture(loupe::app::tools::ExplodeController& explode)
{
    explode.setHierarchy({
        {QStringLiteral("root"), QString{}},
        {QStringLiteral("sub"), QStringLiteral("root")},
        {QStringLiteral("left"), QStringLiteral("sub")},
        {QStringLiteral("right"), QStringLiteral("sub")},
        {QStringLiteral("far"), QStringLiteral("root")},
    });
    explode.includePartBounds(QStringLiteral("left"), {-11, -1, -1}, {-9, 1, 1});
    explode.includePartBounds(QStringLiteral("right"), {9, -1, -1}, {11, 1, 1});
    explode.includePartBounds(QStringLiteral("far"), {99, -1, -1}, {101, 1, 1});
}

bool closeTo(const QVector3D& actual, const QVector3D& expected)
{
    return (actual - expected).length() < 1e-3F;
}

QVector3D displayedCentre(const loupe::app::tools::ExplodeController& explode, const QString& part)
{
    const auto centre = explode.assembledCentre(part);
    return explode.displayedRotation(part).rotatedVector(centre) + explode.displayedPosition(part);
}

QString describe(const QVector3D& value)
{
    return QStringLiteral("(%1, %2, %3)").arg(value.x()).arg(value.y()).arg(value.z());
}

} // namespace

void InspectionToolsTest::explodeMovesASubAssemblyWithEverythingInIt()
{
    loupe::app::tools::ExplodeController explode;
    buildExplodeFixture(explode);
    QVERIFY(!explode.exploded());
    QSignalSpy moved(&explode, &loupe::app::tools::ExplodeController::partsMoved);

    explode.beginInteraction({QStringLiteral("sub")});
    QVERIFY(explode.interacting());
    explode.previewTranslation({0, 0, 25});
    explode.previewTranslation({0, 0, 30}); // each preview is from where the drag began
    explode.commitInteraction();

    QVERIFY(explode.exploded());
    QVERIFY(explode.canUndo());
    QVERIFY(closeTo(displayedCentre(explode, QStringLiteral("left")), {-10, 0, 30}));
    QVERIFY(closeTo(displayedCentre(explode, QStringLiteral("right")), {10, 0, 30}));
    QVERIFY(closeTo(displayedCentre(explode, QStringLiteral("far")), {100, 0, 0}));
    QVERIFY(explode.isMoved(QStringLiteral("left")));
    QVERIFY(!explode.isMoved(QStringLiteral("far")));
    QVERIFY(closeTo(explode.offsetOf(QStringLiteral("sub")), {0, 0, 30}));
    QVERIFY(moved.count() >= 1);
    QVERIFY(moved.last().at(0).toStringList().contains(QStringLiteral("left")));
    QVERIFY(!moved.last().at(0).toStringList().contains(QStringLiteral("far")));
}

void InspectionToolsTest::explodeHandlesWorkInWorldAxesInsideATurnedSubAssembly()
{
    loupe::app::tools::ExplodeController explode;
    buildExplodeFixture(explode);
    explode.beginInteraction({QStringLiteral("sub")});
    explode.previewRotation({0, 0, 1}, 90, explode.assembledCentre(QStringLiteral("sub")));
    explode.commitInteraction();
    // A quarter turn about Z through the sub-assembly's centre (the origin): left goes to -Y.
    QVERIFY2(closeTo(displayedCentre(explode, QStringLiteral("left")), {0, -10, 0}),
             qPrintable(describe(displayedCentre(explode, QStringLiteral("left")))));

    // Dragging the part along world X moves it along world X, whatever its parent's turn.
    explode.beginInteraction({QStringLiteral("left")});
    explode.previewTranslation({7, 0, 0});
    explode.commitInteraction();
    QVERIFY2(closeTo(displayedCentre(explode, QStringLiteral("left")), {7, -10, 0}),
             qPrintable(describe(displayedCentre(explode, QStringLiteral("left")))));
    QVERIFY(closeTo(displayedCentre(explode, QStringLiteral("right")), {0, 10, 0}));

    // Turning the sub-assembly back carries the moved part with it.
    explode.beginInteraction({QStringLiteral("sub")});
    explode.previewRotation({0, 0, 1}, -90, {0, 0, 0});
    explode.commitInteraction();
    QVERIFY2(closeTo(displayedCentre(explode, QStringLiteral("left")), {-10, -7, 0}),
             qPrintable(describe(displayedCentre(explode, QStringLiteral("left")))));
}

void InspectionToolsTest::explodeRotationTurnsAboutTheGivenPivot()
{
    loupe::app::tools::ExplodeController explode;
    buildExplodeFixture(explode);
    const auto centre = explode.assembledCentre(QStringLiteral("far"));
    explode.beginInteraction({QStringLiteral("far")});
    explode.previewRotation({0, 1, 0}, 45, centre);
    explode.commitInteraction();
    QVERIFY(closeTo(displayedCentre(explode, QStringLiteral("far")), centre));
    QVERIFY(explode.isMoved(QStringLiteral("far")));
    QVERIFY(qAbs(explode.anglesOf(QStringLiteral("far")).y() - 45.0F) < 0.01F);
}

void InspectionToolsTest::explodeMovedCentreStaysPutAsASelectionTurns()
{
    // A lopsided sub-assembly: a small part beside a big one. The centre of its bounding
    // box moves as it turns, so the handle, which turned about that, carried it off a
    // little more with every turn. The moved centre is fixed to the parts.
    loupe::app::tools::ExplodeController explode;
    explode.setHierarchy({
        {QStringLiteral("root"), QString{}},
        {QStringLiteral("sub"), QStringLiteral("root")},
        {QStringLiteral("small"), QStringLiteral("sub")},
        {QStringLiteral("big"), QStringLiteral("sub")},
    });
    explode.includePartBounds(QStringLiteral("small"), {0, 0, 0}, {2, 2, 2});
    explode.includePartBounds(QStringLiteral("big"), {10, -10, 0}, {40, 10, 2});
    const QStringList selection{QStringLiteral("sub")};
    const auto boxCentre = [&explode, &selection]() {
        const auto bounds = explode.displayedBounds(selection);
        return (bounds.value(QStringLiteral("minimum")).value<QVector3D>()
                + bounds.value(QStringLiteral("maximum")).value<QVector3D>()) * 0.5F;
    };
    const auto pivot = explode.movedCentre(selection);
    const auto boxBefore = boxCentre();
    for (int turn = 0; turn < 8; ++turn) {
        explode.beginInteraction(selection);
        explode.previewRotation({0, 0, 1}, 45, explode.movedCentre(selection));
        explode.commitInteraction();
        QVERIFY2(closeTo(explode.movedCentre(selection), pivot), qPrintable(describe(explode.movedCentre(selection))));
    }
    // A full turn brings every part back where it began.
    QVERIFY2(closeTo(boxCentre(), boxBefore), qPrintable(describe(boxCentre())));
    explode.beginInteraction(selection);
    explode.previewRotation({0, 0, 1}, 30, pivot);
    explode.commitInteraction();
    QVERIFY(!closeTo(boxCentre(), boxBefore)); // the box's centre is not a fixed point
    QVERIFY(closeTo(explode.movedCentre(selection), pivot));
}

void InspectionToolsTest::explodeEditOnTheAssemblyStartsFromTheAssembly()
{
    // Spread, then Assembled: the layout is kept, the view shows the assembly. Dragging a
    // part there used to bring the layout back first, so the part jumped from under the
    // pointer and its assembled place was lost. The drag starts from the assembly now, and
    // the kept layout is handed over to be saved.
    loupe::app::tools::ExplodeController explode;
    buildExplodeFixture(explode);
    explode.autoExplode({}, 1.0);
    const auto spreadFar = displayedCentre(explode, QStringLiteral("far"));
    QVERIFY(!closeTo(spreadFar, {100, 0, 0}));
    explode.setBlend(0.0);
    QVERIFY(explode.showingAssembly());
    QVERIFY(closeTo(explode.offsetOf(QStringLiteral("far")), {}));
    QCOMPARE(explode.autoFactor({}), 0.0);
    QVERIFY(closeTo(explode.movedCentre({QStringLiteral("far")}), {100, 0, 0}));

    QSignalSpy setAside(&explode, &loupe::app::tools::ExplodeController::layoutSetAside);
    explode.beginInteraction({QStringLiteral("far")});
    QCOMPARE(setAside.count(), 1);
    QCOMPARE(explode.blend(), 1.0);
    // Nothing moved by starting: everything is still where it is assembled.
    QVERIFY(closeTo(displayedCentre(explode, QStringLiteral("far")), {100, 0, 0}));
    QVERIFY(closeTo(displayedCentre(explode, QStringLiteral("left")), {-10, 0, 0}));
    explode.previewTranslation({0, 0, 20});
    explode.commitInteraction();
    QVERIFY(closeTo(displayedCentre(explode, QStringLiteral("far")), {100, 0, 20}));
    QVERIFY(!explode.isMoved(QStringLiteral("left")));

    // The set-aside layout is the spread one, and restoring it puts the parts back there.
    const auto kept = setAside.at(0).at(0).toMap();
    QVERIFY(explode.restoreLayout(kept));
    QCOMPARE(explode.blend(), 0.0); // the viewport eases out to it
    explode.setBlend(1.0);
    QVERIFY(closeTo(displayedCentre(explode, QStringLiteral("far")), spreadFar));
    QVERIFY(explode.layoutMatches(kept));

    // Undo walks back: the new layout, then the assembly it started from, then the spread.
    explode.undo();
    QVERIFY(closeTo(displayedCentre(explode, QStringLiteral("far")), {100, 0, 20}));
    explode.undo();
    QVERIFY(!explode.exploded());
    explode.undo();
    QVERIFY(closeTo(displayedCentre(explode, QStringLiteral("far")), spreadFar));

    // Putting a part back on the assembly is an edit to the layout: it comes back first.
    explode.setBlend(0.0);
    explode.resetNode(QStringLiteral("far"));
    QCOMPARE(setAside.count(), 1);
    QCOMPARE(explode.blend(), 1.0);
    QVERIFY(explode.isMoved(QStringLiteral("left")));
    QVERIFY(!explode.isMoved(QStringLiteral("far")));
}

void InspectionToolsTest::explodeLayoutSnapshotsRoundTrip()
{
    loupe::app::tools::ExplodeController explode;
    buildExplodeFixture(explode);
    QVERIFY(!explode.restoreLayout(explode.layoutSnapshot())); // nothing to restore
    explode.beginInteraction({QStringLiteral("sub")});
    explode.previewRotation({0, 0, 1}, 30, explode.movedCentre({QStringLiteral("sub")}));
    explode.commitInteraction();
    explode.translateBy({QStringLiteral("far")}, {5, 0, 0});
    QCOMPARE(explode.layoutSummary(), QStringLiteral("3 parts moved · 2 turned"));
    const auto left = displayedCentre(explode, QStringLiteral("left"));
    // Kept as JSON in the settings: through it and back.
    const auto stored = QJsonObject::fromVariantMap(explode.layoutSnapshot()).toVariantMap();
    explode.resetAll();
    QVERIFY(!explode.layoutMatches(stored));
    QVERIFY(explode.restoreLayout(stored));
    explode.setBlend(1.0);
    QVERIFY(explode.layoutMatches(stored));
    QVERIFY(closeTo(displayedCentre(explode, QStringLiteral("left")), left));
    QVERIFY(closeTo(displayedCentre(explode, QStringLiteral("far")), {105, 0, 0}));

    // A node the document no longer has is skipped; garbage is ignored.
    auto foreign = stored;
    auto transforms = foreign.value(QStringLiteral("transforms")).toMap();
    transforms.insert(QStringLiteral("gone"), transforms.value(QStringLiteral("far")));
    transforms.insert(QStringLiteral("far"), QVariantList{1, 2, 3});
    foreign.insert(QStringLiteral("transforms"), transforms);
    explode.resetAll();
    QVERIFY(explode.restoreLayout(foreign));
    QVERIFY(!explode.isMoved(QStringLiteral("far")));
    QVERIFY(explode.isMoved(QStringLiteral("left")));
}

namespace {

using loupe::app::tools::MeasureEntity;

// A flat rectangle as two triangles, at z or (vertical) at x.
MeasureEntity flatFace(const QVector3D& a, const QVector3D& b, const QVector3D& c, const QVector3D& d, const QVector3D& pick)
{
    MeasureEntity face;
    face.kind = MeasureEntity::Kind::Face;
    face.topologyId = 1;
    face.point = pick;
    face.triangles = {a, b, c, a, c, d};
    loupe::app::tools::measure::describeFace(&face);
    return face;
}

MeasureEntity straightEdge(const QVector3D& from, const QVector3D& to)
{
    MeasureEntity edge;
    edge.kind = MeasureEntity::Kind::Edge;
    edge.topologyId = 2;
    edge.point = from;
    edge.polyline = {from, to};
    loupe::app::tools::measure::describeEdge(&edge);
    return edge;
}

QString detailValue(const loupe::app::tools::MeasurementController& controller, const QString& label)
{
    for (const auto& value : controller.resultDetails()) {
        const auto map = value.toMap();
        if (map.value(QStringLiteral("label")).toString() == label) return map.value(QStringLiteral("value")).toString();
    }
    return {};
}

} // namespace

void InspectionToolsTest::surfaceToSurfaceMeasuresTheGapSquareToParallelFaces()
{
    // A wall 5 mm thick, its two faces picked far apart along it. The old measurement was
    // between the two click points, 20-odd mm here; the gap square to the faces is 5.
    const auto bottom = flatFace({0, 0, 0}, {10, 0, 0}, {10, 10, 0}, {0, 10, 0}, {2, 2, 0});
    const auto top = flatFace({20, 0, 5}, {30, 0, 5}, {30, 10, 5}, {20, 10, 5}, {28, 9, 5});
    QVERIFY(bottom.planar && top.planar);
    loupe::app::tools::MeasurementController controller;
    controller.setMode(loupe::app::tools::MeasurementMode::SurfaceToSurface);
    QVERIFY(controller.acceptEntity(bottom));
    QVERIFY(controller.resultText().isEmpty());
    QVERIFY(controller.acceptEntity(top));
    QCOMPARE(controller.resultTitle(), QStringLiteral("Gap"));
    QCOMPARE(controller.resultText(), QStringLiteral("5.000"));
    QCOMPARE(controller.resultLabel(), QStringLiteral("5 mm"));
    QCOMPARE(detailValue(controller, QStringLiteral("Angle")), QStringLiteral("0.00°"));
    QCOMPARE(detailValue(controller, QStringLiteral("Minimum")), QStringLiteral("11.180 mm"));
    // The dimension runs square from the second pick to the first plane.
    const auto annotation = controller.annotation();
    QCOMPARE(annotation.value(QStringLiteral("kind")).toString(), QStringLiteral("distance"));
    QVERIFY(closeTo(annotation.value(QStringLiteral("from")).value<QVector3D>(), {28, 9, 0}));
    QVERIFY(closeTo(annotation.value(QStringLiteral("to")).value<QVector3D>(), {28, 9, 5}));
    controller.setEffectiveUnit(QStringLiteral("in"));
    QCOMPARE(controller.resultText(), QStringLiteral("0.1969"));
}

void InspectionToolsTest::surfaceToSurfaceFindsTheTrueMinimumBetweenFacesAtAnAngle()
{
    // A floor and a wall standing 5 mm off its edge and 1 mm above it: square faces that are
    // not parallel meet nearest at the floor's edge and the wall's bottom edge.
    const auto floor = flatFace({0, 0, 0}, {10, 0, 0}, {10, 10, 0}, {0, 10, 0}, {5, 5, 0});
    const auto wall = flatFace({15, 0, 1}, {15, 10, 1}, {15, 10, 11}, {15, 0, 11}, {15, 5, 6});
    loupe::app::tools::MeasurementController controller;
    controller.setMode(loupe::app::tools::MeasurementMode::SurfaceToSurface);
    QVERIFY(controller.acceptEntity(floor));
    QVERIFY(controller.acceptEntity(wall));
    QCOMPARE(controller.resultTitle(), QStringLiteral("Minimum distance"));
    QCOMPARE(controller.resultText(), QStringLiteral("5.099"));
    QCOMPARE(detailValue(controller, QStringLiteral("Angle")), QStringLiteral("90.00°"));
    QCOMPARE(detailValue(controller, QStringLiteral("ΔX")), QStringLiteral("5.000 mm"));
    QCOMPARE(detailValue(controller, QStringLiteral("ΔZ")), QStringLiteral("1.000 mm"));
}

void InspectionToolsTest::angleIsBetweenFlatFacesAndStraightEdgesOnly()
{
    const auto floor = flatFace({0, 0, 0}, {10, 0, 0}, {10, 10, 0}, {0, 10, 0}, {5, 5, 0});
    // A ramp rising 30 degrees along X.
    const auto rise = static_cast<float>(std::tan(30.0 * 3.14159265358979323846 / 180.0) * 10.0);
    const auto ramp = flatFace({0, 0, 0}, {10, 0, rise}, {10, 10, rise}, {0, 10, 0}, {5, 5, rise / 2});
    loupe::app::tools::MeasurementController controller;
    controller.setMode(loupe::app::tools::MeasurementMode::Angle);
    QVERIFY(controller.acceptEntity(floor));
    QVERIFY(controller.acceptEntity(ramp));
    QCOMPARE(controller.resultText(), QStringLiteral("30.00"));
    QCOMPARE(detailValue(controller, QStringLiteral("Supplement")), QStringLiteral("150.00°"));

    // A curved face is refused, with why: it has no one direction to measure from.
    MeasureEntity curved;
    curved.kind = MeasureEntity::Kind::Face;
    curved.topologyId = 9;
    curved.radiusMm = 4.0;
    curved.triangles = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 0, 0}, {1, 1, 0.5F}, {0, 1, 0}};
    loupe::app::tools::measure::describeFace(&curved);
    QVERIFY(!curved.planar);
    QVERIFY(!controller.acceptEntity(curved));
    QVERIFY(controller.refusal().contains(QStringLiteral("curved face")));
    QVERIFY(!controller.describe(curved).value(QStringLiteral("eligible")).toBool());
    QCOMPARE(controller.resultText(), QStringLiteral("30.00")); // the last result stands

    // Lines: a straight edge against the floor is its rise out of the plane; two edges, the
    // angle between their lines, whichever way each runs.
    QVERIFY(controller.acceptEntity(straightEdge({0, 0, 0}, {10, 0, rise})));
    QVERIFY(controller.acceptEntity(floor));
    QCOMPARE(controller.resultText(), QStringLiteral("30.00"));
    QVERIFY(controller.acceptEntity(straightEdge({0, 0, 0}, {10, 0, 0})));
    QVERIFY(controller.acceptEntity(straightEdge({0, 5, 0}, {0, -5, 0})));
    QCOMPARE(controller.resultText(), QStringLiteral("90.00"));
}

void InspectionToolsTest::closestPointsBetweenSkewEdgesAndAnEdgeThroughAFace()
{
    // Two skew lines 3 apart in Z.
    const auto along = straightEdge({-5, 0, 0}, {5, 0, 0});
    const auto across = straightEdge({0, -5, 3}, {0, 5, 3});
    const auto skew = loupe::app::tools::measure::closestPoints(along, across);
    QVERIFY(std::abs(skew.distance - 3.0) < 1e-5);
    QVERIFY(closeTo(skew.first, {0, 0, 0}));
    QVERIFY(closeTo(skew.second, {0, 0, 3}));
    // An edge that pierces a face touches it.
    const auto floor = flatFace({-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}, {});
    const auto pin = straightEdge({0.2F, 0.3F, -2}, {0.2F, 0.3F, 2});
    QVERIFY(loupe::app::tools::measure::closestPoints(pin, floor).distance < 1e-5);
}

void InspectionToolsTest::circularEdgesKnowTheirCentreAndRadius()
{
    MeasureEntity edge;
    edge.kind = MeasureEntity::Kind::Edge;
    edge.radiusMm = 4.0;
    for (int step = 0; step <= 48; ++step) {
        const auto angle = 2.0 * 3.14159265358979323846 * step / 48;
        edge.polyline.append({static_cast<float>(10 + 4 * std::cos(angle)), static_cast<float>(-3 + 4 * std::sin(angle)), 7.0F});
    }
    edge.point = edge.polyline.front();
    loupe::app::tools::measure::describeEdge(&edge);
    QVERIFY(edge.circular);
    QVERIFY(!edge.linear);
    QVERIFY(closeTo(edge.centre, {10, -3, 7}));
    QVERIFY(std::abs(std::abs(edge.axis.z()) - 1.0F) < 1e-4F);

    loupe::app::tools::MeasurementController controller;
    controller.setMode(loupe::app::tools::MeasurementMode::RadiusDiameter);
    QVERIFY(controller.acceptEntity(edge));
    QCOMPARE(controller.resultText(), QStringLiteral("4.000"));
    QCOMPARE(detailValue(controller, QStringLiteral("Diameter")), QStringLiteral("8.000 mm"));
    QCOMPARE(controller.annotation().value(QStringLiteral("kind")).toString(), QStringLiteral("radius"));
    QVERIFY(!controller.acceptEntity(straightEdge({0, 0, 0}, {1, 0, 0})));
    QVERIFY(controller.refusal().startsWith(QStringLiteral("Not round")));
}

void InspectionToolsTest::pointPicksMeasureWithComponentsAndRefuseTheWrongKind()
{
    loupe::app::tools::MeasurementController controller;
    MeasureEntity vertex;
    vertex.kind = MeasureEntity::Kind::Vertex;
    vertex.point = {1, 2, 3};
    MeasureEntity centre;
    centre.kind = MeasureEntity::Kind::CircleCentre;
    centre.point = {4, 6, 3};
    centre.centre = {4, 6, 3};
    QVERIFY(!controller.acceptEntity(straightEdge({0, 0, 0}, {1, 0, 0}))); // a whole edge is not a point
    QVERIFY(controller.acceptEntity(vertex));
    QCOMPARE(controller.pickInstruction(), QStringLiteral("Select the second point"));
    QVERIFY(controller.acceptEntity(centre));
    QCOMPARE(controller.resultText(), QStringLiteral("5.000"));
    QCOMPARE(detailValue(controller, QStringLiteral("ΔX")), QStringLiteral("3.000 mm"));
    QCOMPARE(detailValue(controller, QStringLiteral("ΔY")), QStringLiteral("4.000 mm"));
    QCOMPARE(detailValue(controller, QStringLiteral("ΔZ")), QStringLiteral("0.000 mm"));
    QCOMPARE(controller.pickedEntities().size(), 2);
    QCOMPARE(controller.pickedEntities().at(1).toMap().value(QStringLiteral("title")).toString(), QStringLiteral("Circle centre"));
    QVERIFY(controller.resultSummary().startsWith(QStringLiteral("Distance: 5.000 mm")));
    // A third pick starts the next measurement.
    QVERIFY(controller.acceptEntity(vertex));
    QCOMPARE(controller.pickedEntities().size(), 1);
    QVERIFY(controller.resultText().isEmpty());
}

void InspectionToolsTest::explodeAutoComesApartLevelByLevel()
{
    loupe::app::tools::ExplodeController explode;
    buildExplodeFixture(explode);
    // The whole model: centre of everything is (45, 0, 0). sub's centre is the origin, so it
    // moves 45 to -X; far moves 55 to +X; inside sub, left and right move 10 apart again.
    explode.autoExplode({}, 1.0);
    QVERIFY(explode.exploded());
    QCOMPARE(explode.autoFactor({}), 1.0);
    QVERIFY2(closeTo(displayedCentre(explode, QStringLiteral("far")), {155, 0, 0}),
             qPrintable(describe(displayedCentre(explode, QStringLiteral("far")))));
    QVERIFY2(closeTo(displayedCentre(explode, QStringLiteral("left")), {-65, 0, 0}),
             qPrintable(describe(displayedCentre(explode, QStringLiteral("left")))));
    QVERIFY2(closeTo(displayedCentre(explode, QStringLiteral("right")), {-25, 0, 0}),
             qPrintable(describe(displayedCentre(explode, QStringLiteral("right")))));

    // The slider back to zero brings everything home.
    explode.beginAutoExplode({});
    explode.previewAutoExplode(0.5);
    explode.previewAutoExplode(0.0);
    explode.commitInteraction();
    QVERIFY(!explode.exploded());
    QCOMPARE(explode.autoFactor({}), 0.0);
}

void InspectionToolsTest::explodeAutoOnlyMovesTheChosenSubAssembly()
{
    loupe::app::tools::ExplodeController explode;
    buildExplodeFixture(explode);
    explode.autoExplode(QStringLiteral("sub"), 2.0);
    QVERIFY(closeTo(displayedCentre(explode, QStringLiteral("left")), {-30, 0, 0}));
    QVERIFY(closeTo(displayedCentre(explode, QStringLiteral("right")), {30, 0, 0}));
    QVERIFY(closeTo(displayedCentre(explode, QStringLiteral("far")), {100, 0, 0}));
    QVERIFY(!explode.isMoved(QStringLiteral("sub")));
    QCOMPARE(explode.autoFactor(QStringLiteral("sub")), 2.0);
    QCOMPARE(explode.autoFactor({}), 0.0);

    // What the panel's auto-explode takes apart for a selection.
    QCOMPARE(explode.autoExplodeScope({QStringLiteral("sub")}), QStringLiteral("sub"));
    QCOMPARE(explode.autoExplodeScope({QStringLiteral("left")}), QStringLiteral("sub"));
    QCOMPARE(explode.autoExplodeScope({}), QString{});
    QCOMPARE(explode.autoExplodeScope({QStringLiteral("left"), QStringLiteral("far")}), QString{});
}

void InspectionToolsTest::explodeAutoTakesStackedLayersApartInOrder()
{
    // Three 100 x 100 plates stacked 2 mm thick, and two identical liners: their centres
    // all but coincide, so moving away from the centre alone left them where they were.
    loupe::app::tools::ExplodeController explode;
    explode.setHierarchy({
        {QStringLiteral("stack"), QString{}},
        {QStringLiteral("bottom"), QStringLiteral("stack")},
        {QStringLiteral("middle"), QStringLiteral("stack")},
        {QStringLiteral("top"), QStringLiteral("stack")},
        {QStringLiteral("linerA"), QStringLiteral("stack")},
        {QStringLiteral("linerB"), QStringLiteral("stack")},
    });
    explode.includePartBounds(QStringLiteral("bottom"), {-50, -50, 0}, {50, 50, 2});
    explode.includePartBounds(QStringLiteral("middle"), {-50, -50, 2}, {50, 50, 4});
    explode.includePartBounds(QStringLiteral("top"), {-50, -50, 4}, {50, 50, 6});
    explode.includePartBounds(QStringLiteral("linerA"), {200, -10, 0}, {240, 10, 1});
    explode.includePartBounds(QStringLiteral("linerB"), {200, -10, 0}, {240, 10, 1});
    explode.autoExplode(QStringLiteral("stack"), 1.0);

    const auto z = [&](const char* part) { return displayedCentre(explode, QString::fromLatin1(part)).z(); };
    // Apart along the stacking direction, in the order they were stacked, by more than their
    // own thickness.
    QVERIFY2(z("bottom") < z("middle") && z("middle") < z("top"),
             qPrintable(QStringLiteral("%1 %2 %3").arg(z("bottom")).arg(z("middle")).arg(z("top"))));
    QVERIFY(z("middle") - z("bottom") > 4.0F);
    QVERIFY(z("top") - z("middle") > 4.0F);
    // Still level across the plate: a stack comes apart along its thin axis only.
    QVERIFY(std::abs(displayedCentre(explode, QStringLiteral("top")).x()
                     - displayedCentre(explode, QStringLiteral("bottom")).x()) < 1e-3F);
    // The two identical liners separate too.
    QVERIFY(std::abs(z("linerA") - z("linerB")) > 4.0F);
}

void InspectionToolsTest::explodeResetUndoAndRedo()
{
    loupe::app::tools::ExplodeController explode;
    buildExplodeFixture(explode);
    explode.setOffset(QStringLiteral("far"), {0, 20, 0});
    explode.setOffset(QStringLiteral("left"), {0, 0, -5});
    QVERIFY(closeTo(displayedCentre(explode, QStringLiteral("left")), {-10, 0, -5}));

    QVERIFY(explode.canReset(QStringLiteral("left")));
    QVERIFY(explode.canReset(QStringLiteral("sub"))); // something in it was moved
    QVERIFY(!explode.canReset(QStringLiteral("right")));
    explode.resetNode(QStringLiteral("left"));
    QVERIFY(!explode.isMoved(QStringLiteral("left")));
    QVERIFY(explode.isMoved(QStringLiteral("far")));

    explode.undo();
    QVERIFY(explode.isMoved(QStringLiteral("left")));
    explode.redo();
    QVERIFY(!explode.isMoved(QStringLiteral("left")));

    explode.resetAll();
    QVERIFY(!explode.exploded());
    QVERIFY(explode.canUndo());
    explode.undo(); // an accidental reset can be taken back
    QVERIFY(explode.isMoved(QStringLiteral("far")));
    QVERIFY(explode.canRedo());

    // A new document starts with nothing moved and no history.
    explode.clear();
    QVERIFY(!explode.exploded());
    QVERIFY(!explode.canUndo());
    QVERIFY(!explode.canRedo());
}

void InspectionToolsTest::explodeCancelRestoresWhereTheDragBegan()
{
    loupe::app::tools::ExplodeController explode;
    buildExplodeFixture(explode);
    explode.beginInteraction({QStringLiteral("far")});
    explode.previewTranslation({0, 50, 0});
    QVERIFY(explode.isMoved(QStringLiteral("far")));
    explode.cancelInteraction();
    QVERIFY(!explode.interacting());
    QVERIFY(!explode.exploded());
    QVERIFY(!explode.canUndo());

    // A drag that ends where it began leaves nothing to undo.
    explode.beginInteraction({QStringLiteral("far")});
    explode.previewTranslation({0, 0, 0});
    explode.commitInteraction();
    QVERIFY(!explode.canUndo());
}

void InspectionToolsTest::explodeBlendEasesPartsHome()
{
    loupe::app::tools::ExplodeController explode;
    buildExplodeFixture(explode);
    explode.setOffset(QStringLiteral("far"), {0, 40, 0});
    explode.setBlend(0.5);
    QVERIFY(closeTo(displayedCentre(explode, QStringLiteral("far")), {100, 20, 0}));
    explode.setBlend(0.0);
    QVERIFY(closeTo(displayedCentre(explode, QStringLiteral("far")), {100, 0, 0}));
    // The placement itself is kept until reset: this is the assembled view of a layout.
    QVERIFY(explode.exploded());
    // A move made on the assembly starts from the assembly, which is what is on screen; the
    // kept layout is set aside (see explodeEditOnTheAssemblyStartsFromTheAssembly).
    explode.translateBy({QStringLiteral("far")}, {0, 0, 1});
    QCOMPARE(explode.blend(), 1.0);
    QVERIFY(closeTo(displayedCentre(explode, QStringLiteral("far")), {100, 0, 1}));
    explode.resetAll();
    QCOMPARE(explode.blend(), 1.0);
}

void InspectionToolsTest::explodeLocalPlaneFollowsTheMovedPart()
{
    loupe::app::tools::ExplodeController explode;
    buildExplodeFixture(explode);
    explode.setOffset(QStringLiteral("far"), {10, 0, 0});
    // The world plane x = 105 cuts the moved part where x = 95 cuts it assembled.
    const auto plane = explode.localPlane(QStringLiteral("far"), {1, 0, 0}, 105);
    QVERIFY(closeTo(plane.toVector3D(), {1, 0, 0}));
    QVERIFY(qAbs(plane.w() - 95.0F) < 1e-3F);

    // Turned a quarter about Z: the world X plane is the part's own -Y plane.
    explode.resetAll();
    explode.setAngles(QStringLiteral("far"), {0, 0, 90});
    const auto turned = explode.localPlane(QStringLiteral("far"), {1, 0, 0}, 100);
    QVERIFY2(closeTo(turned.toVector3D(), {0, -1, 0}), qPrintable(describe(turned.toVector3D())));
    // The plane runs through the part's centre (100, 0, 0), so it does in the part's own frame too.
    QVERIFY2(qAbs(turned.w()) < 1e-3F, qPrintable(QString::number(turned.w())));
}

void InspectionToolsTest::explodeTypedOffsetsAndAnglesAndSnapping()
{
    loupe::app::tools::ExplodeController explode;
    buildExplodeFixture(explode);
    explode.setAngles(QStringLiteral("right"), {0, 0, 30});
    explode.setOffset(QStringLiteral("right"), {3, 4, 5});
    QVERIFY(closeTo(explode.offsetOf(QStringLiteral("right")), {3, 4, 5}));
    QVERIFY(qAbs(explode.anglesOf(QStringLiteral("right")).z() - 30.0F) < 0.01F);
    // The angle keeps the part where it is, and the offset keeps its turn.
    explode.setAngles(QStringLiteral("right"), {0, 0, 60});
    QVERIFY(closeTo(explode.offsetOf(QStringLiteral("right")), {3, 4, 5}));

    // Moving by an exact distance adds to where the nodes are, for several at once.
    explode.translateBy({QStringLiteral("right"), QStringLiteral("far")}, {0, -2.5F, 10});
    QVERIFY(closeTo(explode.offsetOf(QStringLiteral("right")), {3, 1.5F, 15}));
    QVERIFY(closeTo(explode.offsetOf(QStringLiteral("far")), {0, -2.5F, 10}));
    explode.undo();
    QVERIFY(closeTo(explode.offsetOf(QStringLiteral("right")), {3, 4, 5}));
    QVERIFY(!explode.isMoved(QStringLiteral("far")));

    // A handle on a picked face: its Z is the face's normal, X and Y square to it.
    for (const auto& normal : {QVector3D(0, 0, 1), QVector3D(1, 1, 0).normalized(), QVector3D(0, 0.2F, -1).normalized()}) {
        const auto frame = loupe::app::tools::ExplodeController::frameForNormal(normal);
        const auto x = frame.rotatedVector({1, 0, 0});
        const auto y = frame.rotatedVector({0, 1, 0});
        QVERIFY(closeTo(frame.rotatedVector({0, 0, 1}), normal));
        QVERIFY(std::abs(QVector3D::dotProduct(x, normal)) < 1e-5F);
        QVERIFY(closeTo(QVector3D::crossProduct(x, y), normal));
    }

    QVERIFY(explode.snapEnabled());
    explode.setTranslationStep(10);
    explode.setRotationStep(15);
    QCOMPARE(explode.snapTranslation(23.0, false), 20.0);
    QCOMPARE(explode.snapTranslation(23.0, true), 23.0);
    QCOMPARE(explode.snapRotation(52.0, false), 45.0);
    explode.setSnapEnabled(false);
    QCOMPARE(explode.snapRotation(52.0, false), 52.0);
}

QTEST_MAIN(InspectionToolsTest)

#include "test_inspection_tools.moc"
