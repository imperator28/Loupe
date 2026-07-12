#pragma once

#include <QAbstractItemModel>
#include <QString>

#include <vector>

namespace loupe::app::models {

class AssemblyTreeModel final : public QAbstractItemModel {
    Q_OBJECT

public:
    struct SnapshotNode {
        QString stableId;
        QString parentStableId;
        QString kind;
        QString name;
        int definitionQuantity{};
    };

    enum Role {
        StableIdRole = Qt::UserRole + 1,
        KindRole,
        NameRole,
        DefinitionQuantityRole,
    };
    Q_ENUM(Role)

    explicit AssemblyTreeModel(QObject* parent = nullptr);

    void replaceSnapshot(const std::vector<SnapshotNode>& nodes);
    [[nodiscard]] QModelIndex indexForStableId(const QString& stableId) const;

    [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    [[nodiscard]] QModelIndex parent(const QModelIndex& child) const override;
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

private:
    [[nodiscard]] int rowForNode(int nodeIndex) const;
    [[nodiscard]] std::vector<int> childrenOf(int parentIndex) const;

    std::vector<SnapshotNode> nodes_;
    bool initialized_{false};
};

} // namespace loupe::app::models
