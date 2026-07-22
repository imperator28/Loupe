#include "app/models/MaterialLibraryModel.h"

#include <QColor>
#include <QUuid>

#include <cmath>
#include <exception>

namespace loupe::app::models {
MaterialLibraryModel::MaterialLibraryModel(QObject* parent)
    : QObject(parent)
    , entries_{{{"aluminum-6061", "Aluminum 6061", 2.70}, QStringLiteral("#B8C2CC"), false},
               {{"aluminum-casting", "Aluminum casting", 2.68}, QStringLiteral("#9EABB7"), false},
               {{"steel-carbon", "Carbon steel", 7.85}, QStringLiteral("#6F7E8B"), false},
               {{"stainless-304", "Stainless steel 304", 8.00}, QStringLiteral("#C7D0D8"), false},
               {{"zinc-alloy", "Zinc alloy", 6.60}, QStringLiteral("#8A929A"), false},
               {{"copper", "Copper", 8.96}, QStringLiteral("#C9825D"), false},
               {{"abs", "ABS", 1.04}, QStringLiteral("#D1A45B"), false},
               {{"pp", "Polypropylene (PP)", 0.90}, QStringLiteral("#B8B8AD"), false},
               {{"hdpe", "HDPE", 0.95}, QStringLiteral("#D9E3E4"), false},
               {{"pc", "Polycarbonate (PC)", 1.20}, QStringLiteral("#8596A6"), false},
               {{"pa", "Polyamide (PA / nylon)", 1.14}, QStringLiteral("#D3C6A9"), false},
               {{"pmma", "PMMA (acrylic)", 1.18}, QStringLiteral("#BFD8E6"), false},
               {{"fr4", "PCBA (FR-4)", 1.85}, QStringLiteral("#3A7A58"), false},
               {{"rubber-hnbr", "Rubber (HNBR)", 1.20}, QStringLiteral("#36393E"), false},
               {{"silicone", "Silicone rubber", 1.10}, QStringLiteral("#D6D8DC"), false}}
{
    try {
        store_ = std::make_unique<cache::MaterialStore>();
        loadCustomMaterials();
    } catch (const std::exception&) {
    }
}

QVariantList MaterialLibraryModel::materials() const
{
    QVariantList result;
    result.reserve(entries_.size() + 1);
    result.append(QVariantMap{{QStringLiteral("text"), tr("Material - unassigned")}, {QStringLiteral("value"), QString{}},
                              {QStringLiteral("density"), 0.0}, {QStringLiteral("color"), QStringLiteral("transparent")}});
    for (const auto& entry : entries_) {
        result.append(QVariantMap{{QStringLiteral("text"), QString::fromStdString(entry.material.name)},
                                  {QStringLiteral("value"), QString::fromStdString(entry.material.id)},
                                  {QStringLiteral("density"), entry.material.densityGPerCm3}, {QStringLiteral("color"), entry.color},
                                  {QStringLiteral("custom"), entry.custom}});
    }
    return result;
}

std::optional<inspection::Material> MaterialLibraryModel::find(const QString& id) const
{
    for (const auto& entry : entries_) {
        if (QString::fromStdString(entry.material.id) == id) return entry.material;
    }
    return std::nullopt;
}

QString MaterialLibraryModel::colorFor(const QString& id) const
{
    for (const auto& entry : entries_) {
        if (QString::fromStdString(entry.material.id) == id) return entry.color;
    }
    return {};
}

QString MaterialLibraryModel::addMaterial(const QString& name, const double densityGPerCm3, const QString& color)
{
    const auto id = QStringLiteral("custom-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const auto entry = validatedEntry(id, name, densityGPerCm3, color, true);
    if (!entry || !store_ || !store_->insert({id, QString::fromStdString(entry->material.name), entry->material.densityGPerCm3, entry->color})) return {};
    entries_.append(*entry);
    emit materialsChanged();
    return id;
}

bool MaterialLibraryModel::updateMaterial(const QString& id, const QString& name, const double densityGPerCm3, const QString& color)
{
    auto entry = validatedEntry(id, name, densityGPerCm3, color, true);
    if (!entry || !store_) return false;
    for (auto& existing : entries_) {
        if (QString::fromStdString(existing.material.id) != id || !existing.custom) continue;
        if (!store_->update({id, QString::fromStdString(entry->material.name), entry->material.densityGPerCm3, entry->color})) return false;
        existing = *entry;
        emit materialsChanged();
        return true;
    }
    return false;
}

bool MaterialLibraryModel::removeMaterial(const QString& id)
{
    if (!store_ || !store_->remove(id)) return false;
    for (qsizetype index = 0; index < entries_.size(); ++index) {
        if (QString::fromStdString(entries_.at(index).material.id) == id && entries_.at(index).custom) {
            entries_.removeAt(index);
            emit materialsChanged();
            return true;
        }
    }
    return false;
}

std::optional<MaterialLibraryModel::Entry> MaterialLibraryModel::validatedEntry(const QString& id, const QString& name, const double densityGPerCm3,
                                                                                  const QString& color, const bool custom) const
{
    const auto trimmedName = name.trimmed();
    const QColor parsedColor(color);
    if (id.isEmpty() || trimmedName.isEmpty() || !std::isfinite(densityGPerCm3) || densityGPerCm3 <= 0.0 || !parsedColor.isValid()) return std::nullopt;
    return Entry{{id.toStdString(), trimmedName.toStdString(), densityGPerCm3}, parsedColor.name(QColor::HexRgb).toUpper(), custom};
}

void MaterialLibraryModel::loadCustomMaterials()
{
    if (!store_) return;
    for (const auto& material : store_->list()) {
        if (const auto entry = validatedEntry(material.id, material.name, material.densityGPerCm3, material.color, true)) entries_.append(*entry);
    }
}

} // namespace loupe::app::models
