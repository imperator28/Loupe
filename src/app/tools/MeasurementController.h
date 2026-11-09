#pragma once

#include "app/tools/MeasureGeometry.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QVector3D>

namespace loupe::app::tools {

enum class MeasurementMode {
    PointToPoint,
    EdgeLength,
    SurfaceToSurface,
    RadiusDiameter,
    Angle,
    SurfaceArea,
    Volume,
    Bounds,
};

struct MeasurementResult final {
    double value{};
    QString unitLabel;
};

// The Measure tool: which mode is on, what has been picked for it, and what that measures.
// A pick is a MeasureEntity -- a point, a vertex, a circle's centre, a whole edge or a whole
// face, with its shape -- so each mode measures what it names: surface to surface is the gap
// square to two parallel flat faces (or the true minimum between any two), an angle is
// between flat faces or straight edges, never between two arbitrary click normals.
class MeasurementController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(MeasurementMode mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(QString modeKey READ modeKey NOTIFY modeChanged)
    Q_PROPERTY(QString pickInstruction READ pickInstruction NOTIFY resultChanged)
    Q_PROPERTY(int requiredPickCount READ requiredPickCount NOTIFY modeChanged)
    Q_PROPERTY(double resultValue READ resultValue NOTIFY resultChanged)
    Q_PROPERTY(QString resultUnit READ resultUnit NOTIFY resultChanged)
    Q_PROPERTY(QString resultLabel READ resultLabel NOTIFY resultChanged)
    // The result as the panel and the viewport show it: what it is ("Distance"), its value at
    // the display precision, the related values ({label, value} each), and how it was found.
    Q_PROPERTY(QString resultTitle READ resultTitle NOTIFY resultChanged)
    Q_PROPERTY(QString resultText READ resultText NOTIFY resultChanged)
    Q_PROPERTY(QVariantList resultDetails READ resultDetails NOTIFY resultChanged)
    Q_PROPERTY(QString resultMethod READ resultMethod NOTIFY resultChanged)
    // What to draw in the viewport: {kind: "distance" | "radius" | "label" | "", from, to,
    // at, text}, in scene coordinates.
    Q_PROPERTY(QVariantMap annotation READ annotation NOTIFY resultChanged)
    // Why the last pick was not taken, until the next pick or mode change.
    Q_PROPERTY(QString refusal READ refusal NOTIFY resultChanged)
    Q_PROPERTY(QString firstPickDescription READ firstPickDescription NOTIFY resultChanged)
    Q_PROPERTY(QString secondPickDescription READ secondPickDescription NOTIFY resultChanged)
    Q_PROPERTY(QVariantList pickedPoints READ pickedPoints NOTIFY resultChanged)
    Q_PROPERTY(QVariantList pickedEntities READ pickedEntities NOTIFY resultChanged)
    // Edge length and area add up several picks: Shift or Ctrl/Cmd-click adds one (again to
    // take it back out), a plain click starts over.
    Q_PROPERTY(bool multiPick READ multiPick NOTIFY modeChanged)
    // What volume and bounds are of: "Whole model", a part's or sub-assembly's name, or
    // "3 parts", with how many parts that is.
    Q_PROPERTY(QString selectionScope READ selectionScope NOTIFY resultChanged)
    // Results pinned to the view: each {id, title, text, annotation, points, edges, color,
    // offset}, drawn where it was measured until unpinned, whatever is measured next. `color`
    // is a key of the theme's measurement colours (new pins take them in turn); `offset` is
    // where its tag has been dragged to, in pixels from where it sits by default.
    Q_PROPERTY(QVariantList pinnedResults READ pinnedResults NOTIFY pinnedChanged)
    Q_PROPERTY(bool canPin READ canPin NOTIFY resultChanged)

public:
    explicit MeasurementController(QObject* parent = nullptr);

    void setEffectiveUnit(const QString& unit);
    void setMode(MeasurementMode mode);
    void setSelectedGeometry(double surfaceAreaMm2, double volumeMm3, const QVector3D& boundsMm);
    void setSelectedTopology(double longestEdgeMm, double circularRadiusMm, int planarFaceCount);
    void clearSelectedGeometry();
    void recordPoint(const QVector3D& pointMm);
    void recordPick(const QVector3D& pointMm, const QVector3D& normal);
    void recordPick(const QVector3D& pointMm, const QVector3D& normal, const QString& nodeLabel, const QString& entityLabel);
    bool recordTopologyPick(const QString& entityKind, quint32 topologyId, const QVector3D& pointMm,
                            const QVector3D& normal, double measureMm, double radiusMm,
                            const QString& nodeLabel);
    // A pick the viewport built from the part's geometry (a MeasureEntity in a QVariant).
    // False, with `refusal` saying why, when the mode cannot use it.
    Q_INVOKABLE bool acceptEntity(const QVariant& entity, const QString& nodeLabel, bool additive = false);
    bool acceptEntity(const MeasureEntity& entity, bool additive = false);
    // The box volume and bounds measure, in scene millimetres, and what it is of.
    void setSelectionBox(const QVector3D& minimum, const QVector3D& maximum, const QString& scope, int partCount);
    // Takes one pick out of a total (edge length, area).
    Q_INVOKABLE void removePick(int index);
    Q_INVOKABLE void pinResult();
    Q_INVOKABLE void unpin(const QString& id);
    Q_INVOKABLE void setPinColor(const QString& id, const QString& colorKey);
    Q_INVOKABLE void setPinOffset(const QString& id, double x, double y);
    Q_INVOKABLE void clearPinned();
    [[nodiscard]] bool multiPick() const noexcept;
    [[nodiscard]] QString selectionScope() const { return hasSelectedGeometry_ ? selectionScope_ : QString{}; }
    [[nodiscard]] QVariantList pinnedResults() const { return pinned_; }
    [[nodiscard]] bool canPin() const noexcept { return result_.valid && !result_.annotation.isEmpty(); }
    // What a pick under the pointer would be, before it is made: {title, detail, eligible,
    // reason}. The hover chip shows it.
    // Also {kind: "surfacePoint" | "edgePoint" | "vertex" | "midpoint" | "centre" | "edge" |
    // "face", topologyId, point} for the viewport to draw the hover with, and its shape:
    // {normal, planar, planeNormal, planePoint, linear, direction, circular, centre, axis}.
    Q_INVOKABLE [[nodiscard]] QVariantMap describe(const QVariant& entity) const;
    // A point on a face where the pointer is, for point to point away from any edge.
    Q_INVOKABLE [[nodiscard]] static QVariant surfacePointEntity(const QVector3D& point, const QVector3D& normal);
    // The centerline of a round face or circular edge (an entity from the viewport's geometry)
    // as a pick of its own, its point the spot on it nearest `near`; invalid when it has none.
    Q_INVOKABLE [[nodiscard]] static QVariant axisEntity(const QVariant& source, const QVector3D& near);
    [[nodiscard]] QVariantMap describe(const MeasureEntity& entity) const;
    Q_INVOKABLE void clearPicks();
    Q_INVOKABLE void setModeName(const QString& modeName);
    // The result and both picks as plain text, onto the clipboard.
    Q_INVOKABLE void copyResult() const;
    [[nodiscard]] QString resultSummary() const;
    [[nodiscard]] MeasurementMode mode() const noexcept { return mode_; }
    [[nodiscard]] QString modeKey() const;
    [[nodiscard]] int requiredPickCount() const noexcept;
    [[nodiscard]] QString pickInstruction() const;
    [[nodiscard]] MeasurementResult pointDistance(const QVector3D& first, const QVector3D& second) const;
    [[nodiscard]] double resultValue() const;
    [[nodiscard]] QString resultUnit() const;
    [[nodiscard]] QString resultLabel() const;
    [[nodiscard]] QString resultTitle() const;
    [[nodiscard]] QString resultText() const;
    [[nodiscard]] QVariantList resultDetails() const { return result_.details; }
    [[nodiscard]] QString resultMethod() const { return result_.method; }
    [[nodiscard]] QVariantMap annotation() const { return result_.annotation; }
    [[nodiscard]] QString refusal() const { return refusal_; }
    [[nodiscard]] QString firstPickDescription() const;
    [[nodiscard]] QString secondPickDescription() const;
    [[nodiscard]] QVariantList pickedPoints() const;
    [[nodiscard]] QVariantList pickedEntities() const;
    [[nodiscard]] const QVector<MeasureEntity>& picks() const noexcept { return picks_; }

signals:
    void modeChanged();
    void resultChanged();
    void pinnedChanged();

private:
    enum class Quantity { None, Length, Angle, Area, Volume, Bounds };
    struct Computed final {
        bool valid{false};
        Quantity quantity{Quantity::None};
        double valueMm{}; // millimetres, mm², mm³ or degrees
        QString title;
        QVariantList details;
        QString method;
        QVariantMap annotation;
    };

    [[nodiscard]] bool eligible(const MeasureEntity& entity, QString* reason) const;
    void append(const MeasureEntity& entity, const QString& description);
    void recompute();
    [[nodiscard]] QString formatLength(double millimetres) const;
    [[nodiscard]] QString formatArea(double squareMillimetres) const;
    [[nodiscard]] QString lengthUnit() const;
    [[nodiscard]] double lengthDivisor() const;
    [[nodiscard]] QString describePick(const MeasureEntity& entity) const;

    QString effectiveUnit_{QStringLiteral("mm")};
    MeasurementMode mode_{MeasurementMode::PointToPoint};
    double surfaceAreaMm2_{};
    double volumeMm3_{};
    QVector3D boundsMm_;
    bool hasSelectedGeometry_{false};
    QVector<MeasureEntity> picks_;
    QVector<QString> pickedDescriptions_;
    // Picks recorded through the older point/topology calls carry no shape: they measure
    // between their points and normals, as those calls always did.
    QVector<bool> legacyPicks_;
    Computed result_;
    QString refusal_;
    QString selectionScope_;
    int selectionPartCount_{};
    QVector3D boxMinimum_;
    QVector3D boxMaximum_;
    bool hasBox_{false};
    QVariantList pinned_;
    int pinCounter_{};
    double longestEdgeMm_{};
    double circularRadiusMm_{};
    int planarFaceCount_{};
};

} // namespace loupe::app::tools
