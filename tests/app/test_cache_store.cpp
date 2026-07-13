#include <QtTest/QTest>

#include <QTemporaryDir>

#include "app/cache/CacheKey.h"
#include "app/cache/CacheStore.h"

class CacheStoreTest final : public QObject
{
    Q_OBJECT

private slots:
    void effectiveUnitChangesKey();
    void evictsLeastRecentlyUsedOverBudget();
    void rejectsNetworkCacheRoots();
    void clearOnlyRemovesLoupeCacheEntries();
    void readsCachedSnapshotForProgressiveReopen();
    void readsOnlyTheExactSourceAndImportProfile();
};

void CacheStoreTest::effectiveUnitChangesKey()
{
    const auto mm = loupe::app::cache::CacheKey::from({QStringLiteral("hash"), 100, 10}, QStringLiteral("importer-1"), QStringLiteral("coarse"), {QStringLiteral("mm"), 1.0});
    const auto inch = loupe::app::cache::CacheKey::from({QStringLiteral("hash"), 100, 10}, QStringLiteral("importer-1"), QStringLiteral("coarse"), {QStringLiteral("in"), 25.4});

    QVERIFY(mm != inch);
}

void CacheStoreTest::evictsLeastRecentlyUsedOverBudget()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    loupe::app::cache::CacheStore store(directory.path(), 1024);
    const auto first = loupe::app::cache::CacheKey::from({QStringLiteral("a"), 1, 1}, QStringLiteral("importer"), QStringLiteral("coarse"), {QStringLiteral("mm"), 1.0});
    const auto second = loupe::app::cache::CacheKey::from({QStringLiteral("b"), 1, 1}, QStringLiteral("importer"), QStringLiteral("coarse"), {QStringLiteral("mm"), 1.0});

    QVERIFY(store.put(first, QByteArray(700, 'a')));
    QVERIFY(store.put(second, QByteArray(700, 'b')));
    QVERIFY(!store.contains(first));
    QVERIFY(store.contains(second));
}

void CacheStoreTest::rejectsNetworkCacheRoots()
{
    QVERIFY(!loupe::app::cache::CacheStore::isSafeLocalRoot(QStringLiteral("//server/share/loupe")));
    QVERIFY(!loupe::app::cache::CacheStore::isSafeLocalRoot(QStringLiteral("\\\\server\\share\\loupe")));
}

void CacheStoreTest::clearOnlyRemovesLoupeCacheEntries()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    loupe::app::cache::CacheStore store(directory.path(), 1024);
    const auto key = loupe::app::cache::CacheKey::from({QStringLiteral("a"), 1, 1}, QStringLiteral("importer"), QStringLiteral("coarse"), {QStringLiteral("mm"), 1.0});

    QVERIFY(store.put(key, QByteArray("cache")));
    store.clear();
    QVERIFY(!store.contains(key));
}

void CacheStoreTest::readsCachedSnapshotForProgressiveReopen()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    loupe::app::cache::CacheStore store(directory.path(), 1024);
    const auto key = loupe::app::cache::CacheKey::from({QStringLiteral("source"), 9, 3}, QStringLiteral("importer"), QStringLiteral("coarse"), {QStringLiteral("mm"), 1.0});

    QVERIFY(store.put(key, QByteArray("snapshot-first")));
    QCOMPARE(store.read(key), QByteArray("snapshot-first"));
}

void CacheStoreTest::readsOnlyTheExactSourceAndImportProfile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    loupe::app::cache::CacheStore store(directory.path(), 1024);
    const loupe::app::cache::SourceIdentity source{QStringLiteral("source-a"), 99, 77};
    const auto key = loupe::app::cache::CacheKey::from(source, QStringLiteral("importer-1"), QStringLiteral("snapshot"), {QStringLiteral("mm"), 1.0});
    loupe::app::cache::CacheMetadata metadata{source, QStringLiteral("importer-1"), QStringLiteral("snapshot"), {QStringLiteral("mm"), 1.0}};

    QVERIFY(store.put(key, QByteArray("cached-snapshot"), metadata));
    QCOMPARE(store.readSnapshotForSource(source, QStringLiteral("importer-1"), QStringLiteral("snapshot")), QByteArray("cached-snapshot"));
    QVERIFY(!store.readSnapshotForSource({QStringLiteral("source-b"), 99, 77}, QStringLiteral("importer-1"), QStringLiteral("snapshot")).has_value());
    QVERIFY(!store.readSnapshotForSource(source, QStringLiteral("importer-2"), QStringLiteral("snapshot")).has_value());
}

QTEST_MAIN(CacheStoreTest)

#include "test_cache_store.moc"
