#include "app/tools/MeasureGeometry.h"

#include <QCoreApplication>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace loupe::app::tools {

QString MeasureEntity::kindName() const
{
    switch (kind) {
    case Kind::None: return {};
    case Kind::SurfacePoint: return QCoreApplication::translate("Measure", "Point on face");
    case Kind::EdgePoint: return QCoreApplication::translate("Measure", "Point on edge");
    case Kind::Vertex: return QCoreApplication::translate("Measure", "Vertex");
    case Kind::Midpoint: return QCoreApplication::translate("Measure", "Midpoint");
    case Kind::CircleCentre: return QCoreApplication::translate("Measure", "Circle centre");
    case Kind::Edge:
        if (circular) return QCoreApplication::translate("Measure", "Circular edge");
        if (linear) return QCoreApplication::translate("Measure", "Straight edge");
        return QCoreApplication::translate("Measure", "Curved edge");
    case Kind::Face:
        if (planar) return QCoreApplication::translate("Measure", "Flat face");
        if (radiusMm > 0.0) return QCoreApplication::translate("Measure", "Round face");
        return QCoreApplication::translate("Measure", "Curved face");
    }
    return {};
}

namespace measure {
namespace {

constexpr double kRadiansToDegrees = 180.0 / std::numbers::pi;

struct Segment final {
    QVector3D start;
    QVector3D end;
};

struct Triangle final {
    QVector3D a;
    QVector3D b;
    QVector3D c;
    QVector3D centre;
    float radius{};
};

// Each entity as what it is geometrically: a point, a polyline, or a triangle set.
struct Primitives final {
    QVector<QVector3D> points;
    QVector<Segment> segments;
    QVector<Triangle> triangles;
};

Primitives primitivesOf(const MeasureEntity& entity)
{
    Primitives result;
    if (entity.kind == MeasureEntity::Kind::Face && entity.triangles.size() >= 3) {
        result.triangles.reserve(entity.triangles.size() / 3);
        for (qsizetype index = 0; index + 2 < entity.triangles.size(); index += 3) {
            Triangle triangle{entity.triangles.at(index), entity.triangles.at(index + 1), entity.triangles.at(index + 2), {}, {}};
            triangle.centre = (triangle.a + triangle.b + triangle.c) / 3.0F;
            triangle.radius = std::max({(triangle.a - triangle.centre).length(), (triangle.b - triangle.centre).length(),
                                        (triangle.c - triangle.centre).length()});
            result.triangles.append(triangle);
        }
    } else if (entity.kind == MeasureEntity::Kind::Edge && entity.polyline.size() >= 2) {
        result.segments.reserve(entity.polyline.size() - 1);
        for (qsizetype index = 0; index + 1 < entity.polyline.size(); ++index)
            result.segments.append({entity.polyline.at(index), entity.polyline.at(index + 1)});
    } else {
        result.points.append(entity.point);
    }
    return result;
}

// Ericson, Real-Time Collision Detection, 5.1.9.
void segmentSegment(const Segment& first, const Segment& second, QVector3D* onFirst, QVector3D* onSecond)
{
    const auto d1 = first.end - first.start;
    const auto d2 = second.end - second.start;
    const auto r = first.start - second.start;
    const auto a = static_cast<double>(QVector3D::dotProduct(d1, d1));
    const auto e = static_cast<double>(QVector3D::dotProduct(d2, d2));
    const auto f = static_cast<double>(QVector3D::dotProduct(d2, r));
    constexpr double epsilon = 1e-18;
    double s{};
    double t{};
    if (a <= epsilon && e <= epsilon) {
        *onFirst = first.start;
        *onSecond = second.start;
        return;
    }
    if (a <= epsilon) {
        t = std::clamp(f / e, 0.0, 1.0);
    } else {
        const auto c = static_cast<double>(QVector3D::dotProduct(d1, r));
        if (e <= epsilon) {
            s = std::clamp(-c / a, 0.0, 1.0);
        } else {
            const auto b = static_cast<double>(QVector3D::dotProduct(d1, d2));
            const auto denominator = a * e - b * b;
            s = denominator > epsilon ? std::clamp((b * f - c * e) / denominator, 0.0, 1.0) : 0.0;
            t = (b * s + f) / e;
            if (t < 0.0) {
                t = 0.0;
                s = std::clamp(-c / a, 0.0, 1.0);
            } else if (t > 1.0) {
                t = 1.0;
                s = std::clamp((b - c) / a, 0.0, 1.0);
            }
        }
    }
    *onFirst = first.start + d1 * static_cast<float>(s);
    *onSecond = second.start + d2 * static_cast<float>(t);
}

// Where the segment passes through the triangle, if it does (Moller-Trumbore).
bool segmentCrossesTriangle(const Segment& segment, const Triangle& triangle, QVector3D* where)
{
    const auto direction = segment.end - segment.start;
    const auto edge1 = triangle.b - triangle.a;
    const auto edge2 = triangle.c - triangle.a;
    const auto p = QVector3D::crossProduct(direction, edge2);
    const auto determinant = static_cast<double>(QVector3D::dotProduct(edge1, p));
    if (std::abs(determinant) < 1e-18) return false;
    const auto inverse = 1.0 / determinant;
    const auto s = segment.start - triangle.a;
    const auto u = static_cast<double>(QVector3D::dotProduct(s, p)) * inverse;
    if (u < 0.0 || u > 1.0) return false;
    const auto q = QVector3D::crossProduct(s, edge1);
    const auto v = static_cast<double>(QVector3D::dotProduct(direction, q)) * inverse;
    if (v < 0.0 || u + v > 1.0) return false;
    const auto t = static_cast<double>(QVector3D::dotProduct(edge2, q)) * inverse;
    if (t < 0.0 || t > 1.0) return false;
    *where = segment.start + direction * static_cast<float>(t);
    return true;
}

class Search final {
public:
    void consider(const QVector3D& onFirst, const QVector3D& onSecond)
    {
        const auto distance = static_cast<double>((onSecond - onFirst).lengthSquared());
        if (distance < bestSquared_) {
            bestSquared_ = distance;
            result_.first = onFirst;
            result_.second = onSecond;
        }
    }
    [[nodiscard]] double bound() const { return std::sqrt(bestSquared_); }
    [[nodiscard]] ClosestPoints result() const
    {
        auto result = result_;
        result.distance = std::isfinite(bestSquared_) ? std::sqrt(bestSquared_) : -1.0;
        return result;
    }

private:
    double bestSquared_{std::numeric_limits<double>::infinity()};
    ClosestPoints result_;
};

void pointToTriangles(const QVector3D& point, const QVector<Triangle>& triangles, Search* search, const bool swap)
{
    for (const auto& triangle : triangles) {
        if (static_cast<double>((point - triangle.centre).length() - triangle.radius) > search->bound()) continue;
        const auto onTriangle = closestPointOnTriangle(point, triangle.a, triangle.b, triangle.c);
        swap ? search->consider(onTriangle, point) : search->consider(point, onTriangle);
    }
}

void segmentToTriangles(const Segment& segment, const QVector<Triangle>& triangles, Search* search, const bool swap)
{
    const auto centre = (segment.start + segment.end) * 0.5F;
    const auto reach = (segment.end - segment.start).length() * 0.5F;
    for (const auto& triangle : triangles) {
        if (static_cast<double>((centre - triangle.centre).length() - triangle.radius - reach) > search->bound()) continue;
        QVector3D where;
        if (segmentCrossesTriangle(segment, triangle, &where)) {
            search->consider(where, where);
            continue;
        }
        const auto consider = [&](const QVector3D& onSegment, const QVector3D& onTriangle) {
            swap ? search->consider(onTriangle, onSegment) : search->consider(onSegment, onTriangle);
        };
        consider(segment.start, closestPointOnTriangle(segment.start, triangle.a, triangle.b, triangle.c));
        consider(segment.end, closestPointOnTriangle(segment.end, triangle.a, triangle.b, triangle.c));
        for (const auto& side : {Segment{triangle.a, triangle.b}, Segment{triangle.b, triangle.c}, Segment{triangle.c, triangle.a}}) {
            QVector3D onSegment;
            QVector3D onSide;
            segmentSegment(segment, side, &onSegment, &onSide);
            consider(onSegment, onSide);
        }
    }
}

} // namespace

QVector3D closestPointOnSegment(const QVector3D& point, const QVector3D& start, const QVector3D& end)
{
    const auto direction = end - start;
    const auto lengthSquared = direction.lengthSquared();
    if (lengthSquared <= 0.0F) return start;
    const auto amount = std::clamp(QVector3D::dotProduct(point - start, direction) / lengthSquared, 0.0F, 1.0F);
    return start + direction * amount;
}

// Ericson, Real-Time Collision Detection, 5.1.5.
QVector3D closestPointOnTriangle(const QVector3D& point, const QVector3D& a, const QVector3D& b, const QVector3D& c)
{
    const auto ab = b - a;
    const auto ac = c - a;
    const auto ap = point - a;
    const auto d1 = QVector3D::dotProduct(ab, ap);
    const auto d2 = QVector3D::dotProduct(ac, ap);
    if (d1 <= 0.0F && d2 <= 0.0F) return a;
    const auto bp = point - b;
    const auto d3 = QVector3D::dotProduct(ab, bp);
    const auto d4 = QVector3D::dotProduct(ac, bp);
    if (d3 >= 0.0F && d4 <= d3) return b;
    const auto vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0F && d1 >= 0.0F && d3 <= 0.0F) return a + ab * (d1 / (d1 - d3));
    const auto cp = point - c;
    const auto d5 = QVector3D::dotProduct(ab, cp);
    const auto d6 = QVector3D::dotProduct(ac, cp);
    if (d6 >= 0.0F && d5 <= d6) return c;
    const auto vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0F && d2 >= 0.0F && d6 <= 0.0F) return a + ac * (d2 / (d2 - d6));
    const auto va = d3 * d6 - d5 * d4;
    if (va <= 0.0F && (d4 - d3) >= 0.0F && (d5 - d6) >= 0.0F)
        return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
    const auto denominator = 1.0F / (va + vb + vc);
    return a + ab * (vb * denominator) + ac * (vc * denominator);
}

