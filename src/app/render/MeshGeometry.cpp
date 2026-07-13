#include "app/render/MeshGeometry.h"

#include <QVector3D>
#include <QVector2D>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <limits>
#include <algorithm>
#include <cmath>

namespace loupe::app::render {

MeshGeometry::MeshGeometry(QQuick3DObject* parent)
    : QQuick3DGeometry(parent)
{
    setPrimitiveType(PrimitiveType::Triangles);
    setStride(static_cast<int>(3 * sizeof(float)));
    addAttribute(Attribute::PositionSemantic, 0, Attribute::F32Type);
    addAttribute(Attribute::IndexSemantic, 0, Attribute::U32Type);
}

void MeshGeometry::replaceMesh(const MeshData& mesh)
{
    sourceVertexData_ = mesh.vertexData;
    sourceIndexData_ = mesh.indexData;
    rebuildDisplayMesh();
}

bool MeshGeometry::appendWorkerMesh(const QByteArray& meshJson)
{
    const auto payload = QJsonDocument::fromJson(meshJson);
    if (!payload.isObject()) return false;
    const auto vertices = payload.object().value(QStringLiteral("vertices")).toArray();
    const auto indices = payload.object().value(QStringLiteral("indices")).toArray();
    if (vertices.isEmpty() || vertices.size() % 3 != 0 || indices.isEmpty() || indices.size() % 3 != 0) return false;
    const auto offset = static_cast<quint32>(sourceVertexData_.size() / 3);
    for (const auto& value : vertices) {
        if (!value.isDouble()) return false;
        sourceVertexData_.append(static_cast<float>(value.toDouble()));
    }
    for (const auto& value : indices) {
        if (!value.isDouble() || value.toInt(-1) < 0 || value.toInt() >= vertices.size() / 3) return false;
        sourceIndexData_.append(offset + static_cast<quint32>(value.toInt()));
    }
    rebuildDisplayMesh();
    return true;
}

void MeshGeometry::clearMesh()
{
    sourceVertexData_.clear();
    sourceIndexData_.clear();
    vertexData_.clear();
    indexData_.clear();
    upload();
}

void MeshGeometry::setSection(const bool enabled, const int axis, const double position, const bool flipped)
{
    const auto normalizedAxis = std::clamp(axis, 0, 2);
    const QVector3D normal = normalizedAxis == 0 ? QVector3D{1.0F, 0.0F, 0.0F}
                           : normalizedAxis == 1 ? QVector3D{0.0F, 1.0F, 0.0F}
                                                 : QVector3D{0.0F, 0.0F, 1.0F};
    setSectionPlane(enabled, normal.x(), normal.y(), normal.z(), position, flipped);
}

void MeshGeometry::setSectionPlane(const bool enabled, const double normalX, const double normalY, const double normalZ, const double offset, const bool flipped)
{
    const QVector3D normal{static_cast<float>(normalX), static_cast<float>(normalY), static_cast<float>(normalZ)};
    if (normal.isNull()) return;
    const auto normalized = normal.normalized();
    if (sectionEnabled_ == enabled && sectionNormal_ == normalized && qFuzzyCompare(sectionOffset_ + 1.0, offset + 1.0) && sectionFlipped_ == flipped) return;
    sectionEnabled_ = enabled;
    sectionNormal_ = normalized;
    sectionOffset_ = offset;
    sectionFlipped_ = flipped;
    rebuildDisplayMesh();
}

void MeshGeometry::setSectionOptions(const bool capEnabled, const bool sliceOnly)
{
    if (sectionCapEnabled_ == capEnabled && sectionSliceOnly_ == sliceOnly) return;
    sectionCapEnabled_ = capEnabled;
    sectionSliceOnly_ = sliceOnly;
    rebuildDisplayMesh();
}

float MeshGeometry::minimumCoordinate(const int axis) const noexcept
{
    if (vertexData_.isEmpty()) return std::numeric_limits<float>::quiet_NaN();
    const auto normalizedAxis = std::clamp(axis, 0, 2);
    float value = std::numeric_limits<float>::max();
    for (qsizetype index = normalizedAxis; index < vertexData_.size(); index += 3) value = std::min(value, vertexData_.at(index));
    return value;
}

void MeshGeometry::rebuildDisplayMesh()
{
    vertexData_.clear();
    indexData_.clear();
    sectionCapTriangleCount_ = 0;
    if (!sectionEnabled_) {
        vertexData_ = sourceVertexData_;
        indexData_ = sourceIndexData_;
        upload();
        return;
    }

    const auto pointAt = [this](const quint32 index) {
        const auto offset = static_cast<qsizetype>(index) * 3;
        return QVector3D(sourceVertexData_.at(offset), sourceVertexData_.at(offset + 1), sourceVertexData_.at(offset + 2));
    };
    const auto distance = [this](const QVector3D& point) { return static_cast<double>(QVector3D::dotProduct(sectionNormal_, point)) - sectionOffset_; };
    const auto inside = [this](const double value) { return sectionFlipped_ ? value <= 0.0 : value >= 0.0; };
    const auto appendVertex = [this](const QVector3D& point) {
        const auto index = static_cast<quint32>(vertexData_.size() / 3);
        vertexData_.append(point.x()); vertexData_.append(point.y()); vertexData_.append(point.z());
        indexData_.append(index);
    };

    struct Segment { QVector3D start; QVector3D end; };
    QVector<Segment> segments;
    constexpr auto epsilon = 0.0001F;
    const auto near = [](const QVector3D& left, const QVector3D& right) { return (left - right).lengthSquared() <= 0.00000001F; };
    for (qsizetype triangle = 0; triangle + 2 < sourceIndexData_.size(); triangle += 3) {
        QVector<QVector3D> input{pointAt(sourceIndexData_.at(triangle)), pointAt(sourceIndexData_.at(triangle + 1)), pointAt(sourceIndexData_.at(triangle + 2))};
        QVector<QVector3D> intersections;
        QVector<QVector3D> output;
        for (qsizetype index = 0; index < input.size(); ++index) {
            const auto& start = input.at(index);
            const auto& end = input.at((index + 1) % input.size());
            const auto startDistance = distance(start);
            const auto endDistance = distance(end);
            const auto startInside = inside(startDistance);
            const auto endInside = inside(endDistance);
            if (startInside != endInside) {
                const auto t = static_cast<float>(startDistance / (startDistance - endDistance));
                const auto intersection = start + (end - start) * t;
                output.append(intersection);
                if (intersections.isEmpty() || !near(intersections.constLast(), intersection)) intersections.append(intersection);
            }
            if (endInside) output.append(end);
        }
        for (qsizetype index = 1; index + 1 < output.size(); ++index) {
            appendVertex(output.at(0)); appendVertex(output.at(index)); appendVertex(output.at(index + 1));
        }
        if (intersections.size() == 2 && !near(intersections.at(0), intersections.at(1))) segments.append({intersections.at(0), intersections.at(1)});
    }

    if (sectionCapEnabled_ && !segments.isEmpty()) {
        QVector<float> capVertices;
        const auto appendCapTriangle = [&capVertices](const QVector3D& first, const QVector3D& second, const QVector3D& third) {
            const auto append = [&capVertices](const QVector3D& point) { capVertices.append(point.x()); capVertices.append(point.y()); capVertices.append(point.z()); };
            append(first); append(second); append(third);
            append(third); append(second); append(first);
        };
        const auto projectionAxes = [this]() {
            QVector3D reference = std::abs(sectionNormal_.z()) < 0.9F ? QVector3D{0.0F, 0.0F, 1.0F} : QVector3D{0.0F, 1.0F, 0.0F};
            const auto horizontal = QVector3D::crossProduct(reference, sectionNormal_).normalized();
            return qMakePair(horizontal, QVector3D::crossProduct(sectionNormal_, horizontal));
        };
        const auto [horizontal, vertical] = projectionAxes();
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
                if (polygon.size() > 2 && near(polygon.constLast(), polygon.constFirst())) { polygon.removeLast(); closed = true; break; }
            }
            if (polygon.size() < 3 || !closed) continue;
            QVector<QVector2D> points;
            points.reserve(polygon.size());
            for (const auto& point : polygon) points.append({QVector3D::dotProduct(point, horizontal), QVector3D::dotProduct(point, vertical)});
            double signedArea{};
            for (qsizetype index = 0; index < points.size(); ++index) {
                const auto& first = points.at(index); const auto& second = points.at((index + 1) % points.size());
                signedArea += static_cast<double>(first.x()) * second.y() - static_cast<double>(second.x()) * first.y();
            }
            if (std::abs(signedArea) < epsilon) continue;
            QVector<int> remaining; for (int index = 0; index < polygon.size(); ++index) remaining.append(index);
            const auto contains = [](const QVector2D& point, const QVector2D& first, const QVector2D& second, const QVector2D& third, const float orientation) {
                const auto cross = [](const QVector2D& a, const QVector2D& b, const QVector2D& c) { return (b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x()); };
                return orientation * cross(first, second, point) >= -0.00001F && orientation * cross(second, third, point) >= -0.00001F && orientation * cross(third, first, point) >= -0.00001F;
            };
            const auto orientation = signedArea > 0.0 ? 1.0F : -1.0F;
            while (remaining.size() > 2) {
                bool clipped = false;
                for (qsizetype index = 0; index < remaining.size(); ++index) {
                    const auto previous = remaining.at((index + remaining.size() - 1) % remaining.size()); const auto current = remaining.at(index); const auto next = remaining.at((index + 1) % remaining.size());
                    const auto cross = (points.at(current).x() - points.at(previous).x()) * (points.at(next).y() - points.at(previous).y()) - (points.at(current).y() - points.at(previous).y()) * (points.at(next).x() - points.at(previous).x());
                    if (orientation * cross <= epsilon) continue;
                    bool hasInteriorPoint = false;
                    for (const auto candidate : remaining) if (candidate != previous && candidate != current && candidate != next && contains(points.at(candidate), points.at(previous), points.at(current), points.at(next), orientation)) { hasInteriorPoint = true; break; }
                    if (hasInteriorPoint) continue;
                    appendCapTriangle(polygon.at(previous), polygon.at(current), polygon.at(next));
                    remaining.removeAt(index); clipped = true; break;
                }
                if (!clipped) break;
            }
        }
        sectionCapTriangleCount_ = capVertices.size() / 9;
        if (sectionSliceOnly_) { vertexData_ = capVertices; indexData_.clear(); for (quint32 index = 0; index < static_cast<quint32>(vertexData_.size() / 3); ++index) indexData_.append(index); }
        else for (qsizetype index = 0; index + 2 < capVertices.size(); index += 3) appendVertex({capVertices.at(index), capVertices.at(index + 1), capVertices.at(index + 2)});
    }
    upload();
}

void MeshGeometry::upload()
{
    QByteArray vertices(reinterpret_cast<const char*>(vertexData_.constData()), static_cast<int>(vertexData_.size() * sizeof(float)));
    QByteArray indices(reinterpret_cast<const char*>(indexData_.constData()), static_cast<int>(indexData_.size() * sizeof(quint32)));
    setVertexData(vertices);
    setIndexData(indices);

    QVector3D minimum(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    QVector3D maximum(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
    for (qsizetype index = 0; index + 2 < vertexData_.size(); index += 3) {
        const QVector3D point(vertexData_.at(index), vertexData_.at(index + 1), vertexData_.at(index + 2));
        minimum.setX(std::min(minimum.x(), point.x()));
        minimum.setY(std::min(minimum.y(), point.y()));
        minimum.setZ(std::min(minimum.z(), point.z()));
        maximum.setX(std::max(maximum.x(), point.x()));
        maximum.setY(std::max(maximum.y(), point.y()));
        maximum.setZ(std::max(maximum.z(), point.z()));
    }
    if (!vertexData_.isEmpty()) {
        setBounds(minimum, maximum);
    }
    update();
}

} // namespace loupe::app::render
