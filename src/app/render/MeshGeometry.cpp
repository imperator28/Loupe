#include "app/render/MeshGeometry.h"

#include "app/render/SectionMeshBuilder.h"

#include "protocol/GeometryPayload.h"
#include "protocol/ProtocolTypes.h"

#include <QVector3D>
#include <QVector2D>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

#include <limits>
#include <algorithm>
#include <cmath>

namespace loupe::app::render {
namespace {

float pointTriangleDistanceSquared(const QVector3D& point, const QVector3D& first,
                                   const QVector3D& second, const QVector3D& third)
{
    const auto firstSecond = second - first;
    const auto firstThird = third - first;
    const auto firstPoint = point - first;
    const auto d1 = QVector3D::dotProduct(firstSecond, firstPoint);
    const auto d2 = QVector3D::dotProduct(firstThird, firstPoint);
    if (d1 <= 0.0F && d2 <= 0.0F) return (point - first).lengthSquared();

    const auto secondPoint = point - second;
    const auto d3 = QVector3D::dotProduct(firstSecond, secondPoint);
    const auto d4 = QVector3D::dotProduct(firstThird, secondPoint);
    if (d3 >= 0.0F && d4 <= d3) return (point - second).lengthSquared();

    const auto vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0F && d1 >= 0.0F && d3 <= 0.0F) {
        const auto amount = d1 / (d1 - d3);
        return (point - (first + firstSecond * amount)).lengthSquared();
    }

    const auto thirdPoint = point - third;
    const auto d5 = QVector3D::dotProduct(firstSecond, thirdPoint);
    const auto d6 = QVector3D::dotProduct(firstThird, thirdPoint);
    if (d6 >= 0.0F && d5 <= d6) return (point - third).lengthSquared();

    const auto vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0F && d2 >= 0.0F && d6 <= 0.0F) {
        const auto amount = d2 / (d2 - d6);
        return (point - (first + firstThird * amount)).lengthSquared();
    }

    const auto va = d3 * d6 - d5 * d4;
    if (va <= 0.0F && (d4 - d3) >= 0.0F && (d5 - d6) >= 0.0F) {
        const auto amount = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return (point - (second + (third - second) * amount)).lengthSquared();
    }

    const auto denominator = 1.0F / (va + vb + vc);
    const auto closest = first + firstSecond * (vb * denominator) + firstThird * (vc * denominator);
    return (point - closest).lengthSquared();
}

QVector<float> calculateNormals(const QVector<float>& vertices, const QVector<quint32>& indices)
{
    QVector<QVector3D> accumulated(vertices.size() / 3, QVector3D{});
    const auto pointAt = [&vertices](const quint32 index) {
        const auto offset = static_cast<qsizetype>(index) * 3;
        return QVector3D{vertices.at(offset), vertices.at(offset + 1), vertices.at(offset + 2)};
    };
    for (qsizetype index = 0; index + 2 < indices.size(); index += 3) {
        const auto firstIndex = indices.at(index);
        const auto secondIndex = indices.at(index + 1);
        const auto thirdIndex = indices.at(index + 2);
        const auto normal = QVector3D::crossProduct(pointAt(secondIndex) - pointAt(firstIndex),
                                                    pointAt(thirdIndex) - pointAt(firstIndex));
        if (normal.isNull()) continue;
        accumulated[static_cast<qsizetype>(firstIndex)] += normal;
        accumulated[static_cast<qsizetype>(secondIndex)] += normal;
        accumulated[static_cast<qsizetype>(thirdIndex)] += normal;
    }
    QVector<float> result;
    result.reserve(vertices.size());
    for (const auto& value : accumulated) {
        const auto normal = value.isNull() ? QVector3D{0.0F, 0.0F, 1.0F} : value.normalized();
        result.append(normal.x()); result.append(normal.y()); result.append(normal.z());
    }
    return result;
}

} // namespace

MeshGeometry::MeshGeometry(QQuick3DObject* parent)
    : QQuick3DGeometry(parent)
{
    setPrimitiveType(PrimitiveType::Triangles);
    setStride(static_cast<int>(6 * sizeof(float)));
    addAttribute(Attribute::PositionSemantic, 0, Attribute::F32Type);
    addAttribute(Attribute::NormalSemantic, static_cast<int>(3 * sizeof(float)), Attribute::F32Type);
    addAttribute(Attribute::IndexSemantic, 0, Attribute::U32Type);
}

