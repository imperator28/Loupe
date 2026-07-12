#include <QtTest/QTest>

#include "app/models/VisibilityModel.h"

class VisibilityModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void isolateSavesAndRestoresPresentationSnapshot();
    void ghostComplementsKeepsTargetVisible();
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

QTEST_MAIN(VisibilityModelTest)

#include "test_visibility_model.moc"
