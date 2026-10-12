#include <QtTest/QTest>

#include "app/models/VisibilityModel.h"

class VisibilityModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void isolateSavesAndRestoresPresentationSnapshot();
    void ghostComplementsKeepsTargetVisible();
    void transparencyIsRestorableAndShowAllClearsIt();
};

void VisibilityModelTest::isolateSavesAndRestoresPresentationSnapshot()
{
    loupe::app::models::VisibilityModel model;
    model.setNodes({QStringLiteral("occ-a"), QStringLiteral("occ-b")});
    model.hide(QStringLiteral("occ-b"));
    model.isolate(QStringLiteral("occ-a"));
    QVERIFY(model.isVisible(QStringLiteral("occ-a")));
    QVERIFY(!model.isVisible(QStringLiteral("occ-b")));

    model.restorePrevious();
    QVERIFY(!model.isVisible(QStringLiteral("occ-b")));
}

void VisibilityModelTest::ghostComplementsKeepsTargetVisible()
{
    loupe::app::models::VisibilityModel model;
    model.setNodes({QStringLiteral("occ-a"), QStringLiteral("occ-b")});
    model.ghostComplements(QStringLiteral("occ-a"));

    QVERIFY(!model.isGhosted(QStringLiteral("occ-a")));
    QVERIFY(model.isGhosted(QStringLiteral("occ-b")));
}

void VisibilityModelTest::transparencyIsRestorableAndShowAllClearsIt()
{
    loupe::app::models::VisibilityModel model;
    model.setNodes({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")});
    model.hide(QStringLiteral("c"));

    model.setGhosted({QStringLiteral("a"), QStringLiteral("c")}, true);
    QVERIFY(model.isGhosted(QStringLiteral("a")));
    QVERIFY(!model.isGhosted(QStringLiteral("b")));
    // Making a hidden part transparent means wanting to see it.
    QVERIFY(model.isVisible(QStringLiteral("c")));
    model.restorePrevious();
    QVERIFY(!model.isGhosted(QStringLiteral("a")));
    QVERIFY(!model.isVisible(QStringLiteral("c")));

    model.ghostComplements(QStringList{QStringLiteral("b")});
    QVERIFY(model.isGhosted(QStringLiteral("a")) && model.isGhosted(QStringLiteral("c")));
    QVERIFY(!model.isGhosted(QStringLiteral("b")));
    model.showAll();
    QVERIFY(!model.isGhosted(QStringLiteral("a")) && !model.isGhosted(QStringLiteral("c")));
}

QTEST_MAIN(VisibilityModelTest)

#include "test_visibility_model.moc"
