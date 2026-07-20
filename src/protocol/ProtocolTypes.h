#pragma once

#include <QByteArray>
#include <QString>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <variant>

namespace loupe::protocol {

struct Version { std::uint16_t major{2}; std::uint16_t minor{0}; };
struct UnitOverride { QString unit; double customFactor{1.0}; QString reason; };
struct OpenFile { std::uint64_t requestId{}; QString path; std::optional<UnitOverride> unitOverride; };
struct Cancel { std::uint64_t requestId{}; };
struct ExecuteExportPlan { std::uint64_t requestId{}; QByteArray planJson; QString fingerprint; };
struct MeasureAtPoint { std::uint64_t requestId{}; QString nodeId; double x{}; double y{}; double z{}; QString mode; };
struct SetVisible { QString nodeId; bool visible{}; };
struct Ready { Version version; };
struct Progress { std::uint64_t requestId{}; QString stage; double fraction{}; };
struct SnapshotReady { std::uint64_t requestId{}; QByteArray snapshotJson; };
struct ComponentMetadata {
    std::uint64_t requestId{};
    QString nodeId;
    double surfaceAreaMm2{};
    double volumeMm3{};
    double widthMm{};
    double heightMm{};
    double depthMm{};
    double longestEdgeMm{};
    double circularRadiusMm{};
    int planarFaceCount{};
};
struct MeshReady { std::uint64_t requestId{}; QString definitionId; int refinement{}; QString segmentKey; QByteArray meshJson; };
struct EdgeReady { std::uint64_t requestId{}; QString nodeId; QByteArray edgeJson; };
struct ImportMetrics {
    std::uint64_t requestId{};
    QString sourceName;
    QString sourceHash;
    qint64 stepReadMs{};
    qint64 xcafTransferMs{};
    qint64 snapshotBuildMs{};
    qint64 treeReadyMs{};
    qint64 firstGeometryMs{};
    qint64 previewReadyMs{};
    qint64 finalReadyMs{};
    qint64 previewTriangleCount{};
    qint64 refinedTriangleCount{};
    int bodyCount{};
};
struct ExportProgress {
    std::uint64_t requestId{};
    int rowIndex{};
    int rowCount{};
    QString stage;
    double fraction{};
};
struct ExportRowResult {
    std::uint64_t requestId{};
    int rowIndex{};
    QString nodeId;
    QString path;
    bool passed{};
    QString message;
};
struct ExportCompleted { std::uint64_t requestId{}; int succeededCount{}; int failedCount{}; };
struct Failed { std::uint64_t requestId{}; QString code; QString message; bool recoverable{}; };
struct Canceled { std::uint64_t requestId{}; };

using Command = std::variant<OpenFile, Cancel, ExecuteExportPlan, MeasureAtPoint, SetVisible>;
using Event = std::variant<Ready, Progress, SnapshotReady, ComponentMetadata, MeshReady, EdgeReady, ImportMetrics,
                           ExportProgress, ExportRowResult, ExportCompleted, Failed, Canceled>;

class ProtocolError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

QByteArray encode(const OpenFile& command);
QByteArray encode(const Command& command);
Command decodeCommand(const QByteArray& bytes);
Event decodeEvent(const QByteArray& bytes);

} // namespace loupe::protocol