void MeshGeometry::replaceMesh(const MeshData& mesh)
{
    sectionSource_.clear();
    sourceVertexData_ = mesh.vertexData;
    sourceNormalData_ = mesh.normalData;
    sourceIndexData_ = mesh.indexData;
    if (sourceNormalData_.size() != sourceVertexData_.size()) rebuildSourceNormals();
    rebuildDisplayMesh();
}

bool MeshGeometry::appendWorkerMesh(const QByteArray& meshJson)
{
    try {
        const auto payload = protocol::decodeGeometry(meshJson);
        const auto* mesh = std::get_if<protocol::MeshPayload>(&payload);
        if (!mesh) return false;
        MeshData data{mesh->definitionId, mesh->vertices, mesh->indices, mesh->normals};
        replaceMesh(data);
        sourceTopology_ = mesh->topology;
        return true;
    } catch (const protocol::ProtocolError&) {
    }
    const auto payload = QJsonDocument::fromJson(meshJson);
    if (!payload.isObject()) return false;
    const auto vertices = payload.object().value(QStringLiteral("vertices")).toArray();
    const auto normals = payload.object().value(QStringLiteral("normals")).toArray();
    const auto indices = payload.object().value(QStringLiteral("indices")).toArray();
    if (vertices.isEmpty() || vertices.size() % 3 != 0 || indices.isEmpty() || indices.size() % 3 != 0) return false;
    const auto offset = static_cast<quint32>(sourceVertexData_.size() / 3);
    for (const auto& value : vertices) {
        if (!value.isDouble()) return false;
        sourceVertexData_.append(static_cast<float>(value.toDouble()));
    }
    const auto hasNormals = normals.size() == vertices.size();
    if (hasNormals) {
        for (const auto& value : normals) {
            if (!value.isDouble()) return false;
            sourceNormalData_.append(static_cast<float>(value.toDouble()));
        }
    } else {
        sourceNormalData_.resize(sourceVertexData_.size());
    }
    for (const auto& value : indices) {
        if (!value.isDouble() || value.toInt(-1) < 0 || value.toInt() >= vertices.size() / 3) return false;
        sourceIndexData_.append(offset + static_cast<quint32>(value.toInt()));
    }
    if (!hasNormals) rebuildSourceNormals();
    sourceTopology_.clear();
    rebuildDisplayMesh();
    return true;
}

bool MeshGeometry::replaceWorkerMesh(const QByteArray& meshJson)
{
    clearMesh();
    return appendWorkerMesh(meshJson);
}

void MeshGeometry::clearMesh()
{
    sectionSource_.clear();
    sourceVertexData_.clear();
    sourceNormalData_.clear();
    sourceIndexData_.clear();
    sourceTopology_.clear();
    vertexData_.clear();
    normalData_.clear();
    indexData_.clear();
    upload();
}

