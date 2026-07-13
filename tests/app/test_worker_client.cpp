#include <QtTest/QTest>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonObject>

#include "app/worker/WorkerClient.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QRandomGenerator>

namespace {

QString serverName()
{
    return QStringLiteral("loupe-worker-client-test-%1").arg(QRandomGenerator::global()->generate());
}

QByteArray readLine(QLocalSocket& socket)
{
    if (!socket.canReadLine()) {
        socket.waitForReadyRead(3'000);
    }
    return socket.readLine();
}

} // namespace

class WorkerClientTest final : public QObject
{
    Q_OBJECT

private slots:
    void openFileDeliversWorkerSnapshot();
    void openFileSendsRequestedUnitOverride();
    void meshEventDeliversTriangulationPayload();
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

    peer->write("{\"version\":{\"major\":1,\"minor\":0},\"type\":\"ready\"}\n");
    QVERIFY(peer->waitForBytesWritten(1'000));
    const auto requestId = client.openFile(QStringLiteral("C:/models/assembly.step"));
    QCOMPARE(requestId, 1ULL);

    const auto command = readLine(*peer);
    QVERIFY(command.contains("\"type\":\"openFile\""));
    QVERIFY(command.contains("C:/models/assembly.step"));

    peer->write("{\"version\":{\"major\":1,\"minor\":0},\"type\":\"snapshotReady\",\"requestId\":1,\"snapshotBase64\":\"eyJub2RlcyI6W119\"}\n");
    QVERIFY(peer->waitForBytesWritten(1'000));
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
    const auto command = QJsonDocument::fromJson(readLine(*peer)).object();
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

    peer->write("{\"version\":{\"major\":1,\"minor\":0},\"type\":\"meshReady\",\"requestId\":1,\"definitionId\":\"body-1\",\"refinement\":0,\"segmentKey\":\"body-1\",\"meshBase64\":\"e30=\"}\n");
    QVERIFY(peer->waitForBytesWritten(1'000));
    QTRY_COMPARE_WITH_TIMEOUT(meshSpy.count(), 1, 3'000);
    QCOMPARE(meshSpy.first().at(0).toULongLong(), 1ULL);
    QCOMPARE(meshSpy.first().at(1).toString(), QStringLiteral("body-1"));
    QCOMPARE(meshSpy.first().at(2).toByteArray(), QByteArrayLiteral("{}"));
}

QTEST_MAIN(WorkerClientTest)

#include "test_worker_client.moc"
