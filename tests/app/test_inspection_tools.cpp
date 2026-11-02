#include <QtTest/QTest>
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
    void explodeAutoComesApartLevelByLevel();
    void explodeAutoOnlyMovesTheChosenSubAssembly();
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
    // The placement itself is kept until reset.
    QVERIFY(explode.exploded());
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
