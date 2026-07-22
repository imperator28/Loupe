#pragma once

#include "core/domain/AssemblyTypes.h"
#include "core/units/UnitPolicy.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>

#include <Standard_Handle.hxx>
#include <TDF_Label.hxx>
#include <TDocStd_Document.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Trsf.hxx>

namespace loupe::import {

struct NativeDocument {
    occ::handle<TDocStd_Document> document;
    std::vector<TDF_Label> labels;
    std::vector<TDF_Label> definitionLabels;
    std::vector<TopoDS_Shape> shapes;
    std::vector<gp_Trsf> shapePlacements;
    std::vector<std::string> shapeNodeIds;
    std::vector<std::string> definitionIds;
};

struct ImportPhaseTimes final {
    std::uint64_t stepReadMs{};
    std::uint64_t xcafTransferMs{};
    std::uint64_t snapshotBuildMs{};
};

struct ImportResult {
    domain::AssemblySnapshot snapshot;
    units::UnitEvidence unitEvidence;
    std::size_t definitionCount{};
    std::size_t occurrenceCount{};
    ImportPhaseTimes phaseTimes;
    std::shared_ptr<const NativeDocument> native;
};

class StepImporter {
public:
    [[nodiscard]] ImportResult read(const std::filesystem::path& file) const;
};

} // namespace loupe::import
