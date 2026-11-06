#include "app/models/SavedViews.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>

#include <algorithm>

namespace loupe::app::models {
namespace {

QString thumbnailDirectory()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("view-thumbnails"));
}

} // namespace

SavedViews::SavedViews(QObject* parent)
    : QObject(parent)
{
}

QString SavedViews::storageKeyFor(const QString& documentPath)
{
    const auto hash = QCryptographicHash::hash(QDir::cleanPath(documentPath).toUtf8(), QCryptographicHash::Sha1).toHex();
    return QStringLiteral("savedViews/%1").arg(QString::fromLatin1(hash));
}

void SavedViews::setDocument(const QString& documentPath)
{
    if (documentPath == documentPath_) return;
    documentPath_ = documentPath;
    entries_.clear();
    if (!documentPath_.isEmpty()) {
        const auto stored = QSettings().value(storageKeyFor(documentPath_)).toString().toUtf8();
        for (const auto& value : QJsonDocument::fromJson(stored).array()) {
            const auto object = value.toObject();
            Entry entry;
            entry.id = object.value(QStringLiteral("id")).toString();
            entry.kind = object.value(QStringLiteral("kind")).toString();
            entry.name = object.value(QStringLiteral("name")).toString();
            entry.summary = object.value(QStringLiteral("summary")).toString();
            entry.created = static_cast<qint64>(object.value(QStringLiteral("created")).toDouble());
            entry.data = object.value(QStringLiteral("data")).toObject().toVariantMap();
            if (!entry.id.isEmpty() && (entry.kind == QStringLiteral("display") || entry.kind == QStringLiteral("camera")
                                        || entry.kind == QStringLiteral("layout")))
                entries_.append(entry);
        }
    }
    emit changed();
}

QString SavedViews::add(const QString& kind, const QString& name, const QString& summary, const QVariantMap& data)
{
    Entry entry;
    entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.kind = kind;
    entry.name = name.trimmed().isEmpty() ? nextName(kind) : name.trimmed();
    entry.summary = summary;
    entry.created = QDateTime::currentMSecsSinceEpoch();
    entry.data = data;
    entries_.prepend(entry);
    persist();
    emit changed();
    return entry.id;
}

QVariantMap SavedViews::data(const QString& id) const
{
    for (const auto& entry : entries_)
        if (entry.id == id) return entry.data;
    return {};
}

SavedViews::Entry* SavedViews::find(const QString& id)
{
    for (auto& entry : entries_)
        if (entry.id == id) return &entry;
    return nullptr;
}

void SavedViews::rename(const QString& id, const QString& name)
{
    auto* entry = find(id);
    const auto trimmed = name.trimmed();
    if (!entry || trimmed.isEmpty() || entry->name == trimmed) return;
    entry->name = trimmed;
    persist();
    emit changed();
}

bool SavedViews::replace(const QString& id, const QString& summary, const QVariantMap& data)
{
    auto* entry = find(id);
    if (!entry || data.isEmpty()) return false;
    entry->summary = summary;
    entry->data = data;
    persist();
    emit changed();
    return true;
}

void SavedViews::remove(const QString& id)
{
    for (qsizetype index = 0; index < entries_.size(); ++index) {
        if (entries_.at(index).id != id) continue;
        entries_.removeAt(index);
        QFile::remove(thumbnailPath(id));
        persist();
        emit changed();
        return;
    }
}

QString SavedViews::nextName(const QString& kind) const
{
    const auto stem = kind == QStringLiteral("camera") ? tr("View %1")
                    : kind == QStringLiteral("layout") ? tr("Layout %1") : tr("Display %1");
    for (int number = 1;; ++number) {
        const auto candidate = stem.arg(number);
        const bool taken = std::any_of(entries_.cbegin(), entries_.cend(),
                                       [&](const Entry& entry) { return entry.kind == kind && entry.name == candidate; });
        if (!taken) return candidate;
    }
}

QString SavedViews::thumbnailPath(const QString& id) const
{
    QDir().mkpath(thumbnailDirectory());
    return QDir(thumbnailDirectory()).filePath(id + QStringLiteral(".png"));
}

void SavedViews::thumbnailSaved(const QString& id)
{
    auto* entry = find(id);
    if (!entry) return;
    ++entry->thumbnailRevision;
    emit changed();
}

QVariantList SavedViews::entriesOf(const QString& kind) const
{
    QVariantList list;
    for (const auto& entry : entries_) {
        if (entry.kind != kind) continue;
        const auto path = QDir(thumbnailDirectory()).filePath(entry.id + QStringLiteral(".png"));
        // The revision in the URL makes Image load a thumbnail saved again under one name.
        auto thumbnail = QFile::exists(path) ? QUrl::fromLocalFile(path) : QUrl();
        if (!thumbnail.isEmpty()) thumbnail.setQuery(QStringLiteral("r=%1").arg(entry.thumbnailRevision));
        list.append(QVariantMap{{QStringLiteral("id"), entry.id},
                                {QStringLiteral("name"), entry.name},
                                {QStringLiteral("summary"), entry.summary},
                                {QStringLiteral("created"), entry.created},
                                {QStringLiteral("thumbnail"), thumbnail}});
    }
    return list;
}

void SavedViews::persist() const
{
    if (documentPath_.isEmpty()) return;
    QJsonArray array;
    for (const auto& entry : entries_) {
        array.append(QJsonObject{{QStringLiteral("id"), entry.id},
                                 {QStringLiteral("kind"), entry.kind},
                                 {QStringLiteral("name"), entry.name},
                                 {QStringLiteral("summary"), entry.summary},
                                 {QStringLiteral("created"), static_cast<double>(entry.created)},
                                 {QStringLiteral("data"), QJsonObject::fromVariantMap(entry.data)}});
    }
    QSettings settings;
    if (array.isEmpty()) settings.remove(storageKeyFor(documentPath_));
    else settings.setValue(storageKeyFor(documentPath_), QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)));
}

} // namespace loupe::app::models
