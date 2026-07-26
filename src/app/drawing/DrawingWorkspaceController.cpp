#include "app/drawing/DrawingWorkspaceController.h"

#include "core/drawing/DrawingPlan.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <algorithm>
#include <array>
#include <cmath>

namespace loupe::app::drawing {
namespace {

// A face flatter than this projects to the same outline as a true plane at any tolerance
// a cutter can hold. Beyond it the "normal" is an average that belongs to no part of the
// face, and projecting from it would silently produce a drawing of nothing real.
constexpr double kMaximumFaceDeviationDegrees = 0.5;

[[nodiscard]] loupe::drawing::DrawingFormat formatFor(const QString& format)
{
    if (format == QStringLiteral("SVG")) return loupe::drawing::DrawingFormat::Svg;
    if (format == QStringLiteral("PDF")) return loupe::drawing::DrawingFormat::Pdf;
    return loupe::drawing::DrawingFormat::Dxf;
}

[[nodiscard]] QString extensionFor(const QString& format)
{
    if (format == QStringLiteral("SVG")) return QStringLiteral(".svg");
    if (format == QStringLiteral("PDF")) return QStringLiteral(".pdf");
    return QStringLiteral(".dxf");
}

[[nodiscard]] loupe::drawing::DrawingContent contentFor(const QString& mode)
{
    if (mode == QStringLiteral("Outer contour only")) {
        return loupe::drawing::DrawingContent::OuterContourOnly;
    }
    if (mode == QStringLiteral("Technical view")) return loupe::drawing::DrawingContent::TechnicalView;
    return loupe::drawing::DrawingContent::CutContours;
}

[[nodiscard]] bool isKnownContentMode(const QString& mode)
{
    return mode == QStringLiteral("Outer contour only") || mode == QStringLiteral("Cut contours")
        || mode == QStringLiteral("Technical view");
}

[[nodiscard]] bool isKnownFormat(const QString& format)
{
    return format == QStringLiteral("DXF") || format == QStringLiteral("SVG")
        || format == QStringLiteral("PDF");
}

[[nodiscard]] std::string utf8(const QString& value)
{
    return value.toUtf8().toStdString();
}

} // namespace

DrawingWorkspaceController::DrawingWorkspaceController(QObject* parent)
    : QObject(parent)
{
}

QVariantList DrawingWorkspaceController::components() const
{
    QVariantList result;
    result.reserve(picker_.components().size());
    for (const auto& component : picker_.components()) {
        if (!component.visibleInPicker) continue;
        result.append(QVariantMap{{QStringLiteral("nodeId"), component.id},
                                  {QStringLiteral("parentId"), component.parentId},
                                  {QStringLiteral("name"), component.name},
                                  {QStringLiteral("path"), component.hierarchyPath},
                                  {QStringLiteral("kind"), models::pickerKindLabel(component.kind)},
                                  {QStringLiteral("depth"), component.depth},
                                  {QStringLiteral("exportable"), component.exportable},
                                  {QStringLiteral("hasChildren"), component.hasVisibleChildren},
                                  {QStringLiteral("drawingCount"), drawingCountForNode(component.id)}});
    }
    return result;
}

QVariantList DrawingWorkspaceController::queue() const
{
    QVariantList result;
    result.reserve(queue_.size());
    for (const auto& drawing : queue_) {
        const auto* component = picker_.find(drawing.nodeId);
        result.append(QVariantMap{
            {QStringLiteral("drawingId"), drawing.drawingId},
            {QStringLiteral("nodeId"), drawing.nodeId},
            {QStringLiteral("name"), component ? component->name : tr("Unknown part")},
            {QStringLiteral("viewKind"), drawing.view.viewKind},
            {QStringLiteral("viewLabel"), drawing.view.viewLabel},
            {QStringLiteral("contentMode"), drawing.contentMode},
            {QStringLiteral("scaleLabel"), QStringLiteral("%1:%2")
                                               .arg(drawing.scaleNumerator)
                                               .arg(drawing.scaleDenominator)},
            {QStringLiteral("filenameOverridden"), !drawing.filenameOverride.isEmpty()},
            {QStringLiteral("selected"), drawing.drawingId == selectedDrawingId_}});
    }
    return result;
}

void DrawingWorkspaceController::replaceSnapshot(const QString& snapshotJson)
{
    if (locked()) return;
    queue_.clear();
    selectedDrawingId_.clear();
    candidate_ = {};
    if (!picker_.replaceSnapshot(snapshotJson)) {
        reset();
        return;
    }
    clearPlan();
    refreshCandidate();
    emit componentsChanged();
    emit queueChanged();
}

void DrawingWorkspaceController::reset()
{
    picker_.clear();
    queue_.clear();
    selectedDrawingId_.clear();
    candidate_ = {};
    nextDrawingSerial_ = 1;
    documentReady_ = false;
    exporting_ = false;
    exportRequestId_ = 0;
    clearExportResult();
    clearPlan();
    refreshCandidate();
    emit componentsChanged();
    emit queueChanged();
    emit exportStateChanged();
}

void DrawingWorkspaceController::setDocumentReady(const bool ready)
{
    if (documentReady_ == ready) return;
    documentReady_ = ready;
    emit exportStateChanged();
    emit planChanged();
}

void DrawingWorkspaceController::setCandidateNodeId(const QString& nodeId)
{
    if (locked() || candidate_.nodeId == nodeId) return;
    if (!nodeId.isEmpty() && !picker_.contains(nodeId)) return;
    candidate_.nodeId = nodeId;
    refreshCandidate();
}

void DrawingWorkspaceController::setCandidateStandardView(const QString& label, const double x,
                                                          const double y, const double z,
                                                          const double upX, const double upY,
                                                          const double upZ)
{
    if (locked()) return;
    candidate_.view = {QStringLiteral("Standard"), label, x, y, z, upX, upY, upZ};
    refreshCandidate();
}

void DrawingWorkspaceController::setCandidateFaceNormal(const double x, const double y, const double z,
                                                        const double deviationDegrees)
{
    if (locked()) return;
    // A curved face is refused here, with the deviation quoted, rather than projected from
    // an averaged normal that belongs to no part of the face.
    if (!std::isfinite(deviationDegrees) || deviationDegrees > kMaximumFaceDeviationDegrees) {
        candidate_.view = {};
        candidateValid_ = false;
        candidateStatus_ = tr("That face is curved (%1 degrees across it). Pick a flat face, "
                              "or choose a standard view.")
                               .arg(deviationDegrees, 0, 'f', 1);
        emit candidateChanged();
        return;
    }
    // Any up direction not parallel to the normal will do; the workspace offers roll
    // separately. Picking the world axis least aligned with the normal keeps it stable.
    const std::array<double, 3> normal{x, y, z};
    auto smallest = 0;
    for (auto axis = 1; axis < 3; ++axis) {
        if (std::abs(normal.at(axis)) < std::abs(normal.at(smallest))) smallest = axis;
    }
    candidate_.view = {QStringLiteral("FaceNormal"), tr("Normal to face"), x, y, z,
                       smallest == 0 ? 1.0 : 0.0, smallest == 1 ? 1.0 : 0.0, smallest == 2 ? 1.0 : 0.0};
    refreshCandidate();
}

void DrawingWorkspaceController::setCandidateContentMode(const QString& mode)
{
    if (locked() || !isKnownContentMode(mode) || candidate_.contentMode == mode) return;
    candidate_.contentMode = mode;
    refreshCandidate();
}

void DrawingWorkspaceController::setCandidateScale(const int numerator, const int denominator)
{
    if (locked() || numerator <= 0 || denominator <= 0) return;
    if (candidate_.scaleNumerator == numerator && candidate_.scaleDenominator == denominator) return;
    candidate_.scaleNumerator = numerator;
    candidate_.scaleDenominator = denominator;
    refreshCandidate();
}

void DrawingWorkspaceController::clearCandidateView()
{
    if (locked()) return;
    candidate_.view = {};
    refreshCandidate();
}

void DrawingWorkspaceController::refreshCandidate()
{
    candidateValid_ = false;
    candidateStatus_.clear();
    if (candidate_.nodeId.isEmpty() || !picker_.contains(candidate_.nodeId)) {
        candidateStatus_ = tr("Choose a part to draw");
    } else if (candidate_.view.viewKind == QStringLiteral("None")) {
        candidateStatus_ = tr("Choose a view: a cube face, or a flat face on the part");
    } else {
        candidateValid_ = true;
        candidateStatus_ = tr("Ready to add");
    }
    if (candidateValid_) {
        requestPreview();
    } else if (previewRevision_ != 0) {
        // Nothing valid to show, so any preview still in flight is now for a state the
        // user has moved away from.
        emit previewCanceled(previewRevision_);
    }
    emit candidateChanged();
}

void DrawingWorkspaceController::requestPreview()
{
    // Superseding by revision rather than cancelling and waiting: a stale reply can still
    // arrive, and the revision is what lets the view drop it.
    if (previewRevision_ != 0) emit previewCanceled(previewRevision_);
    ++previewRevision_;
    const auto request = QJsonObject{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("revision"), previewRevision_},
        {QStringLiteral("nodeId"), candidate_.nodeId},
        {QStringLiteral("viewLabel"), candidate_.view.viewLabel},
        {QStringLiteral("viewX"), candidate_.view.x},
        {QStringLiteral("viewY"), candidate_.view.y},
        {QStringLiteral("viewZ"), candidate_.view.z},
        {QStringLiteral("upX"), candidate_.view.upX},
        {QStringLiteral("upY"), candidate_.view.upY},
        {QStringLiteral("upZ"), candidate_.view.upZ},
        {QStringLiteral("contentMode"), candidate_.contentMode},
        {QStringLiteral("scaleNumerator"), candidate_.scaleNumerator},
        {QStringLiteral("scaleDenominator"), candidate_.scaleDenominator},
    };
    emit previewRequested(QJsonDocument(request).toJson(QJsonDocument::Compact), previewRevision_);
}

