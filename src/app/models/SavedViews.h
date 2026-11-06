#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

namespace loupe::app::models {

// The Inspect workspace's saved views (Views tool window), like Photoshop snapshots or a
// CAD tool's saved orientations: named display states (what is hidden or transparent),
// named camera views (where the camera is) and named exploded layouts (where each part
// has been moved and turned to). Kept per STEP file on this computer --
// in the app's settings, keyed by the file's path, with a thumbnail image each -- and
// never written into the file.
class SavedViews final : public QObject {
    Q_OBJECT
    // Each entry: id, name, summary (what it holds, in a few words), created (ms since
    // epoch), thumbnail (a file URL, or empty). Newest first.
    Q_PROPERTY(QVariantList displayStates READ displayStates NOTIFY changed)
    Q_PROPERTY(QVariantList cameraViews READ cameraViews NOTIFY changed)
    Q_PROPERTY(QVariantList explodeLayouts READ explodeLayouts NOTIFY changed)

public:
    explicit SavedViews(QObject* parent = nullptr);

    // The file whose views these are; empty for none. Loads that file's views.
    void setDocument(const QString& documentPath);

    // Store a new entry; returns its id. `data` is the state to restore.
    QString add(const QString& kind, const QString& name, const QString& summary, const QVariantMap& data);
    [[nodiscard]] QVariantMap data(const QString& id) const;
    [[nodiscard]] QVariantList displayStates() const { return entriesOf(QStringLiteral("display")); }
    [[nodiscard]] QVariantList cameraViews() const { return entriesOf(QStringLiteral("camera")); }
    [[nodiscard]] QVariantList explodeLayouts() const { return entriesOf(QStringLiteral("layout")); }
    // Save over an entry, keeping its name and place: what it holds now.
    bool replace(const QString& id, const QString& summary, const QVariantMap& data);

    Q_INVOKABLE void rename(const QString& id, const QString& name);
    Q_INVOKABLE void remove(const QString& id);
    // "Display 3", "View 2", "Layout 4": the next unused default name for a kind.
    Q_INVOKABLE [[nodiscard]] QString nextName(const QString& kind) const;
    // Where the viewport saves an entry's thumbnail, and to call once it has.
    Q_INVOKABLE [[nodiscard]] QString thumbnailPath(const QString& id) const;
    Q_INVOKABLE void thumbnailSaved(const QString& id);

    // The settings key a document's views are kept under.
    [[nodiscard]] static QString storageKeyFor(const QString& documentPath);

signals:
    void changed();

private:
    struct Entry {
        QString id;
        QString kind;
        QString name;
        QString summary;
        qint64 created{};
        int thumbnailRevision{};
        QVariantMap data;
    };

    [[nodiscard]] QVariantList entriesOf(const QString& kind) const;
    void persist() const;
    [[nodiscard]] Entry* find(const QString& id);

    QString documentPath_;
    QVector<Entry> entries_;
};

} // namespace loupe::app::models
