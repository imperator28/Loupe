#include "app/render/SectionMeshBuilder.h"

#include <QHash>
#include <QSet>
#include <QVector2D>

#include <algorithm>
#include <cmath>
#include <limits>

namespace loupe::app::render {

namespace {

struct Segment final {
    QVector3D start;
    QVector3D end;
};

struct Contour final {
    QVector<QVector3D> points;
    bool closed{};
};

struct EndpointNode final {
    QVector3D point;
    QVector<int> edgeIndices;
};

struct IndexedSegment final {
    int first{};
    int second{};
};

QString cellKey(const qint64 x, const qint64 y, const qint64 z)
{
    return QStringLiteral("%1:%2:%3").arg(x).arg(y).arg(z);
}

float sourceDiagonal(const QVector<float>& vertices)
{
    QVector3D minimum(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                      std::numeric_limits<float>::max());
    QVector3D maximum(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
                      std::numeric_limits<float>::lowest());
    for (qsizetype index = 0; index + 2 < vertices.size(); index += 3) {
        const QVector3D point{vertices.at(index), vertices.at(index + 1), vertices.at(index + 2)};
        minimum.setX(std::min(minimum.x(), point.x()));
        minimum.setY(std::min(minimum.y(), point.y()));
        minimum.setZ(std::min(minimum.z(), point.z()));
        maximum.setX(std::max(maximum.x(), point.x()));
        maximum.setY(std::max(maximum.y(), point.y()));
        maximum.setZ(std::max(maximum.z(), point.z()));
    }
    return vertices.isEmpty() ? 0.0F : (maximum - minimum).length();
}

QVector<Contour> assembleContours(const QVector<Segment>& segments, const float tolerance)
{
    if (segments.isEmpty()) return {};

    const auto toleranceSquared = tolerance * tolerance;
    const auto gridCoordinate = [tolerance](const float value) {
        return static_cast<qint64>(std::floor(value / tolerance));
    };

    QVector<EndpointNode> nodes;
    QHash<QString, QVector<int>> nodesByCell;
    const auto nodeForPoint = [&nodes, &nodesByCell, &gridCoordinate, toleranceSquared](const QVector3D& point) {
        const auto cellX = gridCoordinate(point.x());
        const auto cellY = gridCoordinate(point.y());
        const auto cellZ = gridCoordinate(point.z());
        auto closest = -1;
        auto closestDistance = toleranceSquared;
        for (qint64 x = cellX - 1; x <= cellX + 1; ++x) {
            for (qint64 y = cellY - 1; y <= cellY + 1; ++y) {
                for (qint64 z = cellZ - 1; z <= cellZ + 1; ++z) {
                    const auto candidates = nodesByCell.value(cellKey(x, y, z));
                    for (const auto candidate : candidates) {
                        const auto distance = (nodes.at(candidate).point - point).lengthSquared();
                        if (distance <= closestDistance) {
                            closest = candidate;
                            closestDistance = distance;
                        }
                    }
                }
            }
        }
        if (closest >= 0) return closest;
        const auto index = static_cast<int>(nodes.size());
        nodes.append({point, {}});
        nodesByCell[cellKey(cellX, cellY, cellZ)].append(index);
        return index;
    };

    QVector<IndexedSegment> indexedSegments;
    QSet<quint64> uniqueSegments;
    for (const auto& segment : segments) {
        const auto first = nodeForPoint(segment.start);
        const auto second = nodeForPoint(segment.end);
        if (first == second) continue;
        const auto minimum = std::min(first, second);
        const auto maximum = std::max(first, second);
        const auto key = (static_cast<quint64>(static_cast<quint32>(minimum)) << 32U)
                | static_cast<quint32>(maximum);
        if (uniqueSegments.contains(key)) continue;
        uniqueSegments.insert(key);
        const auto edgeIndex = static_cast<int>(indexedSegments.size());
        indexedSegments.append({first, second});
        nodes[first].edgeIndices.append(edgeIndex);
        nodes[second].edgeIndices.append(edgeIndex);
    }

    QVector<Contour> contours;
    QVector<bool> used(indexedSegments.size(), false);
    const auto otherNode = [&indexedSegments](const int edgeIndex, const int node) {
        const auto& edge = indexedSegments.at(edgeIndex);
        return edge.first == node ? edge.second : edge.first;
    };
    for (int initialEdge = 0; initialEdge < indexedSegments.size(); ++initialEdge) {
        if (used.at(initialEdge)) continue;
        const auto& firstEdge = indexedSegments.at(initialEdge);
        auto start = firstEdge.first;
        if (nodes.at(firstEdge.second).edgeIndices.size() == 1) start = firstEdge.second;
        auto previous = start;
        auto current = otherNode(initialEdge, start);
        used[initialEdge] = true;

        Contour contour;
        contour.points.append(nodes.at(start).point);
        contour.points.append(nodes.at(current).point);
        while (contour.points.size() <= indexedSegments.size() + 1) {
            auto nextEdge = -1;
            for (const auto candidate : nodes.at(current).edgeIndices) {
                if (used.at(candidate)) continue;
                if (otherNode(candidate, current) != previous) {
                    nextEdge = candidate;
                    break;
                }
                if (nextEdge < 0) nextEdge = candidate;
            }
            if (nextEdge < 0) break;
            used[nextEdge] = true;
            const auto next = otherNode(nextEdge, current);
            if (next == start) {
                contour.closed = true;
                break;
            }
            contour.points.append(nodes.at(next).point);
            previous = current;
            current = next;
        }
        if (contour.points.size() >= 2) contours.append(std::move(contour));
    }
    return contours;
}

void simplifyContour(QVector<QVector3D>* points, const QVector3D& normal, const float tolerance, const bool closed)
{
    if (!points || points->size() < 3) return;
    const auto toleranceSquared = tolerance * tolerance;
    for (qsizetype index = points->size() - 1; index > 0; --index) {
        if ((points->at(index) - points->at(index - 1)).lengthSquared() <= toleranceSquared)
            points->removeAt(index);
    }
    if (closed && points->size() > 2
        && (points->constFirst() - points->constLast()).lengthSquared() <= toleranceSquared)
        points->removeLast();

    bool removed = true;
    while (removed && points->size() >= (closed ? 3 : 2)) {
        removed = false;
        const auto start = closed ? 0 : 1;
        const auto end = closed ? points->size() : points->size() - 1;
        for (qsizetype index = start; index < end; ++index) {
            const auto previous = points->at((index + points->size() - 1) % points->size());
            const auto current = points->at(index);
            const auto next = points->at((index + 1) % points->size());
            const auto before = current - previous;
            const auto after = next - current;
            const auto length = before.length() + after.length();
            const auto turn = std::abs(QVector3D::dotProduct(QVector3D::crossProduct(before, after), normal));
            if (length > 0.0F && turn <= tolerance * length) {
                points->removeAt(index);
                removed = true;
                break;
            }
        }
    }
}

bool appendFilledContour(SectionBuildResult* result, const Contour& contour,
                         const QVector3D& horizontal, const QVector3D& vertical)
{
    if (!result || !contour.closed || contour.points.size() < 3) return false;

    QVector<QVector2D> points;
    points.reserve(contour.points.size());
    for (const auto& point : contour.points)
        points.append({QVector3D::dotProduct(point, horizontal), QVector3D::dotProduct(point, vertical)});

    double signedArea{};
    for (qsizetype index = 0; index < points.size(); ++index) {
        const auto& first = points.at(index);
        const auto& second = points.at((index + 1) % points.size());
        signedArea += static_cast<double>(first.x()) * second.y()
                - static_cast<double>(second.x()) * first.y();
    }
    constexpr auto epsilon = 0.000001F;
    if (std::abs(signedArea) < epsilon) return false;

    const auto appendTriangle = [result](const QVector3D& first, const QVector3D& second, const QVector3D& third) {
        const auto append = [result](const QVector3D& point) {
            const auto index = static_cast<quint32>(result->vertices.size() / 3);
            result->vertices.append(point.x());
            result->vertices.append(point.y());
            result->vertices.append(point.z());
            result->indices.append(index);
        };
        append(first); append(second); append(third);
        append(third); append(second); append(first);
    };
    const auto contains = [](const QVector2D& point, const QVector2D& first,
                             const QVector2D& second, const QVector2D& third, const float orientation) {
        const auto cross = [](const QVector2D& left, const QVector2D& middle, const QVector2D& right) {
            return (middle.x() - left.x()) * (right.y() - left.y())
                    - (middle.y() - left.y()) * (right.x() - left.x());
        };
        return orientation * cross(first, second, point) >= -0.00001F
                && orientation * cross(second, third, point) >= -0.00001F
                && orientation * cross(third, first, point) >= -0.00001F;
    };

    QVector<int> remaining;
    remaining.reserve(contour.points.size());
    for (int index = 0; index < contour.points.size(); ++index) remaining.append(index);
    const auto orientation = signedArea > 0.0 ? 1.0F : -1.0F;
    auto appended = false;
    while (remaining.size() > 2) {
        auto clipped = false;
        for (qsizetype index = 0; index < remaining.size(); ++index) {
            const auto previous = remaining.at((index + remaining.size() - 1) % remaining.size());
            const auto current = remaining.at(index);
            const auto next = remaining.at((index + 1) % remaining.size());
            const auto cross = (points.at(current).x() - points.at(previous).x())
                    * (points.at(next).y() - points.at(previous).y())
                    - (points.at(current).y() - points.at(previous).y())
                    * (points.at(next).x() - points.at(previous).x());
            if (orientation * cross <= epsilon) continue;
            auto hasInteriorPoint = false;
            for (const auto candidate : remaining) {
                if (candidate != previous && candidate != current && candidate != next
                    && contains(points.at(candidate), points.at(previous), points.at(current),
                                points.at(next), orientation)) {
                    hasInteriorPoint = true;
                    break;
                }
            }
            if (hasInteriorPoint) continue;
            appendTriangle(contour.points.at(previous), contour.points.at(current), contour.points.at(next));
            remaining.removeAt(index);
            clipped = true;
            appended = true;
            break;
        }
        if (!clipped) return false;
    }
    return appended;
}

void appendRoundedOutline(SectionBuildResult* result, const QVector<Contour>& contours,
                          const QVector3D& normal, const float halfWidth, const QVector3D& depthLift)
{
    if (!result || halfWidth <= 0.0F) return;
    const auto reference = std::abs(normal.z()) < 0.9F ? QVector3D{0.0F, 0.0F, 1.0F}
                                                        : QVector3D{0.0F, 1.0F, 0.0F};
    const auto horizontal = QVector3D::crossProduct(reference, normal).normalized();
    const auto vertical = QVector3D::crossProduct(normal, horizontal).normalized();
    const auto appendVertex = [result](const QVector3D& point) {
        const auto index = static_cast<quint32>(result->vertices.size() / 3);
        result->vertices.append(point.x());
        result->vertices.append(point.y());
        result->vertices.append(point.z());
        result->indices.append(index);
    };
    const auto appendDoubleSidedTriangle = [&appendVertex](const QVector3D& first, const QVector3D& second,
                                                           const QVector3D& third) {
        appendVertex(first); appendVertex(second); appendVertex(third);
        appendVertex(third); appendVertex(second); appendVertex(first);
    };
    const auto appendDisk = [&appendDoubleSidedTriangle, &horizontal, &vertical, halfWidth, &depthLift](const QVector3D& center) {
        constexpr int segments = 10;
        constexpr float pi = 3.14159265358979323846F;
        const auto liftedCenter = center + depthLift;
        for (int index = 0; index < segments; ++index) {
            const auto firstAngle = static_cast<float>(index) * 2.0F * pi / segments;
            const auto secondAngle = static_cast<float>(index + 1) * 2.0F * pi / segments;
            const auto first = liftedCenter + horizontal * (std::cos(firstAngle) * halfWidth)
                    + vertical * (std::sin(firstAngle) * halfWidth);
            const auto second = liftedCenter + horizontal * (std::cos(secondAngle) * halfWidth)
                    + vertical * (std::sin(secondAngle) * halfWidth);
            appendDoubleSidedTriangle(liftedCenter, first, second);
        }
    };

    for (const auto& contour : contours) {
        if (contour.points.size() < 2) continue;
        const auto segmentCount = contour.closed ? contour.points.size() : contour.points.size() - 1;
        for (qsizetype index = 0; index < segmentCount; ++index) {
            const auto& first = contour.points.at(index);
            const auto& second = contour.points.at((index + 1) % contour.points.size());
            const auto direction = (second - first).normalized();
            const auto offset = QVector3D::crossProduct(normal, direction).normalized() * halfWidth;
            if (offset.isNull()) continue;
            const auto firstLeft = first - offset + depthLift;
            const auto firstRight = first + offset + depthLift;
            const auto secondLeft = second - offset + depthLift;
            const auto secondRight = second + offset + depthLift;
            appendDoubleSidedTriangle(firstLeft, secondLeft, secondRight);
            appendDoubleSidedTriangle(firstLeft, secondRight, firstRight);
        }
        for (const auto& point : contour.points) appendDisk(point);
    }
}

} // namespace

SectionBuildResult buildSectionOverlay(const SectionBuildRequest& request)
{
    SectionBuildResult result;
    if (!request.source || request.source->vertices.isEmpty() || request.source->indices.isEmpty()
        || request.normal.isNull())
        return result;

    const auto& sourceVertices = request.source->vertices;
    const auto& sourceIndices = request.source->indices;
    const auto normal = request.normal.normalized();
    const auto diagonal = sourceDiagonal(sourceVertices);
    const auto tolerance = std::max(0.00001F, diagonal * 0.000001F);
    const auto toleranceSquared = tolerance * tolerance;
    const auto pointAt = [&sourceVertices](const quint32 index) {
        const auto offset = static_cast<qsizetype>(index) * 3;
        return QVector3D(sourceVertices.at(offset), sourceVertices.at(offset + 1), sourceVertices.at(offset + 2));
    };
    const auto distance = [&request, &normal](const QVector3D& point) {
        return static_cast<double>(QVector3D::dotProduct(normal, point)) - request.offset;
    };
    const auto inside = [&request](const double value) { return request.flipped ? value <= 0.0 : value >= 0.0; };

    QVector<Segment> segments;
    for (qsizetype triangle = 0; triangle + 2 < sourceIndices.size(); triangle += 3) {
        const QVector<QVector3D> input{pointAt(sourceIndices.at(triangle)), pointAt(sourceIndices.at(triangle + 1)),
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
            if (intersections.isEmpty()
                || (intersections.constLast() - intersection).lengthSquared() > toleranceSquared)
                intersections.append(intersection);
        }
        if (intersections.size() == 2
            && (intersections.at(0) - intersections.at(1)).lengthSquared() > toleranceSquared)
            segments.append({intersections.at(0), intersections.at(1)});
    }

    auto contours = assembleContours(segments, tolerance);
    for (auto& contour : contours) simplifyContour(&contour.points, normal, tolerance, contour.closed);

    if (request.sliceOnly && !request.sliceFill) {
        if (request.sliceOutline) {
            const auto halfWidth = request.outlineWidth > 0.0F
                    ? request.outlineWidth * 0.5F : std::max(0.01F, diagonal * 0.00035F);
            const auto removedNormal = normal * (request.flipped ? 1.0F : -1.0F);
            appendRoundedOutline(&result, contours, normal, halfWidth,
                                 removedNormal * std::max(diagonal * 0.000002F, halfWidth * 0.025F));
        }
        result.capTriangleCount = static_cast<int>(result.indices.size() / 3);
        return result;
    }

    if ((request.capEnabled || (request.sliceOnly && request.sliceFill)) && !contours.isEmpty()) {
        const auto reference = std::abs(normal.z()) < 0.9F ? QVector3D{0.0F, 0.0F, 1.0F}
                                                            : QVector3D{0.0F, 1.0F, 0.0F};
        const auto horizontal = QVector3D::crossProduct(reference, normal).normalized();
        const auto vertical = QVector3D::crossProduct(normal, horizontal).normalized();
        for (const auto& contour : contours) appendFilledContour(&result, contour, horizontal, vertical);
    }
    result.fillIndexCount = static_cast<int>(result.indices.size());

    if (request.sliceOnly && request.sliceOutline) {
        const auto halfWidth = request.outlineWidth > 0.0F
                ? request.outlineWidth * 0.5F : std::max(0.01F, diagonal * 0.00035F);
        const auto removedNormal = normal * (request.flipped ? 1.0F : -1.0F);
        appendRoundedOutline(&result, contours, normal, halfWidth,
                             removedNormal * std::max(diagonal * 0.000002F, halfWidth * 0.025F));
    }
    result.capTriangleCount = request.sliceOnly ? static_cast<int>(result.indices.size() / 3)
                                                : result.fillIndexCount / 3;
    return result;
}

} // namespace loupe::app::render
