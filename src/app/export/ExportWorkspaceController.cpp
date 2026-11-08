#include "app/export/ExportWorkspaceController.h"

#include "core/domain/AssemblyTypes.h"
#include "core/export/ExportPlan.h"
#include "core/export/OutputNaming.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFileInfo>

#include <algorithm>

namespace loupe::app::exporting {
namespace {

loupe::exporting::SelectionKind selectionKindFor(const int kind)
{
    using loupe::domain::NodeKind;
    switch (static_cast<NodeKind>(kind)) {
    case NodeKind::Subassembly: return loupe::exporting::SelectionKind::Subassembly;
    case NodeKind::Body: return loupe::exporting::SelectionKind::Body;
    case NodeKind::Definition: return loupe::exporting::SelectionKind::Definition;
    case NodeKind::Occurrence:
    case NodeKind::Root: return loupe::exporting::SelectionKind::Occurrence;
    }
    return loupe::exporting::SelectionKind::Occurrence;
}

} // namespace

ExportWorkspaceController::ExportWorkspaceController(QObject* parent)
    : QObject(parent)
{
}

QVariantList ExportWorkspaceController::components() const
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
                                  {QStringLiteral("checked"), checkedNodeIds_.contains(component.id)}});
    }
    return result;
}

void ExportWorkspaceController::replaceSnapshot(const QString& snapshotJson)
{
    if (exporting_) return;
    checkedNodeIds_.clear();
    filenameOverrides_.clear();
    focusedNodeId_.clear();
    hoveredNodeId_.clear();
    if (!picker_.replaceSnapshot(snapshotJson)) {
        reset();
        return;
    }
    clearPlan();
    ++selectionRevision_;
    emit componentsChanged();
    emit selectionChanged();
    emit previewChanged();
}

void ExportWorkspaceController::reset()
{
    picker_.clear();
    checkedNodeIds_.clear();
    filenameOverrides_.clear();
    focusedNodeId_.clear();
    hoveredNodeId_.clear();
    documentReady_ = false;
    exporting_ = false;
    exportRequestId_ = 0;
    clearExportResult();
    clearPlan();
    ++selectionRevision_;
    emit componentsChanged();
    emit selectionChanged();
    emit previewChanged();
    emit exportStateChanged();
}

void ExportWorkspaceController::setDocumentReady(const bool ready)
{
    if (documentReady_ == ready) return;
    documentReady_ = ready;
    emit exportStateChanged();
    emit planChanged();
}

void ExportWorkspaceController::setChecked(const QString& nodeId, const bool checked)
{
    if (exporting_) return;
    const auto index = picker_.indexOf(nodeId);
    if (index < 0 || !picker_.components().at(index).exportable) return;
    const auto changed = checked ? !checkedNodeIds_.contains(nodeId) : checkedNodeIds_.contains(nodeId);
    if (!changed) return;
    if (checked) checkedNodeIds_.append(nodeId);
    else {
        checkedNodeIds_.removeAll(nodeId);
        filenameOverrides_.remove(nodeId);
    }
    refreshPlan();
    ++selectionRevision_;
    emit selectionChanged();
}

void ExportWorkspaceController::setAllChecked(const bool checked)
{
    if (exporting_) return;
    if (checked) {
        for (const auto& component : picker_.components()) {
            if (component.exportable && !checkedNodeIds_.contains(component.id)) checkedNodeIds_.append(component.id);
        }
    } else {
        checkedNodeIds_.clear();
        filenameOverrides_.clear();
    }
    refreshPlan();
    ++selectionRevision_;
    emit selectionChanged();
}

void ExportWorkspaceController::removeBucketRow(const QString& nodeId)
{
    if (exporting_) return;
    const auto groups = groupedOutputs();
    const auto group = std::ranges::find_if(groups, [&nodeId](const OutputGroup& candidate) {
        return candidate.nodeIds.contains(nodeId);
    });
    if (group == groups.end()) return;
    for (const auto& member : group->nodeIds) {
        checkedNodeIds_.removeAll(member);
        filenameOverrides_.remove(member);
    }
    refreshPlan();
    ++selectionRevision_;
    emit selectionChanged();
}

bool ExportWorkspaceController::isChecked(const QString& nodeId) const
{
    return checkedNodeIds_.contains(nodeId);
}

