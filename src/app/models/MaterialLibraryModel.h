#pragma once

#include "core/inspection/MaterialProperties.h"
#include "app/cache/MaterialStore.h"

#include <QObject>
#include <QVariantList>

#include <optional>
#include <memory>

namespace loupe::app::models {

class MaterialLibraryModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList materials READ materials NOTIFY materialsChanged)

public:
    explicit MaterialLibraryModel(QObject* parent = nullptr);

    [[nodiscard]] QVariantList materials() const;
    [[nodiscard]] std::optional<inspection::Material> find(const QString& id) const;
    [[nodiscard]] QString colorFor(const QString& id) const;
    Q_INVOKABLE QString addMaterial(const QString& name, double densityGPerCm3, const QString& color);
    Q_INVOKABLE bool updateMaterial(const QString& id, const QString& name, double densityGPerCm3, const QString& color);
    Q_INVOKABLE bool removeMaterial(const QString& id);

signals:
    void materialsChanged();

private:
    struct Entry final { inspection::Material material; QString color; bool custom{}; };

    [[nodiscard]] std::optional<Entry> validatedEntry(const QString& id, const QString& name, double densityGPerCm3, const QString& color, bool custom) const;
    void loadCustomMaterials();

    QVector<Entry> entries_;
    std::unique_ptr<cache::MaterialStore> store_;
};

} // namespace loupe::app::models
