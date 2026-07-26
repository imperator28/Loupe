#include <QtTest/QTest>

#include "protocol/ProtocolTypes.h"

class ProtocolTest final : public QObject
{
    Q_OBJECT

private slots:
    void openFileRoundTrips();
    void rejectsUnknownMajorVersion();
    void ignoresUnknownFieldsFromNewerMinorVersion();
    void canceledEventDecodes();
    void componentMetadataDecodes();
    void meshEventCarriesPayload();
    void edgeEventCarriesCadPayload();
    void measureAtPointRoundTrips();
    void exportPlanRoundTrips();
    void exportEventsDecode();
    void drawingPlanRoundTrips();
    void drawingPreviewRequestRoundTrips();
    void drawingEventsDecode();
    void malformedDrawingMessagesAreRefused();
};

void ProtocolTest::openFileRoundTrips()
{
    const loupe::protocol::OpenFile command{42, R"(C:\fixtures\assembly.step)", std::nullopt};

    const QByteArray bytes = loupe::protocol::encode(command);
    const auto decoded = loupe::protocol::decodeCommand(bytes);

    QCOMPARE(std::get<loupe::protocol::OpenFile>(decoded).requestId, 42ULL);
}

void ProtocolTest::rejectsUnknownMajorVersion()
{
    QVERIFY_THROWS_EXCEPTION(
        loupe::protocol::ProtocolError,
        loupe::protocol::decodeEvent(QByteArrayLiteral("{\"version\":{\"major\":99,\"minor\":0},\"type\":\"ready\"}\n")));
}

void ProtocolTest::ignoresUnknownFieldsFromNewerMinorVersion()
{
    const auto event = loupe::protocol::decodeEvent(
        QByteArrayLiteral("{\"version\":{\"major\":2,\"minor\":99},\"type\":\"ready\",\"futureField\":\"ignored\"}\n"));

    QVERIFY(std::holds_alternative<loupe::protocol::Ready>(event));
}

void ProtocolTest::canceledEventDecodes()
{
    const auto event = loupe::protocol::decodeEvent(
        QByteArrayLiteral("{\"version\":{\"major\":2,\"minor\":0},\"type\":\"canceled\",\"requestId\":7}\n"));

    QCOMPARE(std::get<loupe::protocol::Canceled>(event).requestId, 7ULL);
}

void ProtocolTest::componentMetadataDecodes()
{
    const auto event = loupe::protocol::decodeEvent(
        QByteArrayLiteral("{\"version\":{\"major\":2,\"minor\":0},\"type\":\"componentMetadata\",\"requestId\":7,\"nodeId\":\"body-1\",\"surfaceAreaMm2\":12.5,\"volumeMm3\":3.0,\"boundsMm\":{\"width\":1.0,\"height\":2.0,\"depth\":3.0},\"longestEdgeMm\":4.0,\"circularRadiusMm\":5.0,\"planarFaceCount\":6}\n"));

    const auto& metadata = std::get<loupe::protocol::ComponentMetadata>(event);
    QCOMPARE(metadata.nodeId, QStringLiteral("body-1"));
    QCOMPARE(metadata.volumeMm3, 3.0);
    QCOMPARE(metadata.planarFaceCount, 6);
}

void ProtocolTest::meshEventCarriesPayload()
{
    const auto event = loupe::protocol::decodeEvent(
        QByteArrayLiteral("{\"version\":{\"major\":2,\"minor\":0},\"type\":\"meshReady\",\"requestId\":7,\"definitionId\":\"body-1\",\"refinement\":0,\"segmentKey\":\"body-1\",\"meshBase64\":\"e30=\"}\n"));

    const auto& mesh = std::get<loupe::protocol::MeshReady>(event);
    QCOMPARE(mesh.requestId, 7ULL);
    QCOMPARE(mesh.meshJson, QByteArrayLiteral("{}"));
}

void ProtocolTest::edgeEventCarriesCadPayload()
{
    const auto event = loupe::protocol::decodeEvent(
        QByteArrayLiteral("{\"version\":{\"major\":2,\"minor\":0},\"type\":\"edgeReady\",\"requestId\":7,\"nodeId\":\"body-1\",\"edgeBase64\":\"e30=\"}\n"));

    const auto& edges = std::get<loupe::protocol::EdgeReady>(event);
    QCOMPARE(edges.requestId, 7ULL);
    QCOMPARE(edges.nodeId, QStringLiteral("body-1"));
    QCOMPARE(edges.edgeJson, QByteArrayLiteral("{}"));
}