ClosestPoints closestPoints(const MeasureEntity& first, const MeasureEntity& second)
{
    if (!first.isValid() || !second.isValid()) return {};
    const auto a = primitivesOf(first);
    const auto b = primitivesOf(second);
    Search search;
    // Points first: a cheap bound that prunes most of the pairs that follow.
    for (const auto& p : a.points) {
        for (const auto& q : b.points) search.consider(p, q);
        for (const auto& segment : b.segments) search.consider(p, closestPointOnSegment(p, segment.start, segment.end));
        pointToTriangles(p, b.triangles, &search, false);
    }
    for (const auto& q : b.points) {
        for (const auto& segment : a.segments) search.consider(closestPointOnSegment(q, segment.start, segment.end), q);
        pointToTriangles(q, a.triangles, &search, true);
    }
    for (const auto& s : a.segments) {
        for (const auto& t : b.segments) {
            QVector3D onFirst;
            QVector3D onSecond;
            segmentSegment(s, t, &onFirst, &onSecond);
            search.consider(onFirst, onSecond);
        }
        segmentToTriangles(s, b.triangles, &search, false);
    }
    for (const auto& t : b.segments) segmentToTriangles(t, a.triangles, &search, true);
    if (!a.triangles.isEmpty() && !b.triangles.isEmpty()) {
        // Face to face. A seed from the triangle centres bounds the search, then each pair
        // the bound cannot rule out is measured: vertex to triangle both ways, and side to
        // side, which together find the closest points of two triangles that do not cross.
        for (const auto& t : a.triangles) {
            for (const auto& u : b.triangles) search.consider(t.centre, u.centre);
        }
        for (const auto& t : a.triangles) {
            for (const auto& u : b.triangles) {
                if (static_cast<double>((t.centre - u.centre).length() - t.radius - u.radius) > search.bound()) continue;
                for (const auto& vertex : {t.a, t.b, t.c}) search.consider(vertex, closestPointOnTriangle(vertex, u.a, u.b, u.c));
                for (const auto& vertex : {u.a, u.b, u.c}) search.consider(closestPointOnTriangle(vertex, t.a, t.b, t.c), vertex);
                for (const auto& side : {Segment{t.a, t.b}, Segment{t.b, t.c}, Segment{t.c, t.a}}) {
                    QVector3D where;
                    if (segmentCrossesTriangle(side, u, &where)) search.consider(where, where);
                    for (const auto& other : {Segment{u.a, u.b}, Segment{u.b, u.c}, Segment{u.c, u.a}}) {
                        QVector3D onFirst;
                        QVector3D onSecond;
                        segmentSegment(side, other, &onFirst, &onSecond);
                        search.consider(onFirst, onSecond);
                    }
                }
            }
        }
    }
    return search.result();
}

