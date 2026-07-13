#include "app/render/MeshGeometry.h"

#include <QVector3D>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <limits>

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
    vertexData_ = mesh.vertexData;
    indexData_ = mesh.indexData;
    upload();
}

bool MeshGeometry::appendWorkerMesh(const QByteArray& meshJson)
{
    const auto payload = QJsonDocument::fromJson(meshJson);
    if (!payload.isObject()) return false;
    const auto vertices = payload.object().value(QStringLiteral("vertices")).toArray();
    const auto indices = payload.object().value(QStringLiteral("indices")).toArray();
    if (vertices.isEmpty() || vertices.size() % 3 != 0 || indices.isEmpty() || indices.size() % 3 != 0) return false;
    const auto offset = static_cast<quint32>(vertexCount());
    for (const auto& value : vertices) {
        if (!value.isDouble()) return false;
        vertexData_.append(static_cast<float>(value.toDouble()));
    }
    for (const auto& value : indices) {
        if (!value.isDouble() || value.toInt(-1) < 0 || value.toInt() >= vertices.size() / 3) return false;
        indexData_.append(offset + static_cast<quint32>(value.toInt()));
    }
    upload();
    return true;
}

void MeshGeometry::clearMesh()
{
    vertexData_.clear();
    indexData_.clear();
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
