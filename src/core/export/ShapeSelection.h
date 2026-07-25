#pragma once

#include "core/import/StepImporter.h"

#include <TopoDS_Shape.hxx>

#include <string_view>

// Resolving a snapshot node ID to the shape it stands for.
//
// Shared because it is not a lookup: a component label has to be followed to its referred
// shape, a node that names one body of a multi-body compound has to be indexed out by
// sub-solid, the node's placement has to be applied to reach assembly coordinates, and
// OCCT may have normalised source coordinates into the XCAF native unit, which has to be
// rebased out before any reviewed scale means what it says.
//
// The STEP and STL exporters each carried a copy of this. The 2D drawing path needs it
// too, and a third copy of a unit rebase is exactly the kind of thing that drifts into a
// silently wrong-sized output.
namespace loupe::exporting::detail {

// Throws when the snapshot has no such node.
[[nodiscard]] const domain::AssemblyNode& selectedNode(const import::ImportResult& imported,
                                                      std::string_view nodeId);

// The node's own shape in its local coordinates, with a component reference followed and a
// sub-solid selected when the node names one body of a compound.
[[nodiscard]] TopoDS_Shape localShape(const import::ImportResult& imported,
                                      const domain::AssemblyNode& node);

// The node's placement, for assembly coordinates.
[[nodiscard]] TopoDS_Shape placedInAssembly(const TopoDS_Shape& shape,
                                            const domain::AssemblyNode& node);

// The factor that undoes OCCT's normalisation into the XCAF native unit. Multiply a
// reviewed source-to-output factor by this before scaling the shape.
[[nodiscard]] double nativeUnitRebase(const import::ImportResult& imported);

// Uniform scale about the origin. Returns the shape unchanged when the factor is 1.
[[nodiscard]] TopoDS_Shape scaledAboutOrigin(const TopoDS_Shape& shape, double factor);

} // namespace loupe::exporting::detail