QString DrawingWorkspaceController::addCandidateToQueue()
{
    if (locked() || !candidateValid_) return {};
    // An exact duplicate would produce two identical files under two names, so it is
    // reported rather than silently accepted.
    const auto duplicate = std::ranges::find_if(queue_, [this](const QueuedDrawing& drawing) {
        return drawing.nodeId == candidate_.nodeId && drawing.contentMode == candidate_.contentMode
            && drawing.scaleNumerator == candidate_.scaleNumerator
            && drawing.scaleDenominator == candidate_.scaleDenominator
            && drawing.view.viewLabel == candidate_.view.viewLabel
            && drawing.view.x == candidate_.view.x && drawing.view.y == candidate_.view.y
            && drawing.view.z == candidate_.view.z && drawing.view.upX == candidate_.view.upX
            && drawing.view.upY == candidate_.view.upY && drawing.view.upZ == candidate_.view.upZ;
    });
    if (duplicate != queue_.end()) {
        candidateStatus_ = tr("That drawing is already in the queue");
        selectedDrawingId_ = duplicate->drawingId;
        emit candidateChanged();
        emit queueChanged();
        return {};
    }

    QueuedDrawing drawing;
    drawing.drawingId = QStringLiteral("d%1").arg(nextDrawingSerial_++);
    drawing.nodeId = candidate_.nodeId;
    // Snapshotted, not referenced: orbiting after this must not change what was queued.
    drawing.view = candidate_.view;
    drawing.contentMode = candidate_.contentMode;
    drawing.scaleNumerator = candidate_.scaleNumerator;
    drawing.scaleDenominator = candidate_.scaleDenominator;
    const auto drawingId = drawing.drawingId;
    queue_.append(std::move(drawing));
    selectedDrawingId_ = drawingId;
    refreshPlan();
    emit queueChanged();
    emit componentsChanged();
    emit candidateChanged();
    return drawingId;
}

