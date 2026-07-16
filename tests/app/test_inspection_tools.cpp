#include <QtTest/QTest>
#include <QtCore/qmath.h>
#include <QFileInfo>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QUrl>

#include "app/tools/MeasurementController.h"
#include "app/tools/CaptureController.h"
#include "app/tools/SectionController.h"

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
    void captureSettingsResolveTransparentPngDimensions();
    void captureSettingsBoundCustomScaleAndInclusionOptions();
    void captureWritesAndVerifiesPngAtomically();
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
    QCOMPARE(controller.resultLabel(), QStringLiteral("600 mm²"));
    controller.setMode(loupe::app::tools::MeasurementMode::Volume);
    QCOMPARE(controller.resultLabel(), QStringLiteral("1000 mm³"));
    controller.setMode(loupe::app::tools::MeasurementMode::Bounds);
    QCOMPARE(controller.resultLabel(), QStringLiteral("10 × 20 × 30 mm"));
    controller.setMode(loupe::app::tools::MeasurementMode::EdgeLength);
    QCOMPARE(controller.resultLabel(), QStringLiteral("Longest edge: 20 mm"));
    controller.setMode(loupe::app::tools::MeasurementMode::RadiusDiameter);
    QCOMPARE(controller.resultLabel(), QStringLiteral("Radius: 5 mm · Diameter: 10 mm"));

    controller.setEffectiveUnit(QStringLiteral("in"));
    controller.setMode(loupe::app::tools::MeasurementMode::Volume);
    QCOMPARE(controller.resultLabel(), QStringLiteral("0.061023744 in³"));
}

void InspectionToolsTest::viewportPicksProducePointToPointMeasurement()
{
    loupe::app::tools::MeasurementController controller;
    controller.recordPoint({0.0F, 0.0F, 0.0F});
    controller.recordPoint({25.4F, 0.0F, 0.0F});

    QCOMPARE(controller.resultLabel(), QStringLiteral("25.4 mm"));
    controller.setEffectiveUnit(QStringLiteral("in"));
    QCOMPARE(controller.resultLabel(), QStringLiteral("1 in"));
    controller.clearPicks();
    QVERIFY(controller.resultLabel().isEmpty());
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

    QCOMPARE(section.axis(), loupe::app::tools::SectionAxis::Y);
    QCOMPARE(section.position(), 12.5);
    QVERIFY(section.flipped());
    QVERIFY(!section.capEnabled());
    QVERIFY(section.sliceOnly());
    QCOMPARE(section.sliceDisplay(), QStringLiteral("filled"));
}

void InspectionToolsTest::sectionCanUseASelectedFacePlane()
{
    loupe::app::tools::SectionController section;
    section.setCandidatePlane({0.0F, 1.0F, 0.0F}, {0.0F, 12.5F, 0.0F});
    QVERIFY(section.hasSelectedPlane());
    section.useSelectedPlane();

    QVERIFY(section.usingSelectedPlane());
    QCOMPARE(section.planeOffset(), 12.5);
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
    capture.setCustomScale(9.0);
    capture.setIncludeMeasurements(false);
    capture.setIncludeSectionCaps(false);

    QCOMPARE(capture.scale(), 4.0);
    QCOMPARE(capture.resolvedSize(), QSize(1600, 1200));
    QVERIFY(!capture.includeMeasurements());
    QVERIFY(!capture.includeSectionCaps());
}

void InspectionToolsTest::captureWritesAndVerifiesPngAtomically()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("capture.png"));
    QImage image(32, 24, QImage::Format_RGBA8888);
    image.fill(QColor(QStringLiteral("#336699")));
    loupe::app::tools::CaptureController capture;

    QVERIFY(capture.saveImage(image, QUrl::fromLocalFile(path)));
    QVERIFY(capture.lastSaveSucceeded());
    QVERIFY(capture.statusMessage().contains(QStringLiteral("capture.png")));
    QVERIFY(QFileInfo(path).size() > 0);
    QFile output(path);
    QVERIFY(output.open(QIODevice::ReadOnly));
    QCOMPARE(output.read(8), QByteArray::fromHex("89504e470d0a1a0a"));
}

QTEST_MAIN(InspectionToolsTest)

#include "test_inspection_tools.moc"
