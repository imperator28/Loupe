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

QTEST_MAIN(ProtocolTest)

#include "test_protocol.moc"
