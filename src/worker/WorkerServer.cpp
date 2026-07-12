#include "worker/WorkerServer.h"

#include "protocol/ProtocolTypes.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QTimer>

#include <type_traits>

namespace loupe::worker {

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
        activeSessions_.remove(requestId);
        send({{QStringLiteral("type"), QStringLiteral("snapshotReady")}, {QStringLiteral("requestId"), static_cast<qint64>(requestId)},
              {QStringLiteral("snapshotBase64"), QString()}});
        completion->deleteLater();
    });
    completion->start(500);
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
