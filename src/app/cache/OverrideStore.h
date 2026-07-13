#pragma once

#include <QString>

#include <optional>

namespace loupe::app::cache {

struct SourceIdentity { QString hash; qint64 size{}; qint64 mtime{}; };
struct UnitOverride { QString unit; double customFactor{1.0}; QString reason; };

class OverrideStore final {
public:
    explicit OverrideStore(const QString& databasePath);
    ~OverrideStore();

    void put(const SourceIdentity& source, const UnitOverride& override);
    [[nodiscard]] std::optional<UnitOverride> get(const SourceIdentity& source) const;

private:
    QString connectionName_;
};

} // namespace loupe::app::cache
