#include <QtTest/QTest>

#include "app/models/AssemblyTreeModel.h"
#include "app/models/SelectionModel.h"

class AssemblyTreeModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void repeatedDefinitionRoleIsExposed();
    void selectionIsTransientAndStartsEmpty();
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

QTEST_MAIN(AssemblyTreeModelTest)

#include "test_assembly_tree_model.moc"
