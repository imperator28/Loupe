#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QVector>

#include "protocol/ProtocolFrame.h"
#include "protocol/ProtocolTypes.h"

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
    [[nodiscard]] std::uint64_t executeExportPlan(const QByteArray& planJson, const QString& fingerprint);
    [[nodiscard]] std::uint64_t executeDrawingPlan(const QByteArray& planJson, const QString& fingerprint);
    [[nodiscard]] std::uint64_t requestDrawingPreview(const QByteArray& requestJson, int revision);
    [[nodiscard]] std::uint64_t exportAssembly(const QString& destination, const QStringList& excludedNodeIds, const QString& outputUnit);
    // The assembly as the exploded view has it (#17), in any of the assembly formats.
    [[nodiscard]] std::uint64_t exportAssembly(protocol::ExportAssembly request);
    void cancel(std::uint64_t requestId);

signals:
    void progress(quint64 requestId, const QString& stage, double fraction);
    void snapshotReady(quint64 requestId, const QByteArray& snapshotJson);
    void componentMetadata(quint64 requestId, const QString& nodeId, double surfaceAreaMm2, double volumeMm3,
                           double widthMm, double heightMm, double depthMm, double longestEdgeMm,
                           double circularRadiusMm, int planarFaceCount);
    void meshReady(quint64 requestId, const QString& nodeId, const QString& segmentKey, const QString& sourceColor, const QByteArray& meshJson);
    void edgeReady(quint64 requestId, const QString& nodeId, const QByteArray& edgeJson);
    // One part's mesh segment (or edges) in the part's own coordinates, shared by its copies.
    void sharedMeshReady(quint64 requestId, const QString& sourceKey, const QString& sourceColor, const QByteArray& meshJson);
    void sharedEdgesReady(quint64 requestId, const QString& sourceKey, const QByteArray& edgeJson);
    // One copy of a shared segment (or edges), placed by a row-major 3x4 [R | t] in millimetres.
    void meshInstanceReady(quint64 requestId, const QString& nodeId, const QString& segmentKey, const QString& sourceKey,
                           const QString& sourceColor, const QVector<float>& placement);
    void edgeInstanceReady(quint64 requestId, const QString& nodeId, const QString& sourceKey, const QVector<float>& placement);
    void importMetrics(quint64 requestId, const QString& sourceName, const QString& sourceHash, qint64 stepReadMs,
                       qint64 xcafTransferMs, qint64 snapshotBuildMs, qint64 treeReadyMs, qint64 firstGeometryMs,
                       qint64 previewReadyMs, qint64 finalReadyMs, qint64 previewTriangleCount,
                       qint64 refinedTriangleCount, int bodyCount);
    void exportProgress(quint64 requestId, int rowIndex, int rowCount, const QString& stage, double fraction);
    void exportRowResult(quint64 requestId, int rowIndex, const QString& nodeId, const QString& path,
                         bool passed, const QString& message);
    void exportCompleted(quint64 requestId, int succeededCount, int failedCount);
    void drawingProgress(quint64 requestId, int rowIndex, int rowCount, const QString& stage, double fraction);
    void drawingRowResult(quint64 requestId, int rowIndex, const QString& drawingId, const QString& path,
                          bool passed, const QString& message);
    void drawingCompleted(quint64 requestId, int succeededCount, int failedCount);
    void drawingPreviewReady(quint64 requestId, int revision, const QByteArray& previewJson, bool approximate);
    void assemblyExported(quint64 requestId, const QString& path, int partCount, int removedCount);
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
