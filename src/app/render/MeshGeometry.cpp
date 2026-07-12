#include "app/render/MeshGeometry.h"

#include <QVector3D>

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
    QByteArray vertices(reinterpret_cast<const char*>(mesh.vertexData.constData()), static_cast<int>(mesh.vertexData.size() * sizeof(float)));
    QByteArray indices(reinterpret_cast<const char*>(mesh.indexData.constData()), static_cast<int>(mesh.indexData.size() * sizeof(quint32)));
    setVertexData(vertices);
    setIndexData(indices);

    QVector3D minimum(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    QVector3D maximum(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
    for (qsizetype index = 0; index + 2 < mesh.vertexData.size(); index += 3) {
        const QVector3D point(mesh.vertexData.at(index), mesh.vertexData.at(index + 1), mesh.vertexData.at(index + 2));
        minimum.setX(std::min(minimum.x(), point.x()));
        minimum.setY(std::min(minimum.y(), point.y()));
        minimum.setZ(std::min(minimum.z(), point.z()));
        maximum.setX(std::max(maximum.x(), point.x()));
        maximum.setY(std::max(maximum.y(), point.y()));
        maximum.setZ(std::max(maximum.z(), point.z()));
    }
    if (!mesh.vertexData.isEmpty()) {
        setBounds(minimum, maximum);
    }
    update();
}

} // namespace loupe::app::render
