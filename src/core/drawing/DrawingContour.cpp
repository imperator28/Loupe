#include "core/drawing/DrawingContour.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace loupe::drawing {
namespace {

constexpr double twoPi = 2.0 * std::numbers::pi;
// Angle comparisons only ever decide whether a cardinal direction falls inside
// an arc's sweep. Including a boundary case costs nothing (the point is on the
// arc either way); excluding one would understate the bounds.
constexpr double angleEpsilon = 1.0e-12;
constexpr double coefficientEpsilon = 1.0e-12;

[[nodiscard]] double normalizeToZeroTwoPi(double angle)
{
    angle = std::fmod(angle, twoPi);
    if (angle < 0.0) angle += twoPi;
    return angle;
}

[[nodiscard]] bool angleWithinSweep(const double angle, const double startAngle, const double sweepAngle)
{
    if (std::abs(sweepAngle) >= twoPi - angleEpsilon) return true;
    if (sweepAngle >= 0.0) {
        return normalizeToZeroTwoPi(angle - startAngle) <= sweepAngle + angleEpsilon;
    }
    return normalizeToZeroTwoPi(startAngle - angle) <= -sweepAngle + angleEpsilon;
}

[[nodiscard]] gp_Pnt2d translated(const gp_Pnt2d& point, const double dx, const double dy)
{
    return {point.X() + dx, point.Y() + dy};
}

[[nodiscard]] gp_Pnt2d mirrored(const gp_Pnt2d& point, const double pageHeightMm)
{
    return {point.X(), pageHeightMm - point.Y()};
}

// Evaluate one axis of a cubic Bezier.
[[nodiscard]] double cubicAt(const double p0, const double p1, const double p2, const double p3, const double t)
{
    const double u = 1.0 - t;
    return u * u * u * p0 + 3.0 * u * u * t * p1 + 3.0 * u * t * t * p2 + t * t * t * p3;
}

// Parameters in (0,1) where one axis of a cubic Bezier reaches an extremum.
//
// B'(t)/3 reduces to the quadratic  a*t^2 + b*t + c  with
//   a = (p1-p0) - 2(p2-p1) + (p3-p2),  b = 2((p2-p1) - (p1-p0)),  c = (p1-p0)
[[nodiscard]] std::vector<double> cubicExtremumParameters(const double p0, const double p1,
                                                          const double p2, const double p3)
{
    const double d0 = p1 - p0;
    const double d1 = p2 - p1;
    const double d2 = p3 - p2;
    const double a = d0 - 2.0 * d1 + d2;
    const double b = 2.0 * (d1 - d0);
    const double c = d0;

    std::vector<double> roots;
    const auto keep = [&roots](const double t) {
        if (t > 0.0 && t < 1.0) roots.push_back(t);
    };

    if (std::abs(a) < coefficientEpsilon) {
        if (std::abs(b) >= coefficientEpsilon) keep(-c / b);
        return roots;
    }
    const double discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0.0) return roots;
    const double root = std::sqrt(discriminant);
    keep((-b + root) / (2.0 * a));
    keep((-b - root) / (2.0 * a));
    return roots;
}

} // namespace

gp_Pnt2d Arc::pointAtAngle(const double angle) const
{
    return {centre.X() + radius * std::cos(angle), centre.Y() + radius * std::sin(angle)};
}

gp_Pnt2d Arc::startPoint() const { return pointAtAngle(startAngle); }

gp_Pnt2d Arc::endPoint() const { return pointAtAngle(startAngle + sweepAngle); }

bool Arc::isFullCircle() const noexcept { return std::abs(sweepAngle) >= twoPi - angleEpsilon; }

gp_Pnt2d primitiveStart(const Primitive& primitive)
{
    return std::visit([](const auto& value) -> gp_Pnt2d {
        using Kind = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Kind, Arc>) return value.startPoint();
        else return value.start;
    }, primitive);
}

gp_Pnt2d primitiveEnd(const Primitive& primitive)
{
    return std::visit([](const auto& value) -> gp_Pnt2d {
        using Kind = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Kind, Arc>) return value.endPoint();
        else return value.end;
    }, primitive);
}

Primitive reversedPrimitive(const Primitive& primitive)
{
    if (const auto* segment = std::get_if<Segment>(&primitive)) {
        return Segment{segment->end, segment->start};
    }
    if (const auto* arc = std::get_if<Arc>(&primitive)) {
        // Start where the arc ended and sweep back the other way; the traced
        // point set is identical, only the direction of travel changes.
        return Arc{arc->centre, arc->radius, arc->startAngle + arc->sweepAngle, -arc->sweepAngle};
    }
    const auto& cubic = std::get<Cubic>(primitive);
    return Cubic{cubic.end, cubic.secondControl, cubic.firstControl, cubic.start};
}

void Bounds2d::add(const gp_Pnt2d& point)
{
    if (!valid) {
        minX = maxX = point.X();
        minY = maxY = point.Y();
        valid = true;
        return;
    }
    minX = std::min(minX, point.X());
    maxX = std::max(maxX, point.X());
    minY = std::min(minY, point.Y());
    maxY = std::max(maxY, point.Y());
}

void Bounds2d::add(const Bounds2d& other)
{
    if (!other.valid) return;
    add(gp_Pnt2d(other.minX, other.minY));
    add(gp_Pnt2d(other.maxX, other.maxY));
}