bool ExportWorkspaceController::containsNode(const QString& selectionId, const QString& nodeId) const
{
    auto current = nodeId;
    QSet<QString> visited;
    while (!current.isEmpty() && !visited.contains(current)) {
        if (current == selectionId) return true;
        visited.insert(current);
        const auto index = picker_.indexOf(current);
        if (index < 0) return false;
        current = picker_.components().at(index).parentId;
    }
    return false;
}

void ExportWorkspaceController::setFocusedNodeId(const QString& nodeId)
{
    if (focusedNodeId_ == nodeId || (!nodeId.isEmpty() && !picker_.contains(nodeId))) return;
    focusedNodeId_ = nodeId;
    emit previewChanged();
}

void ExportWorkspaceController::setHoveredNodeId(const QString& nodeId)
{
    if (hoveredNodeId_ == nodeId || (!nodeId.isEmpty() && !picker_.contains(nodeId))) return;
    hoveredNodeId_ = nodeId;
    emit previewChanged();
}

void ExportWorkspaceController::setDestination(const QString& destination)
{
    if (exporting_) return;
    if (destination_ == destination) return;
    destination_ = destination;
    destinationIsDefault_ = false;
    refreshPlan();
    emit settingsChanged();
}

void ExportWorkspaceController::setDefaultDestination(const QString& folder)
{
    destination_ = folder;
    destinationIsDefault_ = !folder.isEmpty();
    refreshPlan();
    emit settingsChanged();
}

void ExportWorkspaceController::setDestinationUrl(const QUrl& destinationUrl)
{
    if (!destinationUrl.isValid() || !destinationUrl.isLocalFile()) return;
    const auto localPath = destinationUrl.toLocalFile();
    if (localPath.trimmed().isEmpty()) return;
    setDestination(localPath);
}

void ExportWorkspaceController::setFormat(const QString& format)
{
    if (exporting_) return;
    const auto normalized = format.toUpper() == QStringLiteral("STL") ? QStringLiteral("STL") : QStringLiteral("STEP");
    if (format_ == normalized) return;
    format_ = normalized;
    refreshPlan();
    emit settingsChanged();
}

void ExportWorkspaceController::setCoordinates(const QString& coordinates)
{
    if (exporting_) return;
    const auto normalized = coordinates == QStringLiteral("Local") ? QStringLiteral("Local") : QStringLiteral("Assembly");
    if (coordinates_ == normalized) return;
    coordinates_ = normalized;
    refreshPlan();
    emit settingsChanged();
}

void ExportWorkspaceController::setGrouping(const QString& grouping)
{
    if (exporting_) return;
    if (grouping_ == grouping) return;
    grouping_ = grouping;
    refreshPlan();
    emit settingsChanged();
}

void ExportWorkspaceController::setNamingStrategy(const QString& strategy)
{
    if (exporting_) return;
    const auto normalized = strategy == QStringLiteral("sequence") || strategy == QStringLiteral("prefix")
        ? strategy : QStringLiteral("keep");
    if (namingStrategy_ == normalized) return;
    namingStrategy_ = normalized;
    refreshPlan();
    emit settingsChanged();
}

void ExportWorkspaceController::setNamingValue(const QString& value)
{
    if (exporting_) return;
    if (namingValue_ == value) return;
    namingValue_ = value;
    refreshPlan();
    emit settingsChanged();
}

// Indexes are bucket rows, i.e. files: a row moves together with every copy it stands for.
void ExportWorkspaceController::moveBucketItem(const int from, const int to)
{
    if (exporting_) return;
    auto groups = groupedOutputs();
    if (from < 0 || from >= groups.size() || to < 0 || to >= groups.size() || from == to) return;
    groups.move(from, to);
    checkedNodeIds_.clear();
    for (const auto& group : groups) checkedNodeIds_.append(group.nodeIds);
    refreshPlan();
}

void ExportWorkspaceController::moveBucketItemById(const QString& nodeId, const int to)
{
    moveBucketItem(groupIndexOf(nodeId), to);
}

void ExportWorkspaceController::setFilenameOverride(const QString& nodeId, const QString& filename)
{
    if (exporting_) return;
    if (!checkedNodeIds_.contains(nodeId)) return;
    auto leaf = filename.trimmed();
    if (leaf.endsWith(QStringLiteral(".step"), Qt::CaseInsensitive)) leaf.chop(5);
    else if (leaf.endsWith(QStringLiteral(".stl"), Qt::CaseInsensitive)) leaf.chop(4);
    if (leaf.isEmpty()) filenameOverrides_.remove(nodeId);
    else filenameOverrides_.insert(nodeId, leaf);
    refreshPlan();
}

