#include "core/drawing/DrawingPreview.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>
#include <numbers>

namespace loupe::drawing {
namespace {

// Chord tolerance for flattening. The preview is on a screen, so this is a pixel-level
// question rather than a cutting one; 0.05 mm is well below what any zoom level resolves.
constexpr double kPreviewDeflectionMm = 0.05;
constexpr int kMinimumArcSteps = 4;
constexpr int kMaximumArcSteps = 720;
constexpr int kCubicSteps = 16;

void appendPoint(QJsonArray& points, const gp_Pnt2d& point)
{
    points.append(point.X());
    points.append(point.Y());
}

[[nodiscard]] int arcSteps(const Arc& arc)
{
    if (!(arc.radius > 0.0)) return kMinimumArcSteps;
    // Steps needed for the chord to stay within tolerance of the true arc.
    const auto ratio = std::clamp(1.0 - kPreviewDeflectionMm / arc.radius, -1.0, 1.0);
    const auto stepAngle = 2.0 * std::acos(ratio);
    if (!(stepAngle > 0.0)) return kMaximumArcSteps;
    const auto steps = static_cast<int>(std::ceil(std::abs(arc.sweepAngle) / stepAngle));
    return std::clamp(steps, kMinimumArcSteps, kMaximumArcSteps);
}

void flatten(QJsonArray& points, const Primitive& primitive, const bool first)
{
    if (first) appendPoint(points, primitiveStart(primitive));
    if (const auto* segment = std::get_if<Segment>(&primitive)) {
        appendPoint(points, segment->end);
        return;
    }
    if (const auto* arc = std::get_if<Arc>(&primitive)) {
        const auto steps = arcSteps(*arc);
        for (int step = 1; step <= steps; ++step) {
            const auto fraction = static_cast<double>(step) / static_cast<double>(steps);
            appendPoint(points, arc->pointAtAngle(arc->startAngle + arc->sweepAngle * fraction));
        }
        return;
    }
    const auto& cubic = std::get<Cubic>(primitive);
    for (int step = 1; step <= kCubicSteps; ++step) {
        const auto t = static_cast<double>(step) / static_cast<double>(kCubicSteps);
        const auto u = 1.0 - t;
        const auto x = u * u * u * cubic.start.X() + 3.0 * u * u * t * cubic.firstControl.X()
            + 3.0 * u * t * t * cubic.secondControl.X() + t * t * t * cubic.end.X();
        const auto y = u * u * u * cubic.start.Y() + 3.0 * u * u * t * cubic.firstControl.Y()
            + 3.0 * u * t * t * cubic.secondControl.Y() + t * t * t * cubic.end.Y();
        appendPoint(points, gp_Pnt2d(x, y));
    }
}

[[nodiscard]] QString roleName(const LayerRole role)
{
    switch (role) {
    case LayerRole::Cut: return QStringLiteral("cut");
    case LayerRole::Outline: return QStringLiteral("outline");
    case LayerRole::Smooth: return QStringLiteral("smooth");
    case LayerRole::Hidden: return QStringLiteral("hidden");
    case LayerRole::Reference: return QStringLiteral("reference");
    }
    return QStringLiteral("cut");
}

} // namespace

std::string encodePreview(const ProjectionResult& projected)
{
    QJsonArray layers;
    for (const auto& layer : projected.drawing.layers) {
        QJsonArray contours;
        for (const auto& contour : layer.contours) {
            QJsonArray points;
            bool first = true;
            for (const auto& primitive : contour.primitives) {
                flatten(points, primitive, first);
                first = false;
            }
            if (points.isEmpty()) continue;
            contours.append(QJsonObject{{QStringLiteral("closed"), contour.closed},
                                        {QStringLiteral("points"), points}});
        }
        if (contours.isEmpty()) continue;
        layers.append(QJsonObject{{QStringLiteral("name"), QString::fromStdString(layer.name)},
                                  {QStringLiteral("role"), roleName(layer.role)},
                                  {QStringLiteral("contours"), contours}});
    }

    QJsonArray warnings;
    for (const auto& warning : projected.drawing.warnings) {
        warnings.append(QJsonObject{{QStringLiteral("code"), QString::fromStdString(warning.code)},
                                    {QStringLiteral("count"), warning.count}});
    }

    const auto bounds = projected.drawing.bounds();
    return QJsonDocument(QJsonObject{
        {QStringLiteral("schemaVersion"), 1},
        // Stated, not implied: the measured size is what a user checks a drawing against.
        {QStringLiteral("widthMm"), bounds.width()},
        {QStringLiteral("heightMm"), bounds.height()},
        {QStringLiteral("minX"), bounds.valid ? bounds.minX : 0.0},
        {QStringLiteral("minY"), bounds.valid ? bounds.minY : 0.0},
        {QStringLiteral("empty"), !bounds.valid},
        // Stated as its own field, not left for the UI to detect in the warning list.
        {QStringLiteral("approximate"), projected.approximate},
        {QStringLiteral("closedContours"), projected.statistics.closedContours},
        {QStringLiteral("openContours"), projected.statistics.openContours},
        {QStringLiteral("layers"), layers},
        {QStringLiteral("warnings"), warnings},
    }).toJson(QJsonDocument::Compact).toStdString();
}

} // namespace loupe::drawing
