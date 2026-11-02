#include "app/tools/ExplodeController.h"

#include <QGenericMatrix>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace loupe::app::tools {

namespace {

constexpr int kHistoryLimit = 200;

// A rigid transform that moves nothing visible: a residue of float arithmetic after a part
// is dragged back, say. Treated as none, so `exploded` means a part is really out of place.
bool isIdentity(const QMatrix4x4& matrix)
{
    const QMatrix4x4 identity;
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            const auto tolerance = column == 3 ? 1e-4F : 1e-6F;
            if (std::abs(matrix(row, column) - identity(row, column)) > tolerance) return false;
        }
    }
    return true;
}

QQuaternion rotationOf(const QMatrix4x4& matrix)
{
    return QQuaternion::fromRotationMatrix(matrix.toGenericMatrix<3, 3>()).normalized();
}

QMatrix4x4 aboutPoint(const QVector3D& to, const QQuaternion& rotation, const QVector3D& from)
{
    QMatrix4x4 matrix;
    matrix.translate(to);
    matrix.rotate(rotation);
    matrix.translate(-from);
    return matrix;
}

QMatrix4x4 translation(const QVector3D& delta)
{
    QMatrix4x4 matrix;
    matrix.translate(delta);
    return matrix;
}

float cleanAngle(const float degrees)
{
    return std::abs(degrees) < 1e-4F ? 0.0F : degrees;
}

} // namespace

void ExplodeController::Bounds::include(const Bounds& other)
{
    if (!other.valid) return;
    if (!valid) {
        *this = other;
        return;
    }
    minimum = QVector3D(std::min(minimum.x(), other.minimum.x()), std::min(minimum.y(), other.minimum.y()),
                        std::min(minimum.z(), other.minimum.z()));
    maximum = QVector3D(std::max(maximum.x(), other.maximum.x()), std::max(maximum.y(), other.maximum.y()),
                        std::max(maximum.z(), other.maximum.z()));
}

ExplodeController::ExplodeController(QObject* parent)
    : QObject(parent)
{
}

void ExplodeController::setHierarchy(const QHash<QString, QString>& parentByNode)
{
    // A new tree starts every node at home, but keeps the part bounds already seen: the
    // geometry they came from is still what is drawn.
    const auto bounds = partBounds_;
    clear();
    partBounds_ = bounds;
    parentByNode_.clear();
    childrenByNode_.clear();
    for (auto it = parentByNode.cbegin(); it != parentByNode.cend(); ++it) {
        if (it.key().isEmpty()) continue;
        // A parent the tree does not name makes its child a top-level node.
        const auto parent = it.value().isEmpty() || parentByNode.contains(it.value()) ? it.value() : QString{};
        parentByNode_.insert(it.key(), parent == it.key() ? QString{} : parent);
    }
    for (auto it = partBounds_.cbegin(); it != partBounds_.cend(); ++it) {
        if (!parentByNode_.contains(it.key())) parentByNode_.insert(it.key(), {});
    }
    for (auto it = parentByNode_.cbegin(); it != parentByNode_.cend(); ++it) childrenByNode_[it.value()].append(it.key());
    for (auto& children : childrenByNode_) std::sort(children.begin(), children.end());
    invalidate();
}

void ExplodeController::includePartBounds(const QString& nodeId, const QVector3D& minimum, const QVector3D& maximum)
{
    if (nodeId.isEmpty()) return;
    if (!parentByNode_.contains(nodeId)) {
        parentByNode_.insert(nodeId, {});
        childrenByNode_[{}].append(nodeId);
    }
    partBounds_[nodeId].include({minimum, maximum, true});
    boundsCache_.clear();
}

void ExplodeController::clear()
{
    const bool wasInteracting = interacting();
    const bool hadHistory = canUndo() || canRedo();
    const auto movedParts = exploded() ? partsUnder({}) : QStringList{};
    interaction_ = Interaction::None;
    interactionNodes_.clear();
    interactionPlacements_.clear();
    state_ = {};
    undo_.clear();
    redo_.clear();
    partBounds_.clear();
    blend_ = 1.0;
    invalidate();
    ++revision_;
    if (!movedParts.isEmpty()) emit partsMoved(movedParts);
    emit changed();
    if (wasInteracting) emit interactionChanged();
    if (hadHistory) emit historyChanged();
}