void ProtocolTest::measureAtPointRoundTrips()
{
    const loupe::protocol::MeasureAtPoint command{19, QStringLiteral("def-cylinder"), 4.0, 0.0, 2.0, QStringLiteral("radius")};

    const auto decoded = loupe::protocol::decodeCommand(loupe::protocol::encode(loupe::protocol::Command{command}));
    const auto& measure = std::get<loupe::protocol::MeasureAtPoint>(decoded);
    QCOMPARE(measure.requestId, 19ULL);
    QCOMPARE(measure.nodeId, QStringLiteral("def-cylinder"));
    QCOMPARE(measure.mode, QStringLiteral("radius"));
}

void ProtocolTest::exportPlanRoundTrips()
{
    const loupe::protocol::ExecuteExportPlan command{23, QByteArrayLiteral("{\"schemaVersion\":1}"),
                                                      QStringLiteral("abc123")};

    const auto decoded = loupe::protocol::decodeCommand(
        loupe::protocol::encode(loupe::protocol::Command{command}));
    const auto& exportPlan = std::get<loupe::protocol::ExecuteExportPlan>(decoded);
    QCOMPARE(exportPlan.requestId, 23ULL);
    QCOMPARE(exportPlan.planJson, command.planJson);
    QCOMPARE(exportPlan.fingerprint, QStringLiteral("abc123"));
}

void ProtocolTest::exportEventsDecode()
{
    const auto progress = loupe::protocol::decodeEvent(QByteArrayLiteral(
        "{\"version\":{\"major\":2,\"minor\":0},\"type\":\"exportProgress\",\"requestId\":8,\"rowIndex\":0,\"rowCount\":2,\"stage\":\"Writing A.step\",\"fraction\":0.25}\n"));
    QCOMPARE(std::get<loupe::protocol::ExportProgress>(progress).rowCount, 2);

    const auto row = loupe::protocol::decodeEvent(QByteArrayLiteral(
        "{\"version\":{\"major\":2,\"minor\":0},\"type\":\"exportRowResult\",\"requestId\":8,\"rowIndex\":0,\"nodeId\":\"a\",\"path\":\"/tmp/A.step\",\"passed\":true,\"message\":\"ok\"}\n"));
    QVERIFY(std::get<loupe::protocol::ExportRowResult>(row).passed);

    const auto completed = loupe::protocol::decodeEvent(QByteArrayLiteral(
        "{\"version\":{\"major\":2,\"minor\":0},\"type\":\"exportCompleted\",\"requestId\":8,\"succeededCount\":2,\"failedCount\":0}\n"));
    QCOMPARE(std::get<loupe::protocol::ExportCompleted>(completed).succeededCount, 2);
}

void ProtocolTest::drawingPlanRoundTrips()
{
    const loupe::protocol::ExecuteDrawingPlan command{31, QByteArrayLiteral("{\"schemaVersion\":1}"),
                                                      QStringLiteral("fed321")};

    const auto decoded = loupe::protocol::decodeCommand(
        loupe::protocol::encode(loupe::protocol::Command{command}));
    const auto& plan = std::get<loupe::protocol::ExecuteDrawingPlan>(decoded);
    QCOMPARE(plan.requestId, 31ULL);
    QCOMPARE(plan.planJson, command.planJson);
    QCOMPARE(plan.fingerprint, QStringLiteral("fed321"));
}

void ProtocolTest::drawingPreviewRequestRoundTrips()
{
    const loupe::protocol::RequestDrawingPreview command{32, QByteArrayLiteral("{\"nodeId\":\"plate\"}"), 7};

    const auto decoded = loupe::protocol::decodeCommand(
        loupe::protocol::encode(loupe::protocol::Command{command}));
    const auto& preview = std::get<loupe::protocol::RequestDrawingPreview>(decoded);
    QCOMPARE(preview.requestId, 32ULL);
    QCOMPARE(preview.requestJson, command.requestJson);
    // The revision is what lets a late reply be dropped, so it has to survive the trip.
    QCOMPARE(preview.revision, 7);
}

