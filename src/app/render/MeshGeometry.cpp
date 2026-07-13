#include "app/render/MeshGeometry.h"

#include <QVector3D>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <limits>
#include <algorithm>

namespace loupe::app::render {

MeshGeometry::MeshGeometry(QQuick3DObject* parent)
    : QQuick3DGeometry(parent)
{
    setPrimitiveType(PrimitiveType::Triangles);
    setStride(static_cast<int>(3 * sizeof(float)));
    addAttribute(Attribute::PositionSemantic, 0, Attribute::F32Type);
    addAttribute(Attribute::IndexSemantic, 0, Attribute::U32Type);
}

void MeshGeometry::replaceMesh(const MeshData& mesh)
{
    sourceVertexData_ = mesh.vertexData;
    sourceIndexData_ = mesh.indexData;
    rebuildDisplayMesh();
}

bool MeshGeometry::appendWorkerMesh(const QByteArray& meshJson)
{
    const auto payload = QJsonDocument::fromJson(meshJson);
    if (!payload.isObject()) return false;
    const auto vertices = payload.object().value(QStringLiteral("vertices")).toArray();
    const auto indices = payload.object().value(QStringLiteral("indices")).toArray();
    if (vertices.isEmpty() || vertices.size() % 3 != 0 || indices.isEmpty() || indices.size() % 3 != 0) return false;
    const auto offset = static_cast<quint32>(sourceVertexData_.size() / 3);
    for (const auto& value : vertices) {
        if (!value.isDouble()) return false;
        sourceVertexData_.append(static_cast<float>(value.toDouble()));
    }
    for (const auto& value : indices) {
        if (!value.isDouble() || value.toInt(-1) < 0 || value.toInt() >= vertices.size() / 3) return false;
        sourceIndexData_.append(offset + static_cast<quint32>(value.toInt()));
    }
    rebuildDisplayMesh();
    return true;
}

void MeshGeometry::clearMesh()
{
    sourceVertexData_.clear();
    sourceIndexData_.clear();
    vertexData_.clear();
    indexData_.clear();
    upload();
}

void MeshGeometry::setSection(const bool enabled, const int axis, const double position, const bool flipped)
{
    const auto normalizedAxis = std::clamp(axis, 0, 2);
    const QVector3D normal = normalizedAxis == 0 ? QVector3D{1.0F, 0.0F, 0.0F}
                           : normalizedAxis == 1 ? QVector3D{0.0F, 1.0F, 0.0F}
                                                 : QVector3D{0.0F, 0.0F, 1.0F};
    setSectionPlane(enabled, normal.x(), normal.y(), normal.z(), position, flipped);
}

void MeshGeometry::setSectionPlane(const bool enabled, const double normalX, const double normalY, const double normalZ, const double offset, const bool flipped)
{
    const QVector3D normal{static_cast<float>(normalX), static_cast<float>(normalY), static_cast<float>(normalZ)};
    if (normal.isNull()) return;
    const auto normalized = normal.normalized();
    if (sectionEnabled_ == enabled && sectionNormal_ == normalized && qFuzzyCompare(sectionOffset_ + 1.0, offset + 1.0) && sectionFlipped_ == flipped) return;
    sectionEnabled_ = enabled;
    sectionNormal_ = normalized;
    sectionOffset_ = offset;
    sectionFlipped_ = flipped;
    rebuildDisplayMesh();
}

float MeshGeometry::minimumCoordinate(const int axis) const noexcept
{
    if (vertexData_.isEmpty()) return std::numeric_limits<float>::quiet_NaN();
    const auto normalizedAxis = std::clamp(axis, 0, 2);
    float value = std::numeric_limits<float>::max();
    for (qsizetype index = normalizedAxis; index < vertexData_.size(); index += 3) value = std::min(value, vertexData_.at(index));
    return value;
}

void MeshGeometry::rebuildDisplayMesh()
{
    vertexData_.clear();
    indexData_.clear();
    if (!sectionEnabled_) {
        vertexData_ = sourceVertexData_;
        indexData_ = sourceIndexData_;
        upload();
        return;
    }

    const auto pointAt = [this](const quint32 index) {
        const auto offset = static_cast<qsizetype>(index) * 3;
        return QVector3D(sourceVertexData_.at(offset), sourceVertexData_.at(offset + 1), sourceVertexData_.at(offset + 2));
    };
    const auto distance = [this](const QVector3D& point) { return static_cast<double>(QVector3D::dotProduct(sectionNormal_, point)) - sectionOffset_; };
    const auto inside = [this](const double value) { return sectionFlipped_ ? value <= 0.0 : value >= 0.0; };
    const auto appendVertex = [this](const QVector3D& point) {
        const auto index = static_cast<quint32>(vertexData_.size() / 3);
        vertexData_.append(point.x()); vertexData_.append(point.y()); vertexData_.append(point.z());
        indexData_.append(index);
    };
    for (qsizetype triangle = 0; triangle + 2 < sourceIndexData_.size(); triangle += 3) {
        QVector<QVector3D> input{pointAt(sourceIndexData_.at(triangle)), pointAt(sourceIndexData_.at(triangle + 1)), pointAt(sourceIndexData_.at(triangle + 2))};
        QVector<QVector3D> output;
        for (qsizetype index = 0; index < input.size(); ++index) {
            const auto& start = input.at(index);
            const auto& end = input.at((index + 1) % input.size());
            const auto startDistance = distance(start);
            const auto endDistance = distance(end);
            const auto startInside = inside(startDistance);
            const auto endInside = inside(endDistance);
            if (startInside != endInside) {
                const auto t = static_cast<float>(startDistance / (startDistance - endDistance));
                output.append(start + (end - start) * t);
            }
            if (endInside) output.append(end);
        }
        for (qsizetype index = 1; index + 1 < output.size(); ++index) {
            appendVertex(output.at(0)); appendVertex(output.at(index)); appendVertex(output.at(index + 1));
        }
    }
    upload();
}

void MeshGeometry::upload()
{
    QByteArray vertices(reinterpret_cast<const char*>(vertexData_.constData()), static_cast<int>(vertexData_.size() * sizeof(float)));
    QByteArray indices(reinterpret_cast<const char*>(indexData_.constData()), static_cast<int>(indexData_.size() * sizeof(quint32)));
    setVertexData(vertices);
    setIndexData(indices);

    QVector3D minimum(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    QVector3D maximum(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
    for (qsizetype index = 0; index + 2 < vertexData_.size(); index += 3) {
        const QVector3D point(vertexData_.at(index), vertexData_.at(index + 1), vertexData_.at(index + 2));
        minimum.setX(std::min(minimum.x(), point.x()));
        minimum.setY(std::min(minimum.y(), point.y()));
        minimum.setZ(std::min(minimum.z(), point.z()));
        maximum.setX(std::max(maximum.x(), point.x()));
        maximum.setY(std::max(maximum.y(), point.y()));
        maximum.setZ(std::max(maximum.z(), point.z()));
    }
    if (!vertexData_.isEmpty()) {
        setBounds(minimum, maximum);
    }
    update();
}

} // namespace loupe::app::render
