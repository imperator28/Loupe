#pragma once

#include "app/models/PickerComponents.h"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include <cstdint>

// Controller for the 2D drawing workspace.
//
// The queue is an ordered list keyed by a generated drawing ID, deliberately not a
// checkbox set over the tree: a checkbox cannot express one part queued three times, and
// several views of one part is the normal case for this feature rather than an edge one.
//
// A queued drawing stores its view as a resolved vector, snapshotted when it was added.
// Orbiting afterwards must never change what a queued drawing means.
namespace loupe::app::drawing {

class DrawingWorkspaceController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList components READ components NOTIFY componentsChanged)
    Q_PROPERTY(QVariantList queue READ queue NOTIFY queueChanged)
    Q_PROPERTY(int queueCount READ queueCount NOTIFY queueChanged)
    Q_PROPERTY(QString selectedDrawingId READ selectedDrawingId NOTIFY queueChanged)

    Q_PROPERTY(QString candidateNodeId READ candidateNodeId WRITE setCandidateNodeId NOTIFY candidateChanged)
    Q_PROPERTY(QString candidateViewKind READ candidateViewKind NOTIFY candidateChanged)
    Q_PROPERTY(QString candidateViewLabel READ candidateViewLabel NOTIFY candidateChanged)
    Q_PROPERTY(QString candidateContentMode READ candidateContentMode WRITE setCandidateContentMode NOTIFY candidateChanged)
    Q_PROPERTY(int candidateScaleNumerator READ candidateScaleNumerator NOTIFY candidateChanged)
    Q_PROPERTY(int candidateScaleDenominator READ candidateScaleDenominator NOTIFY candidateChanged)
    Q_PROPERTY(bool candidateValid READ candidateValid NOTIFY candidateChanged)
    Q_PROPERTY(QString candidateStatus READ candidateStatus NOTIFY candidateChanged)
    Q_PROPERTY(int previewRevision READ previewRevision NOTIFY candidateChanged)

    Q_PROPERTY(QString destination READ destination WRITE setDestination NOTIFY settingsChanged)
    Q_PROPERTY(QString format READ format WRITE setFormat NOTIFY settingsChanged)
    Q_PROPERTY(bool includeScaleFiducial READ includeScaleFiducial WRITE setIncludeScaleFiducial NOTIFY settingsChanged)

    Q_PROPERTY(QVariantList planRows READ planRows NOTIFY planChanged)
    Q_PROPERTY(QString planFingerprint READ planFingerprint NOTIFY planChanged)
    Q_PROPERTY(QString planError READ planError NOTIFY planChanged)
    Q_PROPERTY(bool canExport READ canExport NOTIFY planChanged)
    Q_PROPERTY(bool documentReady READ documentReady NOTIFY exportStateChanged)
    Q_PROPERTY(bool exporting READ exporting NOTIFY exportStateChanged)
    Q_PROPERTY(double exportProgress READ exportProgress NOTIFY exportStateChanged)
    Q_PROPERTY(QString exportStage READ exportStage NOTIFY exportStateChanged)
    Q_PROPERTY(QString exportSummary READ exportSummary NOTIFY exportStateChanged)
    Q_PROPERTY(bool exportSucceeded READ exportSucceeded NOTIFY exportStateChanged)

