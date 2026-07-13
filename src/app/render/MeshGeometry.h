#pragma once

#include <QQuick3DGeometry>
#include <QVector>
#include <QVector3D>

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
    Q_INVOKABLE void setSectionPlane(bool enabled, double normalX, double normalY, double normalZ, double offset, bool flipped);
    Q_INVOKABLE void setSectionOptions(bool capEnabled, bool sliceOnly);
    [[nodiscard]] int vertexCount() const noexcept { return vertexData_.size() / 3; }
    [[nodiscard]] int triangleCount() const noexcept { return indexData_.size() / 3; }
    [[nodiscard]] int sectionCapTriangleCount() const noexcept { return sectionCapTriangleCount_; }
    [[nodiscard]] float minimumCoordinate(int axis) const noexcept;

private:
    void upload();
    void rebuildDisplayMesh();

    QVector<float> sourceVertexData_;
    QVector<quint32> sourceIndexData_;
    QVector<float> vertexData_;
    QVector<quint32> indexData_;
    bool sectionEnabled_{false};
    bool sectionCapEnabled_{true};
    bool sectionSliceOnly_{false};
    int sectionCapTriangleCount_{};
    QVector3D sectionNormal_{0.0F, 0.0F, 1.0F};
    double sectionOffset_{};
    bool sectionFlipped_{false};
};

} // namespace loupe::app::render