int DrawingWorkspaceController::indexOfDrawing(const QString& drawingId) const
{
    for (qsizetype index = 0; index < queue_.size(); ++index) {
        if (queue_.at(index).drawingId == drawingId) return static_cast<int>(index);
    }
    return -1;
}

bool DrawingWorkspaceController::removeDrawing(const QString& drawingId)
{
    if (locked()) return false;
    const auto index = indexOfDrawing(drawingId);
    if (index < 0) return false;
    queue_.remove(index);
    if (selectedDrawingId_ == drawingId) {
        selectedDrawingId_ = queue_.isEmpty()
            ? QString{}
            : queue_.at(std::min<qsizetype>(index, queue_.size() - 1)).drawingId;
    }
    refreshPlan();
    emit queueChanged();
    emit componentsChanged();
    return true;
}

bool DrawingWorkspaceController::moveDrawing(const QString& drawingId, const int index)
{
    if (locked()) return false;
    const auto from = indexOfDrawing(drawingId);
    if (from < 0 || index < 0 || index >= queue_.size() || from == index) return false;
    queue_.move(from, index);
    refreshPlan();
    emit queueChanged();
    return true;
}

bool DrawingWorkspaceController::setFilenameOverride(const QString& drawingId, const QString& filename)
{
    if (locked()) return false;
    const auto index = indexOfDrawing(drawingId);
    if (index < 0) return false;
    queue_[index].filenameOverride = filename.trimmed();
    refreshPlan();
    emit queueChanged();
    return true;
}

