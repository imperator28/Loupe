#include "app/worker/WorkerClient.h"

#include "protocol/ProtocolTypes.h"

#include <QLocalSocket>

#include <exception>
#include <variant>

namespace loupe::app::worker {

WorkerClient::WorkerClient(QObject* parent)
    : QObject(parent)
    , socket_(new QLocalSocket(this))
{
    connect(socket_, &QLocalSocket::readyRead, this, &WorkerClient::readEvents);
    connect(socket_, &QLocalSocket::disconnected, this, &WorkerClient::connectionLost);
}

bool WorkerClient::connectToServer(const QString& serverName, const int timeoutMs)
{
    socket_->abort();
    inputBuffer_.clear();
    socket_->connectToServer(serverName);
    return socket_->waitForConnected(timeoutMs);
}

std::uint64_t WorkerClient::openFile(const QString& path, const std::optional<QString>& unitOverride)
{
    const auto requestId = nextRequestId_++;
    const auto overrideValue = unitOverride ? std::optional<protocol::UnitOverride>{protocol::UnitOverride{*unitOverride, 1.0, QStringLiteral("User requested unit interpretation")}}
                                           : std::nullopt;
    writeCommand(protocol::encode(protocol::OpenFile{requestId, path, overrideValue}));
    return requestId;
}

void WorkerClient::cancel(const std::uint64_t requestId)
{
    writeCommand(protocol::encode(protocol::Command{protocol::Cancel{requestId}}));
}

void WorkerClient::readEvents()
{
    inputBuffer_.append(socket_->readAll());
    for (;;) {
        const auto newline = inputBuffer_.indexOf('\n');
        if (newline < 0) return;
        const auto frame = inputBuffer_.left(newline + 1);
        inputBuffer_.remove(0, newline + 1);
        try {
            const auto event = protocol::decodeEvent(frame);
            std::visit([this](const auto& value) {
                using Value = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, protocol::SnapshotReady>) {
                    emit snapshotReady(value.requestId, value.snapshotJson);
                } else if constexpr (std::is_same_v<Value, protocol::MeshReady>) {
                    emit meshReady(value.requestId, value.definitionId, value.meshJson);
                } else if constexpr (std::is_same_v<Value, protocol::Failed>) {
                    emit requestFailed(value.requestId, value.code, value.message, value.recoverable);
                } else if constexpr (std::is_same_v<Value, protocol::Canceled>) {
                    emit requestCanceled(value.requestId);
                }
            }, event);
        } catch (const std::exception& error) {
            emit protocolError(QString::fromUtf8(error.what()));
        }
    }
}

void WorkerClient::writeCommand(const QByteArray& command)
{
    if (socket_->state() == QLocalSocket::ConnectedState) {
        socket_->write(command);
        socket_->flush();
    } else {
        emit protocolError(QStringLiteral("Worker is not connected"));
    }
}

} // namespace loupe::app::worker
