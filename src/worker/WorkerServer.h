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
    // Starts importing before any client connects (the app launched this worker on the file
    // it was opened with). What the import sends is held until a client connects, then
    // delivered in order; with no client within a couple of minutes the worker quits.
    void openBeforeConnection(std::uint64_t requestId, const QString& path);

private slots:
    void acceptConnection();
    void readCommands();

private:
    struct DocumentSession;
    struct ImportJob final {
        std::atomic_bool canceled{false};
    };
    struct ExportJob final {
        std::atomic_bool canceled{false};
    };

    void send(QJsonObject event);
    void sendGeometry(const protocol::GeometryPayload& payload);
    void fail(std::uint64_t requestId, const QString& code, const QString& message);
    void open(std::uint64_t requestId, const QString& path, const std::optional<protocol::UnitOverride>& unitOverride);
    void executeExportPlan(std::uint64_t requestId, const QByteArray& planJson, const QString& fingerprint);
    void executeDrawingPlan(std::uint64_t requestId, const QByteArray& planJson, const QString& fingerprint);
    void requestDrawingPreview(std::uint64_t requestId, const QByteArray& requestJson, int revision);
    void exportAssembly(const protocol::ExportAssembly& command);
    void cancel(std::uint64_t requestId);

    QLocalServer server_;
    QPointer<QLocalSocket> socket_;
    // Frames sent before the first client connected, when an import was started without one.
    bool holdUntilConnected_{false};
    QList<QByteArray> heldFrames_;
    protocol::FrameDecoder commandFrames_;
    QHash<std::uint64_t, std::shared_ptr<ImportJob>> activeSessions_;
    QHash<std::uint64_t, std::shared_ptr<ExportJob>> activeExports_;
    std::shared_ptr<DocumentSession> documentSession_;
    // Highest preview revision seen. A request older than this is dropped rather than
    // projected and discarded: only the newest candidate is worth the cost.
    int latestPreviewRevision_{};
};

} // namespace loupe::worker
