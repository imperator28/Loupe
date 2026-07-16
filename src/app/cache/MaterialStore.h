#pragma once

#include <QString>
#include <QVector>

namespace loupe::app::cache {

struct StoredMaterial final {
    QString id;
    QString name;
    double densityGPerCm3{};
    QString color;
};

class MaterialStore final {
public:
    explicit MaterialStore(const QString& databasePath = defaultPath());
    ~MaterialStore();

    [[nodiscard]] static QString defaultPath();
    [[nodiscard]] QVector<StoredMaterial> list() const;
    [[nodiscard]] bool insert(const StoredMaterial& material);
    [[nodiscard]] bool update(const StoredMaterial& material);
    [[nodiscard]] bool remove(const QString& id);

private:
    QString connectionName_;
};

} // namespace loupe::app::cache