QMatrix4x4 ExplodeController::effectiveTransform(const QString& nodeId) const
{
    if (nodeId.isEmpty() || state_.transforms.isEmpty()) return {};
    if (const auto cached = effectiveCache_.constFind(nodeId); cached != effectiveCache_.cend()) return *cached;
    const auto parent = parentOf(nodeId);
    // A cycle would never end; the tree has none, but nothing here depends on that.
    QMatrix4x4 result = parent.isEmpty() || isAncestor(nodeId, parent) ? QMatrix4x4{} : effectiveTransform(parent);
    if (const auto own = state_.transforms.constFind(nodeId); own != state_.transforms.cend()) result = result * *own;
    effectiveCache_.insert(nodeId, result);
    return result;
}

QMatrix4x4 ExplodeController::parentTransform(const QString& nodeId) const
{
    return effectiveTransform(parentOf(nodeId));
}

QMatrix4x4 ExplodeController::displayedTransform(const QString& partId) const
{
    const auto placement = effectiveTransform(partId);
    if (blend_ >= 1.0 || isIdentity(placement)) return placement;
    // Ease from home about the part's own centre, so a turned part swings in place rather
    // than about the model's origin on its way back.
    const auto centre = nodeBounds(partId).centre();
    const auto moved = placement.map(centre);
    const auto factor = static_cast<float>(std::clamp(blend_, 0.0, 1.0));
    const auto rotation = QQuaternion::slerp(QQuaternion{}, rotationOf(placement), factor);
    return aboutPoint(centre + (moved - centre) * factor, rotation, centre);
}

QVector3D ExplodeController::displayedPosition(const QString& nodeId) const
{
    return displayedTransform(nodeId).column(3).toVector3D();
}

QQuaternion ExplodeController::displayedRotation(const QString& nodeId) const
{
    return rotationOf(displayedTransform(nodeId));
}

bool ExplodeController::isMoved(const QString& nodeId) const
{
    return !isIdentity(effectiveTransform(nodeId));
}

bool ExplodeController::canReset(const QString& nodeId) const
{
    if (!hasNode(nodeId) || state_.transforms.isEmpty()) return false;
    if (state_.transforms.contains(nodeId) || state_.autoFactors.contains(nodeId)) return true;
    const auto descendants = descendantsOf(nodeId);
    return std::any_of(descendants.cbegin(), descendants.cend(), [this](const QString& node) {
        return state_.transforms.contains(node);
    });
}

bool ExplodeController::hasNode(const QString& nodeId) const
{
    return !nodeId.isEmpty() && (parentByNode_.contains(nodeId) || partBounds_.contains(nodeId));
}

QString ExplodeController::parentOf(const QString& nodeId) const
{
    return parentByNode_.value(nodeId);
}

bool ExplodeController::isAncestor(const QString& ancestorId, const QString& nodeId) const
{
    if (ancestorId.isEmpty()) return !nodeId.isEmpty();
    auto current = parentOf(nodeId);
    for (int depth = 0; !current.isEmpty() && depth <= parentByNode_.size(); ++depth) {
        if (current == ancestorId) return true;
        current = parentOf(current);
    }
    return false;
}

QStringList ExplodeController::descendantsOf(const QString& nodeId) const
{
    QStringList result;
    QSet<QString> seen;
    QStringList pending = childrenByNode_.value(nodeId);
    while (!pending.isEmpty()) {
        const auto next = pending.takeLast();
        if (seen.contains(next)) continue;
        seen.insert(next);
        result.append(next);
        pending.append(childrenByNode_.value(next));
    }
    return result;
}

QStringList ExplodeController::partsUnder(const QString& nodeId) const
{
    if (nodeId.isEmpty()) return partBounds_.keys();
    QStringList result;
    if (partBounds_.contains(nodeId)) result.append(nodeId);
    for (const auto& descendant : descendantsOf(nodeId)) {
        if (partBounds_.contains(descendant)) result.append(descendant);
    }
    return result;
}

ExplodeController::Bounds ExplodeController::nodeBounds(const QString& nodeId) const
{
    if (const auto cached = boundsCache_.constFind(nodeId); cached != boundsCache_.cend()) return *cached;
    Bounds bounds;
    for (const auto& part : partsUnder(nodeId)) bounds.include(partBounds_.value(part));
    boundsCache_.insert(nodeId, bounds);
    return bounds;
}

