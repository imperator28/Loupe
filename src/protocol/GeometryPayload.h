#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

#include <array>
#include <optional>
#include <variant>

namespace loupe::protocol {

enum class TopologyKind : quint8 {
    Face = 1,
    Edge = 2,
};

struct TopologyRange final {
    quint32 topologyId{};
    TopologyKind kind{TopologyKind::Face};
    quint32 firstIndex{};
    quint32 indexCount{};
    float measureMm{};
    float radiusMm{};
};

struct MeshPayload final {
    quint64 requestId{};
    quint64 documentGeneration{};
    QString definitionId;
    QString nodeId;
    QString segmentKey;
    QString sourceColor;
    quint8 refinement{};
    QVector<float> vertices;
    QVector<float> normals;
    QVector<quint32> indices;
    QVector<TopologyRange> topology;
};

struct EdgePayload final {
    quint64 requestId{};
    quint64 documentGeneration{};
    QString definitionId;
    QString nodeId;
    quint8 refinement{};
    QVector<float> vertices;
    QVector<quint32> indices;
    QVector<TopologyRange> topology;
};

using GeometryPayload = std::variant<MeshPayload, EdgePayload>;

[[nodiscard]] QByteArray encodeGeometry(const GeometryPayload& payload);
[[nodiscard]] GeometryPayload decodeGeometry(const QByteArray& bytes);
// The summed face area (mm^2) of an encoded mesh payload, read from its topology ranges
// without decoding the vertex data. 0 for an edge payload or anything malformed.
[[nodiscard]] double meshFaceArea(const QByteArray& bytes) noexcept;

struct MeshBounds final {
    std::array<float, 3> minimum{};
    std::array<float, 3> maximum{};
};
// The axis-aligned bounds (mm) of an encoded mesh payload's vertices, read without decoding
// the rest. Empty for an edge payload, a payload with no vertices, or anything malformed.
[[nodiscard]] std::optional<MeshBounds> meshBounds(const QByteArray& bytes) noexcept;

} // namespace loupe::protocol
