#include "app/tools/MeasurementController.h"

#include <QClipboard>
#include <QGuiApplication>

#include <algorithm>
#include <cmath>

namespace {

QString formatValue(const double value)
{
    const auto rounded = std::round(value);
    const auto normalized = std::abs(value - rounded) <= 1.0e-6 ? rounded : value;
    return QString::number(normalized, 'g', 8);
}

QString fixed(const double value, const int decimals)
{
    // -0.000 reads as a sign that means nothing.
    const auto shown = std::abs(value) < 0.5 * std::pow(10.0, -decimals) ? 0.0 : value;
    return QString::number(shown, 'f', decimals);
}

QVariantMap detail(const QString& label, const QString& value)
{
    return {{QStringLiteral("label"), label}, {QStringLiteral("value"), value}};
}

QString coordinates(const QVector3D& point)
{
    return QStringLiteral("(%1, %2, %3) mm").arg(formatValue(point.x()), formatValue(point.y()), formatValue(point.z()));
}

// The point halfway along a polyline, for a label on an edge.
QVector3D midpointOf(const QVector<QVector3D>& polyline, const QVector3D& fallback)
{
    if (polyline.size() < 2) return fallback;
    double total{};
    for (qsizetype index = 0; index + 1 < polyline.size(); ++index)
        total += static_cast<double>((polyline.at(index + 1) - polyline.at(index)).length());
    auto remaining = total * 0.5;
    for (qsizetype index = 0; index + 1 < polyline.size(); ++index) {
        const auto length = static_cast<double>((polyline.at(index + 1) - polyline.at(index)).length());
        if (remaining <= length && length > 0.0)
            return polyline.at(index) + (polyline.at(index + 1) - polyline.at(index)) * static_cast<float>(remaining / length);
        remaining -= length;
    }
    return polyline.constLast();
}

} // namespace

