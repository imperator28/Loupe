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
    return QStringLiteral("loupe-worker-test-%1").arg(QCoreApplication::applicationPid());
}

QJsonObject readEvent(QLocalSocket& socket)
{
    if (!socket.waitForReadyRead(3'000)) {
        return {};
    }
    const auto line = socket.readLine();
    const auto document = QJsonDocument::fromJson(line);
    return document.isObject() ? document.object() : QJsonObject{};
}

void send(QLocalSocket& socket, const QJsonObject& object)
{
    socket.write(QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n');
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
    QCOMPARE(readEvent(socket).value(QStringLiteral("type")).toString(), QStringLiteral("ready"));

    send(socket, {{QStringLiteral("version"), QJsonObject{{QStringLiteral("major"), 1}, {QStringLiteral("minor"), 0}}},
                  {QStringLiteral("type"), QStringLiteral("openFile")}, {QStringLiteral("requestId"), 7},
                  {QStringLiteral("path"), fixture.fileName()}});
    send(socket, {{QStringLiteral("version"), QJsonObject{{QStringLiteral("major"), 1}, {QStringLiteral("minor"), 0}}},
                  {QStringLiteral("type"), QStringLiteral("cancel")}, {QStringLiteral("requestId"), 7}});

    QCOMPARE(readEvent(socket).value(QStringLiteral("type")).toString(), QStringLiteral("canceled"));
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
    readEvent(socket);
    send(socket, {{QStringLiteral("version"), QJsonObject{{QStringLiteral("major"), 1}, {QStringLiteral("minor"), 0}}},
                  {QStringLiteral("type"), QStringLiteral("openFile")}, {QStringLiteral("requestId"), 8},
                  {QStringLiteral("path"), QStringLiteral("missing.step")}});

    const auto event = readEvent(socket);
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
    readEvent(socket);
    send(socket, {{QStringLiteral("version"), QJsonObject{{QStringLiteral("major"), 1}, {QStringLiteral("minor"), 0}}},
                  {QStringLiteral("type"), QStringLiteral("openFile")}, {QStringLiteral("requestId"), 9},
                  {QStringLiteral("path"), QString::fromStdString(fixture.string())}});

    const auto event = readEvent(socket);
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
    QVERIFY(geometry.first().toObject().value(QStringLiteral("volumeMm3")).toDouble() > 0.0);
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
    readEvent(socket);
    send(socket, {{QStringLiteral("version"), QJsonObject{{QStringLiteral("major"), 1}, {QStringLiteral("minor"), 0}}},
                  {QStringLiteral("type"), QStringLiteral("openFile")}, {QStringLiteral("requestId"), 10},
                  {QStringLiteral("path"), QString::fromStdString(fixture.string())}});

    QCOMPARE(readEvent(socket).value(QStringLiteral("type")).toString(), QStringLiteral("snapshotReady"));
    const auto meshEvent = readEvent(socket);
    QCOMPARE(meshEvent.value(QStringLiteral("type")).toString(), QStringLiteral("meshReady"));
    const auto mesh = QJsonDocument::fromJson(QByteArray::fromBase64(meshEvent.value(QStringLiteral("meshBase64")).toString().toLatin1())).object();
    QVERIFY(mesh.value(QStringLiteral("vertices")).toArray().size() >= 9);
    QVERIFY(mesh.value(QStringLiteral("indices")).toArray().size() >= 3);
    QVERIFY(worker.state() == QProcess::Running);
}

QTEST_MAIN(WorkerProcessTest)

#include "test_worker_process.moc"
