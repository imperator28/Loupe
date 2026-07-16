#include "app/models/AssemblyTreeModel.h"

#include <algorithm>

namespace loupe::app::models {

AssemblyTreeModel::AssemblyTreeModel(QObject* parent)
    : QAbstractItemModel(parent)
{
}

void AssemblyTreeModel::replaceSnapshot(const std::vector<SnapshotNode>& nodes)
{
    if (!initialized_) {
        beginResetModel();
        nodes_ = nodes;
        initialized_ = true;
        endResetModel();
        return;
    }
    emit layoutAboutToBeChanged();
    nodes_ = nodes;
    emit layoutChanged();
}

QModelIndex AssemblyTreeModel::indexForStableId(const QString& stableId) const
{
    const auto it = std::find_if(nodes_.begin(), nodes_.end(), [&stableId](const SnapshotNode& node) { return node.stableId == stableId; });
    if (it == nodes_.end()) {
        return {};
    }
    const auto nodeIndex = static_cast<int>(std::distance(nodes_.begin(), it));
    return createIndex(rowForNode(nodeIndex), 0, nodeIndex);
}

QModelIndex AssemblyTreeModel::index(const int row, const int column, const QModelIndex& parent) const
{
    if (column != 0 || row < 0) {
        return {};
    }
    const auto children = childrenOf(parent.isValid() ? parent.internalId() : -1);
    if (row >= static_cast<int>(children.size())) {
        return {};
    }
    return createIndex(row, column, children.at(static_cast<std::size_t>(row)));
}

QModelIndex AssemblyTreeModel::parent(const QModelIndex& child) const
{
    if (!child.isValid()) {
        return {};
    }
    const auto& node = nodes_.at(child.internalId());
    const auto it = std::find_if(nodes_.begin(), nodes_.end(), [&node](const SnapshotNode& candidate) { return candidate.stableId == node.parentStableId; });
    if (it == nodes_.end()) {
        return {};
    }
    const auto parentIndex = static_cast<int>(std::distance(nodes_.begin(), it));
    return createIndex(rowForNode(parentIndex), 0, parentIndex);
}

int AssemblyTreeModel::rowCount(const QModelIndex& parent) const
{
    return static_cast<int>(childrenOf(parent.isValid() ? parent.internalId() : -1).size());
}

int AssemblyTreeModel::columnCount(const QModelIndex&) const
{
    return 1;
}

QVariant AssemblyTreeModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid()) {
        return {};
    }
    const auto& node = nodes_.at(index.internalId());
    switch (role) {
    case Qt::DisplayRole:
        return node.definitionQuantity > 0 ? QStringLiteral("%1  %2x").arg(node.name).arg(node.definitionQuantity) : node.name;
    case NameRole:
        return node.name;
    case StableIdRole:
        return node.stableId;
    case KindRole:
        return node.kind;
    case DefinitionQuantityRole:
        return node.definitionQuantity;
    default:
        return {};
    }
}

QHash<int, QByteArray> AssemblyTreeModel::roleNames() const
{
    return {{Qt::DisplayRole, "display"}, {StableIdRole, "stableId"}, {KindRole, "kind"}, {NameRole, "name"}, {DefinitionQuantityRole, "definitionQuantity"}};
}

int AssemblyTreeModel::rowForNode(const int nodeIndex) const
{
    const auto& node = nodes_.at(static_cast<std::size_t>(nodeIndex));
    const auto siblings = childrenOf(node.parentStableId.isEmpty() ? -1 : [&] {
        const auto it = std::find_if(nodes_.begin(), nodes_.end(), [&node](const SnapshotNode& candidate) { return candidate.stableId == node.parentStableId; });
        return it == nodes_.end() ? -1 : static_cast<int>(std::distance(nodes_.begin(), it));
    }());
    const auto it = std::find(siblings.begin(), siblings.end(), nodeIndex);
    return it == siblings.end() ? -1 : static_cast<int>(std::distance(siblings.begin(), it));
}

std::vector<int> AssemblyTreeModel::childrenOf(const int parentIndex) const
{
    const QString parentId = parentIndex < 0 ? QString{} : nodes_.at(static_cast<std::size_t>(parentIndex)).stableId;
    std::vector<int> children;
    for (int index = 0; index < static_cast<int>(nodes_.size()); ++index) {
        if (nodes_.at(static_cast<std::size_t>(index)).parentStableId == parentId) {
            children.push_back(index);
        }
    }
    return children;
}

} // namespace loupe::app::models
