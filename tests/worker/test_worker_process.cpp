#include <QtTest/QTest>

#include <QFile>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QProcess>
#include <QHash>
#include <QSet>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryFile>
#include <QTemporaryDir>

#include "core/drawing/DrawingPlan.h"
#include "core/export/ExportPlan.h"
#include "core/validation/OutputValidator.h"
#include "fixtures/FixtureFactory.h"
#include "protocol/GeometryPayload.h"
#include "protocol/ProtocolFrame.h"

#include <algorithm>
#include <filesystem>
#include <utility>

class WorkerProcessTest final : public QObject
{
    Q_OBJECT

private slots:
    void disconnectStopsWorker();
    void readyThenCancel();
    void invalidFileDoesNotCrashWorker();
    void validStepProducesNonEmptySnapshot();
    void anImportStartedAtLaunchIsHeldUntilTheAppConnects();
    void validStepStreamsTriangulatedMesh();
    void aRoundFaceCarriesItsExactAxis();
    void everyCopyAndEverySolidGetsItsMassPropertiesBeforeReady();
    void copiesOfAPartShareOneMeshAndArePlacedApart();
    void reviewedPlanWritesAndValidatesStep();
    void reviewedDrawingPlanWritesAndValidatesDxf();
    void aChangedDrawingPlanIsRefusedAtTheFingerprint();
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
    // Generous, since it returns as soon as the worker listens: the first launch under Rosetta
    // (the Intel build, tested on an Apple Silicon runner) translates the whole binary first,
    // which can take longer than a few seconds.
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 30'000) {
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

void WorkerProcessTest::disconnectStopsWorker()
{
    QProcess worker;
    const auto name = serverName();
    worker.start(QStringLiteral(LOUPE_WORKER_PATH), {QStringLiteral("--server-name"), name});
    QVERIFY(worker.waitForStarted(3'000));

    QLocalSocket socket;
    QVERIFY(connectToWorker(socket, name));
    loupe::protocol::FrameDecoder decoder;
    QCOMPARE(readEvent(socket, decoder).value(QStringLiteral("type")).toString(), QStringLiteral("ready"));

    socket.disconnectFromServer();
    if (socket.state() != QLocalSocket::UnconnectedState) QVERIFY(socket.waitForDisconnected(1'000));
    QVERIFY(worker.waitForFinished(3'000));
    QCOMPARE(worker.exitStatus(), QProcess::NormalExit);
}

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
    // The allocator malloc goes to: a build linking mimalloc whose override did not take
    // (a DLL loaded too late, a static link keeping the system malloc) fails here.
    const auto allocator = metrics.value(QStringLiteral("allocator")).toString();
#ifdef LOUPE_EXPECT_WORKER_MIMALLOC
    // Except on a Windows machine where a module from outside Windows, such as an endpoint-security
    // agent's, got into the worker ahead of mimalloc's redirect: no import order of the worker's
    // overcomes that, so it says nothing about the build. CI's runners have no such module.
    const auto blockedPrefix = QStringLiteral("system (mimalloc not in effect; loaded before it: ");
    if (allocator.startsWith(blockedPrefix)) {
        QSKIP(qPrintable(QStringLiteral("mimalloc's override is blocked on this machine by %1, which loads before the "
                                        "worker's imports; the worker runs on the system allocator here")
                             .arg(allocator.mid(blockedPrefix.size()).chopped(1))));
    }
    QCOMPARE(allocator, QStringLiteral("mimalloc"));
#else
    QCOMPARE(allocator, QStringLiteral("system"));
#endif
}

// The app hands the worker the file it was launched with on the command line, so the import
// runs while the window is still being built (app/worker/WorkerLaunch.h). Everything it sends
// before the app connects is kept and delivered, in order, after "ready".
void WorkerProcessTest::anImportStartedAtLaunchIsHeldUntilTheAppConnects()
{
    const auto fixture = loupe::tests::writeRepeatedBoxAssembly(QStringLiteral("worker-prelaunch.step").toStdString(), loupe::tests::FixtureUnit::Millimeter);
    QProcess worker;
    const auto name = serverName();
    worker.start(QStringLiteral(LOUPE_WORKER_PATH), {QStringLiteral("--server-name"), name,
                                                     QStringLiteral("--open"), QString::fromStdString(fixture.string()),
                                                     QStringLiteral("--request-id"), QStringLiteral("7")});
    QVERIFY(worker.waitForStarted(3'000));
    // Long enough for this small file to be read and meshed with nobody listening.
    QTest::qWait(2'000);

    QLocalSocket socket;
    QVERIFY(connectToWorker(socket, name));
    loupe::protocol::FrameDecoder decoder;
    QCOMPARE(readEvent(socket, decoder).value(QStringLiteral("type")).toString(), QStringLiteral("ready"));

    // Nothing is sent: the import was asked for on the command line.
    bool sawFirstProgress = false;
    bool sawSnapshot = false;
    int geometryFrames = 0;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 20'000) {
        const auto frame = readFrame(socket, decoder);
        QVERIFY(frame.has_value());
        if (frame->type == loupe::protocol::FrameType::Geometry) {
            QVERIFY(sawSnapshot);
            ++geometryFrames;
            continue;
        }
        const auto event = QJsonDocument::fromJson(frame->payload).object();
        const auto type = event.value(QStringLiteral("type")).toString();
        QVERIFY2(type != QStringLiteral("failed"), qPrintable(event.value(QStringLiteral("message")).toString()));
        if (type == QStringLiteral("progress") || type == QStringLiteral("snapshotReady")) {
            QCOMPARE(event.value(QStringLiteral("requestId")).toInteger(), 7);
        }
        if (type == QStringLiteral("progress") && !sawFirstProgress) {
            // Held from the very start of the import, not just what came after connecting.
            QCOMPARE(event.value(QStringLiteral("stage")).toString(), QStringLiteral("Reading STEP file"));
            sawFirstProgress = true;
        }
        if (type == QStringLiteral("snapshotReady")) sawSnapshot = true;
        if (type == QStringLiteral("progress") && event.value(QStringLiteral("fraction")).toDouble() >= 1.0) break;
    }
    QVERIFY(sawFirstProgress);
    QVERIFY(sawSnapshot);
    QVERIFY(geometryFrames > 0);
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

void WorkerProcessTest::aRoundFaceCarriesItsExactAxis()
{
    // A cylinder of radius 5 along Z: its round face's axis is OCCT's, and its flat ends have none.
    const auto fixture = loupe::tests::writeSingleCylinderStep(QStringLiteral("worker-cylinder.step").toStdString());
    QProcess worker;
    const auto name = serverName();
    worker.start(QStringLiteral(LOUPE_WORKER_PATH), {QStringLiteral("--server-name"), name});
    QVERIFY(worker.waitForStarted(3'000));
    QLocalSocket socket;
    QVERIFY(connectToWorker(socket, name));
    loupe::protocol::FrameDecoder decoder;
    readEvent(socket, decoder);
    send(socket, {{QStringLiteral("version"), QJsonObject{{QStringLiteral("major"), 2}, {QStringLiteral("minor"), 0}}},
                  {QStringLiteral("type"), QStringLiteral("openFile")}, {QStringLiteral("requestId"), 11},
                  {QStringLiteral("path"), QString::fromStdString(fixture.string())}});
    QVERIFY(!readEventOfType(socket, decoder, QStringLiteral("snapshotReady")).isEmpty());
    QVector<loupe::protocol::TopologyRange> faces;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 10'000 && faces.isEmpty()) {
        const auto frame = readFrame(socket, decoder);
        if (!frame || frame->type != loupe::protocol::FrameType::Geometry) continue;
        const auto geometry = loupe::protocol::decodeGeometry(frame->payload);
        if (const auto* mesh = std::get_if<loupe::protocol::MeshPayload>(&geometry)) faces = mesh->topology;
    }
    QCOMPARE(faces.size(), 3);
    int round = 0;
    for (const auto& face : faces) {
        if (!face.hasAxis()) {
            QCOMPARE(face.radiusMm, 0.0F);
            continue;
        }
        ++round;
        QVERIFY(std::abs(face.radiusMm - 5.0F) < 1e-4F);
        QVERIFY(std::abs(std::abs(face.axisDirection[2]) - 1.0F) < 1e-6F);
        QVERIFY(std::abs(face.axisOrigin[0]) < 1e-4F && std::abs(face.axisOrigin[1]) < 1e-4F);
    }
    QCOMPARE(round, 1);
}

namespace {

// Opens a file and returns each body's componentMetadata, keyed by node id, as they stood when the
// worker reported Ready, plus the body node ids the snapshot listed. Empty on a timeout.
std::pair<QHash<QString, QJsonObject>, QStringList> massPropertiesAtReady(const std::filesystem::path& fixture, const int requestId)
{
    QProcess worker;
    const auto name = serverName();
    worker.start(QStringLiteral(LOUPE_WORKER_PATH), {QStringLiteral("--server-name"), name});
    if (!worker.waitForStarted(3'000)) return {};
    QLocalSocket socket;
    if (!connectToWorker(socket, name)) return {};
    loupe::protocol::FrameDecoder decoder;
    readEvent(socket, decoder);
    send(socket, {{QStringLiteral("version"), QJsonObject{{QStringLiteral("major"), 2}, {QStringLiteral("minor"), 0}}},
                  {QStringLiteral("type"), QStringLiteral("openFile")}, {QStringLiteral("requestId"), requestId},
                  {QStringLiteral("path"), QString::fromStdString(fixture.string())}});
    QHash<QString, QJsonObject> metadata;
    QStringList bodies;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 20'000) {
        const auto frame = readFrame(socket, decoder);
        if (!frame || frame->type != loupe::protocol::FrameType::ControlJson) continue;
        const auto event = QJsonDocument::fromJson(frame->payload).object();
        const auto type = event.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("snapshotReady")) {
            const auto snapshot = QJsonDocument::fromJson(
                QByteArray::fromBase64(event.value(QStringLiteral("snapshotBase64")).toString().toLatin1())).object();
            for (const auto& entry : snapshot.value(QStringLiteral("geometry")).toArray())
                bodies.append(entry.toObject().value(QStringLiteral("nodeId")).toString());
        } else if (type == QStringLiteral("componentMetadata")) {
            metadata.insert(event.value(QStringLiteral("nodeId")).toString(), event);
        } else if (type == QStringLiteral("progress") && event.value(QStringLiteral("stage")).toString() == QStringLiteral("Ready")) {
            return {metadata, bodies};
        } else if (type == QStringLiteral("failed")) {
            return {};
        }
    }
    return {};
}

} // namespace

void WorkerProcessTest::everyCopyAndEverySolidGetsItsMassPropertiesBeforeReady()
{
    // Mass properties are worked out once per distinct shape and shared by its copies, so both
    // copies of the repeated box must still get their own, equal, ones -- and the two different
    // solids of one multi-solid part their own, different, ones.
    const auto [copies, copyBodies] = massPropertiesAtReady(
        loupe::tests::writeRepeatedBoxAssembly(QStringLiteral("worker-mass-copies.step").toStdString(), loupe::tests::FixtureUnit::Millimeter), 30);
    QCOMPARE(copyBodies.size(), 2);
    for (const auto& nodeId : copyBodies) {
        QVERIFY2(copies.contains(nodeId), qPrintable(nodeId));
        QVERIFY(qAbs(copies.value(nodeId).value(QStringLiteral("volumeMm3")).toDouble() - 1000.0) < 1.0e-6);
        QVERIFY(qAbs(copies.value(nodeId).value(QStringLiteral("surfaceAreaMm2")).toDouble() - 600.0) < 1.0e-6);
        QCOMPARE(copies.value(nodeId).value(QStringLiteral("planarFaceCount")).toInt(), 6);
    }

    const auto [solids, solidBodies] = massPropertiesAtReady(
        loupe::tests::writeMultiSolidBodyStep(QStringLiteral("worker-mass-solids.step").toStdString()), 31);
    QCOMPARE(solidBodies.size(), 2);
    QList<double> volumes;
    for (const auto& nodeId : solidBodies) {
        QVERIFY2(solids.contains(nodeId), qPrintable(nodeId));
        volumes.append(solids.value(nodeId).value(QStringLiteral("volumeMm3")).toDouble());
    }
    std::sort(volumes.begin(), volumes.end());
    QVERIFY(qAbs(volumes.at(0) - 216.0) < 1.0e-6);
    QVERIFY(qAbs(volumes.at(1) - 1000.0) < 1.0e-6);
}

void WorkerProcessTest::copiesOfAPartShareOneMeshAndArePlacedApart()
{
    // Two copies of one box: each pass sends the box's mesh and edges once, in the box's own
    // coordinates, then places them for each copy -- and sends no mesh of its own for either.
    const auto fixture = loupe::tests::writeRepeatedBoxAssembly(QStringLiteral("worker-instances.step").toStdString(), loupe::tests::FixtureUnit::Millimeter);
    QProcess worker;
    const auto name = serverName();
    worker.start(QStringLiteral(LOUPE_WORKER_PATH), {QStringLiteral("--server-name"), name});
    QVERIFY(worker.waitForStarted(3'000));
    QLocalSocket socket;
    QVERIFY(connectToWorker(socket, name));
    loupe::protocol::FrameDecoder decoder;
    readEvent(socket, decoder);
    send(socket, {{QStringLiteral("version"), QJsonObject{{QStringLiteral("major"), 2}, {QStringLiteral("minor"), 0}}},
                  {QStringLiteral("type"), QStringLiteral("openFile")}, {QStringLiteral("requestId"), 32},
                  {QStringLiteral("path"), QString::fromStdString(fixture.string())}});
    int sharedMeshes = 0;
    int sharedEdges = 0;
    int ownMeshes = 0;
    QSet<QString> sharedKeys;
    QList<loupe::protocol::InstancePayload> meshInstances;
    int edgeInstances = 0;
    QElapsedTimer timer;
    timer.start();
    bool ready = false;
    while (!ready && timer.elapsed() < 20'000) {
        const auto frame = readFrame(socket, decoder);
        if (!frame) continue;
        if (frame->type == loupe::protocol::FrameType::Geometry) {
            const auto geometry = loupe::protocol::decodeGeometry(frame->payload);
            if (const auto* mesh = std::get_if<loupe::protocol::MeshPayload>(&geometry)) {
                if (mesh->shared) {
                    ++sharedMeshes;
                    sharedKeys.insert(mesh->segmentKey);
                } else {
                    ++ownMeshes;
                }
            } else if (const auto* edges = std::get_if<loupe::protocol::EdgePayload>(&geometry)) {
                if (edges->shared) ++sharedEdges;
            } else if (const auto* instance = std::get_if<loupe::protocol::InstancePayload>(&geometry)) {
                if (instance->edges) ++edgeInstances;
                else meshInstances.append(*instance);
            }
            continue;
        }
        const auto event = QJsonDocument::fromJson(frame->payload).object();
        ready = event.value(QStringLiteral("type")).toString() == QStringLiteral("progress")
            && event.value(QStringLiteral("stage")).toString() == QStringLiteral("Ready");
    }
    QVERIFY(ready);
    QCOMPARE(ownMeshes, 0);
    QCOMPARE(sharedMeshes, 2);   // preview and refined
    QCOMPARE(sharedKeys.size(), 1);  // the same key, so the refined mesh replaces the preview's
    QCOMPARE(sharedEdges, 2);
    QCOMPARE(meshInstances.size(), 4);
    QCOMPARE(edgeInstances, 4);
    QSet<QString> nodes;
    QList<float> offsets;
    for (const auto& instance : std::as_const(meshInstances)) {
        QVERIFY(sharedKeys.contains(instance.sourceKey));
        nodes.insert(instance.nodeId);
        offsets.append(instance.placement[3]);
        // A pure translation: the rotation part is the identity.
        QCOMPARE(instance.placement[0], 1.0F);
        QCOMPARE(instance.placement[5], 1.0F);
        QCOMPARE(instance.placement[10], 1.0F);
    }
    QCOMPARE(nodes.size(), 2);
    std::sort(offsets.begin(), offsets.end());
    QVERIFY(qAbs((offsets.last() - offsets.first()) - 25.0F) < 1.0e-4F);
}

void WorkerProcessTest::reviewedPlanWritesAndValidatesStep()
{
    const auto fixture = loupe::tests::writeFlatTwoSolidStep(
        QStringLiteral("worker-export-source.step").toStdString());
    QTemporaryDir destination;
    QVERIFY(destination.isValid());
    QProcess worker;
    const auto name = serverName();
    worker.start(QStringLiteral(LOUPE_WORKER_PATH), {QStringLiteral("--server-name"), name});
    QVERIFY(worker.waitForStarted(3'000));

    QLocalSocket socket;
    QVERIFY(connectToWorker(socket, name));
    loupe::protocol::FrameDecoder decoder;
    readEvent(socket, decoder);
    send(socket, {{QStringLiteral("version"), QJsonObject{{QStringLiteral("major"), 2}, {QStringLiteral("minor"), 0}}},
                  {QStringLiteral("type"), QStringLiteral("openFile")}, {QStringLiteral("requestId"), 20},
                  {QStringLiteral("path"), QString::fromStdString(fixture.string())}});

    const auto snapshotEvent = readEventOfType(socket, decoder, QStringLiteral("snapshotReady"));
    const auto snapshot = QJsonDocument::fromJson(
        QByteArray::fromBase64(snapshotEvent.value(QStringLiteral("snapshotBase64")).toString().toLatin1())).object();
    const auto geometry = snapshot.value(QStringLiteral("geometry")).toArray();
    QVERIFY(geometry.size() >= 2);
    const auto firstNodeId = geometry.at(0).toObject().value(QStringLiteral("nodeId")).toString();
    const auto secondNodeId = geometry.at(1).toObject().value(QStringLiteral("nodeId")).toString();
    QVERIFY(!firstNodeId.isEmpty());
    QVERIFY(!secondNodeId.isEmpty());

    QElapsedTimer readyTimer;
    readyTimer.start();
    bool ready = false;
    while (readyTimer.elapsed() < 15'000 && !ready) {
        const auto event = readEvent(socket, decoder);
        ready = event.value(QStringLiteral("type")).toString() == QStringLiteral("progress")
            && event.value(QStringLiteral("fraction")).toDouble() >= 1.0;
    }
    QVERIFY(ready);

    loupe::exporting::PlanRequest request;
    request.selections = {{firstNodeId.toStdString(), loupe::exporting::SelectionKind::Body},
                          {secondNodeId.toStdString(), loupe::exporting::SelectionKind::Body}};
    request.hierarchyPaths.emplace(firstNodeId.toStdString(), "Assembly/Zed");
    request.hierarchyPaths.emplace(secondNodeId.toStdString(), "Assembly/Alpha");
    request.outputLeafNames.emplace(firstNodeId.toStdString(), "Reviewed-first");
    request.outputLeafNames.emplace(secondNodeId.toStdString(), "Reviewed-second");
    request.destination = destination.path().toStdString();
    request.format = loupe::exporting::Format::Step;
    request.coordinates = loupe::exporting::Coordinates::Assembly;
    request.grouping = loupe::exporting::Grouping::SeparateFiles;
    request.stepOutputUnit = loupe::exporting::StepOutputUnit::Millimeter;
    request.requestedUnitToMillimeters = 1.0;
    request.unitDecision = {loupe::units::LengthUnit::Millimeter, loupe::units::UnitConfidence::Confirmed,
                            1.0, "worker integration test"};
    const auto plan = loupe::exporting::buildPlan(request);
    const auto planJson = QJsonDocument(QJsonObject{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("destination"), destination.path()},
        {QStringLiteral("format"), QStringLiteral("STEP")},
        {QStringLiteral("coordinates"), QStringLiteral("Assembly")},
        {QStringLiteral("effectiveUnit"), QStringLiteral("mm")},
        {QStringLiteral("sourceToMillimeters"), 1.0},
        {QStringLiteral("selections"), QJsonArray{
            QJsonObject{{QStringLiteral("nodeId"), firstNodeId},
                        {QStringLiteral("kind"), static_cast<int>(loupe::exporting::SelectionKind::Body)},
                        {QStringLiteral("hierarchyPath"), QStringLiteral("Assembly/Zed")},
                        {QStringLiteral("leafName"), QStringLiteral("Reviewed-first")}},
            QJsonObject{{QStringLiteral("nodeId"), secondNodeId},
                        {QStringLiteral("kind"), static_cast<int>(loupe::exporting::SelectionKind::Body)},
                        {QStringLiteral("hierarchyPath"), QStringLiteral("Assembly/Alpha")},
                        {QStringLiteral("leafName"), QStringLiteral("Reviewed-second")}},
        }},
    }).toJson(QJsonDocument::Compact);
    send(socket, {{QStringLiteral("version"), QJsonObject{{QStringLiteral("major"), 2}, {QStringLiteral("minor"), 0}}},
                  {QStringLiteral("type"), QStringLiteral("executeExportPlan")},
                  {QStringLiteral("requestId"), 21},
                  {QStringLiteral("planBase64"), QString::fromLatin1(planJson.toBase64())},
                  {QStringLiteral("fingerprint"), QString::fromStdString(plan.fingerprint())}});

    const auto firstResult = readEventOfType(socket, decoder, QStringLiteral("exportRowResult"));
    const auto secondResult = readEventOfType(socket, decoder, QStringLiteral("exportRowResult"));
    QVERIFY(firstResult.value(QStringLiteral("passed")).toBool());
    QVERIFY(secondResult.value(QStringLiteral("passed")).toBool());
    QCOMPARE(firstResult.value(QStringLiteral("rowIndex")).toInt(), 1);
    QCOMPARE(secondResult.value(QStringLiteral("rowIndex")).toInt(), 0);
    const auto completed = readEventOfType(socket, decoder, QStringLiteral("exportCompleted"));
    QCOMPARE(completed.value(QStringLiteral("succeededCount")).toInt(), 2);
    QCOMPARE(completed.value(QStringLiteral("failedCount")).toInt(), 0);
    for (const auto* name : {"Reviewed-first.step", "Reviewed-second.step"}) {
        const auto outputPath = std::filesystem::path(destination.path().toStdString()) / name;
        QVERIFY(std::filesystem::exists(outputPath));
        const auto validation = loupe::validation::OutputValidator{}.validate(
            {outputPath, loupe::validation::OutputUnit::Millimeter, 1, {}, {}, 1.0e-5, false, false});
        QVERIFY(validation.passed);
    }
}

namespace {

// The reviewed-plan JSON the app sends, and the plan object the worker must independently
// arrive at. Built together so a test cannot accidentally describe two different batches.
struct DrawingBatch final {
    QByteArray planJson;
    QString fingerprint;
};

DrawingBatch drawingBatchFor(const QString& nodeId, const QString& hierarchyPath,
                             const QString& destinationPath, const QString& leafName)
{
    loupe::drawing::DrawingSelection selection;
    selection.drawingId = "d1";
    selection.nodeId = nodeId.toUtf8().toStdString();
    selection.viewLabel = "Top";
    selection.viewDirection = {0.0, 0.0, 1.0};
    selection.upDirection = {0.0, 1.0, 0.0};
    selection.content = loupe::drawing::DrawingContent::CutContours;

    loupe::drawing::DrawingPlanRequest request;
    request.selections = {selection};
    request.hierarchyPaths.emplace(selection.nodeId, hierarchyPath.toUtf8().toStdString());
    request.outputLeafNames.emplace(selection.drawingId, leafName.toUtf8().toStdString());
    request.destination = destinationPath.toUtf8().toStdString();
    request.format = loupe::drawing::DrawingFormat::Dxf;
    request.unitDecision = {loupe::units::LengthUnit::Millimeter, loupe::units::UnitConfidence::Confirmed,
                            1.0, "worker integration test"};
    const auto plan = loupe::drawing::buildDrawingPlan(request);

    const auto planJson = QJsonDocument(QJsonObject{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("destination"), destinationPath},
        {QStringLiteral("format"), QStringLiteral("DXF")},
        {QStringLiteral("includeScaleFiducial"), false},
        {QStringLiteral("sourceToMillimeters"), 1.0},
        {QStringLiteral("selections"), QJsonArray{QJsonObject{
            {QStringLiteral("drawingId"), QStringLiteral("d1")},
            {QStringLiteral("nodeId"), nodeId},
            {QStringLiteral("hierarchyPath"), hierarchyPath},
            {QStringLiteral("viewLabel"), QStringLiteral("Top")},
            {QStringLiteral("viewX"), 0.0}, {QStringLiteral("viewY"), 0.0}, {QStringLiteral("viewZ"), 1.0},
            {QStringLiteral("upX"), 0.0}, {QStringLiteral("upY"), 1.0}, {QStringLiteral("upZ"), 0.0},
            {QStringLiteral("contentMode"), QStringLiteral("Cut contours")},
            {QStringLiteral("scaleNumerator"), 1},
            {QStringLiteral("scaleDenominator"), 1},
            {QStringLiteral("leafName"), leafName},
        }}},
    }).toJson(QJsonDocument::Compact);
    return {planJson, QString::fromStdString(plan.fingerprint())};
}

} // namespace

void WorkerProcessTest::reviewedDrawingPlanWritesAndValidatesDxf()
{
    const auto fixture = loupe::tests::writeFlatTwoSolidStep(
        QStringLiteral("worker-drawing-source.step").toStdString());
    QTemporaryDir destination;
    QVERIFY(destination.isValid());
    QProcess worker;
    const auto name = serverName();
    worker.start(QStringLiteral(LOUPE_WORKER_PATH), {QStringLiteral("--server-name"), name});
    QVERIFY(worker.waitForStarted(3'000));

    QLocalSocket socket;
    QVERIFY(connectToWorker(socket, name));
    loupe::protocol::FrameDecoder decoder;
    readEvent(socket, decoder);
    send(socket, {{QStringLiteral("version"), QJsonObject{{QStringLiteral("major"), 2}, {QStringLiteral("minor"), 1}}},
                  {QStringLiteral("type"), QStringLiteral("openFile")}, {QStringLiteral("requestId"), 40},
                  {QStringLiteral("path"), QString::fromStdString(fixture.string())}});

    const auto snapshotEvent = readEventOfType(socket, decoder, QStringLiteral("snapshotReady"));
    const auto snapshot = QJsonDocument::fromJson(
        QByteArray::fromBase64(snapshotEvent.value(QStringLiteral("snapshotBase64")).toString().toLatin1())).object();
    const auto geometry = snapshot.value(QStringLiteral("geometry")).toArray();
    QVERIFY(!geometry.isEmpty());
    const auto nodeId = geometry.at(0).toObject().value(QStringLiteral("nodeId")).toString();
    QVERIFY(!nodeId.isEmpty());

    QElapsedTimer readyTimer;
    readyTimer.start();
    bool ready = false;
    while (readyTimer.elapsed() < 15'000 && !ready) {
        const auto event = readEvent(socket, decoder);
        ready = event.value(QStringLiteral("type")).toString() == QStringLiteral("progress")
            && event.value(QStringLiteral("fraction")).toDouble() >= 1.0;
    }
    QVERIFY(ready);

    const auto batch = drawingBatchFor(nodeId, QStringLiteral("Assembly/Zed"), destination.path(),
                                       QStringLiteral("Reviewed-drawing"));
    send(socket, {{QStringLiteral("version"), QJsonObject{{QStringLiteral("major"), 2}, {QStringLiteral("minor"), 1}}},
                  {QStringLiteral("type"), QStringLiteral("executeDrawingPlan")},
                  {QStringLiteral("requestId"), 41},
                  {QStringLiteral("planBase64"), QString::fromLatin1(batch.planJson.toBase64())},
                  {QStringLiteral("fingerprint"), batch.fingerprint}});

    const auto result = readEventOfType(socket, decoder, QStringLiteral("drawingRowResult"));
    QCOMPARE(result.value(QStringLiteral("drawingId")).toString(), QStringLiteral("d1"));
    QVERIFY2(result.value(QStringLiteral("passed")).toBool(),
             qPrintable(result.value(QStringLiteral("message")).toString()));
    const auto completed = readEventOfType(socket, decoder, QStringLiteral("drawingCompleted"));
    QCOMPARE(completed.value(QStringLiteral("succeededCount")).toInt(), 1);
    QCOMPARE(completed.value(QStringLiteral("failedCount")).toInt(), 0);

    // The worker already validated the file; reopening it here proves the file the app is
    // told about is the file on disk.
    const auto outputPath = std::filesystem::path(destination.path().toStdString()) / "Reviewed-drawing.dxf";
    QVERIFY(std::filesystem::exists(outputPath));
    QVERIFY(std::filesystem::file_size(outputPath) > 0);
}

void WorkerProcessTest::aChangedDrawingPlanIsRefusedAtTheFingerprint()
{
    // The review contract: the worker rebuilds the plan itself and refuses anything whose
    // fingerprint does not match what the user approved.
    const auto fixture = loupe::tests::writeFlatTwoSolidStep(
        QStringLiteral("worker-drawing-gate-source.step").toStdString());
    QTemporaryDir destination;
    QVERIFY(destination.isValid());
    QProcess worker;
    const auto name = serverName();
    worker.start(QStringLiteral(LOUPE_WORKER_PATH), {QStringLiteral("--server-name"), name});
    QVERIFY(worker.waitForStarted(3'000));

    QLocalSocket socket;
    QVERIFY(connectToWorker(socket, name));
    loupe::protocol::FrameDecoder decoder;
    readEvent(socket, decoder);
    send(socket, {{QStringLiteral("version"), QJsonObject{{QStringLiteral("major"), 2}, {QStringLiteral("minor"), 1}}},
                  {QStringLiteral("type"), QStringLiteral("openFile")}, {QStringLiteral("requestId"), 50},
                  {QStringLiteral("path"), QString::fromStdString(fixture.string())}});

    const auto snapshotEvent = readEventOfType(socket, decoder, QStringLiteral("snapshotReady"));
    const auto snapshot = QJsonDocument::fromJson(
        QByteArray::fromBase64(snapshotEvent.value(QStringLiteral("snapshotBase64")).toString().toLatin1())).object();
    const auto nodeId = snapshot.value(QStringLiteral("geometry")).toArray().at(0).toObject()
                            .value(QStringLiteral("nodeId")).toString();
    QVERIFY(!nodeId.isEmpty());

    QElapsedTimer readyTimer;
    readyTimer.start();
    bool ready = false;
    while (readyTimer.elapsed() < 15'000 && !ready) {
        const auto event = readEvent(socket, decoder);
        ready = event.value(QStringLiteral("type")).toString() == QStringLiteral("progress")
            && event.value(QStringLiteral("fraction")).toDouble() >= 1.0;
    }
    QVERIFY(ready);

    const auto batch = drawingBatchFor(nodeId, QStringLiteral("Assembly/Zed"), destination.path(),
                                       QStringLiteral("Reviewed-drawing"));
    send(socket, {{QStringLiteral("version"), QJsonObject{{QStringLiteral("major"), 2}, {QStringLiteral("minor"), 1}}},
                  {QStringLiteral("type"), QStringLiteral("executeDrawingPlan")},
                  {QStringLiteral("requestId"), 51},
                  {QStringLiteral("planBase64"), QString::fromLatin1(batch.planJson.toBase64())},
                  {QStringLiteral("fingerprint"), QStringLiteral("0000000000000000not-the-reviewed-one")}});

    const auto failed = readEventOfType(socket, decoder, QStringLiteral("failed"));
    QCOMPARE(failed.value(QStringLiteral("code")).toString(), QStringLiteral("drawing_gate_failed"));
    // Refused before anything was written.
    QVERIFY(!std::filesystem::exists(
        std::filesystem::path(destination.path().toStdString()) / "Reviewed-drawing.dxf"));
}

QTEST_MAIN(WorkerProcessTest)

#include "test_worker_process.moc"
