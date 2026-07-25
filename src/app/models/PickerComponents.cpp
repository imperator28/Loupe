#include "app/models/PickerComponents.h"

#include "core/domain/AssemblyTypes.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace loupe::app::models {

QString pickerKindLabel(const int kind)
{
    using loupe::domain::NodeKind;
    switch (static_cast<NodeKind>(kind)) {
    case NodeKind::Subassembly: return QCoreApplication::translate("PickerComponents", "Subassembly");
    case NodeKind::Occurrence: return QCoreApplication::translate("PickerComponents", "Component");
    case NodeKind::Body: return QCoreApplication::translate("PickerComponents", "Body");
    case NodeKind::Definition: return QCoreApplication::translate("PickerComponents", "Definition");
    case NodeKind::Root: return QCoreApplication::translate("PickerComponents", "Assembly");
    }
    return QCoreApplication::translate("PickerComponents", "Component");
}

void PickerComponents::clear()
{
    components_.clear();
    indexById_.clear();
    effectiveUnit_ = QStringLiteral("mm");
    sourceToMillimeters_ = 1.0;
}

bool PickerComponents::replaceSnapshot(const QString& snapshotJson)
{
    const auto document = QJsonDocument::fromJson(snapshotJson.toUtf8());
    if (!document.isObject()) {
        clear();
        return false;
    }
    const auto root = document.object();
    clear();
    effectiveUnit_ = root.value(QStringLiteral("effectiveUnit")).toString(QStringLiteral("mm"));
    sourceToMillimeters_ = root.value(QStringLiteral("sourceToMillimeters"))
                               .toDouble(effectiveUnit_ == QStringLiteral("in") ? 25.4 : 1.0);

    const auto nodes = root.value(QStringLiteral("nodes")).toArray();
    for (const auto& value : nodes) {
        const auto node = value.toObject();
        if (node.value(QStringLiteral("kind")).toInt()
            == static_cast<int>(loupe::domain::NodeKind::Definition)) {
            continue;
        }
        PickerComponent component;
        component.id = node.value(QStringLiteral("id")).toString();
        component.parentId = node.value(QStringLiteral("parentId")).toString();
        component.name = node.value(QStringLiteral("name")).toString().trimmed();
        component.kind = node.value(QStringLiteral("kind")).toInt();
        component.exportable = component.kind != static_cast<int>(loupe::domain::NodeKind::Root);
        if (component.name.isEmpty()) {
            component.name = QCoreApplication::translate("PickerComponents", "Unnamed component");
        }
        if (component.id.isEmpty()) continue;
        indexById_.insert(component.id, static_cast<int>(components_.size()));
        components_.append(std::move(component));
    }
    for (auto& component : components_) {
        QSet<QString> resolving;
        component.hierarchyPath = hierarchyPathFor(component.id, resolving);
        auto parentId = component.parentId;
        while (!parentId.isEmpty() && indexById_.contains(parentId)) {
            ++component.depth;
            parentId = components_.at(indexById_.value(parentId)).parentId;
        }
        const auto normalizedName = component.name.trimmed().toUpper();
        const auto parentIndex = indexById_.value(component.parentId, -1);
        const bool rawBodyName = normalizedName == QStringLiteral("SOLID")
            || normalizedName == QStringLiteral("COMPOUND") || normalizedName == QStringLiteral("BODY");
        component.visibleInPicker = !(component.kind == static_cast<int>(loupe::domain::NodeKind::Body)
            && rawBodyName && parentIndex >= 0
            && components_.at(parentIndex).kind != static_cast<int>(loupe::domain::NodeKind::Root));
    }
    for (const auto& component : components_) {
        if (!component.visibleInPicker || component.parentId.isEmpty()) continue;
        const auto parentIndex = indexById_.value(component.parentId, -1);
        if (parentIndex >= 0 && components_.at(parentIndex).visibleInPicker) {
            components_[parentIndex].hasVisibleChildren = true;
        }
    }
    return true;
}

const PickerComponent* PickerComponents::find(const QString& nodeId) const
{
    const auto index = indexById_.value(nodeId, -1);
    return index < 0 ? nullptr : &components_.at(index);
}

QString PickerComponents::hierarchyPathFor(const QString& nodeId, QSet<QString>& resolving) const
{
    const auto index = indexById_.value(nodeId, -1);
    if (index < 0 || resolving.contains(nodeId)) return {};
    resolving.insert(nodeId);
    const auto& component = components_.at(index);
    const auto parentPath = hierarchyPathFor(component.parentId, resolving);
    resolving.remove(nodeId);
    return parentPath.isEmpty() ? component.name : parentPath + QLatin1Char('/') + component.name;
}

QString PickerComponents::pickerNodeForSceneNode(const QString& nodeId) const
{
    auto current = nodeId;
    QSet<QString> visited;
    while (!current.isEmpty() && !visited.contains(current)) {
        visited.insert(current);
        const auto index = indexById_.value(current, -1);
        if (index < 0) return {};
        const auto& component = components_.at(index);
        if (component.visibleInPicker && component.exportable) return component.id;
        current = component.parentId;
    }
    return {};
}

} // namespace loupe::app::models
