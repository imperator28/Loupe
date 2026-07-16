#include <QtTest/QTest>

#include <QFile>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QProcess>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryFile>

#include "fixtures/FixtureFactory.h"
#include "protocol/GeometryPayload.h"
#include "protocol/ProtocolFrame.h"

#include <algorithm>

class WorkerProcessTest final : public QObject
{
    Q_OBJECT

private slots:
    void readyThenCancel();
    void invalidFileDoesNotCrashWorker();
    void validStepProducesNonEmptySnapshot();
    void validStepStreamsTriangulatedMesh();
};

namespace {

QString serverName()
{
    static quint64 sequence{};
    return QStringLiteral("loupe-worker-test-%1-%2").arg(QCoreApplication::applicationPid()).arg(++sequence);
}

std::optional<loupe::protocol::Frame> readFrame(QLocalSocket& socket, loupe::protocol::FrameDecoder& decoder)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 10'000) {
        if (const auto frame = decoder.take()) return frame;
        if (!socket.waitForReadyRead(100)) continue;
        decoder.append(socket.readAll());
    }
    return std::nullopt;
}

QJsonObject readEvent(QLocalSocket& socket, loupe::protocol::FrameDecoder& decoder)
{
    const auto frame = readFrame(socket, decoder);
    if (!frame || frame->type != loupe::protocol::FrameType::ControlJson) return {};
    const auto document = QJsonDocument::fromJson(frame->payload);
    return document.isObject() ? document.object() : QJsonObject{};
}

QJsonObject readEventOfType(QLocalSocket& socket, loupe::protocol::FrameDecoder& decoder, const QString& type)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 10'000) {
        const auto event = readEvent(socket, decoder);
        if (event.value(QStringLiteral("type")).toString() == type) return event;
    }
    return {};
}

void send(QLocalSocket& socket, const QJsonObject& object)
{
    socket.write(loupe::protocol::encodeFrame(loupe::protocol::FrameType::ControlJson,
                                              QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n'));
    socket.waitForBytesWritten(1'000);
}

bool connectToWorker(QLocalSocket& socket, const QString& name)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 3'000) {
        socket.connectToServer(name);
        if (socket.waitForConnected(100)) {
            return true;
        }
        socket.abort();
        QTest::qWait(20);
    }
    return false;
}

} // namespace