bool DrawingWorkspaceController::selectDrawing(const QString& drawingId)
{
    const auto index = indexOfDrawing(drawingId);
    if (index < 0) return false;
    selectedDrawingId_ = drawingId;
    if (!locked()) {
        // Selecting a row loads it back into the candidate, so the preview shows the
        // drawing being looked at rather than whatever was configured last.
        const auto& drawing = queue_.at(index);
        candidate_.nodeId = drawing.nodeId;
        candidate_.view = drawing.view;
        candidate_.contentMode = drawing.contentMode;
        candidate_.scaleNumerator = drawing.scaleNumerator;
        candidate_.scaleDenominator = drawing.scaleDenominator;
        refreshCandidate();
    }
    emit queueChanged();
    return true;
}

int DrawingWorkspaceController::drawingCountForNode(const QString& nodeId) const
{
    return static_cast<int>(std::ranges::count_if(
        queue_, [&nodeId](const QueuedDrawing& drawing) { return drawing.nodeId == nodeId; }));
}

QString DrawingWorkspaceController::focusSceneNode(const QString& nodeId)
{
    const auto pickerNodeId = picker_.pickerNodeForSceneNode(nodeId);
    setCandidateNodeId(pickerNodeId);
    return pickerNodeId;
}

void DrawingWorkspaceController::setDestination(const QString& destination)
{
    if (locked() || destination_ == destination) return;
    destination_ = destination;
    refreshPlan();
    emit settingsChanged();
}

void DrawingWorkspaceController::setDestinationUrl(const QUrl& destinationUrl)
{
    setDestination(destinationUrl.isLocalFile() ? destinationUrl.toLocalFile() : destinationUrl.toString());
}

void DrawingWorkspaceController::setFormat(const QString& format)
{
    if (locked() || !isKnownFormat(format) || format_ == format) return;
    format_ = format;
    refreshPlan();
    emit settingsChanged();
}

void DrawingWorkspaceController::setIncludeScaleFiducial(const bool include)
{
    if (locked() || includeScaleFiducial_ == include) return;
    includeScaleFiducial_ = include;
    refreshPlan();
    emit settingsChanged();
}

