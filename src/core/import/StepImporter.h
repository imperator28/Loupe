#pragma once

#include "core/domain/AssemblyTypes.h"
#include "core/units/UnitPolicy.h"

#include <cstddef>
#include <filesystem>
#include <memory>

#include <Standard_Handle.hxx>
#include <TDF_Label.hxx>
#include <TDocStd_Document.hxx>
#include <TopoDS_Shape.hxx>

namespace loupe::import {

struct NativeDocument {
    occ::handle<TDocStd_Document> document;
    std::vector<TDF_Label> labels;
    std::vector<TopoDS_Shape> shapes;
};

struct ImportResult {
    domain::AssemblySnapshot snapshot;
    units::UnitEvidence unitEvidence;
    std::size_t definitionCount{};
    std::size_t occurrenceCount{};
    std::shared_ptr<const NativeDocument> native;
};

class StepImporter {
public:
    [[nodiscard]] ImportResult read(const std::filesystem::path& file) const;
};

} // namespace loupe::import
