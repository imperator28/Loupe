#include "app/render/SectionMeshBuilder.h"

#include <QVector2D>

#include <algorithm>
#include <cmath>
#include <limits>

namespace loupe::app::render {

SectionBuildResult buildSectionOverlay(const SectionBuildRequest& request)
{
    SectionBuildResult result;
    if (!request.source || request.source->vertices.isEmpty() || request.source->indices.isEmpty()
        || request.normal.isNull())
        return result;

    const auto& sourceVertices = request.source->vertices;
    const auto& sourceIndices = request.source->indices;
    const auto normal = request.normal.normalized();
    const auto pointAt = [&sourceVertices](const quint32 index) {
        const auto offset = static_cast<qsizetype>(index) * 3;
        return QVector3D(sourceVertices.at(offset), sourceVertices.at(offset + 1),
                         sourceVertices.at(offset + 2));
    };
    const auto distance = [&request, &normal](const QVector3D& point) {
        return static_cast<double>(QVector3D::dotProduct(normal, point)) - request.offset;
    };
    const auto inside = [&request](const double value) { return request.flipped ? value <= 0.0 : value >= 0.0; };
    const auto near = [](const QVector3D& left, const QVector3D& right) {
        return (left - right).lengthSquared() <= 0.00000001F;
    };
    const auto appendVertex = [&result](const QVector3D& point) {
        const auto index = static_cast<quint32>(result.vertices.size() / 3);
        result.vertices.append(point.x());
        result.vertices.append(point.y());
        result.vertices.append(point.z());
        result.indices.append(index);
    };

    struct Segment { QVector3D start; QVector3D end; };
    QVector<Segment> segments;
    for (qsizetype triangle = 0; triangle + 2 < sourceIndices.size(); triangle += 3) {
        const QVector<QVector3D> input{pointAt(sourceIndices.at(triangle)),
                                       pointAt(sourceIndices.at(triangle + 1)),
                                       pointAt(sourceIndices.at(triangle + 2))};
        QVector<QVector3D> intersections;
        for (qsizetype index = 0; index < input.size(); ++index) {
            const auto& start = input.at(index);
            const auto& end = input.at((index + 1) % input.size());
            const auto startDistance = distance(start);
            const auto endDistance = distance(end);
            if (inside(startDistance) == inside(endDistance)) continue;
            const auto amount = static_cast<float>(startDistance / (startDistance - endDistance));
            const auto intersection = start + (end - start) * amount;
            if (intersections.isEmpty() || !near(intersections.constLast(), intersection))
                intersections.append(intersection);
        }
        if (intersections.size() == 2 && !near(intersections.at(0), intersections.at(1)))
            segments.append({intersections.at(0), intersections.at(1)});
    }

    const auto appendSliceOutline = [&sourceVertices, &normal, &appendVertex](const QVector<Segment>& sliceSegments) {
        QVector3D minimum(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                          std::numeric_limits<float>::max());
        QVector3D maximum(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
                          std::numeric_limits<float>::lowest());
        for (qsizetype index = 0; index + 2 < sourceVertices.size(); index += 3) {
            const QVector3D point{sourceVertices.at(index), sourceVertices.at(index + 1),
                                  sourceVertices.at(index + 2)};
            minimum.setX(std::min(minimum.x(), point.x()));
            minimum.setY(std::min(minimum.y(), point.y()));
            minimum.setZ(std::min(minimum.z(), point.z()));
            maximum.setX(std::max(maximum.x(), point.x()));
            maximum.setY(std::max(maximum.y(), point.y()));
            maximum.setZ(std::max(maximum.z(), point.z()));
        }
        const auto halfWidth = std::max(0.01F, (maximum - minimum).length() * 0.00035F);
        for (const auto& segment : sliceSegments) {
            const auto direction = (segment.end - segment.start).normalized();
            const auto offset = QVector3D::crossProduct(normal, direction).normalized() * halfWidth;
            if (offset.isNull()) continue;
            const auto firstLeft = segment.start - offset;
            const auto firstRight = segment.start + offset;
            const auto secondLeft = segment.end - offset;
            const auto secondRight = segment.end + offset;
            appendVertex(firstLeft); appendVertex(secondLeft); appendVertex(secondRight);
            appendVertex(firstLeft); appendVertex(secondRight); appendVertex(firstRight);
            appendVertex(secondRight); appendVertex(secondLeft); appendVertex(firstLeft);
            appendVertex(firstRight); appendVertex(secondRight); appendVertex(firstLeft);
        }
    };
    const auto sliceSegments = segments;

    if (request.sliceOnly && !request.sliceFill) {
        if (request.sliceOutline) appendSliceOutline(sliceSegments);
        result.capTriangleCount = static_cast<int>(result.indices.size() / 3);
        return result;
    }

    if ((request.capEnabled || (request.sliceOnly && request.sliceFill)) && !segments.isEmpty()) {
        QVector<float> capVertices;
        const auto appendCapTriangle = [&capVertices](const QVector3D& first, const QVector3D& second,
                                                       const QVector3D& third) {
            const auto append = [&capVertices](const QVector3D& point) {
                capVertices.append(point.x()); capVertices.append(point.y()); capVertices.append(point.z());
            };
            append(first); append(second); append(third);
            append(third); append(second); append(first);
        };
        const auto reference = std::abs(normal.z()) < 0.9F ? QVector3D{0.0F, 0.0F, 1.0F}
                                                            : QVector3D{0.0F, 1.0F, 0.0F};
        const auto horizontal = QVector3D::crossProduct(reference, normal).normalized();
        const auto vertical = QVector3D::crossProduct(normal, horizontal);
        constexpr auto epsilon = 0.0001F;
        while (!segments.isEmpty()) {
            QVector<QVector3D> polygon{segments.constFirst().start, segments.constFirst().end};
            segments.removeFirst();
            bool extended = true;
            bool closed = false;
            while (extended && polygon.size() < 4096) {
                extended = false;
                const auto end = polygon.constLast();
                for (qsizetype index = 0; index < segments.size(); ++index) {
                    if (near(segments.at(index).start, end)) {
                        polygon.append(segments.at(index).end); segments.removeAt(index); extended = true; break;
                    }
                    if (near(segments.at(index).end, end)) {
                        polygon.append(segments.at(index).start); segments.removeAt(index); extended = true; break;
                    }
                }
                if (polygon.size() > 2 && near(polygon.constLast(), polygon.constFirst())) {
                    polygon.removeLast(); closed = true; break;
                }
            }
            if (polygon.size() < 3 || !closed) continue;
            QVector<QVector2D> points;
            points.reserve(polygon.size());
            for (const auto& point : polygon)
                points.append({QVector3D::dotProduct(point, horizontal), QVector3D::dotProduct(point, vertical)});
            double signedArea{};
            for (qsizetype index = 0; index < points.size(); ++index) {
                const auto& first = points.at(index);
                const auto& second = points.at((index + 1) % points.size());
                signedArea += static_cast<double>(first.x()) * second.y()
                        - static_cast<double>(second.x()) * first.y();
            }
            if (std::abs(signedArea) < epsilon) continue;
            QVector<int> remaining;
            for (int index = 0; index < polygon.size(); ++index) remaining.append(index);
            const auto contains = [](const QVector2D& point, const QVector2D& first,
                                     const QVector2D& second, const QVector2D& third, const float orientation) {
                const auto cross = [](const QVector2D& a, const QVector2D& b, const QVector2D& c) {
                    return (b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x());
                };
                return orientation * cross(first, second, point) >= -0.00001F
                        && orientation * cross(second, third, point) >= -0.00001F
                        && orientation * cross(third, first, point) >= -0.00001F;
            };
            const auto orientation = signedArea > 0.0 ? 1.0F : -1.0F;
            while (remaining.size() > 2) {
                bool clipped = false;
                for (qsizetype index = 0; index < remaining.size(); ++index) {
                    const auto previous = remaining.at((index + remaining.size() - 1) % remaining.size());
                    const auto current = remaining.at(index);
                    const auto next = remaining.at((index + 1) % remaining.size());
                    const auto cross = (points.at(current).x() - points.at(previous).x())
                            * (points.at(next).y() - points.at(previous).y())
                            - (points.at(current).y() - points.at(previous).y())
                            * (points.at(next).x() - points.at(previous).x());
                    if (orientation * cross <= epsilon) continue;
                    bool hasInteriorPoint = false;
                    for (const auto candidate : remaining) {
                        if (candidate != previous && candidate != current && candidate != next
                            && contains(points.at(candidate), points.at(previous), points.at(current),
                                        points.at(next), orientation)) {
                            hasInteriorPoint = true;
                            break;
                        }
                    }
                    if (hasInteriorPoint) continue;
                    appendCapTriangle(polygon.at(previous), polygon.at(current), polygon.at(next));
                    remaining.removeAt(index);
                    clipped = true;
                    break;
                }
                if (!clipped) break;
            }
        }
        result.capTriangleCount = static_cast<int>(capVertices.size() / 9);
        for (qsizetype index = 0; index + 2 < capVertices.size(); index += 3)
            appendVertex({capVertices.at(index), capVertices.at(index + 1), capVertices.at(index + 2)});
    }
    if (request.sliceOnly && request.sliceOutline) appendSliceOutline(sliceSegments);
    if (request.sliceOnly) result.capTriangleCount = static_cast<int>(result.indices.size() / 3);
    return result;
}

} // namespace loupe::app::render
