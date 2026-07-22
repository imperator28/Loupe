#include "app/export/ExportWorkspaceController.h"

#include "core/domain/AssemblyTypes.h"
#include "core/export/ExportPlan.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFileInfo>

#include <algorithm>

namespace loupe::app::exporting {
namespace {

QString kindLabel(const int kind)
{
    using loupe::domain::NodeKind;
    switch (static_cast<NodeKind>(kind)) {
    case NodeKind::Subassembly: return QObject::tr("Subassembly");
    case NodeKind::Occurrence: return QObject::tr("Component");
    case NodeKind::Body: return QObject::tr("Body");
    case NodeKind::Definition: return QObject::tr("Definition");
    case NodeKind::Root: return QObject::tr("Assembly");
    }
    return QObject::tr("Component");
}

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
    result.reserve(components_.size());
    for (const auto& component : components_) {
        if (!component.visibleInPicker) continue;
        result.append(QVariantMap{{QStringLiteral("nodeId"), component.id},
                                  {QStringLiteral("parentId"), component.parentId},
                                  {QStringLiteral("name"), component.name},
                                  {QStringLiteral("path"), component.hierarchyPath},
                                  {QStringLiteral("kind"), kindLabel(component.kind)},
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
    const auto document = QJsonDocument::fromJson(snapshotJson.toUtf8());
    if (!document.isObject()) {
        reset();
        return;
    }
    const auto root = document.object();
    effectiveUnit_ = root.value(QStringLiteral("effectiveUnit")).toString(QStringLiteral("mm"));
    sourceToMillimeters_ = root.value(QStringLiteral("sourceToMillimeters")).toDouble(effectiveUnit_ == QStringLiteral("in") ? 25.4 : 1.0);
    components_.clear();
    componentIndexById_.clear();
    checkedNodeIds_.clear();
    filenameOverrides_.clear();
    focusedNodeId_.clear();
    hoveredNodeId_.clear();

    const auto nodes = root.value(QStringLiteral("nodes")).toArray();
    for (const auto& value : nodes) {
        const auto node = value.toObject();
        if (node.value(QStringLiteral("kind")).toInt() == static_cast<int>(loupe::domain::NodeKind::Definition)) continue;
        Component component;
        component.id = node.value(QStringLiteral("id")).toString();
        component.parentId = node.value(QStringLiteral("parentId")).toString();
        component.name = node.value(QStringLiteral("name")).toString().trimmed();
        component.kind = node.value(QStringLiteral("kind")).toInt();
        component.exportable = component.kind != static_cast<int>(loupe::domain::NodeKind::Root);
        if (component.name.isEmpty()) component.name = tr("Unnamed component");
        if (component.id.isEmpty()) continue;
        componentIndexById_.insert(component.id, static_cast<int>(components_.size()));
        components_.append(std::move(component));
    }
    for (auto& component : components_) {
        QSet<QString> resolving;
        component.hierarchyPath = hierarchyPathFor(component.id, resolving);
        auto parentId = component.parentId;
        while (!parentId.isEmpty() && componentIndexById_.contains(parentId)) {
            ++component.depth;
            parentId = components_.at(componentIndexById_.value(parentId)).parentId;
        }
        const auto normalizedName = component.name.trimmed().toUpper();
        const auto parentIndex = componentIndexById_.value(component.parentId, -1);
        const bool rawBodyName = normalizedName == QStringLiteral("SOLID")
            || normalizedName == QStringLiteral("COMPOUND") || normalizedName == QStringLiteral("BODY");
        component.visibleInPicker = !(component.kind == static_cast<int>(loupe::domain::NodeKind::Body)
            && rawBodyName && parentIndex >= 0
            && components_.at(parentIndex).kind != static_cast<int>(loupe::domain::NodeKind::Root));
    }
    for (const auto& component : components_) {
        if (!component.visibleInPicker || component.parentId.isEmpty()) continue;
        const auto parentIndex = componentIndexById_.value(component.parentId, -1);
        if (parentIndex >= 0 && components_.at(parentIndex).visibleInPicker)
            components_[parentIndex].hasVisibleChildren = true;
    }
    clearPlan();
    ++selectionRevision_;
    emit componentsChanged();
    emit selectionChanged();
    emit previewChanged();
}

void ExportWorkspaceController::reset()
{
    components_.clear();
    componentIndexById_.clear();
    checkedNodeIds_.clear();
    filenameOverrides_.clear();
    focusedNodeId_.clear();
    hoveredNodeId_.clear();
    effectiveUnit_ = QStringLiteral("mm");
    sourceToMillimeters_ = 1.0;
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
    const auto index = componentIndexById_.value(nodeId, -1);
    if (index < 0 || !components_.at(index).exportable) return;
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
        const auto index = componentIndexById_.value(current, -1);
        if (index < 0) return false;
        current = components_.at(index).parentId;
    }
    return false;
}

void ExportWorkspaceController::setFocusedNodeId(const QString& nodeId)
{
    if (focusedNodeId_ == nodeId || (!nodeId.isEmpty() && !componentIndexById_.contains(nodeId))) return;
    focusedNodeId_ = nodeId;
    emit previewChanged();
}

void ExportWorkspaceController::setHoveredNodeId(const QString& nodeId)
{
    if (hoveredNodeId_ == nodeId || (!nodeId.isEmpty() && !componentIndexById_.contains(nodeId))) return;
    hoveredNodeId_ = nodeId;
    emit previewChanged();
}

void ExportWorkspaceController::setDestination(const QString& destination)
{
    if (exporting_) return;
    if (destination_ == destination) return;
    destination_ = destination;
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

void ExportWorkspaceController::moveBucketItem(const int from, const int to)
{
    if (exporting_) return;
    if (from < 0 || from >= checkedNodeIds_.size() || to < 0 || to >= checkedNodeIds_.size() || from == to) return;
    checkedNodeIds_.move(from, to);
    refreshPlan();
}

void ExportWorkspaceController::moveBucketItemById(const QString& nodeId, const int to)
{
    const auto from = static_cast<int>(checkedNodeIds_.indexOf(nodeId));
    moveBucketItem(from, to);
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
    const auto pickerNodeId = pickerNodeForSceneNode(nodeId);
    setFocusedNodeId(pickerNodeId);
    return pickerNodeId;
}

bool ExportWorkspaceController::canExport() const noexcept
{
    return documentReady_ && !exporting_ && !checkedNodeIds_.isEmpty() && !destination_.trimmed().isEmpty()
        && planError_.isEmpty() && planRows_.size() == checkedNodeIds_.size();
}

QString ExportWorkspaceController::generatedLeafName(const QString& nodeId, const int bucketIndex) const
{
    const auto override = filenameOverrides_.constFind(nodeId);
    if (override != filenameOverrides_.cend()) return override.value();
    const auto componentIndex = componentIndexById_.value(nodeId, -1);
    if (componentIndex < 0) return tr("Unnamed component");
    const auto& component = components_.at(componentIndex);
    if (namingStrategy_ == QStringLiteral("sequence")) {
        return QStringLiteral("%1-%2").arg(namingValue_.trimmed()).arg(bucketIndex + 1, 3, 10, QLatin1Char('0'));
    }
    if (namingStrategy_ == QStringLiteral("prefix")) return namingValue_ + component.name;
    return component.name;
}

void ExportWorkspaceController::refreshPlan()
{
    clearExportResult();
    planRows_.clear();
    planFingerprint_.clear();
    planError_.clear();
    if (checkedNodeIds_.isEmpty()) {
        emit planChanged();
        return;
    }
    if ((namingStrategy_ == QStringLiteral("sequence") || namingStrategy_ == QStringLiteral("prefix"))
        && namingValue_.trimmed().isEmpty()) {
        planError_ = namingStrategy_ == QStringLiteral("sequence")
            ? tr("Enter a base name for sequential files") : tr("Enter a filename prefix");
    }

    loupe::exporting::PlanRequest request;
    request.destination = destination_.trimmed().isEmpty()
        ? QStringLiteral(".").toStdString() : destination_.trimmed().toUtf8().toStdString();
    request.format = format_ == QStringLiteral("STL") ? loupe::exporting::Format::Stl : loupe::exporting::Format::Step;
    request.coordinates = coordinates_ == QStringLiteral("Local") ? loupe::exporting::Coordinates::Local : loupe::exporting::Coordinates::Assembly;
    request.grouping = loupe::exporting::Grouping::SeparateFiles;
    request.stepOutputUnit = loupe::exporting::StepOutputUnit::Millimeter;
    request.requestedUnitToMillimeters = 1.0;
    request.unitDecision = {effectiveUnit_ == QStringLiteral("in") ? loupe::units::LengthUnit::Inch : loupe::units::LengthUnit::Millimeter,
                            loupe::units::UnitConfidence::Confirmed, sourceToMillimeters_, "reviewed in Loupe"};
    for (int bucketIndex = 0; bucketIndex < checkedNodeIds_.size(); ++bucketIndex) {
        const auto& nodeId = checkedNodeIds_.at(bucketIndex);
        const auto index = componentIndexById_.value(nodeId, -1);
        if (index < 0) continue;
        const auto& component = components_.at(index);
        request.selections.push_back({nodeId.toUtf8().toStdString(), selectionKindFor(component.kind)});
        request.hierarchyPaths.emplace(nodeId.toUtf8().toStdString(), component.hierarchyPath.toUtf8().toStdString());
        request.outputLeafNames.emplace(nodeId.toUtf8().toStdString(), generatedLeafName(nodeId, bucketIndex).toUtf8().toStdString());
    }
    try {
        const auto plan = loupe::exporting::buildPlan(request);
        QHash<QString, QString> pathByNode;
        for (const auto& output : plan.outputs()) {
            pathByNode.insert(QString::fromStdString(output.nodeId()), QString::fromStdString(output.finalPath()));
        }
        for (int bucketIndex = 0; bucketIndex < checkedNodeIds_.size(); ++bucketIndex) {
            const auto& nodeId = checkedNodeIds_.at(bucketIndex);
            const auto componentIndex = componentIndexById_.value(nodeId, -1);
            if (componentIndex < 0) continue;
            const auto& component = components_.at(componentIndex);
            const auto path = pathByNode.value(nodeId);
            planRows_.append(QVariantMap{{QStringLiteral("nodeId"), nodeId},
                                         {QStringLiteral("name"), component.name},
                                         {QStringLiteral("filename"), QFileInfo(path).fileName()},
                                         {QStringLiteral("filenameOverridden"), filenameOverrides_.contains(nodeId)},
                                         {QStringLiteral("path"), path},
                                         {QStringLiteral("format"), format_},
                                         {QStringLiteral("status"), destination_.trimmed().isEmpty()
                                             ? tr("Choose destination folder") : tr("Ready")},
                                         {QStringLiteral("error"), QString{}}});
        }
        planFingerprint_ = QString::fromStdString(plan.fingerprint());
    } catch (const loupe::exporting::PlanError& error) {
        if (planError_.isEmpty()) planError_ = QString::fromUtf8(error.what());
        for (int bucketIndex = 0; bucketIndex < checkedNodeIds_.size(); ++bucketIndex) {
            const auto& nodeId = checkedNodeIds_.at(bucketIndex);
            const auto componentIndex = componentIndexById_.value(nodeId, -1);
            if (componentIndex < 0) continue;
            const auto& component = components_.at(componentIndex);
            const auto leaf = generatedLeafName(nodeId, bucketIndex)
                + (format_ == QStringLiteral("STL") ? QStringLiteral(".stl") : QStringLiteral(".step"));
            planRows_.append(QVariantMap{{QStringLiteral("nodeId"), nodeId},
                                         {QStringLiteral("name"), component.name},
                                         {QStringLiteral("filename"), leaf},
                                         {QStringLiteral("filenameOverridden"), filenameOverrides_.contains(nodeId)},
                                         {QStringLiteral("path"), leaf},
                                         {QStringLiteral("format"), format_},
                                         {QStringLiteral("status"), tr("Needs attention")},
                                         {QStringLiteral("error"), planError_}});
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
    for (int bucketIndex = 0; bucketIndex < checkedNodeIds_.size(); ++bucketIndex) {
        const auto& nodeId = checkedNodeIds_.at(bucketIndex);
        const auto componentIndex = componentIndexById_.value(nodeId, -1);
        if (componentIndex < 0) continue;
        const auto& component = components_.at(componentIndex);
        selections.append(QJsonObject{
            {QStringLiteral("nodeId"), nodeId},
            {QStringLiteral("kind"), static_cast<int>(selectionKindFor(component.kind))},
            {QStringLiteral("hierarchyPath"), component.hierarchyPath},
            {QStringLiteral("leafName"), generatedLeafName(nodeId, bucketIndex)},
        });
    }
    return QJsonDocument(QJsonObject{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("destination"), destination_.trimmed()},
        {QStringLiteral("format"), format_},
        {QStringLiteral("coordinates"), coordinates_},
        {QStringLiteral("effectiveUnit"), effectiveUnit_},
        {QStringLiteral("sourceToMillimeters"), sourceToMillimeters_},
        {QStringLiteral("selections"), selections},
    }).toJson(QJsonDocument::Compact);
}

bool ExportWorkspaceController::exportReviewedPlan()
{
    refreshPlan();
    if (!canExport()) return false;
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

QString ExportWorkspaceController::hierarchyPathFor(const QString& nodeId, QSet<QString>& resolving) const
{
    const auto index = componentIndexById_.value(nodeId, -1);
    if (index < 0 || resolving.contains(nodeId)) return {};
    resolving.insert(nodeId);
    const auto& component = components_.at(index);
    const auto parentPath = hierarchyPathFor(component.parentId, resolving);
    resolving.remove(nodeId);
    return parentPath.isEmpty() ? component.name : parentPath + QLatin1Char('/') + component.name;
}

QString ExportWorkspaceController::pickerNodeForSceneNode(const QString& nodeId) const
{
    auto current = nodeId;
    QSet<QString> visited;
    while (!current.isEmpty() && !visited.contains(current)) {
        visited.insert(current);
        const auto index = componentIndexById_.value(current, -1);
        if (index < 0) return {};
        const auto& component = components_.at(index);
        if (component.visibleInPicker && component.exportable) return component.id;
        current = component.parentId;
    }
    return {};
}

void ExportWorkspaceController::clearPlan()
{
    if (planRows_.isEmpty() && planFingerprint_.isEmpty() && planError_.isEmpty()) return;
    planRows_.clear();
    planFingerprint_.clear();
    planError_.clear();
    emit planChanged();
}

} // namespace loupe::app::exporting