bool parallelPlanes(const MeasureEntity& first, const MeasureEntity& second, const double toleranceDegrees)
{
    if (!first.planar || !second.planar || first.planeNormal.isNull() || second.planeNormal.isNull()) return false;
    const auto alignment = std::abs(static_cast<double>(QVector3D::dotProduct(first.planeNormal.normalized(), second.planeNormal.normalized())));
    return std::acos(std::clamp(alignment, 0.0, 1.0)) * kRadiansToDegrees <= toleranceDegrees;
}

ClosestPoints planeGap(const MeasureEntity& first, const MeasureEntity& second)
{
    if (!parallelPlanes(first, second)) return {};
    const auto normal = first.planeNormal.normalized();
    const auto offset = static_cast<double>(QVector3D::dotProduct(second.point - first.planePoint, normal));
    ClosestPoints result;
    result.second = second.point;
    result.first = second.point - normal * static_cast<float>(offset);
    result.distance = std::abs(offset);
    return result;
}

double angleBetween(const MeasureEntity& first, const MeasureEntity& second)
{
    const auto directionOf = [](const MeasureEntity& entity, bool* line) {
        *line = false;
        if (entity.kind == MeasureEntity::Kind::Face && entity.planar) return entity.planeNormal.normalized();
        if (entity.kind == MeasureEntity::Kind::Edge && entity.linear) {
            *line = true;
            return entity.direction.normalized();
        }
        return QVector3D{};
    };
    bool firstLine = false;
    bool secondLine = false;
    const auto a = directionOf(first, &firstLine);
    const auto b = directionOf(second, &secondLine);
    if (a.isNull() || b.isNull()) return -1.0;
    const auto dot = std::clamp(static_cast<double>(QVector3D::dotProduct(a, b)), -1.0, 1.0);
    if (!firstLine && !secondLine) return std::acos(dot) * kRadiansToDegrees;
    if (firstLine && secondLine) return std::acos(std::abs(dot)) * kRadiansToDegrees;
    // A line against a plane: the line's angle out of the plane.
    return std::asin(std::abs(dot)) * kRadiansToDegrees;
}

