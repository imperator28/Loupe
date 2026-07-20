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

QTEST_MAIN(ProtocolTest)

#include "test_protocol.moc"
