#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>

#include "protocol/ProtocolFrame.h"

#include <cstdint>
#include <optional>

class QLocalSocket;

namespace loupe::app::worker {

class WorkerClient final : public QObject {
    Q_OBJECT

public:
    explicit WorkerClient(QObject* parent = nullptr);

    [[nodiscard]] bool connectToServer(const QString& serverName, int timeoutMs = 3'000);
    [[nodiscard]] std::uint64_t openFile(const QString& path, const std::optional<QString>& unitOverride = std::nullopt);
    void cancel(std::uint64_t requestId);

signals:
    void progress(quint64 requestId, const QString& stage, double fraction);
    void snapshotReady(quint64 requestId, const QByteArray& snapshotJson);
    void componentMetadata(quint64 requestId, const QString& nodeId, double surfaceAreaMm2, double volumeMm3,
                           double widthMm, double heightMm, double depthMm, double longestEdgeMm,
                           double circularRadiusMm, int planarFaceCount);
    void meshReady(quint64 requestId, const QString& nodeId, const QString& segmentKey, const QString& sourceColor, const QByteArray& meshJson);
    void edgeReady(quint64 requestId, const QString& nodeId, const QByteArray& edgeJson);
    void importMetrics(quint64 requestId, const QString& sourceName, const QString& sourceHash, qint64 stepReadMs,
                       qint64 xcafTransferMs, qint64 snapshotBuildMs, qint64 treeReadyMs, qint64 firstGeometryMs,
                       qint64 previewReadyMs, qint64 finalReadyMs, qint64 previewTriangleCount,
                       qint64 refinedTriangleCount, int bodyCount);
    void requestFailed(quint64 requestId, const QString& code, const QString& message, bool recoverable);
    void requestCanceled(quint64 requestId);
    void protocolError(const QString& message);
    void connectionLost();

private slots:
    void readEvents();

private:
    void writeCommand(const QByteArray& command);

    QLocalSocket* socket_{};
    protocol::FrameDecoder eventFrames_;
    std::uint64_t nextRequestId_{1};
};

} // namespace loupe::app::worker