AngleArc angleArc(const MeasureEntity& first, const MeasureEntity& second)
{
    AngleArc arc;
    const auto isPlane = [](const MeasureEntity& entity) { return entity.kind == MeasureEntity::Kind::Face && entity.planar; };
    const auto isLine = [](const MeasureEntity& entity) { return entity.kind == MeasureEntity::Kind::Edge && entity.linear; };
    // Where each pick sits on its own face or line.
    const auto anchorOf = [](const MeasureEntity& entity) { return entity.point; };
    const auto finish = [&arc](const QVector3D& centre, QVector3D from, QVector3D to, const float reach) {
        if (from.isNull() || to.isNull()) return;
        from.normalize();
        to.normalize();
        arc.valid = true;
        arc.centre = centre;
        arc.from = from;
        arc.to = to;
        arc.degrees = std::acos(std::clamp(static_cast<double>(QVector3D::dotProduct(from, to)), -1.0, 1.0)) * kRadiansToDegrees;
        arc.radius = std::max(reach * 0.55F, 1e-3F);
    };
    // The point of a line (through `point` along `direction`) nearest `target`.
    const auto onLine = [](const QVector3D& point, const QVector3D& direction, const QVector3D& target) {
        return point + direction * QVector3D::dotProduct(target - point, direction);
    };

    if (isPlane(first) && isPlane(second)) {
        const auto n1 = first.planeNormal.normalized();
        const auto n2 = second.planeNormal.normalized();
        const auto axis = QVector3D::crossProduct(n1, n2);
        if (axis.lengthSquared() < 1e-8F) return arc;
        const auto direction = axis.normalized();
        // A point on both planes: solve n1.x = d1, n2.x = d2 on the line through the origin
        // square to both normals, then slide it to nearest the picks.
        const auto d1 = QVector3D::dotProduct(n1, first.planePoint);
        const auto d2 = QVector3D::dotProduct(n2, second.planePoint);
        const auto onBoth = (QVector3D::crossProduct(n2, direction) * d1 + QVector3D::crossProduct(direction, n1) * d2)
                            / QVector3D::dotProduct(direction, axis);
        const auto p1 = anchorOf(first);
        const auto p2 = anchorOf(second);
        const auto centre = onLine(onBoth, direction, (p1 + p2) * 0.5F);
        // Within each plane, square to the line, toward that face's pick.
        auto from = QVector3D::crossProduct(direction, n1);
        if (QVector3D::dotProduct(from, p1 - centre) < 0.0F) from = -from;
        auto to = QVector3D::crossProduct(direction, n2);
        if (QVector3D::dotProduct(to, p2 - centre) < 0.0F) to = -to;
        const auto reach = std::min(QVector3D::dotProduct(from.normalized(), p1 - centre), QVector3D::dotProduct(to.normalized(), p2 - centre));
        finish(centre, from, to, std::max(reach, 0.25F * (p1 - p2).length()));
        return arc;
    }
    if (isLine(first) && isLine(second)) {
        const auto d1 = first.direction.normalized();
        const auto d2 = second.direction.normalized();
        if (QVector3D::crossProduct(d1, d2).lengthSquared() < 1e-8F) return arc;
        QVector3D on1;
        QVector3D on2;
        // Lines, not segments: edges that would meet if carried on.
        const auto far = static_cast<float>(1e5);
        segmentSegment({first.point - d1 * far, first.point + d1 * far}, {second.point - d2 * far, second.point + d2 * far}, &on1, &on2);
        const auto centre = (on1 + on2) * 0.5F;
        // Toward the far end of each edge from where they meet, so a corner reads as its corner.
        const auto farEnd = [&centre](const MeasureEntity& edge) {
            const auto& a = edge.polyline.isEmpty() ? edge.point : edge.polyline.constFirst();
            const auto& b = edge.polyline.isEmpty() ? edge.point : edge.polyline.constLast();
            return (a - centre).lengthSquared() > (b - centre).lengthSquared() ? a : b;
        };
        const auto e1 = farEnd(first);
        const auto e2 = farEnd(second);
        auto from = d1;
        if (QVector3D::dotProduct(from, e1 - centre) < 0.0F) from = -from;
        auto to = d2;
        if (QVector3D::dotProduct(to, e2 - centre) < 0.0F) to = -to;
        finish(centre, from, to, std::min((e1 - centre).length(), (e2 - centre).length()));
        return arc;
    }
    const auto* line = isLine(first) ? &first : isLine(second) ? &second : nullptr;
    const auto* plane = isPlane(first) ? &first : isPlane(second) ? &second : nullptr;
    if (line && plane) {
        const auto d = line->direction.normalized();
        const auto n = plane->planeNormal.normalized();
        const auto along = QVector3D::dotProduct(d, n);
        const auto inPlane = d - n * along;
        if (std::abs(along) < 1e-6F || inPlane.lengthSquared() < 1e-12F) return arc;
        // Where the line meets the plane.
        const auto t = QVector3D::dotProduct(plane->planePoint - line->point, n) / along;
        const auto centre = line->point + d * t;
        auto from = d;
        const auto& a = line->polyline.isEmpty() ? line->point : line->polyline.constFirst();
        const auto& b = line->polyline.isEmpty() ? line->point : line->polyline.constLast();
        const auto end = (a - centre).lengthSquared() > (b - centre).lengthSquared() ? a : b;
        if (QVector3D::dotProduct(from, end - centre) < 0.0F) from = -from;
        const auto to = from - n * QVector3D::dotProduct(from, n);
        finish(centre, line == &first ? from : to, line == &first ? to : from, (end - centre).length());
    }
    return arc;
}

