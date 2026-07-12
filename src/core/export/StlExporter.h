#pragma once

#include "core/export/StepExporter.h"

namespace loupe::exporting {

class StlExporter {
public:
    [[nodiscard]] ExportResult write(const import::ImportResult& imported, const OutputRow& output) const;
};

} // namespace loupe::exporting
