#pragma once

#include <QMatrix4x4>
#include <QVariant>
#include <QVariantList>

#include <QQuick3DGeometry>
#include <QSharedPointer>
#include <QVector>
#include <QQuaternion>
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
    // The whole face rather than one point on it: an area-weighted normal, the area
    // centroid, and how far the face's own triangles tilt away from that normal. A pick
    // gives the normal at a point, which cannot tell a flat face from a curved one, and
    // "normal to this face" is only a meaningful view when the face is flat.
    Q_INVOKABLE [[nodiscard]] QVariantMap faceFrameFor(quint32 topologyId) const;
    // The direction to look along to see a face straight on, for "View normal to face".
    // A flat face gives its exact normal ("plane"), a cylindrical one its axis ("axis"),
    // and anything else nothing, leaving the caller the normal at the picked point. Also
    // the face's area centre to aim at and, when the face has one, its longest straight
    // boundary edge, which the caller can square the view up against.
    Q_INVOKABLE [[nodiscard]] QVariantMap faceViewFor(quint32 topologyId) const;
    Q_INVOKABLE bool copyTopologyFrom(QObject* source, quint32 topologyId);
    // The face as a measurement pick (a tools::MeasureEntity), in scene coordinates through
    // `toScene` (the drawing model's scene transform): flat or not, its plane, its exact area
    // and radius from the CAD face, and -- for a pick being accepted -- its triangles.
    Q_INVOKABLE [[nodiscard]] QVariant faceEntity(quint32 topologyId, const QMatrix4x4& toScene,
                                                  const QVector3D& scenePoint, const QVector3D& sceneNormal,
                                                  bool withTriangles) const;
    Q_INVOKABLE bool copySectionOverlayFrom(QObject* source);
    // One cap for a whole part: the worker splits a part into one mesh per colour, and a
    // cut through any one of them alone is open, so it cannot be filled.
    Q_INVOKABLE bool copySectionOverlayFromSources(const QVariantList& sources);
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
    // The triangles of the mesh as sent, before any section clips it.
    Q_INVOKABLE int sourceTriangleCount() const noexcept { return static_cast<int>(sourceIndexData_.size() / 3); }
    [[nodiscard]] int sectionCapTriangleCount() const noexcept { return sectionCapTriangleCount_; }
    [[nodiscard]] bool sectionBusy() const noexcept { return pendingSectionBuilds_ > 0; }
    // The exact bounds of the vertices as drawn under a node's rigid position and rotation
    // ({minimum, maximum}), or empty when there are none: a shared mesh's bounds for one copy.
    Q_INVOKABLE [[nodiscard]] QVariantMap boundsUnder(const QVector3D& position, const QQuaternion& rotation) const;
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
