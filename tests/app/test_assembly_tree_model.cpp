#include <QtTest/QTest>

#include "app/models/AssemblyTreeModel.h"
#include "app/models/SelectionModel.h"

class AssemblyTreeModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void repeatedDefinitionRoleIsExposed();
    void selectionIsTransientAndStartsEmpty();
    void indexForStableIdIsReachableFromQml();
    void findNodesMatchesNamesInTreeOrder();
    void structureSurvivesALargeTree();
};

void AssemblyTreeModelTest::repeatedDefinitionRoleIsExposed()
{
    loupe::app::models::AssemblyTreeModel model;
    model.replaceSnapshot({
        {QStringLiteral("occ-root"), {}, QStringLiteral("assembly"), QStringLiteral("Housing"), 1},
        {QStringLiteral("occ-fastener-1"), QStringLiteral("occ-root"), QStringLiteral("part"), QStringLiteral("Fastener"), 12},
    });

    const auto fastener = model.indexForStableId(QStringLiteral("occ-fastener-1"));
    QVERIFY(fastener.isValid());
    QCOMPARE(model.data(fastener, loupe::app::models::AssemblyTreeModel::DefinitionQuantityRole).toInt(), 12);
}

void AssemblyTreeModelTest::selectionIsTransientAndStartsEmpty()
{
    loupe::app::models::SelectionModel selection;
    QVERIFY(selection.activeNodeId().isEmpty());

    selection.setActiveNodeId(QStringLiteral("occ-front-cover"));
    QCOMPARE(selection.activeNodeId(), QStringLiteral("occ-front-cover"));
}

void AssemblyTreeModelTest::indexForStableIdIsReachableFromQml()
{
    // The Inspect workspace calls this from QML to reveal the selected node. Without
    // Q_INVOKABLE the call fails as a caught QML TypeError -- no crash, no test failure,
    // just a feature that quietly does nothing. So this asserts reachability through the
    // meta-object, which is exactly what QML uses and what a missing Q_INVOKABLE breaks.
    loupe::app::models::AssemblyTreeModel model;
    model.replaceSnapshot({
        {QStringLiteral("occ-root"), {}, QStringLiteral("assembly"), QStringLiteral("Housing"), 1},
        {QStringLiteral("occ-cover"), QStringLiteral("occ-root"), QStringLiteral("part"), QStringLiteral("Cover"), 1},
    });

    const auto* metaObject = model.metaObject();
    const auto methodIndex = metaObject->indexOfMethod("indexForStableId(QString)");
    QVERIFY2(methodIndex >= 0, "indexForStableId must be Q_INVOKABLE for the QML tree reveal to work");

    QModelIndex resolved;
    QVERIFY(metaObject->method(methodIndex).invoke(&model, Qt::DirectConnection,
                                                   qReturnArg(resolved),
                                                   QStringLiteral("occ-cover")));
    QVERIFY(resolved.isValid());
    QCOMPARE(model.data(resolved, loupe::app::models::AssemblyTreeModel::StableIdRole).toString(),
             QStringLiteral("occ-cover"));

    // An unknown ID has to come back invalid rather than as row 0, because the QML guards on
    // `valid` and would otherwise reveal an unrelated node.
    QModelIndex missing;
    QVERIFY(metaObject->method(methodIndex).invoke(&model, Qt::DirectConnection,
                                                   qReturnArg(missing),
                                                   QStringLiteral("occ-nonexistent")));
    QVERIFY(!missing.isValid());
}

void AssemblyTreeModelTest::findNodesMatchesNamesInTreeOrder()
{
    // Listed out of tree order on purpose: the matches must follow the tree, which is what
    // stepping through them in the panel walks.
    loupe::app::models::AssemblyTreeModel model;
    model.replaceSnapshot({
        {QStringLiteral("r43"), QStringLiteral("board"), QStringLiteral("part"), QStringLiteral("R43"), 1},
        {QStringLiteral("root"), {}, QStringLiteral("assembly"), QStringLiteral("Drawer"), 1},
        {QStringLiteral("board"), QStringLiteral("root"), QStringLiteral("assembly"), QStringLiteral("PCB"), 1},
        {QStringLiteral("r7"), QStringLiteral("board"), QStringLiteral("part"), QStringLiteral("r7 resistor"), 1},
        {QStringLiteral("rail"), QStringLiteral("root"), QStringLiteral("part"), QStringLiteral("Rail"), 1},
    });
    QCOMPARE(model.findNodes(QStringLiteral("r")),
             (QStringList{QStringLiteral("root"), QStringLiteral("r43"), QStringLiteral("r7"), QStringLiteral("rail")}));
    QCOMPARE(model.findNodes(QStringLiteral("  RESIST ")), QStringList{QStringLiteral("r7")});
    QVERIFY(model.findNodes(QString{}).isEmpty());
    QVERIFY(model.findNodes(QStringLiteral("capacitor")).isEmpty());
    QVERIFY(model.metaObject()->indexOfMethod("findNodes(QString)") >= 0);
}

void AssemblyTreeModelTest::structureSurvivesALargeTree()
{
    // 40 boards of 100 parts: rows, parents and lookups must agree, and quickly -- the tree
    // used to scan every node for each row it asked about.
    std::vector<loupe::app::models::AssemblyTreeModel::SnapshotNode> nodes;
    nodes.push_back({QStringLiteral("root"), {}, QStringLiteral("assembly"), QStringLiteral("Root"), 1});
    for (int board = 0; board < 40; ++board) {
        const auto boardId = QStringLiteral("b%1").arg(board);
        nodes.push_back({boardId, QStringLiteral("root"), QStringLiteral("assembly"), boardId, 1});
        for (int part = 0; part < 100; ++part)
            nodes.push_back({QStringLiteral("b%1p%2").arg(board).arg(part), boardId, QStringLiteral("part"), QStringLiteral("C%1").arg(part), 1});
    }
    loupe::app::models::AssemblyTreeModel model;
    model.replaceSnapshot(nodes);
    QCOMPARE(model.rowCount(), 1);
    const auto root = model.index(0, 0);
    QCOMPARE(model.rowCount(root), 40);
    const auto board = model.index(17, 0, root);
    QCOMPARE(model.data(board, loupe::app::models::AssemblyTreeModel::StableIdRole).toString(), QStringLiteral("b17"));
    QCOMPARE(model.rowCount(board), 100);
    const auto part = model.indexForStableId(QStringLiteral("b17p63"));
    QCOMPARE(part.row(), 63);
    QCOMPARE(model.parent(part), board);
    QCOMPARE(model.parent(board), root);
    QVERIFY(!model.parent(root).isValid());
    QCOMPARE(model.findNodes(QStringLiteral("C63")).size(), 40);
}

QTEST_MAIN(AssemblyTreeModelTest)

#include "test_assembly_tree_model.moc"
