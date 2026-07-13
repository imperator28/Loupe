#pragma once

#include "app/cache/CacheKey.h"

#include <QString>

#include <optional>

namespace loupe::app::cache {

struct CacheMetadata final {
    SourceIdentity source;
    QString importerVersion;
    QString meshProfile;
    UnitDecision effectiveUnit;
    int schemaVersion{1};
    QString snapshotPath;
    QString meshPath;
};

class CacheStore final {
public:
    CacheStore(QString rootDirectory, qint64 byteBudget);
    ~CacheStore();

    [[nodiscard]] static QString defaultRoot();
    [[nodiscard]] static bool isSafeLocalRoot(const QString& rootDirectory);
    [[nodiscard]] bool put(const CacheKey& key, const QByteArray& bytes);
    [[nodiscard]] bool put(const CacheKey& key, const QByteArray& bytes, const CacheMetadata& metadata);
    [[nodiscard]] bool contains(const CacheKey& key) const;
    [[nodiscard]] std::optional<QByteArray> read(const CacheKey& key) const;
    void clear();

private:
    [[nodiscard]] bool initializeDatabase();
    void evictOverBudget() const;
    QString rootDirectory_;
    QString connectionName_;
    qint64 byteBudget_{};
};

} // namespace loupe::app::cache
