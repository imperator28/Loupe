#include <QtTest/QTest>

#include <QTemporaryDir>

#include "app/cache/MaterialStore.h"

class MaterialStoreTest final : public QObject
{
    Q_OBJECT

private slots:
    void createsUpdatesListsAndRemovesCustomMaterials();
};

void MaterialStoreTest::createsUpdatesListsAndRemovesCustomMaterials()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto databasePath = directory.filePath(QStringLiteral("materials.sqlite"));

    loupe::app::cache::MaterialStore store(databasePath);
    QVERIFY(store.insert({QStringLiteral("custom-bronze"), QStringLiteral("Review bronze"), 8.73, QStringLiteral("#A76D41")}));
    QCOMPARE(store.list().size(), 1);
    QCOMPARE(store.list().first().name, QStringLiteral("Review bronze"));
    QVERIFY(!store.insert({QStringLiteral("custom-other"), QStringLiteral("review bronze"), 7.0, QStringLiteral("#111111")}));

    QVERIFY(store.update({QStringLiteral("custom-bronze"), QStringLiteral("Review brass"), 8.50, QStringLiteral("#C89B3C")}));
    QCOMPARE(store.list().first().name, QStringLiteral("Review brass"));
    QCOMPARE(store.list().first().densityGPerCm3, 8.50);
    QCOMPARE(store.list().first().color, QStringLiteral("#C89B3C"));

    QVERIFY(store.remove(QStringLiteral("custom-bronze")));
    QVERIFY(store.list().isEmpty());
}

QTEST_MAIN(MaterialStoreTest)

#include "test_material_store.moc"
