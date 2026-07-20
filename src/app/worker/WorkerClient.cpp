#include "app/worker/WorkerClient.h"

#include "protocol/GeometryPayload.h"
#include "protocol/ProtocolFrame.h"
#include "protocol/ProtocolTypes.h"

#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>

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
    eventFrames_.clear();
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

std::uint64_t WorkerClient::executeExportPlan(const QByteArray& planJson, const QString& fingerprint)
{
    const auto requestId = nextRequestId_++;
    writeCommand(protocol::encode(protocol::Command{protocol::ExecuteExportPlan{requestId, planJson, fingerprint}}));
    return requestId;
}

void WorkerClient::cancel(const std::uint64_t requestId)
{
    writeCommand(protocol::encode(protocol::Command{protocol::Cancel{requestId}}));
}

void WorkerClient::readEvents()
{
    try {
        eventFrames_.append(socket_->readAll());
        for (;;) {
            const auto frame = eventFrames_.take();
            if (!frame) return;
            if (frame->type == protocol::FrameType::Geometry) {
                const auto payload = protocol::decodeGeometry(frame->payload);
                std::visit([this, &frame](const auto& value) {
                    using Value = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<Value, protocol::MeshPayload>) {
                        emit meshReady(value.requestId, value.nodeId, value.segmentKey, value.sourceColor, frame->payload);
                    } else {
                        emit edgeReady(value.requestId, value.nodeId, frame->payload);
                    }
                }, payload);
                continue;
            }
            if (frame->type != protocol::FrameType::ControlJson) {
                throw protocol::ProtocolError("Unknown worker frame type");
            }
            const auto event = protocol::decodeEvent(frame->payload);
            std::visit([this](const auto& value) {
                using Value = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, protocol::Progress>) {
                    emit progress(value.requestId, value.stage, value.fraction);
                } else if constexpr (std::is_same_v<Value, protocol::SnapshotReady>) {
                    emit snapshotReady(value.requestId, value.snapshotJson);
                } else if constexpr (std::is_same_v<Value, protocol::ComponentMetadata>) {
                    emit componentMetadata(value.requestId, value.nodeId, value.surfaceAreaMm2, value.volumeMm3,
                                           value.widthMm, value.heightMm, value.depthMm, value.longestEdgeMm,
                                           value.circularRadiusMm, value.planarFaceCount);
                } else if constexpr (std::is_same_v<Value, protocol::ImportMetrics>) {
                    emit importMetrics(value.requestId, value.sourceName, value.sourceHash, value.stepReadMs,
                                       value.xcafTransferMs, value.snapshotBuildMs, value.treeReadyMs, value.firstGeometryMs,
                                       value.previewReadyMs, value.finalReadyMs, value.previewTriangleCount,
                                       value.refinedTriangleCount, value.bodyCount);
                } else if constexpr (std::is_same_v<Value, protocol::ExportProgress>) {
                    emit exportProgress(value.requestId, value.rowIndex, value.rowCount, value.stage, value.fraction);
                } else if constexpr (std::is_same_v<Value, protocol::ExportRowResult>) {
                    emit exportRowResult(value.requestId, value.rowIndex, value.nodeId, value.path,
                                         value.passed, value.message);
                } else if constexpr (std::is_same_v<Value, protocol::ExportCompleted>) {
                    emit exportCompleted(value.requestId, value.succeededCount, value.failedCount);
                } else if constexpr (std::is_same_v<Value, protocol::Failed>) {
                    emit requestFailed(value.requestId, value.code, value.message, value.recoverable);
                } else if constexpr (std::is_same_v<Value, protocol::Canceled>) {
                    emit requestCanceled(value.requestId);
                }
            }, event);
        }
    } catch (const std::exception& error) {
        eventFrames_.clear();
        emit protocolError(QString::fromUtf8(error.what()));
    }
}

void WorkerClient::writeCommand(const QByteArray& command)
{
    if (socket_->state() == QLocalSocket::ConnectedState) {
        socket_->write(protocol::encodeFrame(protocol::FrameType::ControlJson, command));
        socket_->flush();
    } else {
        emit protocolError(QStringLiteral("Worker is not connected"));
    }
}

} // namespace loupe::app::worker