QString ExportWorkspaceController::focusSceneNode(const QString& nodeId)
{
    const auto pickerNodeId = picker_.pickerNodeForSceneNode(nodeId);
    setFocusedNodeId(pickerNodeId);
    return pickerNodeId;
}

bool ExportWorkspaceController::canExport() const noexcept
{
    return documentReady_ && !exporting_ && !checkedNodeIds_.isEmpty() && !destination_.trimmed().isEmpty()
        && planError_.isEmpty() && !outputs_.isEmpty() && planRows_.size() == outputs_.size();
}

QVector<ExportWorkspaceController::OutputGroup> ExportWorkspaceController::groupedOutputs() const
{
    QVector<OutputGroup> groups;
    QHash<QString, int> groupByIdentity;
    for (const auto& nodeId : checkedNodeIds_) {
        if (picker_.indexOf(nodeId) < 0) continue;
        const auto identity = picker_.identityKey(nodeId);
        const auto existing = groupByIdentity.constFind(identity);
        if (existing != groupByIdentity.cend()) {
            groups[existing.value()].nodeIds.append(nodeId);
            continue;
        }
        groupByIdentity.insert(identity, static_cast<int>(groups.size()));
        groups.append(OutputGroup{{nodeId}, {}, false});
    }
    return groups;
}

int ExportWorkspaceController::groupIndexOf(const QString& nodeId) const
{
    const auto groups = groupedOutputs();
    for (int index = 0; index < groups.size(); ++index) {
        if (groups.at(index).nodeIds.contains(nodeId)) return index;
    }
    return -1;
}

QString ExportWorkspaceController::baseName(const OutputGroup& group, const int fileIndex) const
{
    if (namingStrategy_ == QStringLiteral("sequence")) {
        return QStringLiteral("%1-%2").arg(namingValue_.trimmed()).arg(fileIndex + 1, 3, 10, QLatin1Char('0'));
    }
    const auto* first = picker_.find(group.nodeIds.constFirst());
    if (!first) return tr("Unnamed component");
    // Copies usually carry the part's own name. When they are named one by one instead
    // ("Bolt:1", "Bolt:2"), the file is named after the part they are all copies of.
    QString name = first->name;
    if (group.nodeIds.size() > 1) {
        const bool sameName = std::ranges::all_of(group.nodeIds, [this, first](const QString& nodeId) {
            const auto* component = picker_.find(nodeId);
            return component && component->name == first->name;
        });
        const auto definitionName = picker_.definitionName(first->definitionId);
        if (!sameName && !definitionName.isEmpty()) name = definitionName;
    }
    if (namingStrategy_ == QStringLiteral("prefix")) return namingValue_ + name;
    return name;
}

// Gives every file its name. Typed names are kept as they are. Generated names that would
// write to the same file are told apart with "-2", "-3", numbered in assembly tree order
// rather than bucket order, so a part gets the same name however it was checked and on every
// run of the same file.
void ExportWorkspaceController::assignLeafNames(QVector<OutputGroup>& groups) const
{
    std::vector<int> treeOrder;
    treeOrder.reserve(static_cast<std::size_t>(groups.size()));
    for (int fileIndex = 0; fileIndex < groups.size(); ++fileIndex) {
        auto& group = groups[fileIndex];
        QString overridden;
        for (const auto& nodeId : group.nodeIds) {
            if (const auto found = filenameOverrides_.constFind(nodeId); found != filenameOverrides_.cend()) {
                overridden = found.value();
                break;
            }
        }
        group.filenameOverridden = !overridden.isEmpty();
        group.leafName = group.filenameOverridden ? overridden : baseName(group, fileIndex);
        treeOrder.push_back(fileIndex);
    }
    std::ranges::stable_sort(treeOrder, {}, [this, &groups](const int fileIndex) {
        return picker_.indexOf(groups.at(fileIndex).nodeIds.constFirst());
    });
    std::vector<loupe::exporting::detail::LeafNameRequest> requests;
    requests.reserve(treeOrder.size());
    for (const auto fileIndex : treeOrder) {
        const auto& group = groups.at(fileIndex);
        requests.push_back({group.leafName.toUtf8().toStdString(), group.filenameOverridden});
    }
    const auto names = loupe::exporting::detail::uniqueLeafNames(requests);
    for (std::size_t position = 0; position < treeOrder.size(); ++position) {
        groups[treeOrder[position]].leafName = QString::fromUtf8(names[position]);
    }
}

