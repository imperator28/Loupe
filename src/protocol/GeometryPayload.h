#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

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

} // namespace loupe::protocol