bool DrawingWorkspaceController::canExport() const noexcept
{
    return documentReady_ && !exporting_ && !queue_.isEmpty() && !destination_.trimmed().isEmpty()
        && planError_.isEmpty() && planRows_.size() == queue_.size();
}

void DrawingWorkspaceController::refreshPlan()
{
    clearExportResult();
    planRows_.clear();
    planFingerprint_.clear();
    planError_.clear();
    if (queue_.isEmpty()) {
        emit planChanged();
        return;
    }

    loupe::drawing::DrawingPlanRequest request;
    request.destination = destination_.trimmed().isEmpty() ? std::string(".")
                                                           : utf8(destination_.trimmed());
    request.format = formatFor(format_);
    request.includeScaleFiducial = includeScaleFiducial_;
    request.unitDecision = {loupe::units::LengthUnit::Millimeter, loupe::units::UnitConfidence::Confirmed,
                            picker_.sourceToMillimeters(), "reviewed in Loupe"};

    for (const auto& drawing : queue_) {
        const auto* component = picker_.find(drawing.nodeId);
        if (!component) continue;
        loupe::drawing::DrawingSelection selection;
        selection.drawingId = utf8(drawing.drawingId);
        selection.nodeId = utf8(drawing.nodeId);
        selection.viewLabel = utf8(drawing.view.viewLabel);
        selection.viewDirection = {drawing.view.x, drawing.view.y, drawing.view.z};
        selection.upDirection = {drawing.view.upX, drawing.view.upY, drawing.view.upZ};
        selection.content = contentFor(drawing.contentMode);
        selection.scaleNumerator = drawing.scaleNumerator;
        selection.scaleDenominator = drawing.scaleDenominator;
        request.selections.push_back(std::move(selection));
        request.hierarchyPaths.emplace(utf8(drawing.nodeId), utf8(component->hierarchyPath));
        if (!drawing.filenameOverride.isEmpty()) {
            request.outputLeafNames.emplace(utf8(drawing.drawingId), utf8(drawing.filenameOverride));
        }
    }

    const auto rowFor = [this](const QueuedDrawing& drawing, const QString& path, const QString& status,
                               const QString& error) {
        const auto* component = picker_.find(drawing.nodeId);
        return QVariantMap{
            {QStringLiteral("drawingId"), drawing.drawingId},
            {QStringLiteral("nodeId"), drawing.nodeId},
            {QStringLiteral("name"), component ? component->name : tr("Unknown part")},
            {QStringLiteral("viewLabel"), drawing.view.viewLabel},
            {QStringLiteral("contentMode"), drawing.contentMode},
            {QStringLiteral("scaleLabel"), QStringLiteral("%1:%2")
                                               .arg(drawing.scaleNumerator)
                                               .arg(drawing.scaleDenominator)},
            {QStringLiteral("filename"), QFileInfo(path).fileName()},
            {QStringLiteral("filenameOverridden"), !drawing.filenameOverride.isEmpty()},
            {QStringLiteral("path"), path},
            {QStringLiteral("format"), format_},
            {QStringLiteral("status"), status},
            {QStringLiteral("autoNumbered"), false},
            {QStringLiteral("error"), error}};
    };

    try {
        const auto plan = loupe::drawing::buildDrawingPlan(request);
        QHash<QString, QString> pathByDrawing;
        for (const auto& output : plan.outputs()) {
            pathByDrawing.insert(QString::fromStdString(output.drawingId()),
                                 QString::fromStdString(output.finalPath()));
        }
        // Rows follow the queue, not the plan's canonical order: every rowIndex in a
        // worker event has to index what the user is looking at.
        QSet<QString> autoNumbered;
        for (const auto& output : plan.outputs()) {
            if (output.autoNumbered()) autoNumbered.insert(QString::fromStdString(output.drawingId()));
        }
        for (const auto& drawing : queue_) {
            const bool numbered = autoNumbered.contains(drawing.drawingId);
            auto row = rowFor(drawing, pathByDrawing.value(drawing.drawingId),
                              destination_.trimmed().isEmpty()
                                  ? tr("Choose destination folder")
                                  : numbered
                                      // The number says which came first, not which is which,
                                      // so the user is asked to look rather than just told.
                                      ? tr("Numbered automatically — check this name")
                                      : tr("Ready"),
                              QString{});
            row.insert(QStringLiteral("autoNumbered"), numbered);
            planRows_.append(row);
        }
        planFingerprint_ = QString::fromStdString(plan.fingerprint());
    } catch (const loupe::drawing::DrawingPlanError& error) {
        planError_ = QString::fromUtf8(error.what());
        for (const auto& drawing : queue_) {
            const auto* component = picker_.find(drawing.nodeId);
            const auto leaf = drawing.filenameOverride.isEmpty()
                ? QString::fromStdString(loupe::drawing::drawingLeafName(
                      {utf8(drawing.drawingId), utf8(drawing.nodeId), utf8(drawing.view.viewLabel),
                       {drawing.view.x, drawing.view.y, drawing.view.z},
                       {drawing.view.upX, drawing.view.upY, drawing.view.upZ},
                       contentFor(drawing.contentMode), drawing.scaleNumerator,
                       drawing.scaleDenominator},
                      component ? utf8(component->hierarchyPath) : std::string{}))
                : drawing.filenameOverride;
            planRows_.append(rowFor(drawing, leaf + extensionFor(format_), tr("Needs attention"),
                                    planError_));
        }
    }
    emit planChanged();
}