bool circleThrough(const QVector3D& a, const QVector3D& b, const QVector3D& c, QVector3D* centre, QVector3D* axis,
                   double* radius)
{
    const auto ab = b - a;
    const auto ac = c - a;
    const auto normal = QVector3D::crossProduct(ab, ac);
    const auto normalSquared = static_cast<double>(normal.lengthSquared());
    if (normalSquared <= 1e-20) return false;
    const auto offset = (QVector3D::crossProduct(normal, ab) * ac.lengthSquared()
                         + QVector3D::crossProduct(ac, normal) * ab.lengthSquared())
                        / static_cast<float>(2.0 * normalSquared);
    if (centre) *centre = a + offset;
    if (axis) *axis = normal.normalized();
    if (radius) *radius = static_cast<double>(offset.length());
    return true;
}

void describeFace(MeasureEntity* face, const double flatToleranceDegrees)
{
    if (!face || face->triangles.size() < 3) return;
    QVector3D weighted;
    QVector3D centroid;
    double area{};
    QVector<QVector3D> normals;
    normals.reserve(face->triangles.size() / 3);
    for (qsizetype index = 0; index + 2 < face->triangles.size(); index += 3) {
        const auto& a = face->triangles.at(index);
        const auto& b = face->triangles.at(index + 1);
        const auto& c = face->triangles.at(index + 2);
        const auto cross = QVector3D::crossProduct(b - a, c - a);
        const auto doubleArea = static_cast<double>(cross.length());
        if (doubleArea <= 0.0) continue;
        weighted += cross;
        centroid += (a + b + c) / 3.0F * static_cast<float>(doubleArea * 0.5);
        area += doubleArea * 0.5;
        normals.append(cross / static_cast<float>(doubleArea));
    }
    if (area <= 0.0 || weighted.isNull()) return;
    const auto normal = weighted.normalized();
    const auto tolerance = std::cos(flatToleranceDegrees / kRadiansToDegrees);
    face->planar = std::all_of(normals.cbegin(), normals.cend(), [&normal, tolerance](const QVector3D& value) {
        return static_cast<double>(QVector3D::dotProduct(value, normal)) >= tolerance;
    });
    face->planeNormal = face->planar ? normal : QVector3D{};
    face->planePoint = centroid / static_cast<float>(area);
    if (face->areaMm2 <= 0.0) face->areaMm2 = area;
}

