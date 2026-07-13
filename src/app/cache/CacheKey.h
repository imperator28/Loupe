#pragma once

#include "app/cache/OverrideStore.h"

#include <QString>

namespace loupe::app::cache {

struct UnitDecision final { QString unit; double factor{1.0}; };

class CacheKey final {
public:
    static CacheKey from(const SourceIdentity& source, const QString& importerVersion, const QString& meshProfile, const UnitDecision& unit);
    [[nodiscard]] QString value() const { return value_; }
    friend bool operator==(const CacheKey&, const CacheKey&) = default;

private:
    explicit CacheKey(QString value) : value_(std::move(value)) {}
    QString value_;
};

} // namespace loupe::app::cache