void ExportWorkspaceController::refreshPlan()
{
    clearExportResult();
    planRows_.clear();
    planFingerprint_.clear();
    planError_.clear();
    outputs_ = groupedOutputs();
    if (outputs_.isEmpty()) {
        emit planChanged();
        return;
    }
    if ((namingStrategy_ == QStringLiteral("sequence") || namingStrategy_ == QStringLiteral("prefix"))
        && namingValue_.trimmed().isEmpty()) {
        planError_ = namingStrategy_ == QStringLiteral("sequence")
            ? tr("Enter a base name for sequential files") : tr("Enter a filename prefix");
    }
    assignLeafNames(outputs_);

    loupe::exporting::PlanRequest request;
    request.destination = destination_.trimmed().isEmpty()
        ? QStringLiteral(".").toStdString() : destination_.trimmed().toUtf8().toStdString();
    request.format = format_ == QStringLiteral("STL") ? loupe::exporting::Format::Stl : loupe::exporting::Format::Step;
    request.coordinates = coordinates_ == QStringLiteral("Local") ? loupe::exporting::Coordinates::Local : loupe::exporting::Coordinates::Assembly;
    request.grouping = loupe::exporting::Grouping::SeparateFiles;
    request.stepOutputUnit = loupe::exporting::StepOutputUnit::Millimeter;
    request.requestedUnitToMillimeters = 1.0;
    request.unitDecision = {picker_.effectiveUnit() == QStringLiteral("in") ? loupe::units::LengthUnit::Inch : loupe::units::LengthUnit::Millimeter,
                            loupe::units::UnitConfidence::Confirmed, picker_.sourceToMillimeters(), "reviewed in Loupe"};
    for (const auto& group : outputs_) {
        const auto& nodeId = group.nodeIds.constFirst();
        const auto* component = picker_.find(nodeId);
        const auto id = nodeId.toUtf8().toStdString();
        request.selections.push_back({id, selectionKindFor(component->kind)});
        request.hierarchyPaths.emplace(id, component->hierarchyPath.toUtf8().toStdString());
        request.outputLeafNames.emplace(id, group.leafName.toUtf8().toStdString());
    }
    const auto extension = format_ == QStringLiteral("STL") ? QStringLiteral(".stl") : QStringLiteral(".step");
    const auto rowFor = [this](const OutputGroup& group, const QString& filename, const QString& path,
                               const QString& status, const QString& error) {
        const auto& nodeId = group.nodeIds.constFirst();
        return QVariantMap{{QStringLiteral("nodeId"), nodeId},
                           {QStringLiteral("name"), picker_.find(nodeId)->name},
                           {QStringLiteral("filename"), filename},
                           {QStringLiteral("filenameOverridden"), group.filenameOverridden},
                           {QStringLiteral("path"), path},
                           {QStringLiteral("format"), format_},
                           // Checked copies of this component that the one file stands for.
                           {QStringLiteral("copyCount"), static_cast<int>(group.nodeIds.size())},
                           {QStringLiteral("status"), status},
                           {QStringLiteral("error"), error}};
    };
    try {
        const auto plan = loupe::exporting::buildPlan(request);
        QHash<QString, QString> pathByNode;
        for (const auto& output : plan.outputs()) {
            pathByNode.insert(QString::fromStdString(output.nodeId()), QString::fromStdString(output.finalPath()));
        }
        for (const auto& group : outputs_) {
            const auto path = pathByNode.value(group.nodeIds.constFirst());
            planRows_.append(rowFor(group, QFileInfo(path).fileName(), path,
                                    destination_.trimmed().isEmpty() ? tr("Choose destination folder") : tr("Ready"),
                                    QString{}));
        }
        planFingerprint_ = QString::fromStdString(plan.fingerprint());
    } catch (const loupe::exporting::PlanError& error) {
        if (planError_.isEmpty()) {
            // Generated names never collide any more; only two names the user typed can.
            planError_ = error.code() == loupe::exporting::PlanError::Code::OutputPathCollision
                ? tr("Two file names you typed are the same. Change one of them.")
                : QString::fromUtf8(error.what());
        }
        for (const auto& group : outputs_) {
            const auto leaf = group.leafName + extension;
            planRows_.append(rowFor(group, leaf, leaf, tr("Needs attention"), planError_));
        }
    }
    emit planChanged();
}