bool DrawingWorkspaceController::rebuildPlan()
{
    refreshPlan();
    return canExport();
}

void DrawingWorkspaceController::clearPlan()
{
    if (planRows_.isEmpty() && planFingerprint_.isEmpty() && planError_.isEmpty()) return;
    planRows_.clear();
    planFingerprint_.clear();
    planError_.clear();
    emit planChanged();
}

QByteArray DrawingWorkspaceController::reviewedPlanJson() const
{
    QJsonArray selections;
    for (const auto& drawing : queue_) {
        const auto* component = picker_.find(drawing.nodeId);
        if (!component) continue;
        selections.append(QJsonObject{
            {QStringLiteral("drawingId"), drawing.drawingId},
            {QStringLiteral("nodeId"), drawing.nodeId},
            {QStringLiteral("hierarchyPath"), component->hierarchyPath},
            {QStringLiteral("viewLabel"), drawing.view.viewLabel},
            {QStringLiteral("viewX"), drawing.view.x},
            {QStringLiteral("viewY"), drawing.view.y},
            {QStringLiteral("viewZ"), drawing.view.z},
            {QStringLiteral("upX"), drawing.view.upX},
            {QStringLiteral("upY"), drawing.view.upY},
            {QStringLiteral("upZ"), drawing.view.upZ},
            {QStringLiteral("contentMode"), drawing.contentMode},
            {QStringLiteral("scaleNumerator"), drawing.scaleNumerator},
            {QStringLiteral("scaleDenominator"), drawing.scaleDenominator},
            {QStringLiteral("leafName"), drawing.filenameOverride},
        });
    }
    return QJsonDocument(QJsonObject{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("destination"), destination_.trimmed()},
        {QStringLiteral("format"), format_},
        {QStringLiteral("includeScaleFiducial"), includeScaleFiducial_},
        {QStringLiteral("sourceToMillimeters"), picker_.sourceToMillimeters()},
        {QStringLiteral("selections"), selections},
    }).toJson(QJsonDocument::Compact);
}