void describeEdge(MeasureEntity* edge)
{
    if (!edge || edge->polyline.size() < 2) return;
    double length{};
    for (qsizetype index = 0; index + 1 < edge->polyline.size(); ++index)
        length += static_cast<double>((edge->polyline.at(index + 1) - edge->polyline.at(index)).length());
    if (edge->lengthMm <= 0.0) edge->lengthMm = length;
    const auto& first = edge->polyline.constFirst();
    const auto& last = edge->polyline.constLast();
    const auto chord = last - first;
    const auto chordLength = static_cast<double>(chord.length());
    // Straight: every point within a micron per metre of the chord.
    if (chordLength > 0.0 && std::abs(chordLength - length) <= std::max(1e-6, length * 1e-6)) {
        edge->linear = true;
        edge->direction = chord.normalized();
    }
    if (edge->radiusMm > 0.0 && edge->polyline.size() >= 3) {
        const auto count = edge->polyline.size();
        const auto closed = chordLength <= length * 1e-4;
        const auto& a = edge->polyline.at(0);
        const auto& b = edge->polyline.at(closed ? count / 3 : count / 2);
        const auto& c = edge->polyline.at(closed ? (2 * count) / 3 : count - 1);
        QVector3D centre;
        QVector3D axis;
        double radius{};
        if (circleThrough(a, b, c, &centre, &axis, &radius)) {
            edge->circular = true;
            edge->centre = centre;
            edge->axis = axis;
        }
    }
}

} // namespace measure
} // namespace loupe::app::tools
