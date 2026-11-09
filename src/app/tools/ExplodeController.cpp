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
    seatAxes_.clear();
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

bool ExplodeController::assembledBox(const QStringList& nodeIds, QVector3D* minimum, QVector3D* maximum) const
{
    Bounds result;
    if (nodeIds.isEmpty()) {
        for (auto it = partBounds_.cbegin(); it != partBounds_.cend(); ++it) result.include(*it);
    } else {
        for (const auto& nodeId : nodeIds) result.include(nodeBounds(nodeId));
    }
    if (!result.valid) return false;
    if (minimum) *minimum = result.minimum;
    if (maximum) *maximum = result.maximum;
    return true;
}

QVector3D ExplodeController::movedCentre(const QStringList& nodeIds) const
{
    QVector3D sum;
    int count = 0;
    QSet<QString> seen;
    for (const auto& nodeId : nodeIds) {
        if (seen.contains(nodeId)) continue;
        seen.insert(nodeId);
        const auto bounds = nodeBounds(nodeId);
        if (!bounds.valid) continue;
        sum += showingAssembly() ? bounds.centre() : effectiveTransform(nodeId).map(bounds.centre());
        ++count;
    }
    return count > 0 ? sum / static_cast<float>(count) : QVector3D{};
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

// The read-outs follow what is on screen: on the assembly, with a layout kept aside, a part
// is where the file puts it, and that is where an edit starts from.
QVector3D ExplodeController::offsetOf(const QString& nodeId) const
{
    if (showingAssembly()) return {};
    const auto centre = assembledCentre(nodeId);
    return effectiveTransform(nodeId).map(centre) - centre;
}

QVector3D ExplodeController::anglesOf(const QString& nodeId) const
{
    if (showingAssembly()) return {};
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
    if (showingAssembly()) return 0.0;
    return state_.autoFactors.value(scopeId, 0.0);
}

QString ExplodeController::autoExplodeScope(const QStringList& selectedNodeIds) const
{
    if (selectedNodeIds.size() != 1 || !hasNode(selectedNodeIds.front())) return {};
    const auto& node = selectedNodeIds.front();
    return childrenByNode_.value(node).isEmpty() ? parentOf(node) : node;
}

QQuaternion ExplodeController::frameForNormal(const QVector3D& normal)
{
    if (normal.lengthSquared() < 1e-12F) return {};
    const auto z = normal.normalized();
    // Lie X in the world's XY plane unless the normal is nearly vertical, then use world X.
    const QVector3D reference = std::abs(z.z()) < 0.9F ? QVector3D(0, 0, 1) : QVector3D(0, 1, 0);
    const auto x = QVector3D::crossProduct(reference, z).normalized();
    const auto y = QVector3D::crossProduct(z, x);
    return QQuaternion::fromAxes(x, y, z).normalized();
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
    if (!before.sameLayout(state_)) pushUndo(before);
}

void ExplodeController::beginInteraction(const QStringList& nodeIds)
{
    showLayoutForEditing(EditFrom::Assembly);
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
    showLayoutForEditing(EditFrom::Assembly);
    if (interacting()) commitInteraction();
    if (!scopeId.isEmpty() && !hasNode(scopeId)) return;
    interactionStart_ = state_;
    autoScope_ = scopeId;
    autoStartFactor_ = autoFactor(scopeId);
    autoVectors_ = autoExplodeVectors(scopeId);
    interaction_ = Interaction::AutoExplode;
    emit interactionChanged();
}

QHash<QString, QVector3D> ExplodeController::autoExplodeVectors(const QString& scopeId) const
{
    // Where each node goes at factor 1, relative to its parent: away from the parent's
    // centre by its distance from it, and, for siblings whose boxes overlap, apart from each
    // other along the axis they overlap least. The first alone leaves parts that share a
    // centre -- layers stacked on one another, a part inside another -- moving as one; the
    // second is what takes a stack apart, in order, along its stacking direction.
    QHash<QString, QVector3D> vectors;
    QStringList parents{scopeId};
    parents.append(descendantsOf(scopeId));
    for (const auto& parent : parents) {
        const auto parentBounds = nodeBounds(parent);
        if (!parentBounds.valid) continue;
        QStringList children;
        QVector<Bounds> boxes;
        for (const auto& child : childrenByNode_.value(parent)) {
            const auto bounds = nodeBounds(child);
            if (!bounds.valid) continue;
            children.append(child);
            boxes.append(bounds);
        }
        if (children.isEmpty()) continue;
        const auto diagonal = (parentBounds.maximum - parentBounds.minimum).length();
        // A gap a little over the overlap itself, so even parts that only touch come apart.
        const auto gap = std::max(1e-3F, diagonal * 0.04F);
        const auto touching = diagonal * 1e-6F;
        QVector<QVector3D> pushes(children.size());
        for (qsizetype first = 0; first < children.size(); ++first) {
            for (qsizetype second = first + 1; second < children.size(); ++second) {
                const auto& a = boxes.at(first);
                const auto& b = boxes.at(second);
                float overlaps[3];
                bool overlapping = true;
                for (int axis = 0; axis < 3; ++axis) {
                    overlaps[axis] = std::min(a.maximum[axis], b.maximum[axis]) - std::max(a.minimum[axis], b.minimum[axis]);
                    if (overlaps[axis] < -touching) overlapping = false;
                }
                if (!overlapping) continue;
                int axis = 0;
                for (int candidate = 1; candidate < 3; ++candidate) {
                    if (overlaps[candidate] < overlaps[axis]) axis = candidate;
                }
                const auto apart = b.centre()[axis] - a.centre()[axis];
                // Centres level along that axis: the tree's order decides which goes first.
                const auto sign = std::abs(apart) > touching ? (apart > 0 ? 1.0F : -1.0F) : 1.0F;
                QVector3D push;
                push[axis] = sign * (std::max(0.0F, overlaps[axis]) + gap) * 0.5F;
                pushes[first] -= push;
                pushes[second] += push;
            }
        }
        for (qsizetype index = 0; index < children.size(); ++index) {
            const auto vector = boxes.at(index).centre() - parentBounds.centre() + pushes.at(index);
            if (vector.lengthSquared() > 1e-12F) vectors.insert(children.at(index), vector);
        }
    }
    return vectors;
}

void ExplodeController::previewAutoExplode(const double factor)
{
    if (interaction_ != Interaction::AutoExplode) return;
    const auto target = std::max(0.0, factor);
    const auto delta = static_cast<float>(target - autoStartFactor_);
    state_.transforms = interactionStart_.transforms;
    effectiveCache_.clear();
    for (auto it = autoVectors_.cbegin(); it != autoVectors_.cend(); ++it) {
        // In the parent's frame, so a turned sub-assembly still comes apart along its own axes.
        setOwnTransform(it.key(), translation(it.value() * delta) * state_.transforms.value(it.key()));
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
    showLayoutForEditing(EditFrom::Assembly);
    if (!hasNode(nodeId)) return;
    if (interacting()) commitInteraction();
    const auto before = state_;
    const auto centre = assembledCentre(nodeId);
    setPlacement(nodeId, aboutPoint(centre + offset, rotationOf(effectiveTransform(nodeId)), centre));
    applyEdit(before, {nodeId});
}

void ExplodeController::setAngles(const QString& nodeId, const QVector3D& degrees)
{
    showLayoutForEditing(EditFrom::Assembly);
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
    showLayoutForEditing();
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
    if (!before.sameLayout(state_)) pushUndo(before);
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

void ExplodeController::showLayoutForEditing(const EditFrom from)
{
    if (from == EditFrom::Assembly && showingAssembly()) {
        // What is on screen is the assembly, so that is what the edit starts from. Bringing
        // the kept layout back first moved every part away from under the pointer.
        const auto kept = snapshotOf(state_);
        const auto summary = layoutSummary();
        const auto before = state_;
        state_ = {};
        blend_ = 1.0;
        announceStateChange(before);
        pushUndo(before);
        emit layoutSetAside(kept, summary);
        return;
    }
    // An edit to the layout: a view eased back to the assembly comes out again first.
    if (blend_ < 1.0) setBlend(1.0);
}

QVariantMap ExplodeController::snapshotOf(const State& state)
{
    QVariantMap transforms;
    for (auto it = state.transforms.cbegin(); it != state.transforms.cend(); ++it) {
        QVariantList values;
        values.reserve(16);
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) values.append(static_cast<double>((*it)(row, column)));
        }
        transforms.insert(it.key(), values);
    }
    QVariantMap factors;
    for (auto it = state.autoFactors.cbegin(); it != state.autoFactors.cend(); ++it) factors.insert(it.key(), *it);
    QVariantMap anchors;
    for (auto it = state.anchors.cbegin(); it != state.anchors.cend(); ++it) {
        anchors.insert(it.key(), QVariantList{it->point.x(), it->point.y(), it->point.z(),
                                              it->direction.x(), it->direction.y(), it->direction.z()});
    }
    QStringList hidden(state.hiddenLines.cbegin(), state.hiddenLines.cend());
    hidden.sort();
    return {{QStringLiteral("transforms"), transforms}, {QStringLiteral("autoFactors"), factors},
            {QStringLiteral("anchors"), anchors}, {QStringLiteral("hiddenLines"), hidden}};
}

QVariantMap ExplodeController::layoutSnapshot() const
{
    return snapshotOf(state_);
}

bool ExplodeController::restoreLayout(const QVariantMap& snapshot)
{
    if (interacting()) cancelInteraction();
    State restored;
    const auto transforms = snapshot.value(QStringLiteral("transforms")).toMap();
    for (auto it = transforms.cbegin(); it != transforms.cend(); ++it) {
        if (!hasNode(it.key())) continue;
        const auto values = it.value().toList();
        if (values.size() != 16) continue;
        float entries[16];
        bool valid = true;
        for (int index = 0; index < 16; ++index) {
            bool ok = false;
            const auto value = values.at(index).toDouble(&ok);
            valid = valid && ok && std::isfinite(value);
            entries[index] = static_cast<float>(value);
        }
        if (!valid) continue;
        const QMatrix4x4 matrix(entries);
        if (!isIdentity(matrix)) restored.transforms.insert(it.key(), matrix);
    }
    const auto factors = snapshot.value(QStringLiteral("autoFactors")).toMap();
    for (auto it = factors.cbegin(); it != factors.cend(); ++it) {
        if (!it.key().isEmpty() && !hasNode(it.key())) continue;
        const auto factor = it.value().toDouble();
        if (std::isfinite(factor) && factor > 0.0) restored.autoFactors.insert(it.key(), factor);
    }
    const auto anchors = snapshot.value(QStringLiteral("anchors")).toMap();
    for (auto it = anchors.cbegin(); it != anchors.cend(); ++it) {
        if (!hasNode(it.key())) continue;
        const auto values = it.value().toList();
        if (values.size() != 6) continue;
        float entries[6];
        bool valid = true;
        for (int index = 0; index < 6; ++index) {
            bool ok = false;
            const auto value = values.at(index).toDouble(&ok);
            valid = valid && ok && std::isfinite(value);
            entries[index] = static_cast<float>(value);
        }
        if (valid) restored.anchors.insert(it.key(), {{entries[0], entries[1], entries[2]}, QVector3D{entries[3], entries[4], entries[5]}.normalized()});
    }
    for (const auto& node : snapshot.value(QStringLiteral("hiddenLines")).toStringList()) {
        if (hasNode(node)) restored.hiddenLines.insert(node);
    }
    if (restored.transforms.isEmpty()) return false;
    const auto before = state_;
    state_ = restored;
    blend_ = 0.0;
    announceStateChange(before);
    if (!before.sameLayout(state_)) pushUndo(before);
    return true;
}

bool ExplodeController::layoutMatches(const QVariantMap& snapshot) const
{
    const auto transforms = snapshot.value(QStringLiteral("transforms")).toMap();
    if (transforms.size() != state_.transforms.size()) return false;
    for (auto it = state_.transforms.cbegin(); it != state_.transforms.cend(); ++it) {
        const auto values = transforms.value(it.key()).toList();
        if (values.size() != 16) return false;
        for (int index = 0; index < 16; ++index) {
            const auto tolerance = index % 4 == 3 ? 1e-3 : 1e-5;
            if (std::abs(values.at(index).toDouble() - static_cast<double>((*it)(index / 4, index % 4))) > tolerance) return false;
        }
    }
    return true;
}

QString ExplodeController::layoutSummary() const
{
    int moved = 0;
    int turned = 0;
    for (const auto& part : partsUnder({})) {
        const auto transform = effectiveTransform(part);
        if (isIdentity(transform)) continue;
        ++moved;
        if (std::abs(rotationOf(transform).scalar()) < 0.99999F) ++turned;
    }
    if (moved == 0) return tr("Nothing moved");
    const auto parts = moved == 1 ? tr("1 part moved") : tr("%1 parts moved").arg(moved);
    if (turned == 0) return parts;
    return QStringLiteral("%1 · %2").arg(parts, turned == 1 ? tr("1 turned") : tr("%1 turned").arg(turned));
}

QStringList ExplodeController::lineNodes() const
{
    QStringList nodes;
    for (auto it = state_.transforms.cbegin(); it != state_.transforms.cend(); ++it) nodes.append(it.key());
    nodes.sort();
    return nodes;
}

QString ExplodeController::lineOwnerOf(const QString& nodeId) const
{
    for (auto node = nodeId; !node.isEmpty(); node = parentOf(node)) {
        if (state_.transforms.contains(node)) return node;
        if (isAncestor(node, parentOf(node))) break;
    }
    return nodeId;
}

QVector3D ExplodeController::assembledPoint(const QString& nodeId, const QVector3D& scenePoint) const
{
    return displayedTransform(nodeId).inverted().map(scenePoint);
}

QVector3D ExplodeController::assembledDirection(const QString& nodeId, const QVector3D& sceneDirection) const
{
    return displayedTransform(nodeId).inverted().mapVector(sceneDirection).normalized();
}

QVariantList ExplodeController::explodeLines() const
{
    QVariantList lines;
    if (!linesShown_ || state_.transforms.isEmpty()) return lines;
    const auto centerlineStyle = lineStyle_ == QStringLiteral("centerline");
    for (const auto& nodeId : lineNodes()) {
        if (state_.hiddenLines.contains(nodeId) || !hasNode(nodeId)) continue;
        const auto bounds = nodeBounds(nodeId);
        if (!bounds.valid) continue;
        // Where it seats: inside its parent as the parent is shown now.
        const auto seatFrame = displayedTransform(parentOf(nodeId));
        const auto shown = displayedTransform(nodeId);
        const auto centre = bounds.centre();
        Anchor anchor{centre, {}};
        if (const auto picked = state_.anchors.constFind(nodeId); picked != state_.anchors.cend()) anchor = *picked;
        else if (const auto seat = seatAxes_.constFind(nodeId); seat != seatAxes_.cend()) anchor = *seat;
        const auto start = seatFrame.map(anchor.point);
        const auto end = shown.map(anchor.point);
        const auto size = (bounds.maximum - bounds.minimum).length();
        const auto tiny = std::max(1e-4F, size * 1e-5F);
        if ((end - start).length() <= tiny) continue;

        QVariantList segments;
        const auto segment = [&segments](const QVector3D& from, const QVector3D& to, const bool centerline) {
            segments.append(QVariantMap{{QStringLiteral("from"), QVariant::fromValue(from)},
                                        {QStringLiteral("to"), QVariant::fromValue(to)},
                                        {QStringLiteral("centerline"), centerline}});
        };
        float length = 0.0F;
        if (!anchor.direction.isNull()) {
            // Along the axis as it seats, as far as the move goes along it, a little past
            // either end so it reads as a centerline; then across to the part if it left it.
            const auto axis = seatFrame.mapVector(anchor.direction).normalized();
            const auto along = QVector3D::dotProduct(end - start, axis);
            const auto onAxis = start + axis * along;
            const auto sign = along < 0.0F ? -1.0F : 1.0F;
            const auto overshoot = std::max(std::abs(along) * 0.06F, std::max(2.0F, size * 0.02F));
            if (std::abs(along) > tiny) segment(start - axis * (sign * overshoot), onAxis + axis * (sign * overshoot), true);
            if ((end - onAxis).length() > tiny) segment(onAxis, end, centerlineStyle);
            length = std::abs(along) + (end - onAxis).length();
        } else {
            // A turned part's anchor does not travel with its centre: along the travel, then a
            // short jog to the seat, rather than a diagonal through whatever is between.
            const auto travel = shown.map(centre) - seatFrame.map(centre);
            const auto corner = start + travel;
            if ((end - corner).length() > tiny && travel.length() > tiny) {
                segment(start, corner, centerlineStyle);
                segment(corner, end, centerlineStyle);
                length = travel.length() + (end - corner).length();
            } else {
                segment(start, end, centerlineStyle);
                length = (end - start).length();
            }
        }
        if (segments.isEmpty()) continue;
        lines.append(QVariantMap{{QStringLiteral("nodeId"), nodeId}, {QStringLiteral("segments"), segments},
                                 {QStringLiteral("length"), static_cast<double>(length)}});
    }
    return lines;
}

void ExplodeController::setLineAnchor(const QString& nodeId, const QVector3D& point, const QVector3D& direction)
{
    if (!hasNode(nodeId) || !std::isfinite(point.x()) || !std::isfinite(point.y()) || !std::isfinite(point.z())) return;
    const auto before = state_;
    state_.anchors.insert(nodeId, {point, direction.isNull() ? QVector3D{} : direction.normalized()});
    if (state_.sameLayout(before)) return;
    pushUndo(before);
    announce({});
}

void ExplodeController::clearLineAnchor(const QString& nodeId)
{
    if (!state_.anchors.contains(nodeId)) return;
    const auto before = state_;
    state_.anchors.remove(nodeId);
    pushUndo(before);
    announce({});
}

bool ExplodeController::hasLineAnchor(const QString& nodeId) const
{
    return state_.anchors.contains(nodeId);
}

void ExplodeController::setLineHidden(const QString& nodeId, const bool hidden)
{
    if (!hasNode(nodeId) || state_.hiddenLines.contains(nodeId) == hidden) return;
    const auto before = state_;
    if (hidden) state_.hiddenLines.insert(nodeId);
    else state_.hiddenLines.remove(nodeId);
    pushUndo(before);
    announce({});
}

bool ExplodeController::lineHidden(const QString& nodeId) const
{
    return state_.hiddenLines.contains(nodeId);
}

void ExplodeController::setSeatAxes(const QVariantMap& axes)
{
    QHash<QString, Anchor> next;
    for (auto it = axes.cbegin(); it != axes.cend(); ++it) {
        const auto entry = it.value().toMap();
        const auto point = entry.value(QStringLiteral("point")).value<QVector3D>();
        const auto direction = entry.value(QStringLiteral("direction")).value<QVector3D>();
        if (direction.isNull() || !std::isfinite(point.x()) || !std::isfinite(direction.x())) continue;
        next.insert(it.key(), {point, direction.normalized()});
    }
    if (next == seatAxes_) return;
    seatAxes_ = next;
    announce({});
}

void ExplodeController::setLinesShown(const bool shown)
{
    if (linesShown_ == shown) return;
    linesShown_ = shown;
    emit settingsChanged();
    announce({});
}

void ExplodeController::setLineStyle(const QString& style)
{
    const auto normalized = style == QStringLiteral("centerline") ? style : QStringLiteral("dashed");
    if (lineStyle_ == normalized) return;
    lineStyle_ = normalized;
    emit settingsChanged();
    announce({});
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
