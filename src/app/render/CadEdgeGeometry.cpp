#include "app/render/CadEdgeGeometry.h"

#include "protocol/GeometryPayload.h"
#include "protocol/ProtocolTypes.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

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
    rebuildDisplayLines();
    return true;
}

void CadEdgeGeometry::clearEdges()
{
    sourceVertexData_.clear();
    sourceIndexData_.clear();
    vertexData_.clear();
    indexData_.clear();
    upload();
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
    const QVector3D normal{static_cast<float>(normalX), static_cast<float>(normalY), static_cast<float>(normalZ)};
    if (normal.isNull()) return;
    const auto normalized = normal.normalized();
    if (sectionEnabled_ == enabled && sectionNormal_ == normalized
        && qFuzzyCompare(sectionOffset_ + 1.0, offset + 1.0) && sectionFlipped_ == flipped) return;
    sectionEnabled_ = enabled;
    sectionNormal_ = normalized;
    sectionOffset_ = offset;
    sectionFlipped_ = flipped;
    rebuildDisplayLines();
}

void CadEdgeGeometry::setSectionOptions(const bool, const bool sliceOnly, const bool, const bool)
{
    if (sectionSliceOnly_ == sliceOnly) return;
    sectionSliceOnly_ = sliceOnly;
    rebuildDisplayLines();
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
    if (sectionSliceOnly_) {
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
