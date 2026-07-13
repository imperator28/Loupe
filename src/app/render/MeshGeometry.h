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
    Q_INVOKABLE void setSection(bool enabled, int axis, double position, bool flipped);
    [[nodiscard]] int vertexCount() const noexcept { return vertexData_.size() / 3; }
    [[nodiscard]] int triangleCount() const noexcept { return indexData_.size() / 3; }
    [[nodiscard]] float minimumCoordinate(int axis) const noexcept;

private:
    void upload();
    void rebuildDisplayMesh();

    QVector<float> sourceVertexData_;
    QVector<quint32> sourceIndexData_;
    QVector<float> vertexData_;
    QVector<quint32> indexData_;
    bool sectionEnabled_{false};
    int sectionAxis_{2};
    double sectionPosition_{};
    bool sectionFlipped_{false};
};

} // namespace loupe::app::render
