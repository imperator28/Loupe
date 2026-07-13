#include "app/cache/CacheStore.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>

namespace loupe::app::cache {

CacheKey CacheKey::from(const SourceIdentity& source, const QString& importerVersion, const QString& meshProfile, const UnitDecision& unit)
{
    const auto input = QStringLiteral("%1|%2|%3|%4|%5|%6|%7")
        .arg(source.hash).arg(source.size).arg(source.mtime).arg(importerVersion, meshProfile, unit.unit).arg(unit.factor, 0, 'g', 17);
    return CacheKey(QString::fromLatin1(QCryptographicHash::hash(input.toUtf8(), QCryptographicHash::Sha256).toHex()));
}

CacheStore::CacheStore(QString rootDirectory, const qint64 byteBudget)
    : rootDirectory_(std::move(rootDirectory)), connectionName_(QUuid::createUuid().toString(QUuid::WithoutBraces)), byteBudget_(byteBudget)
{
    QDir().mkpath(rootDirectory_);
    QDir().mkpath(QDir(rootDirectory_).filePath(QStringLiteral("entries")));
    auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    database.setDatabaseName(QDir(rootDirectory_).filePath(QStringLiteral("cache.sqlite")));
    if (!database.open()) throw std::runtime_error("Unable to open cache database");
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS cache_entry (cache_key TEXT PRIMARY KEY, bytes INTEGER NOT NULL, last_access INTEGER NOT NULL, path TEXT NOT NULL)"))) {
        throw std::runtime_error("Unable to initialize cache database");
    }
}

CacheStore::~CacheStore()
{
    { auto database = QSqlDatabase::database(connectionName_); database.close(); }
    QSqlDatabase::removeDatabase(connectionName_);
}

bool CacheStore::put(const CacheKey& key, const QByteArray& bytes)
{
    const auto path = QDir(rootDirectory_).filePath(QStringLiteral("entries/%1.bin").arg(key.value()));
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit()) return false;
    QSqlQuery query(QSqlDatabase::database(connectionName_));
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO cache_entry (cache_key, bytes, last_access, path) VALUES (?, ?, ?, ?)"));
    query.addBindValue(key.value()); query.addBindValue(bytes.size()); query.addBindValue(QDateTime::currentMSecsSinceEpoch()); query.addBindValue(path);
    if (!query.exec()) return false;
    evictOverBudget();
    return true;
}

bool CacheStore::contains(const CacheKey& key) const
{
    QSqlQuery query(QSqlDatabase::database(connectionName_));
    query.prepare(QStringLiteral("SELECT path FROM cache_entry WHERE cache_key = ?")); query.addBindValue(key.value());
    if (!query.exec() || !query.next() || !QFile::exists(query.value(0).toString())) return false;
    QSqlQuery update(QSqlDatabase::database(connectionName_)); update.prepare(QStringLiteral("UPDATE cache_entry SET last_access = ? WHERE cache_key = ?")); update.addBindValue(QDateTime::currentMSecsSinceEpoch()); update.addBindValue(key.value()); update.exec();
    return true;
}

void CacheStore::evictOverBudget() const
{
    QSqlQuery total(QSqlDatabase::database(connectionName_)); total.exec(QStringLiteral("SELECT COALESCE(SUM(bytes), 0) FROM cache_entry")); if (!total.next()) return;
    auto size = total.value(0).toLongLong();
    QSqlQuery entries(QSqlDatabase::database(connectionName_)); entries.exec(QStringLiteral("SELECT cache_key, bytes, path FROM cache_entry ORDER BY last_access ASC"));
    while (size > byteBudget_ && entries.next()) { QFile::remove(entries.value(2).toString()); QSqlQuery erase(QSqlDatabase::database(connectionName_)); erase.prepare(QStringLiteral("DELETE FROM cache_entry WHERE cache_key = ?")); erase.addBindValue(entries.value(0)); erase.exec(); size -= entries.value(1).toLongLong(); }
}

} // namespace loupe::app::cache
