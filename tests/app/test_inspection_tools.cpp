#include <QtTest/QTest>
#include <QtCore/qmath.h>

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
    void sectionStateNeverMutatesExportShape();
    void sectionSupportsFlipPositionCapAndSliceOnly();
    void captureSettingsResolveTransparentPngDimensions();
    void captureSettingsBoundCustomScaleAndInclusionOptions();
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

    controller.setMode(loupe::app::tools::MeasurementMode::SurfaceArea);
    QCOMPARE(controller.resultLabel(), QStringLiteral("600 mm²"));
    controller.setMode(loupe::app::tools::MeasurementMode::Volume);
    QCOMPARE(controller.resultLabel(), QStringLiteral("1000 mm³"));
    controller.setMode(loupe::app::tools::MeasurementMode::Bounds);
    QCOMPARE(controller.resultLabel(), QStringLiteral("10 × 20 × 30 mm"));

    controller.setEffectiveUnit(QStringLiteral("in"));
    controller.setMode(loupe::app::tools::MeasurementMode::Volume);
    QCOMPARE(controller.resultLabel(), QStringLiteral("0.061023744 in³"));
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

    QCOMPARE(section.axis(), loupe::app::tools::SectionAxis::Y);
    QCOMPARE(section.position(), 12.5);
    QVERIFY(section.flipped());
    QVERIFY(!section.capEnabled());
    QVERIFY(section.sliceOnly());
}

void InspectionToolsTest::captureSettingsResolveTransparentPngDimensions()
{
    loupe::app::tools::CaptureController capture;
    capture.setViewportSize({800, 600});
    capture.setScale(2);
    capture.setTransparentBackground(true);

    QCOMPARE(capture.format(), QStringLiteral("png"));
    QCOMPARE(capture.resolvedSize(), QSize(1600, 1200));
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

QTEST_MAIN(InspectionToolsTest)

#include "test_inspection_tools.moc"
