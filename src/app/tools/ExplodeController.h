#pragma once

#include <QHash>
#include <QMatrix4x4>
#include <QObject>
#include <QQuaternion>
#include <QStringList>
#include <QVariantMap>
#include <QVector>
#include <QVector3D>
#include <QVector4D>

namespace loupe::app::tools {

// The exploded view (#17): where each node of the assembly has been moved to, in the Inspect
// view only. Every node keeps its own rigid transform, applied inside its parent's, so moving
// a sub-assembly carries everything in it and a part moved inside it stays put relative to
// it. A part is drawn with the product of the transforms down its chain. Geometry, exports
// and measurements are untouched: this is a presentation of the assembly, kept for the
// session until reset.
//
// Hierarchy and part bounds come from the controller that owns the document; the viewport
// asks for each part's displayed placement when partsMoved names it.
class ExplodeController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool exploded READ exploded NOTIFY changed)
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    // 1 shows the parts where they have been moved, 0 where the file has them, with the
    // layout kept: the panel's Assembled / Exploded switch eases between the two, and a reset
    // eases home before it forgets the layout. Any edit shows the layout again.
    Q_PROPERTY(double blend READ blend WRITE setBlend NOTIFY changed)
    Q_PROPERTY(bool interacting READ interacting NOTIFY interactionChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(bool snapEnabled READ snapEnabled WRITE setSnapEnabled NOTIFY settingsChanged)
    Q_PROPERTY(double translationStep READ translationStep WRITE setTranslationStep NOTIFY settingsChanged)
    Q_PROPERTY(double rotationStep READ rotationStep WRITE setRotationStep NOTIFY settingsChanged)

public:
    explicit ExplodeController(QObject* parent = nullptr);

    // Each node's parent in the assembly tree, as the tree shows it. Replacing it starts
    // over: every node goes back to where the file puts it.
    void setHierarchy(const QHash<QString, QString>& parentByNode);
    // A drawn part's assembled bounds, in mm. Called for each mesh segment; they add up.
    void includePartBounds(const QString& nodeId, const QVector3D& minimum, const QVector3D& maximum);
    void clear();

    // The node's full placement: its own transform inside every ancestor's.
    [[nodiscard]] QMatrix4x4 effectiveTransform(const QString& nodeId) const;
    // What the viewport sets on a part's node: x' = rotation * x + position, eased by blend.
    Q_INVOKABLE [[nodiscard]] QVector3D displayedPosition(const QString& nodeId) const;
    Q_INVOKABLE [[nodiscard]] QQuaternion displayedRotation(const QString& nodeId) const;
    Q_INVOKABLE [[nodiscard]] bool isMoved(const QString& nodeId) const;
    // Whether resetting the node would move anything: it, or something in it, was moved
    // itself (a part that only goes along with its moved sub-assembly is not).
    Q_INVOKABLE [[nodiscard]] bool canReset(const QString& nodeId) const;
    Q_INVOKABLE [[nodiscard]] bool hasNode(const QString& nodeId) const;
    Q_INVOKABLE [[nodiscard]] QString parentOf(const QString& nodeId) const;
    // The centre of the node's parts as assembled.
    Q_INVOKABLE [[nodiscard]] QVector3D assembledCentre(const QString& nodeId) const;
    // Where these nodes' own centres are now: each node's assembled centre carried by its
    // move, averaged. A point fixed to the parts, so turning them about it leaves it where it
    // is -- unlike the centre of their bounding box, which shifts as they turn.
    Q_INVOKABLE [[nodiscard]] QVector3D movedCentre(const QStringList& nodeIds) const;
    // {valid, minimum, maximum} of these nodes' parts as displayed; every part for none.
    Q_INVOKABLE [[nodiscard]] QVariantMap displayedBounds(const QStringList& nodeIds) const;
    // How far the node's centre has moved, and its rotation as Euler angles (degrees).
    Q_INVOKABLE [[nodiscard]] QVector3D offsetOf(const QString& nodeId) const;
    Q_INVOKABLE [[nodiscard]] QVector3D anglesOf(const QString& nodeId) const;
    // A world plane (normal . x = offset) in the part's own assembled coordinates, as
    // (normal, offset): what its section cap and edge clip are built against.
    Q_INVOKABLE [[nodiscard]] QVector4D localPlane(const QString& nodeId, const QVector3D& normal, double offset) const;
    // How far the scope (a sub-assembly, or "" for the whole model) is auto-exploded.
    Q_INVOKABLE [[nodiscard]] double autoFactor(const QString& scopeId) const;
    // What auto-explode takes apart for this selection: one sub-assembly selected, that;
    // one part, the sub-assembly it is in; otherwise ("") the whole model.
    Q_INVOKABLE [[nodiscard]] QString autoExplodeScope(const QStringList& selectedNodeIds) const;
    // A handle frame whose Z axis is `normal` (a picked face's), with X and Y chosen the same
    // way every time: X level where it can be, so the handle reads like the world's.
    Q_INVOKABLE [[nodiscard]] static QQuaternion frameForNormal(const QVector3D& normal);
    Q_INVOKABLE [[nodiscard]] double snapTranslation(double millimetres, bool free) const;
    Q_INVOKABLE [[nodiscard]] double snapRotation(double degrees, bool free) const;

    // A handle drag: begin with the nodes it moves, preview as often as the pointer moves
    // (each preview is from where the drag began, in world space), then commit or cancel.
    Q_INVOKABLE void beginInteraction(const QStringList& nodeIds);
    Q_INVOKABLE void previewTranslation(const QVector3D& worldDelta);
    Q_INVOKABLE void previewRotation(const QVector3D& axis, double degrees, const QVector3D& pivot);
    Q_INVOKABLE void commitInteraction();
    Q_INVOKABLE void cancelInteraction();

    // Auto-explode: every node under the scope moves away from its parent's centre by the
    // factor times its distance from it, level by level. The slider drives begin, preview,
    // commit; autoExplode does all three.
    Q_INVOKABLE void beginAutoExplode(const QString& scopeId);
    Q_INVOKABLE void previewAutoExplode(double factor);
    Q_INVOKABLE void autoExplode(const QString& scopeId, double factor);

    // Typed values: place the node's centre at this offset from where it is assembled, or
    // turn it to these angles about its centre.
    // Move these nodes by an exact distance along the world axes, as one step to undo.
    Q_INVOKABLE void translateBy(const QStringList& nodeIds, const QVector3D& delta);
    Q_INVOKABLE void setOffset(const QString& nodeId, const QVector3D& offset);
    Q_INVOKABLE void setAngles(const QString& nodeId, const QVector3D& degrees);
    Q_INVOKABLE void resetNode(const QString& nodeId);
    Q_INVOKABLE void resetAll();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    // Every node's own transform, for an export of the exploded layout.
    [[nodiscard]] const QHash<QString, QMatrix4x4>& ownTransforms() const noexcept { return state_.transforms; }
    [[nodiscard]] bool exploded() const noexcept { return !state_.transforms.isEmpty(); }
    [[nodiscard]] int revision() const noexcept { return revision_; }
    [[nodiscard]] double blend() const noexcept { return blend_; }
    void setBlend(double blend);
    [[nodiscard]] bool interacting() const noexcept { return interaction_ != Interaction::None; }
    [[nodiscard]] bool canUndo() const noexcept { return !undo_.isEmpty(); }
    [[nodiscard]] bool canRedo() const noexcept { return !redo_.isEmpty(); }
    [[nodiscard]] bool snapEnabled() const noexcept { return snapEnabled_; }
    void setSnapEnabled(bool enabled);
    [[nodiscard]] double translationStep() const noexcept { return translationStep_; }
    void setTranslationStep(double step);
    [[nodiscard]] double rotationStep() const noexcept { return rotationStep_; }
    void setRotationStep(double step);

signals:
    void changed();
    // The parts whose displayed placement may have changed.
    void partsMoved(const QStringList& partIds);
    void interactionChanged();
    void historyChanged();
    void settingsChanged();

private:
    struct Bounds {
        QVector3D minimum;
        QVector3D maximum;
        bool valid{false};
        void include(const Bounds& other);
        [[nodiscard]] QVector3D centre() const { return (minimum + maximum) * 0.5F; }
    };
    struct State {
        QHash<QString, QMatrix4x4> transforms;
        QHash<QString, double> autoFactors;
    };
    enum class Interaction { None, Handle, AutoExplode };

    [[nodiscard]] Bounds nodeBounds(const QString& nodeId) const;
    [[nodiscard]] QHash<QString, QVector3D> autoExplodeVectors(const QString& scopeId) const;
    [[nodiscard]] QStringList partsUnder(const QString& nodeId) const;
    [[nodiscard]] QStringList descendantsOf(const QString& nodeId) const;
    [[nodiscard]] bool isAncestor(const QString& ancestorId, const QString& nodeId) const;
    [[nodiscard]] QMatrix4x4 parentTransform(const QString& nodeId) const;
    [[nodiscard]] QMatrix4x4 displayedTransform(const QString& partId) const;
    // Place the node so its full placement is `placement`, given its parent's.
    void setPlacement(const QString& nodeId, const QMatrix4x4& placement);
    void setOwnTransform(const QString& nodeId, const QMatrix4x4& transform);
    void invalidate();
    void showLayoutForEditing();
    void announce(const QStringList& partIds);
    void announceStateChange(const State& before);
    void pushUndo(const State& before);
    void applyEdit(const State& before, const QStringList& changedNodes);

    QHash<QString, QString> parentByNode_;
    QHash<QString, QStringList> childrenByNode_;
    QHash<QString, Bounds> partBounds_;
    State state_;
    QVector<State> undo_;
    QVector<State> redo_;
    double blend_{1.0};
    int revision_{0};
    bool snapEnabled_{true};
    double translationStep_{5.0};
    double rotationStep_{15.0};

    Interaction interaction_{Interaction::None};
    State interactionStart_;
    QStringList interactionNodes_;
    QHash<QString, QMatrix4x4> interactionPlacements_;
    QString autoScope_;
    double autoStartFactor_{};
    QHash<QString, QVector3D> autoVectors_;

    mutable QHash<QString, QMatrix4x4> effectiveCache_;
    mutable QHash<QString, Bounds> boundsCache_;
};

} // namespace loupe::app::tools