QVariantMap MeshGeometry::topologyAtPoint(const double x, const double y, const double z) const
{
    if (sourceTopology_.isEmpty()) return {};
    const QVector3D point{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
    const auto pointAt = [this](const quint32 index) {
        const auto offset = static_cast<qsizetype>(index) * 3;
        return QVector3D{sourceVertexData_.at(offset), sourceVertexData_.at(offset + 1), sourceVertexData_.at(offset + 2)};
    };
    const protocol::TopologyRange* closestRange = nullptr;
    auto closestDistance = std::numeric_limits<float>::max();
    for (const auto& range : sourceTopology_) {
        const auto end = static_cast<qsizetype>(range.firstIndex + range.indexCount);
        for (auto index = static_cast<qsizetype>(range.firstIndex); index + 2 < end; index += 3) {
            const auto distance = pointTriangleDistanceSquared(point, pointAt(sourceIndexData_.at(index)),
                                                               pointAt(sourceIndexData_.at(index + 1)),
                                                               pointAt(sourceIndexData_.at(index + 2)));
            if (distance < closestDistance) {
                closestDistance = distance;
                closestRange = &range;
            }
        }
    }
    if (!closestRange) return {};
    return {{QStringLiteral("topologyId"), closestRange->topologyId},
            {QStringLiteral("entityKind"), QStringLiteral("face")},
            {QStringLiteral("measureMm"), closestRange->measureMm},
            {QStringLiteral("radiusMm"), closestRange->radiusMm}};
}

bool MeshGeometry::copyTopologyFrom(QObject* source, const quint32 topologyId)
{
    const auto* sourceGeometry = qobject_cast<MeshGeometry*>(source);
    if (!sourceGeometry) return false;
    const auto range = std::find_if(sourceGeometry->sourceTopology_.cbegin(), sourceGeometry->sourceTopology_.cend(),
                                    [topologyId](const auto& value) { return value.topologyId == topologyId; });
    if (range == sourceGeometry->sourceTopology_.cend()) return false;

    sectionSource_.clear();
    sourceVertexData_.clear();
    sourceNormalData_.clear();
    sourceIndexData_.clear();
    const auto end = static_cast<qsizetype>(range->firstIndex + range->indexCount);
    for (auto index = static_cast<qsizetype>(range->firstIndex); index < end; ++index) {
        const auto sourceIndex = static_cast<qsizetype>(sourceGeometry->sourceIndexData_.at(index));
        const auto sourceOffset = sourceIndex * 3;
        sourceVertexData_.append(sourceGeometry->sourceVertexData_.at(sourceOffset));
        sourceVertexData_.append(sourceGeometry->sourceVertexData_.at(sourceOffset + 1));
        sourceVertexData_.append(sourceGeometry->sourceVertexData_.at(sourceOffset + 2));
        sourceNormalData_.append(sourceGeometry->sourceNormalData_.at(sourceOffset));
        sourceNormalData_.append(sourceGeometry->sourceNormalData_.at(sourceOffset + 1));
        sourceNormalData_.append(sourceGeometry->sourceNormalData_.at(sourceOffset + 2));
        sourceIndexData_.append(static_cast<quint32>(sourceIndexData_.size()));
    }
    sourceTopology_ = {{topologyId, protocol::TopologyKind::Face, 0,
                        static_cast<quint32>(sourceIndexData_.size()), range->measureMm, range->radiusMm}};
    sectionEnabled_ = false;
    sectionPreview_ = false;
    rebuildDisplayMesh();
    return true;
}

bool MeshGeometry::copySectionOverlayFrom(QObject* source)
{
    const auto* sourceGeometry = qobject_cast<MeshGeometry*>(source);
    if (!sourceGeometry) return false;
    auto sectionSource = QSharedPointer<SectionSourceData>::create();
    sectionSource->vertices = sourceGeometry->sourceVertexData_;
    sectionSource->indices = sourceGeometry->sourceIndexData_;
    sectionSource_ = sectionSource;
    sourceVertexData_.clear();
    sourceNormalData_.clear();
    sourceIndexData_.clear();
    sourceTopology_.clear();
    sectionOverlayOnly_ = true;
    ++sectionBuildGeneration_;
    sectionEnabled_ = false;
    sectionPreview_ = false;
    rebuildDisplayMesh();
    return true;
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
    configureSection(enabled, normalX, normalY, normalZ, offset, flipped, sectionCapEnabled_,
                     sectionSliceOnly_, sectionSliceFill_, sectionSliceOutline_, false);
}

void MeshGeometry::configureSection(const bool enabled, const double normalX, const double normalY,
                                    const double normalZ, const double offset, const bool flipped,
                                    const bool capEnabled, const bool sliceOnly, const bool sliceFill,
                                    const bool sliceOutline, const bool preview, const double outlineWidth)
{
    const QVector3D normal{static_cast<float>(normalX), static_cast<float>(normalY), static_cast<float>(normalZ)};
    if (normal.isNull()) return;
    const auto normalized = normal.normalized();
    if (sectionEnabled_ == enabled && sectionNormal_ == normalized
        && qFuzzyCompare(sectionOffset_ + 1.0, offset + 1.0) && sectionFlipped_ == flipped
        && sectionCapEnabled_ == capEnabled && sectionSliceOnly_ == sliceOnly
        && sectionSliceFill_ == sliceFill && sectionSliceOutline_ == sliceOutline
        && sectionPreview_ == preview
        && qFuzzyCompare(sectionOutlineWidth_ + 1.0, outlineWidth + 1.0)) return;
    sectionEnabled_ = enabled;
    sectionNormal_ = normalized;
    sectionOffset_ = offset;
    sectionFlipped_ = flipped;
    sectionCapEnabled_ = capEnabled;
    sectionSliceOnly_ = sliceOnly;
    sectionSliceFill_ = sliceFill;
    sectionSliceOutline_ = sliceOutline;
    sectionPreview_ = preview;
    sectionOutlineWidth_ = std::max(0.0, outlineWidth);
    if (sectionOverlayOnly_) {
        if (sectionEnabled_ && !sectionPreview_)
            startSectionOverlayBuild();
        else {
            ++sectionBuildGeneration_;
            rebuildDisplayMesh();
        }
        return;
    }
    rebuildDisplayMesh();
}

void MeshGeometry::setSectionOptions(const bool capEnabled, const bool sliceOnly, const bool sliceFill, const bool sliceOutline)
{
    configureSection(sectionEnabled_, sectionNormal_.x(), sectionNormal_.y(), sectionNormal_.z(),
                     sectionOffset_, sectionFlipped_, capEnabled, sliceOnly, sliceFill, sliceOutline, false);
}

float MeshGeometry::minimumCoordinate(const int axis) const noexcept
{
    if (vertexData_.isEmpty()) return std::numeric_limits<float>::quiet_NaN();
    const auto normalizedAxis = std::clamp(axis, 0, 2);
    float value = std::numeric_limits<float>::max();
    for (qsizetype index = normalizedAxis; index < vertexData_.size(); index += 3) value = std::min(value, vertexData_.at(index));
    return value;
}

float MeshGeometry::maximumCoordinate(const int axis) const noexcept
{
    if (vertexData_.isEmpty()) return std::numeric_limits<float>::quiet_NaN();
    const auto normalizedAxis = std::clamp(axis, 0, 2);
    float value = std::numeric_limits<float>::lowest();
    for (qsizetype index = normalizedAxis; index < vertexData_.size(); index += 3) value = std::max(value, vertexData_.at(index));
    return value;
}

void MeshGeometry::rebuildDisplayMesh()
{
    vertexData_.clear();
    normalData_.clear();
    indexData_.clear();
    sectionCapTriangleCount_ = 0;
    sectionFillIndexCount_ = 0;
    if (!sectionEnabled_ && sectionOverlayOnly_) {
        upload();
        return;
    }
    if (!sectionEnabled_) {
        vertexData_ = sourceVertexData_;
        normalData_ = sourceNormalData_;
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
        if (!sectionOverlayOnly_)
            for (qsizetype index = 1; index + 1 < output.size(); ++index) {
                appendVertex(output.at(0)); appendVertex(output.at(index)); appendVertex(output.at(index + 1));
            }
        if (!sectionPreview_ && intersections.size() == 2 && !near(intersections.at(0), intersections.at(1)))
            segments.append({intersections.at(0), intersections.at(1)});
    }

    if (sectionPreview_) {
        rebuildDisplayNormals();
        upload();
        return;
    }

    const auto appendSliceOutline = [this, &appendVertex](const QVector<Segment>& sliceSegments) {
        QVector3D sourceMinimum(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
        QVector3D sourceMaximum(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
        for (qsizetype index = 0; index + 2 < sourceVertexData_.size(); index += 3) {
            const QVector3D point{sourceVertexData_.at(index), sourceVertexData_.at(index + 1), sourceVertexData_.at(index + 2)};
            sourceMinimum.setX(std::min(sourceMinimum.x(), point.x()));
            sourceMinimum.setY(std::min(sourceMinimum.y(), point.y()));
            sourceMinimum.setZ(std::min(sourceMinimum.z(), point.z()));
            sourceMaximum.setX(std::max(sourceMaximum.x(), point.x()));
            sourceMaximum.setY(std::max(sourceMaximum.y(), point.y()));
            sourceMaximum.setZ(std::max(sourceMaximum.z(), point.z()));
        }
        const auto sourceDiagonal = (sourceMaximum - sourceMinimum).length();
        const auto halfWidth = sectionOutlineWidth_ > 0.0
                ? static_cast<float>(sectionOutlineWidth_ * 0.5)
                : std::max(0.01F, sourceDiagonal * 0.00035F);
        const auto removedNormal = sectionNormal_ * (sectionFlipped_ ? 1.0F : -1.0F);
        const auto depthLift = removedNormal * std::max(sourceDiagonal * 0.000002F, halfWidth * 0.025F);
        for (const auto& segment : sliceSegments) {
            const auto direction = (segment.end - segment.start).normalized();
            const auto offset = QVector3D::crossProduct(sectionNormal_, direction).normalized() * halfWidth;
            if (offset.isNull()) continue;
            const auto firstLeft = segment.start - offset + depthLift;
            const auto firstRight = segment.start + offset + depthLift;
            const auto secondLeft = segment.end - offset + depthLift;
            const auto secondRight = segment.end + offset + depthLift;
            appendVertex(firstLeft); appendVertex(secondLeft); appendVertex(secondRight);
            appendVertex(firstLeft); appendVertex(secondRight); appendVertex(firstRight);
            appendVertex(secondRight); appendVertex(secondLeft); appendVertex(firstLeft);
            appendVertex(firstRight); appendVertex(secondRight); appendVertex(firstLeft);
        }
    };
    const auto sliceSegments = segments;

    if (sectionSliceOnly_ && !sectionSliceFill_) {
        vertexData_.clear();
        normalData_.clear();
        indexData_.clear();
        sectionFillIndexCount_ = 0;
        if (sectionSliceOutline_) appendSliceOutline(sliceSegments);
        sectionCapTriangleCount_ = static_cast<int>(indexData_.size() / 3);
        rebuildDisplayNormals();
        upload();
        return;
    }

    if (sectionSliceOnly_) {
        vertexData_.clear();
        normalData_.clear();
        indexData_.clear();
    }

    if ((sectionCapEnabled_ || (sectionSliceOnly_ && sectionSliceFill_)) && !segments.isEmpty()) {
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
        sectionCapTriangleCount_ = static_cast<int>(capVertices.size() / 9);
        for (qsizetype index = 0; index + 2 < capVertices.size(); index += 3) appendVertex({capVertices.at(index), capVertices.at(index + 1), capVertices.at(index + 2)});
    }
    sectionFillIndexCount_ = static_cast<int>(indexData_.size());
    if (sectionSliceOnly_ && sectionSliceOutline_) appendSliceOutline(sliceSegments);
    if (sectionSliceOnly_) sectionCapTriangleCount_ = static_cast<int>(indexData_.size() / 3);
    rebuildDisplayNormals();
    upload();
}

void MeshGeometry::rebuildSourceNormals()
{
    sourceNormalData_ = calculateNormals(sourceVertexData_, sourceIndexData_);
}

void MeshGeometry::rebuildDisplayNormals()
{
    normalData_ = calculateNormals(vertexData_, indexData_);
}

void MeshGeometry::startSectionOverlayBuild()
{
    if (!sectionSource_) return;
    const auto generation = ++sectionBuildGeneration_;
    SectionBuildRequest request{sectionSource_, sectionNormal_, sectionOffset_,
                                sectionFlipped_, sectionCapEnabled_, sectionSliceOnly_, sectionSliceFill_,
                                sectionSliceOutline_, static_cast<float>(sectionOutlineWidth_)};
    const auto wasBusy = sectionBusy();
    ++pendingSectionBuilds_;
    if (!wasBusy) emit sectionBusyChanged();

    auto* watcher = new QFutureWatcher<SectionBuildResult>(this);
    connect(watcher, &QFutureWatcher<SectionBuildResult>::finished, this, [this, watcher, generation]() {
        const auto result = watcher->result();
        if (generation == sectionBuildGeneration_) {
            vertexData_ = result.vertices;
            indexData_ = result.indices;
            sectionCapTriangleCount_ = result.capTriangleCount;
            sectionFillIndexCount_ = result.fillIndexCount;
            rebuildDisplayNormals();
            upload();
        }
        watcher->deleteLater();
        --pendingSectionBuilds_;
        if (!sectionBusy()) emit sectionBusyChanged();
    });
    watcher->setFuture(QtConcurrent::run([request = std::move(request)]() {
        return buildSectionOverlay(request);
    }));
}

void MeshGeometry::upload()
{
    QVector<float> interleaved;
    interleaved.reserve(vertexData_.size() * 2);
    for (qsizetype index = 0; index + 2 < vertexData_.size(); index += 3) {
        interleaved.append(vertexData_.at(index));
        interleaved.append(vertexData_.at(index + 1));
        interleaved.append(vertexData_.at(index + 2));
        if (index + 2 < normalData_.size()) {
            interleaved.append(normalData_.at(index));
            interleaved.append(normalData_.at(index + 1));
            interleaved.append(normalData_.at(index + 2));
        } else {
            interleaved.append(0.0F); interleaved.append(0.0F); interleaved.append(1.0F);
        }
    }
    clear();
    setPrimitiveType(PrimitiveType::Triangles);
    setStride(static_cast<int>(6 * sizeof(float)));
    addAttribute(Attribute::PositionSemantic, 0, Attribute::F32Type);
    addAttribute(Attribute::NormalSemantic, static_cast<int>(3 * sizeof(float)), Attribute::F32Type);
    addAttribute(Attribute::IndexSemantic, 0, Attribute::U32Type);

    QByteArray vertices(reinterpret_cast<const char*>(interleaved.constData()), static_cast<int>(interleaved.size() * static_cast<qsizetype>(sizeof(float))));
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
        if (sectionOverlayOnly_ && sectionSliceOnly_) {
            const auto fillCount = std::clamp(sectionFillIndexCount_, 0, static_cast<int>(indexData_.size()));
            const auto outlineCount = static_cast<int>(indexData_.size()) - fillCount;
            addSubset(0, fillCount, minimum, maximum, QStringLiteral("section-fill"));
            if (outlineCount > 0)
                addSubset(fillCount, outlineCount, minimum, maximum, QStringLiteral("section-outline"));
        }
    }
    update();
}

} // namespace loupe::app::render