bool DrawingWorkspaceController::exportReviewedPlan()
{
    refreshPlan();
    if (!canExport()) return false;
    exporting_ = true;
    exportProgress_ = 0.0;
    exportStage_ = tr("Preparing reviewed drawings");
    exportSummary_.clear();
    exportSucceeded_ = false;
    exportRequestId_ = 0;
    for (qsizetype index = 0; index < planRows_.size(); ++index) {
        auto row = planRows_.at(index).toMap();
        row.insert(QStringLiteral("status"), tr("Queued"));
        row.insert(QStringLiteral("error"), QString{});
        planRows_[index] = row;
    }
    emit planChanged();
    emit exportStateChanged();
    emit executeRequested(reviewedPlanJson(), planFingerprint_);
    return true;
}

void DrawingWorkspaceController::setExportRequestId(const std::uint64_t requestId)
{
    if (exporting_) exportRequestId_ = requestId;
}

void DrawingWorkspaceController::handleDrawingProgress(const int rowIndex, const int rowCount,
                                                       const QString& stage, const double fraction)
{
    if (!exporting_) return;
    exportStage_ = stage;
    exportProgress_ = std::clamp(fraction, 0.0, 1.0);
    if (rowIndex >= 0 && rowIndex < planRows_.size()) {
        auto row = planRows_.at(rowIndex).toMap();
        row.insert(QStringLiteral("status"), stage);
        planRows_[rowIndex] = row;
        emit planChanged();
    }
    Q_UNUSED(rowCount)
    emit exportStateChanged();
}

void DrawingWorkspaceController::handleDrawingRowResult(const int rowIndex, const QString& drawingId,
                                                        const QString& path, const bool passed,
                                                        const QString& message)
{
    if (!exporting_ || rowIndex < 0 || rowIndex >= planRows_.size()) return;
    auto row = planRows_.at(rowIndex).toMap();
    // A result that does not match the reviewed row means the worker and the review have
    // diverged, which invalidates the whole batch rather than one row.
    if (row.value(QStringLiteral("drawingId")).toString() != drawingId
        || row.value(QStringLiteral("path")).toString() != path) {
        handleDrawingFailed(tr("Worker result did not match the reviewed drawing row"));
        return;
    }
    row.insert(QStringLiteral("status"), passed ? tr("Written and validated") : tr("Failed"));
    row.insert(QStringLiteral("error"), passed ? QString{} : message);
    planRows_[rowIndex] = row;
    emit planChanged();
}

void DrawingWorkspaceController::handleDrawingCompleted(const int succeededCount, const int failedCount)
{
    if (!exporting_) return;
    exporting_ = false;
    exportRequestId_ = 0;
    exportProgress_ = 1.0;
    exportStage_ = tr("Drawings complete");
    exportSucceeded_ = failedCount == 0 && succeededCount == planRows_.size();
    exportSummary_ = failedCount == 0
        ? tr("%1 drawings written and validated").arg(succeededCount)
        : tr("%1 written, %2 failed").arg(succeededCount).arg(failedCount);
    emit planChanged();
    emit exportStateChanged();
}

void DrawingWorkspaceController::handleDrawingFailed(const QString& message)
{
    if (!exporting_) return;
    exporting_ = false;
    exportRequestId_ = 0;
    exportStage_ = tr("Drawing export stopped");
    exportSucceeded_ = false;
    exportSummary_ = message;
    emit planChanged();
    emit exportStateChanged();
}

void DrawingWorkspaceController::handleDrawingCanceled()
{
    handleDrawingFailed(tr("Drawing export canceled"));
}

void DrawingWorkspaceController::cancelExport()
{
    if (exporting_ && exportRequestId_ != 0) emit cancelRequested(exportRequestId_);
}

void DrawingWorkspaceController::clearExportResult()
{
    if (exporting_) return;
    const bool changed = exportProgress_ != 0.0 || !exportStage_.isEmpty()
        || !exportSummary_.isEmpty() || exportSucceeded_;
    exportProgress_ = 0.0;
    exportStage_.clear();
    exportSummary_.clear();
    exportSucceeded_ = false;
    if (changed) emit exportStateChanged();
}

} // namespace loupe::app::drawing
