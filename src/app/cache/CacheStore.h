#pragma once

#include "app/cache/CacheKey.h"

#include <QString>

namespace loupe::app::cache {

class CacheStore final {
public:
    CacheStore(QString rootDirectory, qint64 byteBudget);
    ~CacheStore();

    [[nodiscard]] bool put(const CacheKey& key, const QByteArray& bytes);
    [[nodiscard]] bool contains(const CacheKey& key) const;

private:
    void evictOverBudget() const;
    QString rootDirectory_;
    QString connectionName_;
    qint64 byteBudget_{};
};

} // namespace loupe::app::cache