Bounds2d primitiveBounds(const Primitive& primitive)
{
    Bounds2d bounds;
    if (const auto* segment = std::get_if<Segment>(&primitive)) {
        bounds.add(segment->start);
        bounds.add(segment->end);
        return bounds;
    }
    if (const auto* arc = std::get_if<Arc>(&primitive)) {
        bounds.add(arc->startPoint());
        bounds.add(arc->endPoint());
        // An arc only reaches an extremum at a cardinal direction, so the tight
        // bounds are its endpoints plus whichever of the four quadrant points
        // the sweep actually covers.
        for (int quadrant = 0; quadrant < 4; ++quadrant) {
            const double angle = quadrant * std::numbers::pi / 2.0;
            if (angleWithinSweep(angle, arc->startAngle, arc->sweepAngle)) {
                bounds.add(arc->pointAtAngle(angle));
            }
        }
        return bounds;
    }
    const auto& cubic = std::get<Cubic>(primitive);
    bounds.add(cubic.start);
    bounds.add(cubic.end);
    for (const double t : cubicExtremumParameters(cubic.start.X(), cubic.firstControl.X(),
                                                  cubic.secondControl.X(), cubic.end.X())) {
        bounds.add(gp_Pnt2d(cubicAt(cubic.start.X(), cubic.firstControl.X(), cubic.secondControl.X(), cubic.end.X(), t),
                            cubicAt(cubic.start.Y(), cubic.firstControl.Y(), cubic.secondControl.Y(), cubic.end.Y(), t)));
    }
    for (const double t : cubicExtremumParameters(cubic.start.Y(), cubic.firstControl.Y(),
                                                  cubic.secondControl.Y(), cubic.end.Y())) {
        bounds.add(gp_Pnt2d(cubicAt(cubic.start.X(), cubic.firstControl.X(), cubic.secondControl.X(), cubic.end.X(), t),
                            cubicAt(cubic.start.Y(), cubic.firstControl.Y(), cubic.secondControl.Y(), cubic.end.Y(), t)));
    }
    return bounds;
}

Bounds2d contourBounds(const Contour& contour)
{
    Bounds2d bounds;
    for (const auto& primitive : contour.primitives) bounds.add(primitiveBounds(primitive));
    return bounds;
}

std::string defaultLayerName(const LayerRole role)
{
    switch (role) {
    case LayerRole::Cut: return "CUT";
    case LayerRole::Outline: return "OUTLINE";
    case LayerRole::Smooth: return "SMOOTH";
    case LayerRole::Hidden: return "HIDDEN";
    case LayerRole::Reference: return "REFERENCE";
    }
    return "CUT";
}

Bounds2d layerBounds(const Layer& layer)
{
    Bounds2d bounds;
    for (const auto& contour : layer.contours) bounds.add(contourBounds(contour));
    return bounds;
}

Bounds2d Drawing::bounds() const
{
    Bounds2d result;
    for (const auto& layer : layers) result.add(layerBounds(layer));
    return result;
}

Drawing Drawing::translatedToOrigin(const double marginMm) const
{
    const auto extents = bounds();
    if (!extents.valid) return *this;
    const double dx = marginMm - extents.minX;
    const double dy = marginMm - extents.minY;

    Drawing result = *this;
    for (auto& layer : result.layers) {
        for (auto& contour : layer.contours) {
            for (auto& primitive : contour.primitives) {
                std::visit([dx, dy](auto& value) {
                    using Kind = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<Kind, Segment>) {
                        value.start = translated(value.start, dx, dy);
                        value.end = translated(value.end, dx, dy);
                    } else if constexpr (std::is_same_v<Kind, Arc>) {
                        // A translation leaves angles and sweep untouched.
                        value.centre = translated(value.centre, dx, dy);
                    } else {
                        value.start = translated(value.start, dx, dy);
                        value.firstControl = translated(value.firstControl, dx, dy);
                        value.secondControl = translated(value.secondControl, dx, dy);
                        value.end = translated(value.end, dx, dy);
                    }
                }, primitive);
            }
        }
    }
    return result;
}

Drawing Drawing::mirroredVertically(const double pageHeightMm) const
{
    Drawing result = *this;
    for (auto& layer : result.layers) {
        for (auto& contour : layer.contours) {
            for (auto& primitive : contour.primitives) {
                std::visit([pageHeightMm](auto& value) {
                    using Kind = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<Kind, Segment>) {
                        value.start = mirrored(value.start, pageHeightMm);
                        value.end = mirrored(value.end, pageHeightMm);
                    } else if constexpr (std::is_same_v<Kind, Arc>) {
                        // Reflecting y about a horizontal line maps the point at
                        // angle t to the point at angle -t, so both the start
                        // angle and the sweep direction invert. Mirroring the
                        // centre alone would keep the arc bulging the wrong way.
                        value.centre = mirrored(value.centre, pageHeightMm);
                        value.startAngle = -value.startAngle;
                        value.sweepAngle = -value.sweepAngle;
                    } else {
                        value.start = mirrored(value.start, pageHeightMm);
                        value.firstControl = mirrored(value.firstControl, pageHeightMm);
                        value.secondControl = mirrored(value.secondControl, pageHeightMm);
                        value.end = mirrored(value.end, pageHeightMm);
                    }
                }, primitive);
            }
        }
    }
    return result;
}

bool Drawing::empty() const noexcept { return contourCount() == 0; }

std::size_t Drawing::contourCount() const noexcept
{
    std::size_t count = 0;
    for (const auto& layer : layers) count += layer.contours.size();
    return count;
}

int Drawing::warningCount(const std::string_view code) const noexcept
{
    for (const auto& warning : warnings) {
        if (warning.code == code) return warning.count;
    }
    return 0;
}

} // namespace loupe::drawing