public:
    explicit DrawingWorkspaceController(QObject* parent = nullptr);

    [[nodiscard]] QVariantList components() const;
    [[nodiscard]] QVariantList queue() const;
    [[nodiscard]] int queueCount() const noexcept { return static_cast<int>(queue_.size()); }
    [[nodiscard]] const QString& selectedDrawingId() const noexcept { return selectedDrawingId_; }

    [[nodiscard]] const QString& candidateNodeId() const noexcept { return candidate_.nodeId; }
    [[nodiscard]] const QString& candidateViewKind() const noexcept { return candidate_.view.viewKind; }
    [[nodiscard]] const QString& candidateViewLabel() const noexcept { return candidate_.view.viewLabel; }
    [[nodiscard]] const QString& candidateContentMode() const noexcept { return candidate_.contentMode; }
    [[nodiscard]] int candidateScaleNumerator() const noexcept { return candidate_.scaleNumerator; }
    [[nodiscard]] int candidateScaleDenominator() const noexcept { return candidate_.scaleDenominator; }
    [[nodiscard]] bool candidateValid() const noexcept { return candidateValid_; }
    [[nodiscard]] const QString& candidateStatus() const noexcept { return candidateStatus_; }
    [[nodiscard]] int previewRevision() const noexcept { return previewRevision_; }

    [[nodiscard]] const QString& destination() const noexcept { return destination_; }
    [[nodiscard]] const QString& format() const noexcept { return format_; }
    [[nodiscard]] bool includeScaleFiducial() const noexcept { return includeScaleFiducial_; }

    [[nodiscard]] const QVariantList& planRows() const noexcept { return planRows_; }
    [[nodiscard]] const QString& planFingerprint() const noexcept { return planFingerprint_; }
    [[nodiscard]] const QString& planError() const noexcept { return planError_; }
    [[nodiscard]] bool canExport() const noexcept;
    [[nodiscard]] bool documentReady() const noexcept { return documentReady_; }
    [[nodiscard]] bool exporting() const noexcept { return exporting_; }
    [[nodiscard]] double exportProgress() const noexcept { return exportProgress_; }
    [[nodiscard]] const QString& exportStage() const noexcept { return exportStage_; }
    [[nodiscard]] const QString& exportSummary() const noexcept { return exportSummary_; }
    [[nodiscard]] bool exportSucceeded() const noexcept { return exportSucceeded_; }

    void replaceSnapshot(const QString& snapshotJson);
    void reset();
    void setDocumentReady(bool ready);
    void setExportRequestId(std::uint64_t requestId);
    void handleDrawingProgress(int rowIndex, int rowCount, const QString& stage, double fraction);
    void handleDrawingRowResult(int rowIndex, const QString& drawingId, const QString& path,
                                bool passed, const QString& message);
    void handleDrawingCompleted(int succeededCount, int failedCount);
    void handleDrawingFailed(const QString& message);
    void handleDrawingCanceled();

    Q_INVOKABLE void setCandidateNodeId(const QString& nodeId);
    // A named standard view. The direction is resolved by the caller against the document
    // up axis, so the workspace and the view cube cannot disagree about what "Top" means.
    Q_INVOKABLE void setCandidateStandardView(const QString& label, double x, double y, double z,
                                              double upX, double upY, double upZ);
    // A picked face. deviationDegrees comes from MeshGeometry::faceFrameFor and decides
    // whether the face is flat enough to project normal to.
    Q_INVOKABLE void setCandidateFaceNormal(double x, double y, double z, double deviationDegrees);
    Q_INVOKABLE void setCandidateContentMode(const QString& mode);
    Q_INVOKABLE void setCandidateScale(int numerator, int denominator);
    Q_INVOKABLE void clearCandidateView();

    // Returns the new drawing ID, or an empty string when the candidate is not addable.
    Q_INVOKABLE QString addCandidateToQueue();
    Q_INVOKABLE bool removeDrawing(const QString& drawingId);
    Q_INVOKABLE bool moveDrawing(const QString& drawingId, int index);
    Q_INVOKABLE bool setFilenameOverride(const QString& drawingId, const QString& filename);
    Q_INVOKABLE bool selectDrawing(const QString& drawingId);
    Q_INVOKABLE int drawingCountForNode(const QString& nodeId) const;
    Q_INVOKABLE QString focusSceneNode(const QString& nodeId);

    Q_INVOKABLE void setDestination(const QString& destination);
    Q_INVOKABLE void setDestinationUrl(const QUrl& destinationUrl);
    Q_INVOKABLE void setFormat(const QString& format);
    Q_INVOKABLE void setIncludeScaleFiducial(bool include);

    Q_INVOKABLE bool rebuildPlan();
    Q_INVOKABLE bool exportReviewedPlan();
    Q_INVOKABLE void cancelExport();

signals:
    void componentsChanged();
    void queueChanged();
    void candidateChanged();
    void settingsChanged();
    void planChanged();
    void exportStateChanged();
    void executeRequested(const QByteArray& planJson, const QString& fingerprint);
    void cancelRequested(quint64 requestId);
    // revision lets a late reply be discarded: only the newest preview may be shown.
    void previewRequested(const QByteArray& requestJson, int revision);
    void previewCanceled(int revision);

private:
    struct View final {
        QString viewKind{QStringLiteral("None")};
        QString viewLabel;
        double x{};
        double y{};
        double z{};
        double upX{};
        double upY{};
        double upZ{};
    };

    struct Candidate final {
        QString nodeId;
        View view;
        QString contentMode{QStringLiteral("Cut contours")};
        int scaleNumerator{1};
        int scaleDenominator{1};
    };

    struct QueuedDrawing final {
        QString drawingId;
        QString nodeId;
        View view;
        QString contentMode;
        int scaleNumerator{1};
        int scaleDenominator{1};
        QString filenameOverride;
    };

    [[nodiscard]] int indexOfDrawing(const QString& drawingId) const;
    [[nodiscard]] bool locked() const noexcept { return exporting_; }
    void refreshCandidate();
    void requestPreview();
    void refreshPlan();
    void clearPlan();
    void clearExportResult();
    [[nodiscard]] QByteArray reviewedPlanJson() const;

    models::PickerComponents picker_;
    QVector<QueuedDrawing> queue_;
    QString selectedDrawingId_;
    Candidate candidate_;
    bool candidateValid_{};
    QString candidateStatus_;
    int previewRevision_{};
    quint64 nextDrawingSerial_{1};

    QString destination_;
    QString format_{QStringLiteral("DXF")};
    bool includeScaleFiducial_{false};

    QVariantList planRows_;
    QString planFingerprint_;
    QString planError_;
    bool documentReady_{};
    bool exporting_{};
    double exportProgress_{};
    QString exportStage_;
    QString exportSummary_;
    bool exportSucceeded_{};
    std::uint64_t exportRequestId_{};
};

} // namespace loupe::app::drawing
