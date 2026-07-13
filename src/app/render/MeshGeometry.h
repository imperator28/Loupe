#pragma once

#include <QQuick3DGeometry>
#include <QVector>

namespace loupe::app::render {

struct MeshData {
    QString definitionId;
    QVector<float> vertexData;
    QVector<quint32> indexData;
};

class MeshGeometry : public QQuick3DGeometry {
    Q_OBJECT
public:
    explicit MeshGeometry(QQuick3DObject* parent = nullptr);
    void replaceMesh(const MeshData& mesh);
    Q_INVOKABLE bool appendWorkerMesh(const QByteArray& meshJson);
    Q_INVOKABLE void clearMesh();
    [[nodiscard]] int vertexCount() const noexcept { return vertexData_.size() / 3; }
    [[nodiscard]] int triangleCount() const noexcept { return indexData_.size() / 3; }

private:
    void upload();

    QVector<float> vertexData_;
    QVector<quint32> indexData_;
};

} // namespace loupe::app::render
