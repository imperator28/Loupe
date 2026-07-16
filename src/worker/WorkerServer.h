#pragma once

#include <QLocalServer>
#include <QHash>
#include <QJsonObject>
#include <QPointer>

#include <cstdint>
#include <atomic>
#include <memory>
#include <optional>

#include "protocol/ProtocolTypes.h"
#include "protocol/ProtocolFrame.h"
#include "protocol/GeometryPayload.h"

class QLocalSocket;
namespace loupe::worker {

class WorkerServer final : public QObject {
    Q_OBJECT
public:
    explicit WorkerServer(QObject* parent = nullptr);
    [[nodiscard]] bool listen(const QString& serverName);

private slots:
    void acceptConnection();
    void readCommands();

private:
    struct DocumentSession;
    struct ImportJob final {
        std::atomic_bool canceled{false};
    };

    void send(QJsonObject event);
    void sendGeometry(const protocol::GeometryPayload& payload);
    void fail(std::uint64_t requestId, const QString& code, const QString& message);
    void open(std::uint64_t requestId, const QString& path, const std::optional<protocol::UnitOverride>& unitOverride);
    void cancel(std::uint64_t requestId);

    QLocalServer server_;
    QPointer<QLocalSocket> socket_;
    protocol::FrameDecoder commandFrames_;
    QHash<std::uint64_t, std::shared_ptr<ImportJob>> activeSessions_;
    std::shared_ptr<DocumentSession> documentSession_;
};

} // namespace loupe::worker
