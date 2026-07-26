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

QTEST_MAIN(AssemblyTreeModelTest)

#include "test_assembly_tree_model.moc"
