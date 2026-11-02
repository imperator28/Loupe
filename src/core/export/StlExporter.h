#pragma once

#include "core/export/StepExporter.h"

#include <TopoDS_Shape.hxx>

#include <filesystem>

namespace loupe::exporting {

namespace detail {
// Triangulates the shape and writes it as binary STL at `destination`, atomically, dropping
// degenerate triangles. Coordinates are written as they are.
void writeStlShape(const TopoDS_Shape& shape, const std::filesystem::path& destination);
} // namespace detail

class StlExporter {
public:
    [[nodiscard]] ExportResult write(const import::ImportResult& imported, const OutputRow& output) const;
};

} // namespace loupe::exporting
