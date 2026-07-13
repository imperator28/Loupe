#include <QtTest/QTest>

#include "app/cache/OverrideStore.h"
#include "app/models/UnitReviewModel.h"

class OverrideStoreTest final : public QObject
{
    Q_OBJECT

private slots:
    void sourceChangeInvalidatesOverride();
    void proposalPreviewsBeforeApply();
};

void OverrideStoreTest::sourceChangeInvalidatesOverride()
{
    loupe::app::cache::OverrideStore store(QStringLiteral(":memory:"));
    store.put({QStringLiteral("hash-a"), 100, 10}, {QStringLiteral("in"), 1.0, QStringLiteral("reviewed")});

    QVERIFY(store.get({QStringLiteral("hash-a"), 100, 10}).has_value());
    QVERIFY(!store.get({QStringLiteral("hash-b"), 101, 11}).has_value());
}

void OverrideStoreTest::proposalPreviewsBeforeApply()
{
    loupe::app::models::UnitReviewModel model;
    model.setNormalizedExtents({4.0F, 2.0F, 1.0F});
    model.setProposedUnit(QStringLiteral("in"));

    QCOMPARE(model.effectiveUnit(), QStringLiteral("mm"));
    QCOMPARE(model.correctedExtents().x(), 101.6F);
    model.applyOverride(QStringLiteral("reviewed"));
    QCOMPARE(model.effectiveUnit(), QStringLiteral("in"));
}

QTEST_MAIN(OverrideStoreTest)

#include "test_override_store.moc"
