#include "app/render/CadEdgeGeometry.h"

#include "app/render/MeshGeometry.h"

#include "protocol/GeometryPayload.h"
#include "protocol/ProtocolTypes.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>

#include <algorithm>
#include <limits>

namespace loupe::app::render {

CadEdgeGeometry::CadEdgeGeometry(QQuick3DObject* parent)
    : QQuick3DGeometry(parent)
{
    setPrimitiveType(PrimitiveType::Lines);
    setStride(static_cast<int>(3 * sizeof(float)));
    addAttribute(Attribute::PositionSemantic, 0, Attribute::F32Type);
    addAttribute(Attribute::IndexSemantic, 0, Attribute::U32Type);
}

bool CadEdgeGeometry::replaceWorkerEdges(const QByteArray& edgeJson)
{
    try {
        const auto payload = protocol::decodeGeometry(edgeJson);
        const auto* edges = std::get_if<protocol::EdgePayload>(&payload);
        if (!edges) return false;
        sourceVertexData_ = edges->vertices;
        sourceIndexData_ = edges->indices;
        sourceTopology_ = edges->topology;
        rebuildDisplayLines();
        return true;
    } catch (const protocol::ProtocolError&) {
    }
    const auto payload = QJsonDocument::fromJson(edgeJson);
    if (!payload.isObject()) return false;
    const auto vertices = payload.object().value(QStringLiteral("vertices")).toArray();
    const auto indices = payload.object().value(QStringLiteral("indices")).toArray();
    if (vertices.isEmpty() || vertices.size() % 3 != 0 || indices.isEmpty() || indices.size() % 2 != 0) return false;

    QVector<float> parsedVertices;
    QVector<quint32> parsedIndices;
    parsedVertices.reserve(vertices.size());
    parsedIndices.reserve(indices.size());
    for (const auto& value : vertices) {
        if (!value.isDouble()) return false;
        parsedVertices.append(static_cast<float>(value.toDouble()));
    }
    for (const auto& value : indices) {
        if (!value.isDouble() || value.toInt(-1) < 0 || value.toInt() >= vertices.size() / 3) return false;
        parsedIndices.append(static_cast<quint32>(value.toInt()));
    }
    sourceVertexData_ = std::move(parsedVertices);
    sourceIndexData_ = std::move(parsedIndices);
    sourceTopology_.clear();
    rebuildDisplayLines();
    return true;
}

void CadEdgeGeometry::clearEdges()
{
    sourceVertexData_.clear();
    sourceIndexData_.clear();
    sourceTopology_.clear();
    vertexData_.clear();
    indexData_.clear();
    upload();
}

QVariantMap CadEdgeGeometry::topologyAtPoint(const double x, const double y, const double z,
                                             const double tolerance) const
{
    if (sourceTopology_.isEmpty() || tolerance <= 0.0) return {};
    const QVector3D point{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
    const auto pointAt = [this](const quint32 index) {
        const auto offset = static_cast<qsizetype>(index) * 3;
        return QVector3D{sourceVertexData_.at(offset), sourceVertexData_.at(offset + 1), sourceVertexData_.at(offset + 2)};
    };
    const protocol::TopologyRange* closestRange = nullptr;
    auto closestDistance = static_cast<float>(tolerance * tolerance);
    for (const auto& range : sourceTopology_) {
        const auto end = static_cast<qsizetype>(range.firstIndex + range.indexCount);
        for (auto index = static_cast<qsizetype>(range.firstIndex); index + 1 < end; index += 2) {
            const auto first = pointAt(sourceIndexData_.at(index));
            const auto second = pointAt(sourceIndexData_.at(index + 1));
            const auto direction = second - first;
            const auto lengthSquared = direction.lengthSquared();
            const auto amount = lengthSquared <= 1.0e-12F ? 0.0F
                : std::clamp(QVector3D::dotProduct(point - first, direction) / lengthSquared, 0.0F, 1.0F);
            const auto distance = (point - (first + direction * amount)).lengthSquared();
            if (distance <= closestDistance) {
                closestDistance = distance;
                closestRange = &range;
            }
        }
    }
    if (!closestRange) return {};
    return {{QStringLiteral("topologyId"), closestRange->topologyId},
            {QStringLiteral("entityKind"), QStringLiteral("edge")},
            {QStringLiteral("measureMm"), closestRange->measureMm},
            {QStringLiteral("radiusMm"), closestRange->radiusMm}};
}

bool CadEdgeGeometry::copyTopologyFrom(QObject* source, const quint32 topologyId)
{
    const auto* sourceGeometry = qobject_cast<CadEdgeGeometry*>(source);
    if (!sourceGeometry) return false;
    const auto range = std::find_if(sourceGeometry->sourceTopology_.cbegin(), sourceGeometry->sourceTopology_.cend(),
                                    [topologyId](const auto& value) { return value.topologyId == topologyId; });
    if (range == sourceGeometry->sourceTopology_.cend()) return false;

    sourceVertexData_.clear();
    sourceIndexData_.clear();
    const auto end = static_cast<qsizetype>(range->firstIndex + range->indexCount);
    for (auto index = static_cast<qsizetype>(range->firstIndex); index < end; ++index) {
        const auto sourceIndex = static_cast<qsizetype>(sourceGeometry->sourceIndexData_.at(index));
        const auto sourceOffset = sourceIndex * 3;
        sourceVertexData_.append(sourceGeometry->sourceVertexData_.at(sourceOffset));
        sourceVertexData_.append(sourceGeometry->sourceVertexData_.at(sourceOffset + 1));
        sourceVertexData_.append(sourceGeometry->sourceVertexData_.at(sourceOffset + 2));
        sourceIndexData_.append(static_cast<quint32>(sourceIndexData_.size()));
    }
    sourceTopology_ = {{topologyId, protocol::TopologyKind::Edge, 0,
                        static_cast<quint32>(sourceIndexData_.size()), range->measureMm, range->radiusMm}};
    sectionEnabled_ = false;
    sectionPreview_ = false;
    rebuildDisplayLines();
    return true;
}

bool CadEdgeGeometry::copyFaceBoundaryFrom(QObject* source, const quint32 topologyId)
{
    const auto* sourceGeometry = qobject_cast<MeshGeometry*>(source);
    if (!sourceGeometry || sourceGeometry->sourceVertexData_.size() % 3 != 0) return false;
    const auto range = std::find_if(sourceGeometry->sourceTopology_.cbegin(), sourceGeometry->sourceTopology_.cend(),
                                    [topologyId](const auto& value) {
                                        return value.topologyId == topologyId
                                            && value.kind == protocol::TopologyKind::Face;
                                    });
    if (range == sourceGeometry->sourceTopology_.cend() || range->indexCount == 0
        || range->indexCount % 3 != 0) return false;
    const auto first = static_cast<qsizetype>(range->firstIndex);
    const auto end = first + static_cast<qsizetype>(range->indexCount);
    if (first < 0 || end > sourceGeometry->sourceIndexData_.size()) return false;

    const auto vertexCount = static_cast<quint32>(sourceGeometry->sourceVertexData_.size() / 3);
    QHash<quint64, int> edgeUseCounts;
    QVector<quint64> edgeOrder;
    edgeOrder.reserve(static_cast<qsizetype>(range->indexCount));
    const auto addEdge = [&edgeUseCounts, &edgeOrder](const quint32 firstIndex, const quint32 secondIndex) {
        const auto minimum = std::min(firstIndex, secondIndex);
        const auto maximum = std::max(firstIndex, secondIndex);
        const auto key = (static_cast<quint64>(minimum) << 32U) | maximum;
        if (!edgeUseCounts.contains(key)) edgeOrder.append(key);
        edgeUseCounts[key] += 1;
    };
    for (auto index = first; index < end; index += 3) {
        const auto firstIndex = sourceGeometry->sourceIndexData_.at(index);
        const auto secondIndex = sourceGeometry->sourceIndexData_.at(index + 1);
        const auto thirdIndex = sourceGeometry->sourceIndexData_.at(index + 2);
        if (firstIndex >= vertexCount || secondIndex >= vertexCount || thirdIndex >= vertexCount) return false;
        addEdge(firstIndex, secondIndex);
        addEdge(secondIndex, thirdIndex);
        addEdge(thirdIndex, firstIndex);
    }

    QVector<float> boundaryVertices;
    QVector<quint32> boundaryIndices;
    boundaryVertices.reserve(edgeOrder.size() * 6);
    boundaryIndices.reserve(edgeOrder.size() * 2);
    const auto appendVertex = [&sourceGeometry, &boundaryVertices, &boundaryIndices](const quint32 sourceIndex) {
        const auto sourceOffset = static_cast<qsizetype>(sourceIndex) * 3;
        boundaryVertices.append(sourceGeometry->sourceVertexData_.at(sourceOffset));
        boundaryVertices.append(sourceGeometry->sourceVertexData_.at(sourceOffset + 1));
        boundaryVertices.append(sourceGeometry->sourceVertexData_.at(sourceOffset + 2));
        boundaryIndices.append(static_cast<quint32>(boundaryIndices.size()));
    };
    for (const auto key : edgeOrder) {
        if (edgeUseCounts.value(key) != 1) continue;
        appendVertex(static_cast<quint32>(key >> 32U));
        appendVertex(static_cast<quint32>(key & 0xffffffffU));
    }
    if (boundaryIndices.isEmpty()) return false;

    sourceVertexData_ = std::move(boundaryVertices);
    sourceIndexData_ = std::move(boundaryIndices);
    sourceTopology_.clear();
    sectionEnabled_ = false;
    sectionSliceOnly_ = false;
    sectionPreview_ = false;
    rebuildDisplayLines();
    return true;
}

void CadEdgeGeometry::setSection(const bool enabled, const int axis, const double position, const bool flipped)
{
    const auto normalizedAxis = std::clamp(axis, 0, 2);
    const QVector3D normal = normalizedAxis == 0 ? QVector3D{1.0F, 0.0F, 0.0F}
                           : normalizedAxis == 1 ? QVector3D{0.0F, 1.0F, 0.0F}
                                                 : QVector3D{0.0F, 0.0F, 1.0F};
    setSectionPlane(enabled, normal.x(), normal.y(), normal.z(), position, flipped);
}

void CadEdgeGeometry::setSectionPlane(const bool enabled, const double normalX, const double normalY,
                                      const double normalZ, const double offset, const bool flipped)
{
    configureSection(enabled, normalX, normalY, normalZ, offset, flipped, sectionSliceOnly_, false);
}

void CadEdgeGeometry::configureSection(const bool enabled, const double normalX, const double normalY,
                                       const double normalZ, const double offset, const bool flipped,
                                       const bool sliceOnly, const bool preview)
{
    const QVector3D normal{static_cast<float>(normalX), static_cast<float>(normalY), static_cast<float>(normalZ)};
    if (normal.isNull()) return;
    const auto normalized = normal.normalized();
    if (sectionEnabled_ == enabled && sectionNormal_ == normalized
        && qFuzzyCompare(sectionOffset_ + 1.0, offset + 1.0) && sectionFlipped_ == flipped
        && sectionSliceOnly_ == sliceOnly && sectionPreview_ == preview) return;
    sectionEnabled_ = enabled;
    sectionNormal_ = normalized;
    sectionOffset_ = offset;
    sectionFlipped_ = flipped;
    sectionSliceOnly_ = sliceOnly;
    sectionPreview_ = preview;
    rebuildDisplayLines();
}

void CadEdgeGeometry::setSectionOptions(const bool, const bool sliceOnly, const bool, const bool)
{
    configureSection(sectionEnabled_, sectionNormal_.x(), sectionNormal_.y(), sectionNormal_.z(),
                     sectionOffset_, sectionFlipped_, sliceOnly, false);
}

void CadEdgeGeometry::rebuildDisplayLines()
{
    vertexData_.clear();
    indexData_.clear();
    if (!sectionEnabled_) {
        vertexData_ = sourceVertexData_;
        indexData_ = sourceIndexData_;
        upload();
        return;
    }
    if (sectionSliceOnly_ && !sectionPreview_) {
        upload();
        return;
    }

    const auto pointAt = [this](const quint32 index) {
        const auto offset = static_cast<qsizetype>(index) * 3;
        return QVector3D{sourceVertexData_.at(offset), sourceVertexData_.at(offset + 1), sourceVertexData_.at(offset + 2)};
    };
    const auto signedDistance = [this](const QVector3D& point) {
        const auto distance = static_cast<double>(QVector3D::dotProduct(sectionNormal_, point)) - sectionOffset_;
        return sectionFlipped_ ? -distance : distance;
    };
    const auto appendSegment = [this](const QVector3D& first, const QVector3D& second) {
        const auto firstIndex = static_cast<quint32>(vertexData_.size() / 3);
        vertexData_.append(first.x()); vertexData_.append(first.y()); vertexData_.append(first.z());
        vertexData_.append(second.x()); vertexData_.append(second.y()); vertexData_.append(second.z());
        indexData_.append(firstIndex); indexData_.append(firstIndex + 1);
    };

    for (qsizetype index = 0; index + 1 < sourceIndexData_.size(); index += 2) {
        const auto first = pointAt(sourceIndexData_.at(index));
        const auto second = pointAt(sourceIndexData_.at(index + 1));
        const auto firstDistance = signedDistance(first);
        const auto secondDistance = signedDistance(second);
        const auto firstInside = firstDistance >= 0.0;
        const auto secondInside = secondDistance >= 0.0;
        if (firstInside && secondInside) {
            appendSegment(first, second);
        } else if (firstInside != secondInside) {
            const auto amount = static_cast<float>(firstDistance / (firstDistance - secondDistance));
            const auto intersection = first + (second - first) * amount;
            appendSegment(firstInside ? first : intersection, firstInside ? intersection : second);
        }
    }
    upload();
}

void CadEdgeGeometry::upload()
{
    const QByteArray vertices(reinterpret_cast<const char*>(vertexData_.constData()),
                              static_cast<int>(vertexData_.size() * static_cast<qsizetype>(sizeof(float))));
    const QByteArray indices(reinterpret_cast<const char*>(indexData_.constData()),
                             static_cast<int>(indexData_.size() * static_cast<qsizetype>(sizeof(quint32))));
    setVertexData(vertices);
    setIndexData(indices);

    QVector3D minimum(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    QVector3D maximum(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
    for (qsizetype index = 0; index + 2 < vertexData_.size(); index += 3) {
        const QVector3D point{vertexData_.at(index), vertexData_.at(index + 1), vertexData_.at(index + 2)};
        minimum.setX(std::min(minimum.x(), point.x()));
        minimum.setY(std::min(minimum.y(), point.y()));
        minimum.setZ(std::min(minimum.z(), point.z()));
        maximum.setX(std::max(maximum.x(), point.x()));
        maximum.setY(std::max(maximum.y(), point.y()));
        maximum.setZ(std::max(maximum.z(), point.z()));
    }
    if (!vertexData_.isEmpty()) setBounds(minimum, maximum);
    update();
}

} // namespace loupe::app::render
