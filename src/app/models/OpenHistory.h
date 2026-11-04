#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace loupe::app::models {

// How long each file took to open, for every open on this computer: the record behind
// "Opening time" in Document properties and its history, which copies as CSV to set
// against another CAD program's times. One list for the app, newest first, the last 300.
//
// A record: file, path, sizeBytes, parts, cached (opened from Loupe's geometry cache),
// viewMs (the first geometry on screen: the model can be turned), previewMs (every part
// shown), fullMs (every part fully detailed), openedAt (ISO time), version.
class OpenHistory final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList records READ records NOTIFY changed)

public:
    // The one history every tab writes to.
    static OpenHistory& shared();

    [[nodiscard]] QVariantList records() const { return records_; }
    void append(const QVariantMap& record);
    Q_INVOKABLE [[nodiscard]] QString csv() const;
    Q_INVOKABLE void copyCsvToClipboard() const;
    Q_INVOKABLE void clear();

    // Where it is kept; tests point it elsewhere.
    void setStoragePath(const QString& path);
    static constexpr int kLimit = 300;

signals:
    void changed();

private:
    explicit OpenHistory(QObject* parent = nullptr);
    void load();
    void persist() const;

    QString storagePath_;
    QVariantList records_;
};

} // namespace loupe::app::models
