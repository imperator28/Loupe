#pragma once

#include <QQuick3DGeometry>
#include <QVector>
#include <QVector3D>

namespace loupe::app::render {

class CadEdgeGeometry : public QQuick3DGeometry {
    Q_OBJECT

public:
    explicit CadEdgeGeometry(QQuick3DObject* parent = nullptr);

    Q_INVOKABLE bool replaceWorkerEdges(const QByteArray& edgeJson);
    Q_INVOKABLE void clearEdges();
    Q_INVOKABLE void setSection(bool enabled, int axis, double position, bool flipped);
    Q_INVOKABLE void setSectionPlane(bool enabled, double normalX, double normalY, double normalZ, double offset, bool flipped);
    Q_INVOKABLE void setSectionOptions(bool capEnabled, bool sliceOnly, bool sliceFill = true, bool sliceOutline = true);

    [[nodiscard]] int lineCount() const noexcept { return static_cast<int>(indexData_.size() / 2); }

private:
    void rebuildDisplayLines();
    void upload();

    QVector<float> sourceVertexData_;
    QVector<quint32> sourceIndexData_;
    QVector<float> vertexData_;
    QVector<quint32> indexData_;
    bool sectionEnabled_{false};
    bool sectionSliceOnly_{false};
    QVector3D sectionNormal_{0.0F, 0.0F, 1.0F};
    double sectionOffset_{};
    bool sectionFlipped_{false};
};

} // namespace loupe::app::render
