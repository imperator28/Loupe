#include "app/cache/MaterialStore.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

#include <stdexcept>

namespace loupe::app::cache {

MaterialStore::MaterialStore(const QString& databasePath)
    : connectionName_(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    QDir().mkpath(QFileInfo(databasePath).dir().absolutePath());
    auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    database.setDatabaseName(databasePath);
    if (!database.open()) throw std::runtime_error("Unable to open material database");
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS custom_material ("
                                   "id TEXT PRIMARY KEY, name TEXT NOT NULL COLLATE NOCASE UNIQUE, density REAL NOT NULL, color TEXT NOT NULL)"))) {
        throw std::runtime_error("Unable to initialize material database");
    }
}

MaterialStore::~MaterialStore()
{
    { auto database = QSqlDatabase::database(connectionName_); database.close(); }
    QSqlDatabase::removeDatabase(connectionName_);
}

QString MaterialStore::defaultPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("materials.sqlite"));
}

QVector<StoredMaterial> MaterialStore::list() const
{
    QVector<StoredMaterial> materials;
    QSqlQuery query(QSqlDatabase::database(connectionName_));
    if (!query.exec(QStringLiteral("SELECT id, name, density, color FROM custom_material ORDER BY name COLLATE NOCASE"))) return materials;
    while (query.next()) {
        materials.append({query.value(0).toString(), query.value(1).toString(), query.value(2).toDouble(), query.value(3).toString()});
    }
    return materials;
}

bool MaterialStore::insert(const StoredMaterial& material)
{
    QSqlQuery query(QSqlDatabase::database(connectionName_));
    query.prepare(QStringLiteral("INSERT INTO custom_material (id, name, density, color) VALUES (?, ?, ?, ?)"));
    query.addBindValue(material.id); query.addBindValue(material.name); query.addBindValue(material.densityGPerCm3); query.addBindValue(material.color);
    return query.exec();
}

bool MaterialStore::update(const StoredMaterial& material)
{
    QSqlQuery query(QSqlDatabase::database(connectionName_));
    query.prepare(QStringLiteral("UPDATE custom_material SET name = ?, density = ?, color = ? WHERE id = ?"));
    query.addBindValue(material.name); query.addBindValue(material.densityGPerCm3); query.addBindValue(material.color); query.addBindValue(material.id);
    return query.exec() && query.numRowsAffected() == 1;
}

bool MaterialStore::remove(const QString& id)
{
    QSqlQuery query(QSqlDatabase::database(connectionName_));
    query.prepare(QStringLiteral("DELETE FROM custom_material WHERE id = ?"));
    query.addBindValue(id);
    return query.exec() && query.numRowsAffected() == 1;
}

} // namespace loupe::app::cache
