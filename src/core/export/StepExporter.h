#pragma once

#include "core/export/ExportPlan.h"
#include "core/import/StepImporter.h"

#include <cstddef>

namespace loupe::exporting {

struct ExportResult {
    bool written{};
    std::size_t outputCount{};
    bool binary{};
    units::LengthUnit outputUnit{units::LengthUnit::Unknown};
};

class StepExporter {
public:
    [[nodiscard]] ExportResult write(const import::ImportResult& imported, const OutputRow& output) const;
};

} // namespace loupe::exporting