void WorkerProcessTest::readyThenCancel()
{
    QTemporaryFile fixture;
    QVERIFY(fixture.open());
    fixture.write("fixture");
    fixture.flush();

    QProcess worker;
    const auto name = serverName();
    worker.start(QStringLiteral(LOUPE_WORKER_PATH), {QStringLiteral("--server-name"), name});
    QVERIFY(worker.waitForStarted(3'000));

    QLocalSocket socket;
    QVERIFY(connectToWorker(socket, name));
    loupe::protocol::FrameDecoder decoder;
    QCOMPARE(readEvent(socket, decoder).value(QStringLiteral("type")).toString(), QStringLiteral("ready"));

    send(socket, {{QStringLiteral("version"), QJsonObject{{QStringLiteral("major"), 2}, {QStringLiteral("minor"), 0}}},
                  {QStringLiteral("type"), QStringLiteral("openFile")}, {QStringLiteral("requestId"), 7},
                  {QStringLiteral("path"), fixture.fileName()}});
    send(socket, {{QStringLiteral("version"), QJsonObject{{QStringLiteral("major"), 2}, {QStringLiteral("minor"), 0}}},
                  {QStringLiteral("type"), QStringLiteral("cancel")}, {QStringLiteral("requestId"), 7}});

    QCOMPARE(readEvent(socket, decoder).value(QStringLiteral("type")).toString(), QStringLiteral("canceled"));
    QVERIFY(worker.state() == QProcess::Running);
}

void WorkerProcessTest::invalidFileDoesNotCrashWorker()
{
    QProcess worker;
    const auto name = serverName();
    worker.start(QStringLiteral(LOUPE_WORKER_PATH), {QStringLiteral("--server-name"), name});
    QVERIFY(worker.waitForStarted(3'000));

    QLocalSocket socket;
    QVERIFY(connectToWorker(socket, name));
    loupe::protocol::FrameDecoder decoder;
    readEvent(socket, decoder);
    send(socket, {{QStringLiteral("version"), QJsonObject{{QStringLiteral("major"), 2}, {QStringLiteral("minor"), 0}}},
                  {QStringLiteral("type"), QStringLiteral("openFile")}, {QStringLiteral("requestId"), 8},
                  {QStringLiteral("path"), QStringLiteral("missing.step")}});

    const auto event = readEvent(socket, decoder);
    QCOMPARE(event.value(QStringLiteral("type")).toString(), QStringLiteral("failed"));
    QCOMPARE(event.value(QStringLiteral("code")).toString(), QStringLiteral("read_failed"));
    QVERIFY(worker.state() == QProcess::Running);
}

void WorkerProcessTest::validStepProducesNonEmptySnapshot()
{
    const auto fixture = loupe::tests::writeRepeatedBoxAssembly(QStringLiteral("worker-repeated.step").toStdString(), loupe::tests::FixtureUnit::Millimeter);
    QProcess worker;
    const auto name = serverName();
    worker.start(QStringLiteral(LOUPE_WORKER_PATH), {QStringLiteral("--server-name"), name});
    QVERIFY(worker.waitForStarted(3'000));

    QLocalSocket socket;
    QVERIFY(connectToWorker(socket, name));
    loupe::protocol::FrameDecoder decoder;
    readEvent(socket, decoder);
    send(socket, {{QStringLiteral("version"), QJsonObject{{QStringLiteral("major"), 2}, {QStringLiteral("minor"), 0}}},
                  {QStringLiteral("type"), QStringLiteral("openFile")}, {QStringLiteral("requestId"), 9},
                  {QStringLiteral("path"), QString::fromStdString(fixture.string())}});

    const auto event = readEventOfType(socket, decoder, QStringLiteral("snapshotReady"));
    QCOMPARE(event.value(QStringLiteral("type")).toString(), QStringLiteral("snapshotReady"));
    const auto snapshot = QByteArray::fromBase64(event.value(QStringLiteral("snapshotBase64")).toString().toLatin1());
    QVERIFY(!snapshot.isEmpty());
    const auto document = QJsonDocument::fromJson(snapshot).object();
    const auto nodes = document.value(QStringLiteral("nodes")).toArray();
    QVERIFY(std::any_of(nodes.cbegin(), nodes.cend(), [](const QJsonValue& value) {
        return !value.toObject().value(QStringLiteral("parentId")).toString().isEmpty();
    }));
    const auto geometry = document.value(QStringLiteral("geometry")).toArray();
    QVERIFY(!geometry.isEmpty());
    QVERIFY(!geometry.first().toObject().value(QStringLiteral("nodeId")).toString().isEmpty());
    const auto metadata = readEventOfType(socket, decoder, QStringLiteral("componentMetadata"));
    QVERIFY(metadata.value(QStringLiteral("volumeMm3")).toDouble() > 0.0);
    const auto metrics = readEventOfType(socket, decoder, QStringLiteral("importMetrics"));
    QCOMPARE(metrics.value(QStringLiteral("sourceName")).toString(), QString::fromStdString(fixture.filename().string()));
    QVERIFY(!metrics.value(QStringLiteral("sourceHash")).toString().isEmpty());
    QVERIFY(metrics.value(QStringLiteral("stepReadMs")).toInteger() >= 0);
    QVERIFY(metrics.value(QStringLiteral("xcafTransferMs")).toInteger() >= 0);
    QVERIFY(metrics.value(QStringLiteral("snapshotBuildMs")).toInteger() >= 0);
    QVERIFY(metrics.value(QStringLiteral("treeReadyMs")).toInteger() >= 0);
    QVERIFY(metrics.value(QStringLiteral("firstGeometryMs")).toInteger() >= metrics.value(QStringLiteral("treeReadyMs")).toInteger());
    QVERIFY(metrics.value(QStringLiteral("previewReadyMs")).toInteger() >= metrics.value(QStringLiteral("firstGeometryMs")).toInteger());
    QVERIFY(metrics.value(QStringLiteral("finalReadyMs")).toInteger() >= metrics.value(QStringLiteral("previewReadyMs")).toInteger());
    QVERIFY(metrics.value(QStringLiteral("previewTriangleCount")).toInteger() > 0);
    QVERIFY(metrics.value(QStringLiteral("refinedTriangleCount")).toInteger() > 0);
    QVERIFY(metrics.value(QStringLiteral("bodyCount")).toInt() > 0);
    QVERIFY(worker.state() == QProcess::Running);
}

void WorkerProcessTest::validStepStreamsTriangulatedMesh()
{
    const auto fixture = loupe::tests::writeRepeatedBoxAssembly(QStringLiteral("worker-mesh.step").toStdString(), loupe::tests::FixtureUnit::Millimeter);
    QProcess worker;
    const auto name = serverName();
    worker.start(QStringLiteral(LOUPE_WORKER_PATH), {QStringLiteral("--server-name"), name});
    QVERIFY(worker.waitForStarted(3'000));

    QLocalSocket socket;
    QVERIFY(connectToWorker(socket, name));
    loupe::protocol::FrameDecoder decoder;
    readEvent(socket, decoder);
    send(socket, {{QStringLiteral("version"), QJsonObject{{QStringLiteral("major"), 2}, {QStringLiteral("minor"), 0}}},
                  {QStringLiteral("type"), QStringLiteral("openFile")}, {QStringLiteral("requestId"), 10},
                  {QStringLiteral("path"), QString::fromStdString(fixture.string())}});

    QVERIFY(!readEventOfType(socket, decoder, QStringLiteral("snapshotReady")).isEmpty());
    std::optional<loupe::protocol::MeshPayload> mesh;
    std::optional<loupe::protocol::EdgePayload> edges;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 10'000 && (!mesh || !edges)) {
        const auto frame = readFrame(socket, decoder);
        if (!frame || frame->type != loupe::protocol::FrameType::Geometry) continue;
        const auto geometry = loupe::protocol::decodeGeometry(frame->payload);
        if (const auto* value = std::get_if<loupe::protocol::MeshPayload>(&geometry)) mesh = *value;
        if (const auto* value = std::get_if<loupe::protocol::EdgePayload>(&geometry)) edges = *value;
    }
    QVERIFY(mesh.has_value());
    QVERIFY(mesh->vertices.size() >= 9);
    QCOMPARE(mesh->normals.size(), mesh->vertices.size());
    QVERIFY(mesh->indices.size() >= 3);
    QVERIFY(edges.has_value());
    QVERIFY(edges->vertices.size() >= 6);
    QVERIFY(edges->indices.size() >= 2);
    QVERIFY(worker.state() == QProcess::Running);
}

QTEST_MAIN(WorkerProcessTest)

#include "test_worker_process.moc"
