#include "app/render/ExplodeLineGeometry.h"

#include <QByteArray>
#include <QVariantMap>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace loupe::app::render {

namespace {

// More dashes than this on one line means the dash is far too short for the scene; past it,
// the dash grows so the line is still drawn, without filling memory.
constexpr int kMaximumDashesPerSegment = 4000;

} // namespace

ExplodeLineGeometry::ExplodeLineGeometry(QQuick3DObject* parent)
    : QQuick3DGeometry(parent)
{
    setPrimitiveType(PrimitiveType::Lines);
    setStride(static_cast<int>(3 * sizeof(float)));
    addAttribute(Attribute::PositionSemantic, 0, Attribute::F32Type);
}

void ExplodeLineGeometry::clearLines()
{
    setLines({}, 1.0, 1.0);
}

void ExplodeLineGeometry::setLines(const QVariantList& lines, const double dash, const double gap)
{
    std::vector<float> points;
    QVector3D minimum(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    QVector3D maximum(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
    const auto add = [&](const QVector3D& a, const QVector3D& b) {
        for (const auto& point : {a, b}) {
            points.push_back(point.x());
            points.push_back(point.y());
            points.push_back(point.z());
            minimum = QVector3D(std::min(minimum.x(), point.x()), std::min(minimum.y(), point.y()), std::min(minimum.z(), point.z()));
            maximum = QVector3D(std::max(maximum.x(), point.x()), std::max(maximum.y(), point.y()), std::max(maximum.z(), point.z()));
        }
    };
    const auto dashLength = std::isfinite(dash) && dash > 0.0 ? static_cast<float>(dash) : 1.0F;
    const auto gapLength = std::isfinite(gap) && gap > 0.0 ? static_cast<float>(gap) : dashLength * 0.6F;
    for (const auto& lineValue : lines) {
        for (const auto& segmentValue : lineValue.toMap().value(QStringLiteral("segments")).toList()) {
            const auto segment = segmentValue.toMap();
            const auto from = segment.value(QStringLiteral("from")).value<QVector3D>();
            const auto to = segment.value(QStringLiteral("to")).value<QVector3D>();
            const auto length = (to - from).length();
            if (!(length > 0.0F) || !std::isfinite(length)) continue;
            const auto direction = (to - from) / length;
            // Dashed: dash, gap. Centerline: long dash, gap, dot, gap.
            const auto centerline = segment.value(QStringLiteral("centerline")).toBool();
            auto scale = 1.0F;
            const auto period = centerline ? dashLength * 2.0F + dashLength * 0.25F + gapLength * 2.0F : dashLength + gapLength;
            if (length / period > kMaximumDashesPerSegment) scale = length / period / kMaximumDashesPerSegment;
            const std::vector<std::pair<float, float>> strokes = centerline
                ? std::vector<std::pair<float, float>>{{0.0F, dashLength * 2.0F},
                                                       {dashLength * 2.0F + gapLength, dashLength * 2.25F + gapLength}}
                : std::vector<std::pair<float, float>>{{0.0F, dashLength}};
            for (float at = 0.0F; at < length; at += period * scale) {
                for (const auto& [start, end] : strokes) {
                    const auto a = at + start * scale;
                    const auto b = std::min(length, at + end * scale);
                    if (b > a) add(from + direction * a, from + direction * b);
                }
            }
        }
    }
    segmentCount_ = static_cast<int>(points.size() / 6);
    QByteArray vertices(reinterpret_cast<const char*>(points.data()), static_cast<qsizetype>(points.size() * sizeof(float)));
    setVertexData(vertices);
    if (segmentCount_ > 0) setBounds(minimum, maximum);
    else setBounds({}, {});
    update();
    emit linesChanged();
}

} // namespace loupe::app::render
