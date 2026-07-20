#include <QtTest/QTest>

#include "protocol/GeometryPayload.h"
#include "protocol/ProtocolTypes.h"

class GeometryPayloadTest final : public QObject
{
    Q_OBJECT

private slots:
    void meshRoundTrips();
    void edgeRoundTrips();
    void rejectsOutOfRangeIndices();
    void rejectsTruncatedPayload();
    void rejectsOverlappingTopologyRanges();
};

void GeometryPayloadTest::meshRoundTrips()
{
    loupe::protocol::MeshPayload source{7, 3, QStringLiteral("definition"), QStringLiteral("node"), QStringLiteral("segment"), QStringLiteral("#2288CC"), 1,
                                        {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F},
                                        {0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F},
                                        {0, 1, 2},
                                        {{3, loupe::protocol::TopologyKind::Face, 0, 3, 0.5F, 0.0F}}};
    const auto decoded = loupe::protocol::decodeGeometry(loupe::protocol::encodeGeometry(source));
    const auto& mesh = std::get<loupe::protocol::MeshPayload>(decoded);
    QCOMPARE(mesh.requestId, 7ULL);
    QCOMPARE(mesh.documentGeneration, 3ULL);
    QCOMPARE(mesh.segmentKey, QStringLiteral("segment"));
    QCOMPARE(mesh.sourceColor, QStringLiteral("#2288CC"));
    QCOMPARE(mesh.vertices.size(), 9);
    QCOMPARE(mesh.indices, QVector<quint32>({0, 1, 2}));
    QCOMPARE(mesh.topology.size(), 1);
    QCOMPARE(mesh.topology.front().topologyId, 3U);
}

void GeometryPayloadTest::edgeRoundTrips()
{
    loupe::protocol::EdgePayload source{8, 4, QStringLiteral("definition"), QStringLiteral("node"), 0,
                                        {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F}, {0, 1},
                                        {{4, loupe::protocol::TopologyKind::Edge, 0, 2, 1.0F, 0.0F}}};
    const auto decoded = loupe::protocol::decodeGeometry(loupe::protocol::encodeGeometry(source));
    const auto& edges = std::get<loupe::protocol::EdgePayload>(decoded);
    QCOMPARE(edges.requestId, 8ULL);
    QCOMPARE(edges.nodeId, QStringLiteral("node"));
    QCOMPARE(edges.vertices.size(), 6);
    QCOMPARE(edges.indices, QVector<quint32>({0, 1}));
}

void GeometryPayloadTest::rejectsOutOfRangeIndices()
{
    loupe::protocol::EdgePayload source{1, 1, {}, QStringLiteral("node"), 0,
                                        {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F}, {0, 2}, {}};
    QVERIFY_THROWS_EXCEPTION(loupe::protocol::ProtocolError, static_cast<void>(loupe::protocol::encodeGeometry(source)));
}

void GeometryPayloadTest::rejectsTruncatedPayload()
{
    loupe::protocol::EdgePayload source{1, 1, {}, QStringLiteral("node"), 0,
                                        {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F}, {0, 1}, {}};
    const auto bytes = loupe::protocol::encodeGeometry(source);
    QVERIFY_THROWS_EXCEPTION(loupe::protocol::ProtocolError, static_cast<void>(loupe::protocol::decodeGeometry(bytes.left(bytes.size() - 1))));
}

void GeometryPayloadTest::rejectsOverlappingTopologyRanges()
{
    loupe::protocol::MeshPayload source{1, 1, {}, QStringLiteral("node"), QStringLiteral("segment"),
                                        QStringLiteral("#ffffff"), 0,
                                        {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F},
                                        {0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F},
                                        {0, 1, 2},
                                        {{1, loupe::protocol::TopologyKind::Face, 0, 3, 0.5F, 0.0F},
                                         {2, loupe::protocol::TopologyKind::Face, 2, 1, 0.5F, 0.0F}}};
    QVERIFY_THROWS_EXCEPTION(loupe::protocol::ProtocolError,
                             static_cast<void>(loupe::protocol::encodeGeometry(source)));
}

QTEST_MAIN(GeometryPayloadTest)

#include "test_geometry_payload.moc"