void ProtocolTest::drawingEventsDecode()
{
    const auto progress = loupe::protocol::decodeEvent(QByteArrayLiteral(
        "{\"version\":{\"major\":2,\"minor\":1},\"type\":\"drawingProgress\",\"requestId\":9,\"rowIndex\":1,\"rowCount\":3,\"stage\":\"Projecting plate-top.dxf\",\"fraction\":0.5}\n"));
    QCOMPARE(std::get<loupe::protocol::DrawingProgress>(progress).rowIndex, 1);

    const auto row = loupe::protocol::decodeEvent(QByteArrayLiteral(
        "{\"version\":{\"major\":2,\"minor\":1},\"type\":\"drawingRowResult\",\"requestId\":9,\"rowIndex\":1,\"drawingId\":\"d2\",\"path\":\"/out/plate-top.dxf\",\"passed\":false,\"message\":\"empty outline\"}\n"));
    const auto& rowResult = std::get<loupe::protocol::DrawingRowResult>(row);
    QVERIFY(!rowResult.passed);
    QCOMPARE(rowResult.drawingId, QStringLiteral("d2"));
    QCOMPARE(rowResult.message, QStringLiteral("empty outline"));

    const auto completed = loupe::protocol::decodeEvent(QByteArrayLiteral(
        "{\"version\":{\"major\":2,\"minor\":1},\"type\":\"drawingCompleted\",\"requestId\":9,\"succeededCount\":2,\"failedCount\":1}\n"));
    QCOMPARE(std::get<loupe::protocol::DrawingCompleted>(completed).failedCount, 1);

    const auto preview = loupe::protocol::decodeEvent(QByteArrayLiteral(
        "{\"version\":{\"major\":2,\"minor\":1},\"type\":\"drawingPreviewReady\",\"requestId\":9,\"revision\":4,\"previewBase64\":\"e30=\",\"approximate\":true}\n"));
    const auto& ready = std::get<loupe::protocol::DrawingPreviewReady>(preview);
    QCOMPARE(ready.revision, 4);
    QCOMPARE(ready.previewJson, QByteArrayLiteral("{}"));
    // Carried rather than dropped, so the UI can label an approximate preview.
    QVERIFY(ready.approximate);
}

void ProtocolTest::malformedDrawingMessagesAreRefused()
{
    // A zero revision would make every reply look like the newest one, so it is refused
    // rather than accepted and reasoned about later.
    QVERIFY_THROWS_EXCEPTION(loupe::protocol::ProtocolError, loupe::protocol::decodeCommand(
        QByteArrayLiteral("{\"version\":{\"major\":2,\"minor\":1},\"type\":\"requestDrawingPreview\",\"requestId\":1,\"requestBase64\":\"e30=\",\"revision\":0}\n")));

    // A result with no drawing ID cannot be reconciled against a reviewed row at all.
    QVERIFY_THROWS_EXCEPTION(loupe::protocol::ProtocolError, loupe::protocol::decodeEvent(
        QByteArrayLiteral("{\"version\":{\"major\":2,\"minor\":1},\"type\":\"drawingRowResult\",\"requestId\":1,\"rowIndex\":0,\"drawingId\":\"\",\"path\":\"/out/a.dxf\",\"passed\":true}\n")));

    QVERIFY_THROWS_EXCEPTION(loupe::protocol::ProtocolError, loupe::protocol::decodeEvent(
        QByteArrayLiteral("{\"version\":{\"major\":2,\"minor\":1},\"type\":\"drawingProgress\",\"requestId\":1,\"rowIndex\":-1,\"rowCount\":1,\"stage\":\"x\",\"fraction\":0.0}\n")));

    QVERIFY_THROWS_EXCEPTION(loupe::protocol::ProtocolError, loupe::protocol::decodeCommand(
        QByteArrayLiteral("{\"version\":{\"major\":2,\"minor\":1},\"type\":\"executeDrawingPlan\",\"requestId\":1,\"planBase64\":\"e30=\",\"fingerprint\":\"\"}\n")));
}

QTEST_MAIN(ProtocolTest)

#include "test_protocol.moc"