bool ExportWorkspaceController::rebuildPlan()
{
    refreshPlan();
    return canExport();
}

QByteArray ExportWorkspaceController::reviewedPlanJson() const
{
    QJsonArray selections;
    for (const auto& group : outputs_) {
        const auto& nodeId = group.nodeIds.constFirst();
        const auto* component = picker_.find(nodeId);
        if (!component) continue;
        selections.append(QJsonObject{
            {QStringLiteral("nodeId"), nodeId},
            {QStringLiteral("kind"), static_cast<int>(selectionKindFor(component->kind))},
            {QStringLiteral("hierarchyPath"), component->hierarchyPath},
            {QStringLiteral("leafName"), group.leafName},
        });
    }
    return QJsonDocument(QJsonObject{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("destination"), destination_.trimmed()},
        {QStringLiteral("format"), format_},
        {QStringLiteral("coordinates"), coordinates_},
        {QStringLiteral("effectiveUnit"), picker_.effectiveUnit()},
        {QStringLiteral("sourceToMillimeters"), picker_.sourceToMillimeters()},
        {QStringLiteral("selections"), selections},
    }).toJson(QJsonDocument::Compact);
}

bool ExportWorkspaceController::exportReviewedPlan()
{
    refreshPlan();
    if (!canExport()) return false;
    // The suggested folder is made on the first export, never just for opening a file.
    if (destinationIsDefault_ && !QDir(destination_).exists() && !QDir().mkpath(destination_)) {
        planError_ = tr("Could not create %1. Choose another folder.").arg(QDir::toNativeSeparators(destination_));
        emit planChanged();
        return false;
    }
    exporting_ = true;
    exportProgress_ = 0.0;
    exportStage_ = tr("Preparing reviewed export");
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

void ExportWorkspaceController::setExportRequestId(const std::uint64_t requestId)
{
    if (exporting_) exportRequestId_ = requestId;
}

void ExportWorkspaceController::handleExportProgress(const int rowIndex, const int rowCount,
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

void ExportWorkspaceController::handleExportRowResult(const int rowIndex, const QString& nodeId,
                                                      const QString& path, const bool passed,
                                                      const QString& message)
{
    if (!exporting_ || rowIndex < 0 || rowIndex >= planRows_.size()) return;
    auto row = planRows_.at(rowIndex).toMap();
    if (row.value(QStringLiteral("nodeId")).toString() != nodeId
        || row.value(QStringLiteral("path")).toString() != path) {
        handleExportFailed(tr("Worker result did not match the reviewed output row"));
        return;
    }
    row.insert(QStringLiteral("status"), passed ? tr("Exported and validated") : tr("Failed"));
    row.insert(QStringLiteral("error"), passed ? QString{} : message);
    planRows_[rowIndex] = row;
    emit planChanged();
}

void ExportWorkspaceController::handleExportCompleted(const int succeededCount, const int failedCount)
{
    if (!exporting_) return;
    exporting_ = false;
    exportRequestId_ = 0;
    exportProgress_ = 1.0;
    exportStage_ = tr("Export complete");
    exportSucceeded_ = failedCount == 0 && succeededCount == planRows_.size();
    exportSummary_ = failedCount == 0
        ? tr("%1 files exported and validated").arg(succeededCount)
        : tr("%1 exported, %2 failed").arg(succeededCount).arg(failedCount);
    emit planChanged();
    emit exportStateChanged();
}

void ExportWorkspaceController::handleExportFailed(const QString& message)
{
    if (!exporting_) return;
    exporting_ = false;
    exportRequestId_ = 0;
    exportStage_ = tr("Export stopped");
    exportSucceeded_ = false;
    exportSummary_ = message;
    emit planChanged();
    emit exportStateChanged();
}

void ExportWorkspaceController::handleExportCanceled()
{
    handleExportFailed(tr("Export canceled"));
}

void ExportWorkspaceController::cancelExport()
{
    if (exporting_ && exportRequestId_ != 0) emit cancelRequested(exportRequestId_);
}

void ExportWorkspaceController::clearExportResult()
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

void ExportWorkspaceController::clearPlan()
{
    if (planRows_.isEmpty() && planFingerprint_.isEmpty() && planError_.isEmpty() && outputs_.isEmpty()) return;
    outputs_.clear();
    planRows_.clear();
    planFingerprint_.clear();
    planError_.clear();
    emit planChanged();
}

} // namespace loupe::app::exporting
