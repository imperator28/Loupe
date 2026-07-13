#include "app/cache/CacheStore.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

#include <stdexcept>

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
    if (!isSafeLocalRoot(rootDirectory_)) throw std::invalid_argument("Cache root must be local application storage");
    QDir().mkpath(rootDirectory_);
    QDir().mkpath(QDir(rootDirectory_).filePath(QStringLiteral("entries")));
    auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    database.setDatabaseName(QDir(rootDirectory_).filePath(QStringLiteral("cache.sqlite")));
    if (!database.open()) throw std::runtime_error("Unable to open cache database");
    if (!initializeDatabase()) throw std::runtime_error("Unable to initialize cache database");
}

QString CacheStore::defaultRoot()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("cache"));
}

bool CacheStore::isSafeLocalRoot(const QString& rootDirectory)
{
    const auto normalized = QDir::fromNativeSeparators(rootDirectory).toLower();
    if (normalized.startsWith(QStringLiteral("//"))) return false;
    return !normalized.contains(QStringLiteral("/onedrive/"))
        && !normalized.contains(QStringLiteral("/dropbox/"))
        && !normalized.contains(QStringLiteral("/icloud drive/"));
}

bool CacheStore::initializeDatabase()
{
    QSqlQuery query(QSqlDatabase::database(connectionName_));
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next()) return false;
    if (query.value(0).toInt() != 1 && !query.exec(QStringLiteral("DROP TABLE IF EXISTS cache_entry"))) return false;
    return query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS cache_entry ("
                                     "cache_key TEXT PRIMARY KEY, bytes INTEGER NOT NULL, last_access INTEGER NOT NULL, path TEXT NOT NULL, "
                                     "source_hash TEXT NOT NULL DEFAULT '', source_size INTEGER NOT NULL DEFAULT 0, source_mtime INTEGER NOT NULL DEFAULT 0, "
                                     "importer_version TEXT NOT NULL DEFAULT '', mesh_profile TEXT NOT NULL DEFAULT '', effective_unit TEXT NOT NULL DEFAULT '', "
                                     "unit_factor REAL NOT NULL DEFAULT 1.0, schema_version INTEGER NOT NULL DEFAULT 1, snapshot_path TEXT NOT NULL DEFAULT '', mesh_path TEXT NOT NULL DEFAULT '')"))
        && query.exec(QStringLiteral("PRAGMA user_version = 1"));
}

CacheStore::~CacheStore()
{
    { auto database = QSqlDatabase::database(connectionName_); database.close(); }
    QSqlDatabase::removeDatabase(connectionName_);
}

bool CacheStore::put(const CacheKey& key, const QByteArray& bytes)
{
    return put(key, bytes, {});
}

bool CacheStore::put(const CacheKey& key, const QByteArray& bytes, const CacheMetadata& metadata)
{
    const auto path = QDir(rootDirectory_).filePath(QStringLiteral("entries/%1.bin").arg(key.value()));
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit()) return false;
    QSqlQuery query(QSqlDatabase::database(connectionName_));
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO cache_entry (cache_key, bytes, last_access, path, source_hash, source_size, source_mtime, importer_version, mesh_profile, effective_unit, unit_factor, schema_version, snapshot_path, mesh_path) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(key.value()); query.addBindValue(bytes.size()); query.addBindValue(QDateTime::currentMSecsSinceEpoch()); query.addBindValue(path);
    query.addBindValue(metadata.source.hash); query.addBindValue(metadata.source.size); query.addBindValue(metadata.source.mtime);
    query.addBindValue(metadata.importerVersion); query.addBindValue(metadata.meshProfile); query.addBindValue(metadata.effectiveUnit.unit); query.addBindValue(metadata.effectiveUnit.factor);
    query.addBindValue(metadata.schemaVersion); query.addBindValue(metadata.snapshotPath); query.addBindValue(metadata.meshPath);
    if (!query.exec()) return false;
    evictOverBudget();
    return true;
}

