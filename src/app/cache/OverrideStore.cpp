#include "app/cache/OverrideStore.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>

#include <QVector>

namespace loupe::app::cache {

OverrideStore::OverrideStore(const QString& databasePath)
    : connectionName_(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    database.setDatabaseName(databasePath);
    if (!database.open()) {
        throw std::runtime_error("Unable to open override database");
    }
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS unit_override (source_hash TEXT NOT NULL, source_size INTEGER NOT NULL, source_mtime INTEGER NOT NULL, unit TEXT NOT NULL, custom_factor REAL NOT NULL, reason TEXT NOT NULL, PRIMARY KEY(source_hash, source_size, source_mtime))"))) {
        throw std::runtime_error("Unable to initialize override database");
    }
    if (!query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS appearance_override ("
                                   "source_hash TEXT NOT NULL, source_size INTEGER NOT NULL, source_mtime INTEGER NOT NULL, "
                                   "target_id TEXT NOT NULL, scope TEXT NOT NULL, material_id TEXT NOT NULL DEFAULT '', color TEXT NOT NULL DEFAULT '', "
                                   "PRIMARY KEY(source_hash, source_size, source_mtime, target_id, scope))"))) {
        throw std::runtime_error("Unable to initialize appearance database");
    }
}

OverrideStore::~OverrideStore()
{
    {
        auto database = QSqlDatabase::database(connectionName_);
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName_);
}

void OverrideStore::put(const SourceIdentity& source, const UnitOverride& override)
{
    QSqlQuery query(QSqlDatabase::database(connectionName_));
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO unit_override (source_hash, source_size, source_mtime, unit, custom_factor, reason) VALUES (?, ?, ?, ?, ?, ?)"));
    query.addBindValue(source.hash);
    query.addBindValue(source.size);
    query.addBindValue(source.mtime);
    query.addBindValue(override.unit);
    query.addBindValue(override.customFactor);
    query.addBindValue(override.reason);
    if (!query.exec()) {
        throw std::runtime_error("Unable to write override");
    }
}

std::optional<UnitOverride> OverrideStore::get(const SourceIdentity& source) const
{
    QSqlQuery query(QSqlDatabase::database(connectionName_));
    query.prepare(QStringLiteral("SELECT unit, custom_factor, reason FROM unit_override WHERE source_hash = ? AND source_size = ? AND source_mtime = ?"));
    query.addBindValue(source.hash);
    query.addBindValue(source.size);
    query.addBindValue(source.mtime);
    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }
    return UnitOverride{query.value(0).toString(), query.value(1).toDouble(), query.value(2).toString()};
}

void OverrideStore::putAppearance(const SourceIdentity& source, const AppearanceOverride& override)
{
    QSqlQuery query(QSqlDatabase::database(connectionName_));
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO appearance_override (source_hash, source_size, source_mtime, target_id, scope, material_id, color) VALUES (?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(source.hash); query.addBindValue(source.size); query.addBindValue(source.mtime); query.addBindValue(override.targetId);
    query.addBindValue(override.scope); query.addBindValue(override.materialId); query.addBindValue(override.color);
    if (!query.exec()) throw std::runtime_error("Unable to write appearance override");
}

QVector<AppearanceOverride> OverrideStore::appearances(const SourceIdentity& source) const
{
    QVector<AppearanceOverride> result;
    QSqlQuery query(QSqlDatabase::database(connectionName_));
    query.prepare(QStringLiteral("SELECT target_id, scope, material_id, color FROM appearance_override WHERE source_hash = ? AND source_size = ? AND source_mtime = ?"));
    query.addBindValue(source.hash); query.addBindValue(source.size); query.addBindValue(source.mtime);
    if (!query.exec()) return result;
    while (query.next()) result.append({query.value(0).toString(), query.value(1).toString(), query.value(2).toString(), query.value(3).toString()});
    return result;
}

void OverrideStore::removeAppearance(const SourceIdentity& source, const QString& targetId, const QString& scope)
{
    QSqlQuery query(QSqlDatabase::database(connectionName_));
    query.prepare(QStringLiteral("DELETE FROM appearance_override WHERE source_hash = ? AND source_size = ? AND source_mtime = ? AND target_id = ? AND scope = ?"));
    query.addBindValue(source.hash); query.addBindValue(source.size); query.addBindValue(source.mtime); query.addBindValue(targetId); query.addBindValue(scope);
    if (!query.exec()) throw std::runtime_error("Unable to remove appearance override");
}

} // namespace loupe::app::cache
