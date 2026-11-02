#pragma once

#include "core/import/StepImporter.h"
#include "core/units/UnitPolicy.h"

#include <gp_Dir.hxx>
#include <gp_Trsf.hxx>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace loupe::exporting {

// Re-export the opened assembly as one STEP file without some of its parts (#7): the
// assembly tree, names, placements and colours are kept, the excluded occurrences are
// left out, and any subassembly left with nothing in it is dropped. Typical use: strip
// fasteners or purchased parts from a supplier assembly before passing it on.
// What the assembly is written as. STEP keeps the tree; STL is one mesh of everything; the
// drawings are one view of everything, projected along `viewDirection`.
enum class AssemblyOutput { Step, Stl, Dxf, Svg, Pdf };

struct AssemblyExportRequest {
    std::filesystem::path destination;
    // Snapshot node ids to leave out: occurrences or bodies, and the per-solid bodies of a
    // part that bundles several solids (only those solids are removed from it).
    std::unordered_set<std::string> excludedNodeIds;
    // Native document coordinates to millimetres: the reviewed source factor times the
    // rebase from OCCT's normalised units, as for every other export.
    double nativeToMillimeters{1.0};
    // The unit the file is written in, normally the source file's own.
    units::LengthUnit outputUnit{units::LengthUnit::Millimeter};
    AssemblyOutput output{AssemblyOutput::Step};
    // The exploded view (#17): a node's own rigid move, in assembly millimetres, applied
    // inside its parent's. A node that is not listed stays where the file has it.
    std::unordered_map<std::string, gp_Trsf> moved;
    // For the drawings: the direction the view is seen from, and the drawing's up.
    gp_Dir viewDirection{0.0, 0.0, 1.0};
    gp_Dir upDirection{0.0, 1.0, 0.0};
};

struct AssemblyExportResult {
    int partCount{};      // part occurrences written
    int removedCount{};   // occurrences and solids left out
};

class AssemblyExporter {
public:
    // Throws std::runtime_error when nothing would be left, or the file cannot be written.
    [[nodiscard]] AssemblyExportResult write(const import::ImportResult& imported, const AssemblyExportRequest& request) const;
};

} // namespace loupe::exporting