QVector3D ExplodeController::assembledCentre(const QString& nodeId) const
{
    const auto bounds = nodeBounds(nodeId);
    return bounds.valid ? bounds.centre() : QVector3D{};
}

QVariantMap ExplodeController::displayedBounds(const QStringList& nodeIds) const
{
    QStringList parts;
    if (nodeIds.isEmpty()) {
        parts = partsUnder({});
    } else {
        QSet<QString> unique;
        for (const auto& nodeId : nodeIds) {
            for (const auto& part : partsUnder(nodeId)) unique.insert(part);
        }
        parts = unique.values();
    }
    Bounds result;
    for (const auto& part : parts) {
        const auto bounds = partBounds_.value(part);
        if (!bounds.valid) continue;
        const auto transform = displayedTransform(part);
        for (int corner = 0; corner < 8; ++corner) {
            const QVector3D point((corner & 1) ? bounds.maximum.x() : bounds.minimum.x(),
                                  (corner & 2) ? bounds.maximum.y() : bounds.minimum.y(),
                                  (corner & 4) ? bounds.maximum.z() : bounds.minimum.z());
            const auto mapped = transform.map(point);
            result.include({mapped, mapped, true});
        }
    }
    return {
        {QStringLiteral("valid"), result.valid},
        {QStringLiteral("minimum"), result.minimum},
        {QStringLiteral("maximum"), result.maximum},
    };
}

QVector3D ExplodeController::offsetOf(const QString& nodeId) const
{
    const auto centre = assembledCentre(nodeId);
    return effectiveTransform(nodeId).map(centre) - centre;
}

QVector3D ExplodeController::anglesOf(const QString& nodeId) const
{
    const auto angles = rotationOf(effectiveTransform(nodeId)).toEulerAngles();
    return {cleanAngle(angles.x()), cleanAngle(angles.y()), cleanAngle(angles.z())};
}

QVector4D ExplodeController::localPlane(const QString& nodeId, const QVector3D& normal, const double offset) const
{
    const auto transform = displayedTransform(nodeId);
    // x' = R x + t, so n . x' = d  <=>  (R^T n) . x = d - n . t.
    const auto localNormal = rotationOf(transform).conjugated().rotatedVector(normal);
    const auto localOffset = offset - QVector3D::dotProduct(normal, transform.column(3).toVector3D());
    return {localNormal, static_cast<float>(localOffset)};
}

double ExplodeController::autoFactor(const QString& scopeId) const
{
    return state_.autoFactors.value(scopeId, 0.0);
}

QString ExplodeController::autoExplodeScope(const QStringList& selectedNodeIds) const
{
    if (selectedNodeIds.size() != 1 || !hasNode(selectedNodeIds.front())) return {};
    const auto& node = selectedNodeIds.front();
    return childrenByNode_.value(node).isEmpty() ? parentOf(node) : node;
}

double ExplodeController::snapTranslation(const double millimetres, const bool free) const
{
    if (free || !snapEnabled_ || translationStep_ <= 0.0) return millimetres;
    return std::round(millimetres / translationStep_) * translationStep_;
}

double ExplodeController::snapRotation(const double degrees, const bool free) const
{
    if (free || !snapEnabled_ || rotationStep_ <= 0.0) return degrees;
    return std::round(degrees / rotationStep_) * rotationStep_;
}

void ExplodeController::setOwnTransform(const QString& nodeId, const QMatrix4x4& transform)
{
    if (isIdentity(transform)) state_.transforms.remove(nodeId);
    else state_.transforms.insert(nodeId, transform);
    effectiveCache_.clear();
}

void ExplodeController::setPlacement(const QString& nodeId, const QMatrix4x4& placement)
{
    setOwnTransform(nodeId, parentTransform(nodeId).inverted() * placement);
}

void ExplodeController::invalidate()
{
    effectiveCache_.clear();
    boundsCache_.clear();
}

void ExplodeController::announce(const QStringList& partIds)
{
    effectiveCache_.clear();
    ++revision_;
    if (!partIds.isEmpty()) emit partsMoved(partIds);
    emit changed();
}

