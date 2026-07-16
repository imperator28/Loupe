#pragma once

#include <QString>
#include <QVector>

#include <optional>

namespace loupe::app::cache {

struct SourceIdentity { QString hash; qint64 size{}; qint64 mtime{}; };
struct UnitOverride { QString unit; double customFactor{1.0}; QString reason; };
struct AppearanceOverride { QString targetId; QString scope; QString materialId; QString color; };

class OverrideStore final {
public:
    explicit OverrideStore(const QString& databasePath);
    ~OverrideStore();

    void put(const SourceIdentity& source, const UnitOverride& override);
    [[nodiscard]] std::optional<UnitOverride> get(const SourceIdentity& source) const;
    void putAppearance(const SourceIdentity& source, const AppearanceOverride& override);
    [[nodiscard]] QVector<AppearanceOverride> appearances(const SourceIdentity& source) const;
    void removeAppearance(const SourceIdentity& source, const QString& targetId, const QString& scope);

private:
    QString connectionName_;
};

} // namespace loupe::app::cache
