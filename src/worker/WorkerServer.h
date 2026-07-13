#pragma once

#include <QLocalServer>
#include <QHash>
#include <QJsonObject>
#include <QPointer>

#include <cstdint>
#include <optional>

#include "protocol/ProtocolTypes.h"

class QLocalSocket;
class QTimer;

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
    void send(QJsonObject event);
    void fail(std::uint64_t requestId, const QString& code, const QString& message);
    void open(std::uint64_t requestId, const QString& path, const std::optional<protocol::UnitOverride>& unitOverride);
    void cancel(std::uint64_t requestId);

    QLocalServer server_;
    QPointer<QLocalSocket> socket_;
    QHash<std::uint64_t, QTimer*> activeSessions_;
};

} // namespace loupe::worker