namespace loupe::app::tools {

MeasurementController::MeasurementController(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<MeasureEntity>();
}

void MeasurementController::setEffectiveUnit(const QString& unit)
{
    const auto normalized = unit == QStringLiteral("in") ? unit : QStringLiteral("mm");
    if (effectiveUnit_ == normalized) return;
    effectiveUnit_ = normalized;
    recompute();
    emit resultChanged();
}

void MeasurementController::setMode(const MeasurementMode mode)
{
    if (mode_ == mode) return;
    picks_.clear();
    pickedDescriptions_.clear();
    legacyPicks_.clear();
    refusal_.clear();
    mode_ = mode;
    recompute();
    emit modeChanged();
    emit resultChanged();
}

void MeasurementController::setSelectedGeometry(const double surfaceAreaMm2, const double volumeMm3, const QVector3D& boundsMm)
{
    hasSelectedGeometry_ = std::isfinite(surfaceAreaMm2) && surfaceAreaMm2 >= 0.0
        && std::isfinite(volumeMm3) && volumeMm3 >= 0.0
        && std::isfinite(boundsMm.x()) && std::isfinite(boundsMm.y()) && std::isfinite(boundsMm.z());
    surfaceAreaMm2_ = hasSelectedGeometry_ ? surfaceAreaMm2 : 0.0;
    volumeMm3_ = hasSelectedGeometry_ ? volumeMm3 : 0.0;
    boundsMm_ = hasSelectedGeometry_ ? boundsMm : QVector3D{};
    recompute();
    emit resultChanged();
}

void MeasurementController::clearSelectedGeometry()
{
    if (!hasSelectedGeometry_) return;
    hasSelectedGeometry_ = false;
    surfaceAreaMm2_ = 0.0;
    volumeMm3_ = 0.0;
    boundsMm_ = {};
    longestEdgeMm_ = 0.0;
    circularRadiusMm_ = 0.0;
    planarFaceCount_ = 0;
    recompute();
    emit resultChanged();
}

void MeasurementController::setSelectedTopology(const double longestEdgeMm, const double circularRadiusMm, const int planarFaceCount)
{
    longestEdgeMm_ = std::isfinite(longestEdgeMm) && longestEdgeMm >= 0.0 ? longestEdgeMm : 0.0;
    circularRadiusMm_ = std::isfinite(circularRadiusMm) && circularRadiusMm >= 0.0 ? circularRadiusMm : 0.0;
    planarFaceCount_ = std::max(0, planarFaceCount);
    emit resultChanged();
}

int MeasurementController::requiredPickCount() const noexcept
{
    switch (mode_) {
    case MeasurementMode::PointToPoint:
    case MeasurementMode::SurfaceToSurface:
    case MeasurementMode::Angle: return 2;
    case MeasurementMode::EdgeLength:
    case MeasurementMode::RadiusDiameter:
    case MeasurementMode::SurfaceArea: return 1;
    case MeasurementMode::Volume:
    case MeasurementMode::Bounds: return 0;
    }
    return 0;
}

void MeasurementController::append(const MeasureEntity& entity, const QString& description)
{
    // A finished measurement is kept on screen until the next pick, which starts the next one.
    if (picks_.size() >= requiredPickCount()) {
        picks_.clear();
        pickedDescriptions_.clear();
        legacyPicks_.clear();
    }
    picks_.append(entity);
    pickedDescriptions_.append(description);
    refusal_.clear();
}

void MeasurementController::recordPoint(const QVector3D& pointMm)
{
    recordPick(pointMm, {});
}

void MeasurementController::recordPick(const QVector3D& pointMm, const QVector3D& normal)
{
    recordPick(pointMm, normal, {}, QStringLiteral("Surface point"));
}

void MeasurementController::recordPick(const QVector3D& pointMm, const QVector3D& normal, const QString& nodeLabel, const QString& entityLabel)
{
    if (mode_ != MeasurementMode::PointToPoint && mode_ != MeasurementMode::SurfaceToSurface && mode_ != MeasurementMode::Angle) return;
    MeasureEntity entity;
    entity.kind = MeasureEntity::Kind::SurfacePoint;
    entity.point = pointMm;
    entity.normal = normal;
    entity.nodeLabel = nodeLabel;
    const auto where = coordinates(pointMm);
    append(entity, nodeLabel.isEmpty() ? QStringLiteral("%1 at %2").arg(entityLabel, where)
                                       : QStringLiteral("%1 on %2 at %3").arg(entityLabel, nodeLabel, where));
    legacyPicks_.append(true);
    recompute();
    emit resultChanged();
}

bool MeasurementController::recordTopologyPick(const QString& entityKind, const quint32 topologyId,
                                               const QVector3D& pointMm, const QVector3D& normal,
                                               const double measureMm, const double radiusMm,
                                               const QString& nodeLabel)
{
    const auto kind = entityKind.toLower();
    const auto isFace = kind == QStringLiteral("face");
    const auto isEdge = kind == QStringLiteral("edge");
    const auto allowed = (mode_ == MeasurementMode::EdgeLength && isEdge)
        || (mode_ == MeasurementMode::SurfaceToSurface && isFace)
        || (mode_ == MeasurementMode::RadiusDiameter && (isEdge || isFace) && radiusMm > 0.0)
        || (mode_ == MeasurementMode::Angle && isFace)
        || (mode_ == MeasurementMode::SurfaceArea && isFace);
    if (!allowed || topologyId == 0 || !std::isfinite(measureMm) || measureMm < 0.0
        || !std::isfinite(radiusMm) || radiusMm < 0.0) return false;

    MeasureEntity entity;
    entity.kind = isFace ? MeasureEntity::Kind::Face : MeasureEntity::Kind::Edge;
    entity.topologyId = topologyId;
    entity.point = pointMm;
    entity.normal = normal;
    entity.nodeLabel = nodeLabel;
    (isFace ? entity.areaMm2 : entity.lengthMm) = measureMm;
    entity.radiusMm = radiusMm;
    const auto name = isFace ? QStringLiteral("Face %1").arg(topologyId) : QStringLiteral("Edge %1").arg(topologyId);
    append(entity, nodeLabel.isEmpty() ? name : QStringLiteral("%1 on %2").arg(name, nodeLabel));
    legacyPicks_.append(true);
    recompute();
    emit resultChanged();
    return true;
}

bool MeasurementController::eligible(const MeasureEntity& entity, QString* reason) const
{
    const auto refuse = [reason](const QString& why) {
        if (reason) *reason = why;
        return false;
    };
    if (!entity.isValid()) return refuse({});
    const auto isFace = entity.kind == MeasureEntity::Kind::Face;
    const auto isEdge = entity.kind == MeasureEntity::Kind::Edge;
    switch (mode_) {
    case MeasurementMode::PointToPoint:
        return entity.isPointLike() || refuse(tr("Pick a point: a vertex, a circle's centre, or a point on an edge or face."));
    case MeasurementMode::EdgeLength:
        return isEdge || refuse(tr("Pick an edge."));
    case MeasurementMode::SurfaceToSurface:
        return isFace || refuse(tr("Pick a face."));
    case MeasurementMode::RadiusDiameter:
        if ((isEdge || isFace) && entity.radiusMm > 0.0) return true;
        return refuse(isEdge || isFace ? tr("Not round. Pick a circular edge, or a cylindrical or spherical face.")
                                       : tr("Pick a circular edge or a round face."));
    case MeasurementMode::Angle:
        if (isFace && entity.planar) return true;
        if (isEdge && entity.linear) return true;
        if (isFace) return refuse(tr("A curved face has no single direction. Pick a flat face or a straight edge."));
        if (isEdge) return refuse(tr("A curved edge has no single direction. Pick a straight edge or a flat face."));
        return refuse(tr("Pick a flat face or a straight edge."));
    case MeasurementMode::SurfaceArea:
        return isFace || refuse(tr("Pick a face."));
    case MeasurementMode::Volume:
    case MeasurementMode::Bounds:
        return refuse(tr("Select a part: volume and bounds are of the whole part."));
    }
    return false;
}

QString MeasurementController::describePick(const MeasureEntity& entity) const
{
    const auto name = entity.kindName();
    return entity.nodeLabel.isEmpty() ? name : tr("%1 on %2").arg(name, entity.nodeLabel);
}

bool MeasurementController::acceptEntity(const QVariant& value, const QString& nodeLabel)
{
    if (!value.canConvert<MeasureEntity>()) return false;
    auto entity = value.value<MeasureEntity>();
    if (!nodeLabel.isEmpty()) entity.nodeLabel = nodeLabel;
    return acceptEntity(entity);
}

bool MeasurementController::acceptEntity(const MeasureEntity& entity)
{
    QString reason;
    if (!eligible(entity, &reason)) {
        if (refusal_ != reason) {
            refusal_ = reason;
            emit resultChanged();
        }
        return false;
    }
    append(entity, describePick(entity));
    legacyPicks_.append(false);
    recompute();
    emit resultChanged();
    return true;
}

QVariantMap MeasurementController::describe(const QVariant& value) const
{
    if (!value.canConvert<MeasureEntity>()) return {};
    return describe(value.value<MeasureEntity>());
}

QVariantMap MeasurementController::describe(const MeasureEntity& entity) const
{
    if (!entity.isValid()) return {};
    QString detailText;
    switch (entity.kind) {
    case MeasureEntity::Kind::Edge:
        detailText = entity.circular || entity.radiusMm > 0.0
            ? tr("R %1 · %2 long").arg(formatLength(entity.radiusMm), formatLength(entity.lengthMm))
            : formatLength(entity.lengthMm);
        break;
    case MeasureEntity::Kind::Face:
        detailText = entity.radiusMm > 0.0 ? tr("R %1 · %2").arg(formatLength(entity.radiusMm), formatArea(entity.areaMm2))
                                          : formatArea(entity.areaMm2);
        break;
    case MeasureEntity::Kind::CircleCentre:
        detailText = tr("of R %1").arg(formatLength(entity.radiusMm));
        break;
    default:
        break;
    }
    QString reason;
    const auto ok = eligible(entity, &reason);
    const auto kind = [&entity]() {
        switch (entity.kind) {
        case MeasureEntity::Kind::SurfacePoint: return QStringLiteral("surfacePoint");
        case MeasureEntity::Kind::EdgePoint: return QStringLiteral("edgePoint");
        case MeasureEntity::Kind::Vertex: return QStringLiteral("vertex");
        case MeasureEntity::Kind::CircleCentre: return QStringLiteral("centre");
        case MeasureEntity::Kind::Edge: return QStringLiteral("edge");
        case MeasureEntity::Kind::Face: return QStringLiteral("face");
        case MeasureEntity::Kind::None: break;
        }
        return QString{};
    }();
    return {{QStringLiteral("kind"), kind},
            {QStringLiteral("topologyId"), entity.topologyId},
            {QStringLiteral("point"), QVariant::fromValue(entity.point)},
            {QStringLiteral("title"), entity.kindName()},
            {QStringLiteral("detail"), detailText},
            {QStringLiteral("eligible"), ok},
            {QStringLiteral("reason"), reason}};
}

QVariant MeasurementController::surfacePointEntity(const QVector3D& point, const QVector3D& normal)
{
    MeasureEntity entity;
    entity.kind = MeasureEntity::Kind::SurfacePoint;
    entity.point = point;
    entity.normal = normal;
    return QVariant::fromValue(entity);
}

void MeasurementController::clearPicks()
{
    picks_.clear();
    pickedDescriptions_.clear();
    legacyPicks_.clear();
    refusal_.clear();
    recompute();
    emit resultChanged();
}

void MeasurementController::setModeName(const QString& modeName)
{
    const auto mode = modeName.toLower();
    if (mode == QStringLiteral("edge")) setMode(MeasurementMode::EdgeLength);
    else if (mode == QStringLiteral("surface-distance")) setMode(MeasurementMode::SurfaceToSurface);
    else if (mode == QStringLiteral("radius")) setMode(MeasurementMode::RadiusDiameter);
    else if (mode == QStringLiteral("angle")) setMode(MeasurementMode::Angle);
    else if (mode == QStringLiteral("area")) setMode(MeasurementMode::SurfaceArea);
    else if (mode == QStringLiteral("volume")) setMode(MeasurementMode::Volume);
    else if (mode == QStringLiteral("bounds")) setMode(MeasurementMode::Bounds);
    else setMode(MeasurementMode::PointToPoint);
}

QString MeasurementController::modeKey() const
{
    switch (mode_) {
    case MeasurementMode::PointToPoint: return QStringLiteral("point");
    case MeasurementMode::EdgeLength: return QStringLiteral("edge");
    case MeasurementMode::SurfaceToSurface: return QStringLiteral("surface-distance");
    case MeasurementMode::RadiusDiameter: return QStringLiteral("radius");
    case MeasurementMode::Angle: return QStringLiteral("angle");
    case MeasurementMode::SurfaceArea: return QStringLiteral("area");
    case MeasurementMode::Volume: return QStringLiteral("volume");
    case MeasurementMode::Bounds: return QStringLiteral("bounds");
    }
    return {};
}

QString MeasurementController::pickInstruction() const
{
    const auto next = picks_.size() >= requiredPickCount() ? 0 : picks_.size();
    switch (mode_) {
    case MeasurementMode::PointToPoint:
        return next == 0 ? QStringLiteral("Select two points") : tr("Select the second point");
    case MeasurementMode::EdgeLength: return QStringLiteral("Select an edge or curve");
    case MeasurementMode::SurfaceToSurface:
        return next == 0 ? QStringLiteral("Select two planar surfaces") : tr("Select the second face");
    case MeasurementMode::RadiusDiameter: return QStringLiteral("Select a circular edge or face");
    case MeasurementMode::Angle:
        return next == 0 ? QStringLiteral("Select two edges or faces") : tr("Select the second flat face or straight edge");
    case MeasurementMode::SurfaceArea: return QStringLiteral("Select a surface");
    case MeasurementMode::Volume: return QStringLiteral("Select a closed solid");
    case MeasurementMode::Bounds: return QStringLiteral("Select a component");
    }
    return {};
}

MeasurementResult MeasurementController::pointDistance(const QVector3D& first, const QVector3D& second) const
{
    const auto millimeters = static_cast<double>((second - first).length());
    return {millimeters / lengthDivisor(), effectiveUnit_};
}

double MeasurementController::lengthDivisor() const
{
    return effectiveUnit_ == QStringLiteral("in") ? 25.4 : 1.0;
}

QString MeasurementController::lengthUnit() const
{
    return effectiveUnit_;
}

QString MeasurementController::formatLength(const double millimetres) const
{
    return QStringLiteral("%1 %2").arg(fixed(millimetres / lengthDivisor(), effectiveUnit_ == QStringLiteral("in") ? 4 : 3),
                                       lengthUnit());
}

QString MeasurementController::formatArea(const double squareMillimetres) const
{
    const auto divisor = lengthDivisor();
    return QStringLiteral("%1 %2").arg(fixed(squareMillimetres / (divisor * divisor), effectiveUnit_ == QStringLiteral("in") ? 4 : 2),
                                       effectiveUnit_ == QStringLiteral("in") ? QStringLiteral("in²") : QStringLiteral("mm²"));
}

void MeasurementController::recompute()
{
    result_ = {};
    const auto complete = requiredPickCount() > 0 && picks_.size() == requiredPickCount();
    const auto legacy = std::any_of(legacyPicks_.cbegin(), legacyPicks_.cend(), [](const bool value) { return value; });
    const auto distanceAnnotation = [this](const QVector3D& from, const QVector3D& to, const QString& text) {
        result_.annotation = {{QStringLiteral("kind"), QStringLiteral("distance")},
                              {QStringLiteral("from"), QVariant::fromValue(from)},
                              {QStringLiteral("to"), QVariant::fromValue(to)},
                              {QStringLiteral("text"), text}};
    };
    const auto labelAnnotation = [this](const QVector3D& at, const QString& text) {
        result_.annotation = {{QStringLiteral("kind"), QStringLiteral("label")},
                              {QStringLiteral("at"), QVariant::fromValue(at)},
                              {QStringLiteral("text"), text}};
    };
    const auto deltas = [this](const QVector3D& from, const QVector3D& to) {
        const auto delta = to - from;
        result_.details.append(detail(QStringLiteral("ΔX"), formatLength(std::abs(delta.x()))));
        result_.details.append(detail(QStringLiteral("ΔY"), formatLength(std::abs(delta.y()))));
        result_.details.append(detail(QStringLiteral("ΔZ"), formatLength(std::abs(delta.z()))));
    };

    switch (mode_) {
    case MeasurementMode::PointToPoint:
    case MeasurementMode::SurfaceToSurface: {
        if (!complete) break;
        const auto& a = picks_.at(0);
        const auto& b = picks_.at(1);
        result_.valid = true;
        result_.quantity = Quantity::Length;
        if (mode_ == MeasurementMode::PointToPoint || legacy) {
            result_.title = tr("Distance");
            result_.valueMm = static_cast<double>((b.point - a.point).length());
            result_.method = mode_ == MeasurementMode::PointToPoint ? tr("Straight line between the two points")
                                                                    : tr("Between the picked points");
            deltas(a.point, b.point);
            distanceAnnotation(a.point, b.point, formatLength(result_.valueMm));
            break;
        }
        const auto angle = measure::angleBetween(a, b);
        if (measure::parallelPlanes(a, b)) {
            const auto gap = measure::planeGap(a, b);
            result_.title = tr("Gap");
            result_.valueMm = gap.distance;
            result_.method = tr("Square to two parallel flat faces");
            const auto closest = measure::closestPoints(a, b);
            if (closest.valid() && std::abs(closest.distance - gap.distance) > 1e-4)
                result_.details.append(detail(tr("Minimum"), formatLength(closest.distance)));
            distanceAnnotation(gap.first, gap.second, formatLength(gap.distance));
        } else {
            const auto closest = measure::closestPoints(a, b);
            if (!closest.valid()) {
                result_.valid = false;
                break;
            }
            result_.title = tr("Minimum distance");
            result_.valueMm = closest.distance;
            result_.method = a.planar && b.planar ? tr("Closest points of two faces that are not parallel")
                                                  : tr("Closest points, on the displayed tessellation");
            deltas(closest.first, closest.second);
            distanceAnnotation(closest.first, closest.second, formatLength(closest.distance));
        }
        if (angle >= 0.0) result_.details.prepend(detail(tr("Angle"), QStringLiteral("%1°").arg(fixed(angle, 2))));
        break;
    }
    case MeasurementMode::Angle: {
        if (!complete) break;
        const auto& a = picks_.at(0);
        const auto& b = picks_.at(1);
        double degrees = -1.0;
        if (legacy) {
            const auto first = a.normal.normalized();
            const auto second = b.normal.normalized();
            if (!first.isNull() && !second.isNull())
                degrees = std::acos(std::clamp(static_cast<double>(QVector3D::dotProduct(first, second)), -1.0, 1.0)) * 180.0 / 3.14159265358979323846;
            result_.method = tr("Between the normals at the picked points");
        } else {
            degrees = measure::angleBetween(a, b);
            const auto faces = a.kind == MeasureEntity::Kind::Face && b.kind == MeasureEntity::Kind::Face;
            const auto lines = a.kind == MeasureEntity::Kind::Edge && b.kind == MeasureEntity::Kind::Edge;
            result_.method = faces ? tr("Between the two faces' normals")
                           : lines ? tr("Between the two edges' lines")
                                   : tr("Between the edge and the face's plane");
            if (faces && degrees >= 0.0)
                result_.details.append(detail(tr("Supplement"), QStringLiteral("%1°").arg(fixed(180.0 - degrees, 2))));
            else if (degrees >= 0.0)
                result_.details.append(detail(tr("Complement"), QStringLiteral("%1°").arg(fixed(90.0 - degrees, 2))));
        }
        if (degrees < 0.0) break;
        result_.valid = true;
        result_.quantity = Quantity::Angle;
        result_.title = tr("Angle");
        result_.valueMm = degrees;
        labelAnnotation((a.point + b.point) * 0.5F, QStringLiteral("%1°").arg(fixed(degrees, 2)));
        break;
    }
    case MeasurementMode::EdgeLength: {
        if (!complete) break;
        const auto& edge = picks_.front();
        result_.valid = true;
        result_.quantity = Quantity::Length;
        result_.title = tr("Length");
        result_.valueMm = edge.lengthMm;
        result_.method = edge.circular ? tr("Along the circular edge")
                       : edge.linear ? tr("Along the straight edge") : tr("Along the edge");
        if (edge.radiusMm > 0.0) {
            result_.details.append(detail(tr("Radius"), formatLength(edge.radiusMm)));
            result_.details.append(detail(tr("Diameter"), formatLength(edge.radiusMm * 2.0)));
        }
        labelAnnotation(midpointOf(edge.polyline, edge.point), formatLength(edge.lengthMm));
        break;
    }
    case MeasurementMode::RadiusDiameter: {
        if (!complete) break;
        const auto& round = picks_.front();
        result_.valid = true;
        result_.quantity = Quantity::Length;
        result_.title = tr("Radius");
        result_.valueMm = round.radiusMm;
        result_.method = round.kind == MeasureEntity::Kind::Face ? tr("Of the round face, from the CAD surface")
                                                                 : tr("Of the circular edge, from the CAD curve");
        result_.details.append(detail(tr("Diameter"), formatLength(round.radiusMm * 2.0)));
        const auto text = tr("R %1 · Ø %2").arg(fixed(round.radiusMm / lengthDivisor(), effectiveUnit_ == QStringLiteral("in") ? 4 : 3),
                                                formatLength(round.radiusMm * 2.0));
        if (round.circular) {
            result_.details.append(detail(tr("Centre"), coordinates(round.centre)));
            result_.annotation = {{QStringLiteral("kind"), QStringLiteral("radius")},
                                  {QStringLiteral("from"), QVariant::fromValue(round.centre)},
                                  {QStringLiteral("to"), QVariant::fromValue(round.point)},
                                  {QStringLiteral("text"), text}};
        } else {
            labelAnnotation(round.point, text);
        }
        break;
    }
    case MeasurementMode::SurfaceArea: {
        if (!complete) break;
        const auto& face = picks_.front();
        result_.valid = true;
        result_.quantity = Quantity::Area;
        result_.title = tr("Area");
        result_.valueMm = face.areaMm2;
        result_.method = tr("Of the whole face, from the CAD surface");
        labelAnnotation(face.point, formatArea(face.areaMm2));
        break;
    }
    case MeasurementMode::Volume:
        if (!hasSelectedGeometry_) break;
        result_.valid = true;
        result_.quantity = Quantity::Volume;
        result_.title = tr("Volume");
        result_.valueMm = volumeMm3_;
        result_.method = tr("Of the selected solid, from the CAD shape");
        result_.details.append(detail(tr("Surface area"), formatArea(surfaceAreaMm2_)));
        break;
    case MeasurementMode::Bounds:
        if (!hasSelectedGeometry_) break;
        result_.valid = true;
        result_.quantity = Quantity::Bounds;
        result_.title = tr("Bounds");
        result_.method = tr("Axis-aligned box around the selection");
        result_.details.append(detail(QStringLiteral("X"), formatLength(boundsMm_.x())));
        result_.details.append(detail(QStringLiteral("Y"), formatLength(boundsMm_.y())));
        result_.details.append(detail(QStringLiteral("Z"), formatLength(boundsMm_.z())));
        break;
    }
}

double MeasurementController::resultValue() const
{
    if (!result_.valid) return 0.0;
    const auto divisor = lengthDivisor();
    switch (result_.quantity) {
    case Quantity::Length: return result_.valueMm / divisor;
    case Quantity::Angle: return result_.valueMm;
    case Quantity::Area: return result_.valueMm / (divisor * divisor);
    case Quantity::Volume: return result_.valueMm / (divisor * divisor * divisor);
    case Quantity::Bounds:
    case Quantity::None: return 0.0;
    }
    return 0.0;
}

QString MeasurementController::resultUnit() const
{
    const auto inches = effectiveUnit_ == QStringLiteral("in");
    switch (mode_) {
    case MeasurementMode::PointToPoint:
    case MeasurementMode::SurfaceToSurface: return picks_.size() == 2 ? effectiveUnit_ : QString{};
    case MeasurementMode::Angle: return picks_.size() == 2 ? QStringLiteral("°") : QString{};
    case MeasurementMode::SurfaceArea: return inches ? QStringLiteral("in²") : QStringLiteral("mm²");
    case MeasurementMode::Volume: return inches ? QStringLiteral("in³") : QStringLiteral("mm³");
    case MeasurementMode::Bounds:
    case MeasurementMode::EdgeLength:
    case MeasurementMode::RadiusDiameter: return effectiveUnit_;
    }
    return {};
}

QString MeasurementController::resultLabel() const
{
    if (!result_.valid) return {};
    switch (mode_) {
    case MeasurementMode::PointToPoint:
    case MeasurementMode::SurfaceToSurface:
    case MeasurementMode::Angle:
    case MeasurementMode::SurfaceArea:
    case MeasurementMode::Volume: return QStringLiteral("%1 %2").arg(formatValue(resultValue()), resultUnit());
    case MeasurementMode::EdgeLength: return QStringLiteral("Edge length: %1 %2").arg(formatValue(resultValue()), resultUnit());
    case MeasurementMode::RadiusDiameter:
        return QStringLiteral("Radius: %1 %2 · Diameter: %3 %2").arg(formatValue(resultValue()), resultUnit(), formatValue(resultValue() * 2.0));
    case MeasurementMode::Bounds: {
        const auto divisor = static_cast<float>(lengthDivisor());
        return QStringLiteral("%1 × %2 × %3 %4")
            .arg(formatValue(boundsMm_.x() / divisor), formatValue(boundsMm_.y() / divisor), formatValue(boundsMm_.z() / divisor), resultUnit());
    }
    }
    return {};
}

QString MeasurementController::resultTitle() const
{
    return result_.valid ? result_.title : QString{};
}

QString MeasurementController::resultText() const
{
    if (!result_.valid) return {};
    const auto inches = effectiveUnit_ == QStringLiteral("in");
    switch (result_.quantity) {
    case Quantity::Length: return fixed(resultValue(), inches ? 4 : 3);
    case Quantity::Angle: return fixed(resultValue(), 2);
    case Quantity::Area: return fixed(resultValue(), inches ? 4 : 2);
    case Quantity::Volume: return fixed(resultValue(), inches ? 4 : 1);
    case Quantity::Bounds: {
        const auto divisor = lengthDivisor();
        const auto decimals = inches ? 4 : 3;
        return QStringLiteral("%1 × %2 × %3").arg(fixed(boundsMm_.x() / divisor, decimals), fixed(boundsMm_.y() / divisor, decimals),
                                                   fixed(boundsMm_.z() / divisor, decimals));
    }
    case Quantity::None: return {};
    }
    return {};
}

QString MeasurementController::resultSummary() const
{
    if (!result_.valid) return {};
    QStringList lines;
    lines << QStringLiteral("%1: %2 %3").arg(result_.title, resultText(), resultUnit());
    for (const auto& value : result_.details) {
        const auto map = value.toMap();
        lines << QStringLiteral("%1: %2").arg(map.value(QStringLiteral("label")).toString(), map.value(QStringLiteral("value")).toString());
    }
    for (qsizetype index = 0; index < pickedDescriptions_.size(); ++index)
        lines << QStringLiteral("%1. %2").arg(index + 1).arg(pickedDescriptions_.at(index));
    if (!result_.method.isEmpty()) lines << result_.method;
    return lines.join(QLatin1Char('\n'));
}

void MeasurementController::copyResult() const
{
    const auto text = resultSummary();
    if (text.isEmpty()) return;
    if (auto* clipboard = QGuiApplication::clipboard()) clipboard->setText(text);
}

QString MeasurementController::firstPickDescription() const
{
    return pickedDescriptions_.isEmpty() ? QString{} : pickedDescriptions_.front();
}

QString MeasurementController::secondPickDescription() const
{
    return pickedDescriptions_.size() < 2 ? QString{} : pickedDescriptions_.at(1);
}

QVariantList MeasurementController::pickedPoints() const
{
    QVariantList result;
    result.reserve(picks_.size());
    for (const auto& pick : picks_) {
        const auto& point = pick.kind == MeasureEntity::Kind::CircleCentre ? pick.centre : pick.point;
        result.append(QVariant::fromValue(point));
    }
    return result;
}

QVariantList MeasurementController::pickedEntities() const
{
    QVariantList result;
    result.reserve(picks_.size());
    for (qsizetype index = 0; index < picks_.size(); ++index) {
        const auto& pick = picks_.at(index);
        const auto kind = pick.kind == MeasureEntity::Kind::Face ? QStringLiteral("face")
                        : pick.kind == MeasureEntity::Kind::Edge ? QStringLiteral("edge")
                                                                 : QStringLiteral("point");
        const auto description = describe(pick);
        result.append(QVariantMap{{QStringLiteral("entityKind"), kind},
                                  {QStringLiteral("topologyId"), pick.topologyId},
                                  {QStringLiteral("point"), QVariant::fromValue(pick.point)},
                                  {QStringLiteral("measureMm"), pick.kind == MeasureEntity::Kind::Face ? pick.areaMm2 : pick.lengthMm},
                                  {QStringLiteral("radiusMm"), pick.radiusMm},
                                  {QStringLiteral("title"), legacyPicks_.value(index) ? pickedDescriptions_.value(index) : pick.kindName()},
                                  {QStringLiteral("detail"), description.value(QStringLiteral("detail"))},
                                  {QStringLiteral("part"), pick.nodeLabel}});
    }
    return result;
}

} // namespace loupe::app::tools
