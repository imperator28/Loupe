#pragma once

#include <QQuick3DGeometry>
#include <QSharedPointer>
#include <QVector>
#include <QVector3D>
#include <QVariantMap>

#include "protocol/GeometryPayload.h"

namespace loupe::app::render {

class CadEdgeGeometry;
struct SectionSourceData;

struct MeshData {
    QString definitionId;
    QVector<float> vertexData;
    QVector<quint32> indexData;
    QVector<float> normalData;
};

class MeshGeometry : public QQuick3DGeometry {
    Q_OBJECT
    Q_PROPERTY(bool sectionBusy READ sectionBusy NOTIFY sectionBusyChanged)
public:
    explicit MeshGeometry(QQuick3DObject* parent = nullptr);
    void replaceMesh(const MeshData& mesh);
    Q_INVOKABLE bool appendWorkerMesh(const QByteArray& meshJson);
    Q_INVOKABLE bool replaceWorkerMesh(const QByteArray& meshJson);
    Q_INVOKABLE QVariantMap topologyAtPoint(double x, double y, double z) const;
    Q_INVOKABLE bool copyTopologyFrom(QObject* source, quint32 topologyId);
    Q_INVOKABLE bool copySectionOverlayFrom(QObject* source);
    Q_INVOKABLE void clearMesh();
    Q_INVOKABLE void setSection(bool enabled, int axis, double position, bool flipped);
    Q_INVOKABLE void setSectionPlane(bool enabled, double normalX, double normalY, double normalZ, double offset, bool flipped);
    Q_INVOKABLE void setSectionOptions(bool capEnabled, bool sliceOnly, bool sliceFill = true, bool sliceOutline = true);
    Q_INVOKABLE void configureSection(bool enabled, double normalX, double normalY, double normalZ,
                                      double offset, bool flipped, bool capEnabled, bool sliceOnly,
                                      bool sliceFill, bool sliceOutline, bool preview,
                                      double outlineWidth = 0.0);
    [[nodiscard]] int vertexCount() const noexcept { return static_cast<int>(vertexData_.size() / 3); }
    [[nodiscard]] int triangleCount() const noexcept { return static_cast<int>(indexData_.size() / 3); }
    [[nodiscard]] int sectionCapTriangleCount() const noexcept { return sectionCapTriangleCount_; }
    [[nodiscard]] bool sectionBusy() const noexcept { return pendingSectionBuilds_ > 0; }
    Q_INVOKABLE [[nodiscard]] float minimumCoordinate(int axis) const noexcept;
    Q_INVOKABLE [[nodiscard]] float maximumCoordinate(int axis) const noexcept;

signals:
    void sectionBusyChanged();

private:
    friend class CadEdgeGeometry;

    void upload();
    void rebuildDisplayMesh();
    void rebuildSourceNormals();
    void rebuildDisplayNormals();
    void startSectionOverlayBuild();

    QVector<float> sourceVertexData_;
    QVector<float> sourceNormalData_;
    QVector<quint32> sourceIndexData_;
    QVector<protocol::TopologyRange> sourceTopology_;
    QSharedPointer<const SectionSourceData> sectionSource_;
    QVector<float> vertexData_;
    QVector<float> normalData_;
    QVector<quint32> indexData_;
    bool sectionEnabled_{false};
    bool sectionCapEnabled_{true};
    bool sectionSliceOnly_{false};
    bool sectionSliceFill_{true};
    bool sectionSliceOutline_{true};
    bool sectionPreview_{false};
    bool sectionOverlayOnly_{false};
    quint64 sectionBuildGeneration_{};
    int pendingSectionBuilds_{};
    int sectionCapTriangleCount_{};
    int sectionFillIndexCount_{};
    QVector3D sectionNormal_{0.0F, 0.0F, 1.0F};
    double sectionOffset_{};
    bool sectionFlipped_{false};
    double sectionOutlineWidth_{};
};

} // namespace loupe::app::render
