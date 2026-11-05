#pragma once

#include <QQuick3DGeometry>
#include <QVector>
#include <QVector3D>
#include <QMatrix4x4>
#include <QVariant>
#include <QVariantMap>

#include "protocol/GeometryPayload.h"

namespace loupe::app::render {

class CadEdgeGeometry : public QQuick3DGeometry {
    Q_OBJECT

public:
    explicit CadEdgeGeometry(QQuick3DObject* parent = nullptr);

    Q_INVOKABLE bool replaceWorkerEdges(const QByteArray& edgeJson);
    // Takes another edge geometry's edges as its own (the shared edges of a part, for one of its
    // copies), keeping this one's section so it is re-clipped against the new edges.
    Q_INVOKABLE bool copyEdgesFrom(QObject* source);
    Q_INVOKABLE void clearEdges();
    Q_INVOKABLE QVariantMap topologyAtPoint(double x, double y, double z, double tolerance) const;
    Q_INVOKABLE bool copyTopologyFrom(QObject* source, quint32 topologyId);
    // The edge as a measurement pick (a tools::MeasureEntity) in scene coordinates through
    // `toScene`: its polyline, straight or circular (centre and axis), its exact length and
    // radius from the CAD curve, and the point on it nearest `scenePoint`.
    Q_INVOKABLE [[nodiscard]] QVariant edgeEntity(quint32 topologyId, const QMatrix4x4& toScene, const QVector3D& scenePoint) const;
    // A point pick near (x, y, z) in this geometry's frame, snapped as a CAD tool snaps: to an
    // edge's end within `vertexTolerance`, else to a circular edge's centre, else to the
    // nearest point on an edge within `edgeTolerance`. Invalid when no edge is that close.
    Q_INVOKABLE [[nodiscard]] QVariant snapPointAt(double x, double y, double z, double edgeTolerance,
                                                   double vertexTolerance, const QMatrix4x4& toScene) const;
    Q_INVOKABLE bool copyFaceBoundaryFrom(QObject* source, quint32 topologyId);
    Q_INVOKABLE void setSection(bool enabled, int axis, double position, bool flipped);
    Q_INVOKABLE void setSectionPlane(bool enabled, double normalX, double normalY, double normalZ, double offset, bool flipped);
    Q_INVOKABLE void setSectionOptions(bool capEnabled, bool sliceOnly, bool sliceFill = true, bool sliceOutline = true);
    Q_INVOKABLE void configureSection(bool enabled, double normalX, double normalY, double normalZ,
                                      double offset, bool flipped, bool sliceOnly, bool preview);

    [[nodiscard]] int lineCount() const noexcept { return static_cast<int>(indexData_.size() / 2); }

private:
    void rebuildDisplayLines();
    void upload();

    QVector<float> sourceVertexData_;
    QVector<quint32> sourceIndexData_;
    QVector<protocol::TopologyRange> sourceTopology_;
    QVector<float> vertexData_;
    QVector<quint32> indexData_;
    bool sectionEnabled_{false};
    bool sectionSliceOnly_{false};
    bool sectionPreview_{false};
    QVector3D sectionNormal_{0.0F, 0.0F, 1.0F};
    double sectionOffset_{};
    bool sectionFlipped_{false};
};

} // namespace loupe::app::render
