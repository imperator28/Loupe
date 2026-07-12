#pragma once

#include <QQuick3DGeometry>
#include <QVector>

namespace loupe::app::render {

struct MeshData {
    QString definitionId;
    QVector<float> vertexData;
    QVector<quint32> indexData;
};

class MeshGeometry final : public QQuick3DGeometry {
    Q_OBJECT
public:
    explicit MeshGeometry(QQuick3DObject* parent = nullptr);
    void replaceMesh(const MeshData& mesh);
};

} // namespace loupe::app::render
