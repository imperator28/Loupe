#include <QtTest/QTest>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonObject>

#include "app/worker/WorkerClient.h"
#include "protocol/GeometryPayload.h"
#include "protocol/ProtocolFrame.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QRandomGenerator>

namespace {

QString serverName()
{
    return QStringLiteral("loupe-worker-client-test-%1").arg(QRandomGenerator::global()->generate());
}

QByteArray readControlFrame(QLocalSocket& socket)
{
    loupe::protocol::FrameDecoder decoder;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 3'000) {
        if (const auto frame = decoder.take()) {
            return frame->type == loupe::protocol::FrameType::ControlJson ? frame->payload : QByteArray{};
        }
        if (!socket.waitForReadyRead(100)) continue;
        decoder.append(socket.readAll());
    }
    return {};
}

void sendControlFrame(QLocalSocket& socket, const QByteArray& control)
{
    socket.write(loupe::protocol::encodeFrame(loupe::protocol::FrameType::ControlJson, control));
    socket.waitForBytesWritten(1'000);
}

void sendGeometryFrame(QLocalSocket& socket, const loupe::protocol::GeometryPayload& geometry)
{
    socket.write(loupe::protocol::encodeFrame(loupe::protocol::FrameType::Geometry, loupe::protocol::encodeGeometry(geometry)));
    socket.waitForBytesWritten(1'000);
}

} // namespace

class WorkerClientTest final : public QObject
{
    Q_OBJECT

private slots:
    void openFileDeliversWorkerSnapshot();
    void openFileSendsRequestedUnitOverride();
    void meshEventDeliversTriangulationPayload();
    void edgeEventDeliversCadCurvePayload();
};

void WorkerClientTest::openFileDeliversWorkerSnapshot()
{
    QLocalServer server;
    const auto name = serverName();
    QLocalServer::removeServer(name);
    QVERIFY(server.listen(name));

    loupe::app::worker::WorkerClient client;
    QSignalSpy snapshotSpy(&client, &loupe::app::worker::WorkerClient::snapshotReady);
    QVERIFY(client.connectToServer(name));
    QVERIFY(server.waitForNewConnection(3'000));
    auto* peer = server.nextPendingConnection();
    QVERIFY(peer != nullptr);

    sendControlFrame(*peer, "{\"version\":{\"major\":2,\"minor\":0},\"type\":\"ready\"}\n");
    const auto requestId = client.openFile(QStringLiteral("C:/models/assembly.step"));
    QCOMPARE(requestId, 1ULL);

    const auto command = readControlFrame(*peer);
    QVERIFY(command.contains("\"type\":\"openFile\""));
    QVERIFY(command.contains("C:/models/assembly.step"));

    sendControlFrame(*peer, "{\"version\":{\"major\":2,\"minor\":0},\"type\":\"snapshotReady\",\"requestId\":1,\"snapshotBase64\":\"eyJub2RlcyI6W119\"}\n");
    QTRY_COMPARE_WITH_TIMEOUT(snapshotSpy.count(), 1, 3'000);
    QCOMPARE(snapshotSpy.first().at(0).toULongLong(), 1ULL);
    QCOMPARE(snapshotSpy.first().at(1).toByteArray(), QByteArrayLiteral("{\"nodes\":[]}"));
}

void WorkerClientTest::openFileSendsRequestedUnitOverride()
{
    QLocalServer server;
    const auto name = serverName();
    QLocalServer::removeServer(name);
    QVERIFY(server.listen(name));

    loupe::app::worker::WorkerClient client;
    QVERIFY(client.connectToServer(name));
    QVERIFY(server.waitForNewConnection(3'000));
    auto* peer = server.nextPendingConnection();
    QVERIFY(peer != nullptr);

    static_cast<void>(client.openFile(QStringLiteral("C:/models/assembly.step"), QStringLiteral("in")));
    const auto command = QJsonDocument::fromJson(readControlFrame(*peer)).object();
    QCOMPARE(command.value(QStringLiteral("unitOverride")).toObject().value(QStringLiteral("unit")).toString(), QStringLiteral("in"));
    QCOMPARE(command.value(QStringLiteral("unitOverride")).toObject().value(QStringLiteral("customFactor")).toDouble(), 1.0);
}

void WorkerClientTest::meshEventDeliversTriangulationPayload()
{
    QLocalServer server;
    const auto name = serverName();
    QLocalServer::removeServer(name);
    QVERIFY(server.listen(name));

    loupe::app::worker::WorkerClient client;
    QSignalSpy meshSpy(&client, &loupe::app::worker::WorkerClient::meshReady);
    QVERIFY(client.connectToServer(name));
    QVERIFY(server.waitForNewConnection(3'000));
    auto* peer = server.nextPendingConnection();
    QVERIFY(peer != nullptr);

    sendGeometryFrame(*peer, loupe::protocol::MeshPayload{1, 0, QStringLiteral("body-1"), QStringLiteral("body-1"), QStringLiteral("body-1"), QStringLiteral("#67D5C0"), 0,
                                                           {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F},
                                                           {0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F}, {0, 1, 2}});
    QTRY_COMPARE_WITH_TIMEOUT(meshSpy.count(), 1, 3'000);
    QCOMPARE(meshSpy.first().at(0).toULongLong(), 1ULL);
    QCOMPARE(meshSpy.first().at(1).toString(), QStringLiteral("body-1"));
    QCOMPARE(meshSpy.first().at(2).toString(), QStringLiteral("body-1"));
    QCOMPARE(meshSpy.first().at(3).toString(), QStringLiteral("#67D5C0"));
    QVERIFY(!meshSpy.first().at(4).toByteArray().isEmpty());
}

void WorkerClientTest::edgeEventDeliversCadCurvePayload()
{
    QLocalServer server;
    const auto name = serverName();
    QLocalServer::removeServer(name);
    QVERIFY(server.listen(name));

    loupe::app::worker::WorkerClient client;
    QSignalSpy edgeSpy(&client, &loupe::app::worker::WorkerClient::edgeReady);
    QVERIFY(client.connectToServer(name));
    QVERIFY(server.waitForNewConnection(3'000));
    auto* peer = server.nextPendingConnection();
    QVERIFY(peer != nullptr);

    sendGeometryFrame(*peer, loupe::protocol::EdgePayload{1, 0, QStringLiteral("body-1"), QStringLiteral("body-1"), 0,
                                                           {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F}, {0, 1}});
    QTRY_COMPARE_WITH_TIMEOUT(edgeSpy.count(), 1, 3'000);
    QCOMPARE(edgeSpy.first().at(0).toULongLong(), 1ULL);
    QCOMPARE(edgeSpy.first().at(1).toString(), QStringLiteral("body-1"));
    QVERIFY(!edgeSpy.first().at(2).toByteArray().isEmpty());
}

QTEST_MAIN(WorkerClientTest)

#include "test_worker_client.moc"
