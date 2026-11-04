#include "app/models/OpenHistory.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

namespace loupe::app::models {

OpenHistory& OpenHistory::shared()
{
    // Owned by the application, so it goes with it rather than as a static after it: a
    // QObject destroyed once Qt has shut down crashed the process on exit on Windows.
    static OpenHistory* history = new OpenHistory(QCoreApplication::instance());
    return *history;
}

OpenHistory::OpenHistory(QObject* parent)
    : QObject(parent)
    , storagePath_(QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("open-history.json")))
{
    load();
}

void OpenHistory::setStoragePath(const QString& path)
{
    storagePath_ = path;
    load();
    emit changed();
}

void OpenHistory::load()
{
    records_.clear();
    QFile file(storagePath_);
    if (file.open(QIODevice::ReadOnly)) records_ = QJsonDocument::fromJson(file.readAll()).array().toVariantList();
}

void OpenHistory::persist() const
{
    QDir().mkpath(QFileInfo(storagePath_).absolutePath());
    QSaveFile file(storagePath_);
    if (!file.open(QIODevice::WriteOnly)) return;
    file.write(QJsonDocument(QJsonArray::fromVariantList(records_)).toJson(QJsonDocument::Indented));
    static_cast<void>(file.commit());
}

void OpenHistory::append(const QVariantMap& record)
{
    records_.prepend(record);
    while (records_.size() > kLimit) records_.removeLast();
    persist();
    emit changed();
}

void OpenHistory::clear()
{
    if (records_.isEmpty()) return;
    records_.clear();
    persist();
    emit changed();
}

QString OpenHistory::csv() const
{
    const auto seconds = [](const QVariant& value) {
        const auto ms = value.toLongLong();
        return ms > 0 ? QString::number(static_cast<double>(ms) / 1000.0, 'f', 2) : QString{};
    };
    const auto quoted = [](QString text) {
        if (text.contains(QLatin1Char(',')) || text.contains(QLatin1Char('"')) || text.contains(QLatin1Char('\n')))
            text = QLatin1Char('"') + text.replace(QStringLiteral("\""), QStringLiteral("\"\"")) + QLatin1Char('"');
        return text;
    };
    QStringList lines{QStringLiteral("opened_at,file,size_mb,parts,from_cache,ready_to_view_s,all_parts_shown_s,fully_detailed_s,loupe_version,path")};
    for (const auto& value : records_) {
        const auto record = value.toMap();
        lines << QStringList{
            record.value(QStringLiteral("openedAt")).toString(),
            quoted(record.value(QStringLiteral("file")).toString()),
            QString::number(record.value(QStringLiteral("sizeBytes")).toDouble() / (1024.0 * 1024.0), 'f', 2),
            QString::number(record.value(QStringLiteral("parts")).toInt()),
            record.value(QStringLiteral("cached")).toBool() ? QStringLiteral("yes") : QStringLiteral("no"),
            seconds(record.value(QStringLiteral("viewMs"))),
            seconds(record.value(QStringLiteral("previewMs"))),
            seconds(record.value(QStringLiteral("fullMs"))),
            record.value(QStringLiteral("version")).toString(),
            quoted(record.value(QStringLiteral("path")).toString())}.join(QLatin1Char(','));
    }
    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

void OpenHistory::copyCsvToClipboard() const
{
    if (auto* clipboard = QGuiApplication::clipboard()) clipboard->setText(csv());
}

} // namespace loupe::app::models
