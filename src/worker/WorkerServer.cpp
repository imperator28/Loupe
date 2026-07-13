#include "worker/WorkerServer.h"

#include "core/import/StepImporter.h"
#include "core/inspection/GeometryAnalysis.h"
#include "core/units/UnitPolicy.h"
#include "protocol/ProtocolTypes.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QTimer>

#include <exception>
#include <optional>
#include <type_traits>

namespace loupe::worker {
namespace {

QByteArray encodeSnapshot(const loupe::import::ImportResult& imported)
{
    QJsonArray nodes;
    for (const auto& node : imported.snapshot.nodes) {
        nodes.append(QJsonObject{
            {QStringLiteral("id"), QString::fromStdString(node.id)},
            {QStringLiteral("name"), QString::fromStdString(node.name)},
            {QStringLiteral("kind"), static_cast<int>(node.kind)},
            {QStringLiteral("parentId"), node.parentId ? QString::fromStdString(*node.parentId) : QString{}},
            {QStringLiteral("definitionId"), node.definitionId ? QString::fromStdString(*node.definitionId) : QString{}},
        });
    }
    QJsonArray geometry;
    const auto unitDecision = loupe::units::decide(imported.unitEvidence, std::nullopt);
    for (std::size_t index = 0; index < imported.native->shapes.size() && index < imported.native->shapeNodeIds.size(); ++index) {
        const auto analysis = loupe::inspection::analyze(imported.native->shapes[index], unitDecision.sourceToMillimeters);
        if (!analysis.valid) continue;
        geometry.append(QJsonObject{
            {QStringLiteral("nodeId"), QString::fromStdString(imported.native->shapeNodeIds[index])},
            {QStringLiteral("surfaceAreaMm2"), analysis.surfaceAreaMm2},
            {QStringLiteral("volumeMm3"), analysis.volumeMm3},
            {QStringLiteral("boundsMm"), QJsonObject{{QStringLiteral("width"), analysis.boundsMm.width}, {QStringLiteral("height"), analysis.boundsMm.height}, {QStringLiteral("depth"), analysis.boundsMm.depth}}},
        });
    }
    return QJsonDocument(QJsonObject{
        {QStringLiteral("sourceHash"), QString::fromStdString(imported.snapshot.sourceHash)},
        {QStringLiteral("classification"), static_cast<int>(imported.snapshot.classification)},
        {QStringLiteral("definitionCount"), static_cast<qint64>(imported.definitionCount)},
        {QStringLiteral("occurrenceCount"), static_cast<qint64>(imported.occurrenceCount)},
        {QStringLiteral("nodes"), nodes},
        {QStringLiteral("geometry"), geometry},
        {QStringLiteral("effectiveUnit"), unitDecision.effectiveUnit == loupe::units::LengthUnit::Inch ? QStringLiteral("in") : QStringLiteral("mm")},
        {QStringLiteral("sourceToMillimeters"), unitDecision.sourceToMillimeters},
    }).toJson(QJsonDocument::Compact);
}

} // namespace

WorkerServer::WorkerServer(QObject* parent)
    : QObject(parent)
{
    connect(&server_, &QLocalServer::newConnection, this, &WorkerServer::acceptConnection);
}

bool WorkerServer::listen(const QString& serverName)
{
    return server_.listen(serverName);
}

void WorkerServer::acceptConnection()
{
    if (socket_) {
        server_.nextPendingConnection()->disconnectFromServer();
        return;
    }
    socket_ = server_.nextPendingConnection();
    connect(socket_, &QLocalSocket::readyRead, this, &WorkerServer::readCommands);
    connect(socket_, &QLocalSocket::disconnected, this, [this] { socket_.clear(); });
    send({{QStringLiteral("type"), QStringLiteral("ready")}});
}

void WorkerServer::readCommands()
{
    while (socket_ && socket_->canReadLine()) {
        try {
            const auto command = protocol::decodeCommand(socket_->readLine());
            std::visit([this](const auto& value) {
                using Value = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, protocol::OpenFile>) {
                    open(value.requestId, value.path);
                } else if constexpr (std::is_same_v<Value, protocol::Cancel>) {
                    cancel(value.requestId);
                }
            }, command);
        } catch (const protocol::ProtocolError&) {
            fail(0, QStringLiteral("protocol_error"), QStringLiteral("Invalid worker command"));
        }
    }
}

void WorkerServer::send(QJsonObject event)
{
    if (!socket_) {
        return;
    }
    event.insert(QStringLiteral("version"), QJsonObject{{QStringLiteral("major"), 1}, {QStringLiteral("minor"), 0}});
    socket_->write(QJsonDocument(event).toJson(QJsonDocument::Compact) + '\n');
}

void WorkerServer::fail(const std::uint64_t requestId, const QString& code, const QString& message)
{
    send({{QStringLiteral("type"), QStringLiteral("failed")}, {QStringLiteral("requestId"), static_cast<qint64>(requestId)},
          {QStringLiteral("code"), code}, {QStringLiteral("message"), message}, {QStringLiteral("recoverable"), true}});
}

void WorkerServer::open(const std::uint64_t requestId, const QString& path)
{
    if (!QFileInfo::exists(path)) {
        fail(requestId, QStringLiteral("read_failed"), QStringLiteral("The requested file does not exist"));
        return;
    }
    if (!activeSessions_.isEmpty()) {
        fail(requestId, QStringLiteral("busy"), QStringLiteral("A request is already active"));
        return;
    }
    auto* completion = new QTimer(this);
    completion->setSingleShot(true);
    activeSessions_.insert(requestId, completion);
    connect(completion, &QTimer::timeout, this, [this, requestId, completion] {
        const auto path = completion->property("sourcePath").toString();
        activeSessions_.remove(requestId);
        try {
            const auto imported = import::StepImporter{}.read(path.toStdString());
            send({{QStringLiteral("type"), QStringLiteral("snapshotReady")}, {QStringLiteral("requestId"), static_cast<qint64>(requestId)},
                  {QStringLiteral("snapshotBase64"), QString::fromLatin1(encodeSnapshot(imported).toBase64())}});
        } catch (const std::exception&) {
            fail(requestId, QStringLiteral("import_failed"), QStringLiteral("The STEP file could not be imported"));
        }
        completion->deleteLater();
    });
    completion->setProperty("sourcePath", path);
    completion->start(0);
}

void WorkerServer::cancel(const std::uint64_t requestId)
{
    const auto it = activeSessions_.find(requestId);
    if (it != activeSessions_.end()) {
        (*it)->stop();
        (*it)->deleteLater();
        activeSessions_.erase(it);
    }
    send({{QStringLiteral("type"), QStringLiteral("canceled")}, {QStringLiteral("requestId"), static_cast<qint64>(requestId)}});
}

} // namespace loupe::worker