void ExplodeController::announceStateChange(const State& before)
{
    QSet<QString> nodes;
    for (auto it = before.transforms.cbegin(); it != before.transforms.cend(); ++it) nodes.insert(it.key());
    for (auto it = state_.transforms.cbegin(); it != state_.transforms.cend(); ++it) nodes.insert(it.key());
    QSet<QString> parts;
    for (const auto& node : nodes) {
        for (const auto& part : partsUnder(node)) parts.insert(part);
    }
    announce(parts.values());
}

void ExplodeController::pushUndo(const State& before)
{
    undo_.append(before);
    if (undo_.size() > kHistoryLimit) undo_.removeFirst();
    redo_.clear();
    emit historyChanged();
}

void ExplodeController::applyEdit(const State& before, const QStringList& changedNodes)
{
    QSet<QString> parts;
    for (const auto& node : changedNodes) {
        for (const auto& part : partsUnder(node)) parts.insert(part);
    }
    announce(parts.values());
    if (before.transforms != state_.transforms || before.autoFactors != state_.autoFactors) pushUndo(before);
}

void ExplodeController::beginInteraction(const QStringList& nodeIds)
{
    if (interacting()) commitInteraction();
    QStringList nodes;
    for (const auto& nodeId : nodeIds) {
        if (!hasNode(nodeId) || nodes.contains(nodeId)) continue;
        nodes.append(nodeId);
    }
    // A node inside another one being moved already goes with it.
    nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [&](const QString& node) {
        return std::any_of(nodes.cbegin(), nodes.cend(), [&](const QString& other) {
            return other != node && isAncestor(other, node);
        });
    }), nodes.end());
    if (nodes.isEmpty()) return;
    interactionStart_ = state_;
    interactionNodes_ = nodes;
    interactionPlacements_.clear();
    for (const auto& node : nodes) interactionPlacements_.insert(node, effectiveTransform(node));
    interaction_ = Interaction::Handle;
    emit interactionChanged();
}

void ExplodeController::previewTranslation(const QVector3D& worldDelta)
{
    if (interaction_ != Interaction::Handle) return;
    const auto move = translation(worldDelta);
    for (const auto& node : interactionNodes_) setPlacement(node, move * interactionPlacements_.value(node));
    QStringList parts;
    for (const auto& node : interactionNodes_) parts.append(partsUnder(node));
    announce(parts);
}

void ExplodeController::previewRotation(const QVector3D& axis, const double degrees, const QVector3D& pivot)
{
    if (interaction_ != Interaction::Handle || axis.lengthSquared() <= 0.0F) return;
    const auto turn = aboutPoint(pivot, QQuaternion::fromAxisAndAngle(axis.normalized(), static_cast<float>(degrees)), pivot);
    for (const auto& node : interactionNodes_) setPlacement(node, turn * interactionPlacements_.value(node));
    QStringList parts;
    for (const auto& node : interactionNodes_) parts.append(partsUnder(node));
    announce(parts);
}

void ExplodeController::commitInteraction()
{
    if (!interacting()) return;
    const auto changedNodes = interaction_ == Interaction::AutoExplode ? QStringList{autoScope_} : interactionNodes_;
    interaction_ = Interaction::None;
    interactionNodes_.clear();
    interactionPlacements_.clear();
    applyEdit(interactionStart_, changedNodes);
    emit interactionChanged();
}

void ExplodeController::cancelInteraction()
{
    if (!interacting()) return;
    const auto before = state_;
    state_ = interactionStart_;
    interaction_ = Interaction::None;
    interactionNodes_.clear();
    interactionPlacements_.clear();
    announceStateChange(before);
    emit interactionChanged();
}

void ExplodeController::beginAutoExplode(const QString& scopeId)
{
    if (interacting()) commitInteraction();
    if (!scopeId.isEmpty() && !hasNode(scopeId)) return;
    interactionStart_ = state_;
    autoScope_ = scopeId;
    autoStartFactor_ = autoFactor(scopeId);
    interaction_ = Interaction::AutoExplode;
    emit interactionChanged();
}

