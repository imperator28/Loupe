#pragma once

#include <QHash>
#include <QMatrix4x4>
#include <QObject>
#include <QQuaternion>
#include <QSet>
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
    // Explode lines (#41): a thin line from where each moved part seats to where it is now.
    Q_PROPERTY(bool linesShown READ linesShown WRITE setLinesShown NOTIFY settingsChanged)
    // "dashed", or "centerline" (dash-dot) for every line; a line along an axis is always a
    // centerline.
    Q_PROPERTY(QString lineStyle READ lineStyle WRITE setLineStyle NOTIFY settingsChanged)

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
    // The box around these nodes' parts where the file puts them (every part for none); false
    // when none of them has been drawn yet.
    [[nodiscard]] bool assembledBox(const QStringList& nodeIds, QVector3D* minimum, QVector3D* maximum) const;
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

    // Saved layouts: the layout as data to keep ({transforms: {node: 16 numbers, row-major},
    // autoFactors: {scope: factor}}), and back. Restoring is one step to undo, and leaves the
    // view on the assembly (blend 0) so the viewport can ease the parts out to it. Nodes the
    // document does not have are skipped; false when nothing in the snapshot applies.
    Q_INVOKABLE [[nodiscard]] QVariantMap layoutSnapshot() const;
    Q_INVOKABLE bool restoreLayout(const QVariantMap& snapshot);
    // Whether the layout is the one in `snapshot` (to a hair).
    Q_INVOKABLE [[nodiscard]] bool layoutMatches(const QVariantMap& snapshot) const;
    // "3 parts moved · 2 turned": what a layout holds, in a few words.
    Q_INVOKABLE [[nodiscard]] QString layoutSummary() const;
    // Whether the view is on the assembly with a layout kept aside (Assembled, not reset).
    [[nodiscard]] bool showingAssembly() const noexcept { return exploded() && blend_ < 0.5; }
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    // Explode lines. Each node moved in its own right (not only carried by a moved
    // sub-assembly) gets one, from its anchor where it seats -- inside its parent as the
    // parent is now -- to where the anchor is carried by its move, as displayed (eased with
    // blend, so a line is zero long on the assembly and gone):
    //   {nodeId, segments: [{from, to, centerline}], length}
    // The anchor is the node's centre unless one was picked (setLineAnchor), or found where
    // it seats along a shared axis (setSeatAxes). A turned part's line runs along its travel
    // and jogs to the seat; one with an axis runs along the axis, a little past each end, and
    // jogs to the part if it moved off it. Hidden lines, and all when linesShown is off, are
    // left out.
    Q_INVOKABLE [[nodiscard]] QVariantList explodeLines() const;
    // A picked anchor, in the node's assembled coordinates (what moves with it): a point, and
    // an axis through it when `direction` is not zero. Saved with the layout; one step to undo.
    Q_INVOKABLE void setLineAnchor(const QString& nodeId, const QVector3D& point, const QVector3D& direction);
    Q_INVOKABLE void clearLineAnchor(const QString& nodeId);
    Q_INVOKABLE [[nodiscard]] bool hasLineAnchor(const QString& nodeId) const;
    Q_INVOKABLE void setLineHidden(const QString& nodeId, bool hidden);
    Q_INVOKABLE [[nodiscard]] bool lineHidden(const QString& nodeId) const;
    // Axes found where parts seat (a round feature of a moved part coaxial with one of a part
    // it did not move with): {nodeId: {point, direction}} in assembled coordinates. Worked out
    // from the geometry by the viewport; not part of the layout.
    Q_INVOKABLE void setSeatAxes(const QVariantMap& axes);
    // The nodes that get a line (moved in their own right), whether shown or not.
    Q_INVOKABLE [[nodiscard]] QStringList lineNodes() const;
    // The node whose line a part's is: the nearest of it and its ancestors moved in its own
    // right, else the node itself (its line appears once it moves).
    Q_INVOKABLE [[nodiscard]] QString lineOwnerOf(const QString& nodeId) const;
    // A point or direction where the node is displayed, in its assembled coordinates (what
    // anchors are kept in, so they move with it).
    Q_INVOKABLE [[nodiscard]] QVector3D assembledPoint(const QString& nodeId, const QVector3D& scenePoint) const;
    Q_INVOKABLE [[nodiscard]] QVector3D assembledDirection(const QString& nodeId, const QVector3D& sceneDirection) const;
    // The parts a node carries: itself for a part, everything drawn under a sub-assembly.
    Q_INVOKABLE [[nodiscard]] QStringList partsUnderNode(const QString& nodeId) const { return partsUnder(nodeId); }

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
    [[nodiscard]] bool linesShown() const noexcept { return linesShown_; }
    void setLinesShown(bool shown);
    [[nodiscard]] QString lineStyle() const { return lineStyle_; }
    void setLineStyle(const QString& style);

signals:
    void changed();
    // The parts whose displayed placement may have changed.
    void partsMoved(const QStringList& partIds);
    void interactionChanged();
    void historyChanged();
    void settingsChanged();
    // An edit was begun on the assembly while a layout was kept aside: the edit starts from
    // where the file puts the parts, and the kept layout is handed over to be saved, so
    // nothing is lost. Undo also brings it back.
    void layoutSetAside(const QVariantMap& snapshot, const QString& summary);

private:
    struct Bounds {
        QVector3D minimum;
        QVector3D maximum;
        bool valid{false};
        void include(const Bounds& other);
        [[nodiscard]] QVector3D centre() const { return (minimum + maximum) * 0.5F; }
    };
    struct Anchor {
        QVector3D point;
        QVector3D direction; // zero: a point, not an axis
        bool operator==(const Anchor&) const = default;
    };
    struct State {
        QHash<QString, QMatrix4x4> transforms;
        QHash<QString, double> autoFactors;
        QHash<QString, Anchor> anchors;
        QSet<QString> hiddenLines;
        [[nodiscard]] bool sameLayout(const State& other) const
        {
            return transforms == other.transforms && autoFactors == other.autoFactors && anchors == other.anchors
                && hiddenLines == other.hiddenLines;
        }
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
    enum class EditFrom { Layout, Assembly };
    // An edit is made to what is on screen. On the assembly with a layout kept aside, an edit
    // from the assembly (a drag, a spread, a typed move) starts a new layout and sets the
    // kept one aside; anything else brings the kept layout back first.
    void showLayoutForEditing(EditFrom from = EditFrom::Layout);
    [[nodiscard]] static QVariantMap snapshotOf(const State& state);
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
    bool linesShown_{true};
    QString lineStyle_{QStringLiteral("dashed")};
    QHash<QString, Anchor> seatAxes_;

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
