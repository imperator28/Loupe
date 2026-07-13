#include "app/cache/OverrideStore.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>

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

} // namespace loupe::app::cache