void ExplodeController::previewAutoExplode(const double factor)
{
    if (interaction_ != Interaction::AutoExplode) return;
    const auto target = std::max(0.0, factor);
    const auto delta = static_cast<float>(target - autoStartFactor_);
    state_.transforms = interactionStart_.transforms;
    effectiveCache_.clear();
    for (const auto& node : descendantsOf(autoScope_)) {
        const auto bounds = nodeBounds(node);
        const auto parentBounds = nodeBounds(parentOf(node));
        if (!bounds.valid || !parentBounds.valid) continue;
        const auto direction = bounds.centre() - parentBounds.centre();
        if (direction.lengthSquared() < 1e-12F) continue;
        // In the parent's frame, so a turned sub-assembly still comes apart along its own axes.
        setOwnTransform(node, translation(direction * delta) * state_.transforms.value(node));
    }
    if (target < 1e-6) state_.autoFactors.remove(autoScope_);
    else state_.autoFactors.insert(autoScope_, target);
    announce(partsUnder(autoScope_));
}

void ExplodeController::autoExplode(const QString& scopeId, const double factor)
{
    beginAutoExplode(scopeId);
    previewAutoExplode(factor);
    commitInteraction();
}

void ExplodeController::translateBy(const QStringList& nodeIds, const QVector3D& delta)
{
    if (delta.lengthSquared() <= 0.0F) return;
    beginInteraction(nodeIds);
    if (interaction_ != Interaction::Handle) return;
    previewTranslation(delta);
    commitInteraction();
}

void ExplodeController::setOffset(const QString& nodeId, const QVector3D& offset)
{
    if (!hasNode(nodeId)) return;
    if (interacting()) commitInteraction();
    const auto before = state_;
    const auto centre = assembledCentre(nodeId);
    setPlacement(nodeId, aboutPoint(centre + offset, rotationOf(effectiveTransform(nodeId)), centre));
    applyEdit(before, {nodeId});
}

void ExplodeController::setAngles(const QString& nodeId, const QVector3D& degrees)
{
    if (!hasNode(nodeId)) return;
    if (interacting()) commitInteraction();
    const auto before = state_;
    const auto centre = assembledCentre(nodeId);
    const auto moved = effectiveTransform(nodeId).map(centre);
    setPlacement(nodeId, aboutPoint(moved, QQuaternion::fromEulerAngles(degrees), centre));
    applyEdit(before, {nodeId});
}

void ExplodeController::resetNode(const QString& nodeId)
{
    if (!hasNode(nodeId)) return;
    if (interacting()) commitInteraction();
    const auto before = state_;
    auto nodes = descendantsOf(nodeId);
    nodes.append(nodeId);
    for (const auto& node : nodes) {
        state_.transforms.remove(node);
        state_.autoFactors.remove(node);
    }
    applyEdit(before, {nodeId});
}

void ExplodeController::resetAll()
{
    if (interacting()) cancelInteraction();
    const auto before = state_;
    state_ = {};
    blend_ = 1.0;
    announceStateChange(before);
    if (before.transforms != state_.transforms || before.autoFactors != state_.autoFactors) pushUndo(before);
}

void ExplodeController::undo()
{
    if (interacting()) cancelInteraction();
    if (undo_.isEmpty()) return;
    const auto before = state_;
    redo_.append(state_);
    state_ = undo_.takeLast();
    blend_ = 1.0;
    announceStateChange(before);
    emit historyChanged();
}

void ExplodeController::redo()
{
    if (interacting()) cancelInteraction();
    if (redo_.isEmpty()) return;
    const auto before = state_;
    undo_.append(state_);
    state_ = redo_.takeLast();
    blend_ = 1.0;
    announceStateChange(before);
    emit historyChanged();
}

void ExplodeController::setBlend(const double blend)
{
    const auto clamped = std::clamp(blend, 0.0, 1.0);
    if (std::abs(clamped - blend_) < 1e-9) return;
    blend_ = clamped;
    QSet<QString> parts;
    for (auto it = state_.transforms.cbegin(); it != state_.transforms.cend(); ++it) {
        for (const auto& part : partsUnder(it.key())) parts.insert(part);
    }
    announce(parts.values());
}

void ExplodeController::setSnapEnabled(const bool enabled)
{
    if (snapEnabled_ == enabled) return;
    snapEnabled_ = enabled;
    emit settingsChanged();
}

void ExplodeController::setTranslationStep(const double step)
{
    if (step <= 0.0 || std::abs(step - translationStep_) < 1e-9) return;
    translationStep_ = step;
    emit settingsChanged();
}

void ExplodeController::setRotationStep(const double step)
{
    if (step <= 0.0 || std::abs(step - rotationStep_) < 1e-9) return;
    rotationStep_ = step;
    emit settingsChanged();
}

} // namespace loupe::app::tools