bool CacheStore::contains(const CacheKey& key) const
{
    QSqlQuery query(QSqlDatabase::database(connectionName_));
    query.prepare(QStringLiteral("SELECT path FROM cache_entry WHERE cache_key = ?")); query.addBindValue(key.value());
    if (!query.exec() || !query.next()) return false;
    if (!QFile::exists(query.value(0).toString())) {
        QSqlQuery erase(QSqlDatabase::database(connectionName_)); erase.prepare(QStringLiteral("DELETE FROM cache_entry WHERE cache_key = ?")); erase.addBindValue(key.value()); erase.exec();
        return false;
    }
    QSqlQuery update(QSqlDatabase::database(connectionName_)); update.prepare(QStringLiteral("UPDATE cache_entry SET last_access = ? WHERE cache_key = ?")); update.addBindValue(QDateTime::currentMSecsSinceEpoch()); update.addBindValue(key.value()); update.exec();
    return true;
}

std::optional<QByteArray> CacheStore::read(const CacheKey& key) const
{
    QSqlQuery query(QSqlDatabase::database(connectionName_));
    query.prepare(QStringLiteral("SELECT path FROM cache_entry WHERE cache_key = ?"));
    query.addBindValue(key.value());
    if (!query.exec() || !query.next()) return std::nullopt;
    QFile file(query.value(0).toString());
    if (!file.open(QIODevice::ReadOnly)) {
        QSqlQuery erase(QSqlDatabase::database(connectionName_)); erase.prepare(QStringLiteral("DELETE FROM cache_entry WHERE cache_key = ?")); erase.addBindValue(key.value()); erase.exec();
        return std::nullopt;
    }
    const auto bytes = file.readAll();
    QSqlQuery update(QSqlDatabase::database(connectionName_));
    update.prepare(QStringLiteral("UPDATE cache_entry SET last_access = ? WHERE cache_key = ?"));
    update.addBindValue(QDateTime::currentMSecsSinceEpoch()); update.addBindValue(key.value()); update.exec();
    return bytes;
}

std::optional<QByteArray> CacheStore::readSnapshotForSource(const SourceIdentity& source, const QString& importerVersion, const QString& meshProfile) const
{
    QSqlQuery query(QSqlDatabase::database(connectionName_));
    query.prepare(QStringLiteral("SELECT cache_key, path FROM cache_entry WHERE source_hash = ? AND source_size = ? AND source_mtime = ? AND importer_version = ? AND mesh_profile = ? ORDER BY last_access DESC LIMIT 1"));
    query.addBindValue(source.hash);
    query.addBindValue(source.size);
    query.addBindValue(source.mtime);
    query.addBindValue(importerVersion);
    query.addBindValue(meshProfile);
    if (!query.exec() || !query.next()) return std::nullopt;
    const auto cacheKey = query.value(0).toString();
    QFile file(query.value(1).toString());
    if (!file.open(QIODevice::ReadOnly)) {
        QSqlQuery erase(QSqlDatabase::database(connectionName_));
        erase.prepare(QStringLiteral("DELETE FROM cache_entry WHERE cache_key = ?"));
        erase.addBindValue(cacheKey);
        erase.exec();
        return std::nullopt;
    }
    const auto bytes = file.readAll();
    QSqlQuery update(QSqlDatabase::database(connectionName_));
    update.prepare(QStringLiteral("UPDATE cache_entry SET last_access = ? WHERE cache_key = ?"));
    update.addBindValue(QDateTime::currentMSecsSinceEpoch());
    update.addBindValue(cacheKey);
    update.exec();
    return bytes;
}

void CacheStore::clear()
{
    const auto entriesDirectory = QDir(rootDirectory_).filePath(QStringLiteral("entries"));
    QDir(entriesDirectory).removeRecursively();
    QDir().mkpath(entriesDirectory);
    QSqlQuery query(QSqlDatabase::database(connectionName_));
    query.exec(QStringLiteral("DELETE FROM cache_entry"));
}

void CacheStore::evictOverBudget() const
{
    QSqlQuery total(QSqlDatabase::database(connectionName_)); total.exec(QStringLiteral("SELECT COALESCE(SUM(bytes), 0) FROM cache_entry")); if (!total.next()) return;
    auto size = total.value(0).toLongLong();
    QSqlQuery entries(QSqlDatabase::database(connectionName_)); entries.exec(QStringLiteral("SELECT cache_key, bytes, path FROM cache_entry ORDER BY last_access ASC"));
    while (size > byteBudget_ && entries.next()) { QFile::remove(entries.value(2).toString()); QSqlQuery erase(QSqlDatabase::database(connectionName_)); erase.prepare(QStringLiteral("DELETE FROM cache_entry WHERE cache_key = ?")); erase.addBindValue(entries.value(0)); erase.exec(); size -= entries.value(1).toLongLong(); }
}

} // namespace loupe::app::cache
