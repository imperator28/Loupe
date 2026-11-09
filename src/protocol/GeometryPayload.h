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
    // A round face's centerline, in millimetres in the payload's coordinates: a point on it and
    // its unit direction. Cylinders, cones, tori and surfaces of revolution have one; anything
    // else, and every edge, leaves the direction zero.
    std::array<float, 3> axisOrigin{};
    std::array<float, 3> axisDirection{};
    [[nodiscard]] bool hasAxis() const noexcept
    {
        return axisDirection[0] != 0.0F || axisDirection[1] != 0.0F || axisDirection[2] != 0.0F;
    }
};

// A mesh in its own coordinates, or, when `shared`, one part's mesh in that part's own
// coordinates, drawn by every copy of the part through an InstancePayload. A shared payload's
// nodeId and segmentKey carry the key the instances name it by.
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
    bool shared{false};
};

// Shared like a MeshPayload, with the key in nodeId.
struct EdgePayload final {
    quint64 requestId{};
    quint64 documentGeneration{};
    QString definitionId;
    QString nodeId;
    quint8 refinement{};
    QVector<float> vertices;
    QVector<quint32> indices;
    QVector<TopologyRange> topology;
    bool shared{false};
};

// One copy of a shared mesh segment (or, with `edges`, of shared edges), placed in the assembly:
// the copy is drawn as the shared geometry under `placement`, a rigid transform in millimetres
// given as a row-major 3x4 matrix [R | t]. segmentKey is the copy's own key, as a MeshPayload's.
struct InstancePayload final {
    quint64 requestId{};
    quint64 documentGeneration{};
    QString nodeId;
    QString segmentKey;
    QString sourceKey;
    QString sourceColor;
    quint8 refinement{};
    bool edges{false};
    std::array<float, 12> placement{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F};
};

using GeometryPayload = std::variant<MeshPayload, EdgePayload, InstancePayload>;

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
// The same, for the mesh placed by an InstancePayload's placement: the exact bounds of the
// transformed vertices, as the copy is drawn.
[[nodiscard]] std::optional<MeshBounds> meshBounds(const QByteArray& bytes, const std::array<float, 12>& placement) noexcept;

} // namespace loupe::protocol
