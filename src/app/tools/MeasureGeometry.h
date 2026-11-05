#pragma once

#include <QMatrix4x4>
#include <QMetaType>
#include <QString>
#include <QVector>
#include <QVector3D>

namespace loupe::app::tools {

// What a measurement pick is, with enough of its shape to measure it properly rather than
// by the point the click landed on. Built by the viewport's geometry (a face from the part's
// mesh, an edge from its CAD edge polyline) in scene coordinates, so picks on different parts,
// copies and placements measure against one another directly.
struct MeasureEntity final {
    enum class Kind {
        None,
        SurfacePoint, // a point on a face, where the pointer is
        EdgePoint,    // the nearest point on an edge
        Vertex,       // an edge's end
        CircleCentre, // the centre of a circular edge
        Edge,         // a whole edge
        Face,         // a whole face
    };

    Kind kind{Kind::None};
    quint32 topologyId{};
    QString nodeLabel;
    // Where the pick is: the point itself, or for an edge or face the point on it nearest
    // where it was picked.
    QVector3D point;
    QVector3D normal;

    double lengthMm{};
    double areaMm2{};
    double radiusMm{};

    // A flat face: its plane. A straight edge: its direction. A circular edge: its centre and
    // axis. Curved faces and free-form edges have none, and say so.
    bool planar{false};
    bool linear{false};
    bool circular{false};
    QVector3D planeNormal;
    QVector3D planePoint;
    QVector3D direction;
    QVector3D centre;
    QVector3D axis;
    // The face's triangles (three points each) or the edge's polyline, in scene coordinates.
    // Only a pick that is accepted carries them; a hover does not need them.
    QVector<QVector3D> triangles;
    QVector<QVector3D> polyline;

    [[nodiscard]] bool isValid() const noexcept { return kind != Kind::None; }
    [[nodiscard]] bool isPointLike() const noexcept
    {
        return kind == Kind::SurfacePoint || kind == Kind::EdgePoint || kind == Kind::Vertex || kind == Kind::CircleCentre;
    }
    [[nodiscard]] QString kindName() const;
};

struct ClosestPoints final {
    double distance{-1.0};
    QVector3D first;
    QVector3D second;
    [[nodiscard]] bool valid() const noexcept { return distance >= 0.0; }
};

namespace measure {

// Closest points between two entities' geometry, each reduced to what it is: a point, a
// polyline, or a triangle set. Exact for the geometry given; a curved face or edge is as
// exact as its tessellation.
[[nodiscard]] ClosestPoints closestPoints(const MeasureEntity& first, const MeasureEntity& second);

// Two flat faces whose normals are within `toleranceDegrees` of parallel.
[[nodiscard]] bool parallelPlanes(const MeasureEntity& first, const MeasureEntity& second,
                                  double toleranceDegrees = 0.05);

// The gap between two parallel flat faces, measured square to the first, with the two ends
// of that measurement: from the second face's pick point to its foot on the first plane.
[[nodiscard]] ClosestPoints planeGap(const MeasureEntity& first, const MeasureEntity& second);

// The angle between two flat faces or straight edges, in degrees: between face normals
// 0..180, between lines 0..90, between a line and a plane 0..90. Negative when either has
// no single direction (a curved face, a curved edge).
[[nodiscard]] double angleBetween(const MeasureEntity& first, const MeasureEntity& second);

// The centre of the circle through three points, and that circle's axis; false when they
// are in a line.
[[nodiscard]] bool circleThrough(const QVector3D& a, const QVector3D& b, const QVector3D& c,
                                 QVector3D* centre, QVector3D* axis, double* radius);

// The face's own shape from its triangles (three points each): flat within
// `flatToleranceDegrees` (its plane, then), its area, its area centroid.
void describeFace(MeasureEntity* face, double flatToleranceDegrees = 0.1);

// The edge's own shape from its polyline: straight (its direction), or circular (centre,
// axis) when the given radius is not zero, and its length.
void describeEdge(MeasureEntity* edge);

[[nodiscard]] QVector3D closestPointOnSegment(const QVector3D& point, const QVector3D& start, const QVector3D& end);
[[nodiscard]] QVector3D closestPointOnTriangle(const QVector3D& point, const QVector3D& a, const QVector3D& b, const QVector3D& c);

} // namespace measure

} // namespace loupe::app::tools

Q_DECLARE_METATYPE(loupe::app::tools::MeasureEntity)
