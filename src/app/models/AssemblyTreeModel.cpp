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
        rebuildIndex();
        initialized_ = true;
        endResetModel();
        return;
    }
    emit layoutAboutToBeChanged();
    nodes_ = nodes;
    rebuildIndex();
    emit layoutChanged();
}

// Children, rows and positions looked up once per snapshot. Scanning every node for each
// rowCount/index call, as this used to, made the tree's cost grow with the square of the
// part count, and search with it.
void AssemblyTreeModel::rebuildIndex()
{
    childrenByParent_.clear();
    nodeIndexById_.clear();
    rowByNode_.assign(nodes_.size(), -1);
    for (int index = 0; index < static_cast<int>(nodes_.size()); ++index)
        nodeIndexById_.insert(nodes_.at(static_cast<std::size_t>(index)).stableId, index);
    for (int index = 0; index < static_cast<int>(nodes_.size()); ++index) {
        auto& siblings = childrenByParent_[nodes_.at(static_cast<std::size_t>(index)).parentStableId];
        rowByNode_[static_cast<std::size_t>(index)] = static_cast<int>(siblings.size());
        siblings.push_back(index);
    }
}

QModelIndex AssemblyTreeModel::indexForStableId(const QString& stableId) const
{
    const auto it = nodeIndexById_.constFind(stableId);
    if (it == nodeIndexById_.cend()) {
        return {};
    }
    return createIndex(rowForNode(it.value()), 0, it.value());
}

QStringList AssemblyTreeModel::findNodes(const QString& text) const
{
    const auto needle = text.trimmed();
    QStringList matches;
    if (needle.isEmpty()) return matches;
    // Depth-first from the roots: the snapshot's own order need not be the tree's.
    std::vector<int> pending = childrenOf(-1);
    std::reverse(pending.begin(), pending.end());
    while (!pending.empty()) {
        const auto nodeIndex = pending.back();
        pending.pop_back();
        const auto& node = nodes_.at(static_cast<std::size_t>(nodeIndex));
        if (node.name.contains(needle, Qt::CaseInsensitive)) matches.append(node.stableId);
        const auto& children = childrenOf(nodeIndex);
        pending.insert(pending.end(), children.rbegin(), children.rend());
    }
    return matches;
}

QModelIndex AssemblyTreeModel::index(const int row, const int column, const QModelIndex& parent) const
{
    if (column != 0 || row < 0) {
        return {};
    }
    const auto& children = childrenOf(parent.isValid() ? static_cast<int>(parent.internalId()) : -1);
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
    const auto it = nodeIndexById_.constFind(node.parentStableId);
    if (node.parentStableId.isEmpty() || it == nodeIndexById_.cend()) {
        return {};
    }
    return createIndex(rowForNode(it.value()), 0, it.value());
}

int AssemblyTreeModel::rowCount(const QModelIndex& parent) const
{
    return static_cast<int>(childrenOf(parent.isValid() ? static_cast<int>(parent.internalId()) : -1).size());
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
    return rowByNode_.at(static_cast<std::size_t>(nodeIndex));
}

const std::vector<int>& AssemblyTreeModel::childrenOf(const int parentIndex) const
{
    static const std::vector<int> none;
    const QString parentId = parentIndex < 0 ? QString{} : nodes_.at(static_cast<std::size_t>(parentIndex)).stableId;
    const auto it = childrenByParent_.constFind(parentId);
    return it == childrenByParent_.cend() ? none : it.value();
}

} // namespace loupe::app::models
