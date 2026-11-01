#include "core/export/AssemblyExporter.h"
#include "core/domain/StableId.h"
#include "core/export/AtomicExportFile.h"
#include "core/export/ShapeSelection.h"

#include <BRep_Builder.hxx>
#include <DESTEP_Parameters.hxx>
#include <Quantity_ColorRGBA.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <TCollection_AsciiString.hxx>
#include <TDF_Tool.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <NCollection_Sequence.hxx>
#include <gp_Trsf.hxx>

#include <cmath>
#include <optional>
#include <stdexcept>
#include <unordered_map>

namespace loupe::exporting {
namespace {

std::string labelEntry(const TDF_Label& label)
{
    TCollection_AsciiString entry;
    TDF_Tool::Entry(label, entry);
    return entry.ToCString();
}

// Copies a source document's tree into a new one, leaving out excluded occurrences. Ids
// are recomputed exactly as StepImporter makes them -- the source hash, the label's
// entry and the node kind -- so they match the snapshot the app selected from.
class TreeCopy final {
public:
    TreeCopy(const import::ImportResult& imported, const AssemblyExportRequest& request,
             const occ::handle<TDocStd_Document>& target)
        : hash_(imported.snapshot.sourceHash)
        , request_(request)
        , scale_(request.nativeToMillimeters)
        , sourceColors_(XCAFDoc_DocumentTool::ColorTool(imported.native->document->Main()))
        , shapes_(XCAFDoc_DocumentTool::ShapeTool(target->Main()))
        , colors_(XCAFDoc_DocumentTool::ColorTool(target->Main()))
    {
    }

    // A free (top-level) shape of the source document.
    void copyRoot(const TDF_Label& root)
    {
        if (XCAFDoc_ShapeTool::IsAssembly(root)) {
            copyAssembly(root);
            return;
        }
        // A single part file: its one body is the whole export.
        if (excluded(domain::stableId(hash_, labelEntry(root), "body"))) {
            ++removed_;
            return;
        }
        copyPart(root, root);
    }

    [[nodiscard]] AssemblyExportResult result() const { return {parts_, removed_}; }

private:
    [[nodiscard]] bool excluded(const std::string& id) const { return request_.excludedNodeIds.contains(id); }

    void copyAttributes(const TDF_Label& from, const TDF_Label& to)
    {
        occ::handle<TDataStd_Name> name;
        if (from.FindAttribute(TDataStd_Name::GetID(), name)) TDataStd_Name::Set(to, name->Get());
        for (const auto type : {XCAFDoc_ColorGen, XCAFDoc_ColorSurf, XCAFDoc_ColorCurv}) {
            Quantity_ColorRGBA color;
            if (sourceColors_->GetColor(from, type, color)) colors_->SetColor(to, color, type);
        }
    }

    [[nodiscard]] gp_Trsf scaled(gp_Trsf placement) const
    {
        if (std::abs(scale_ - 1.0) > 1e-12) placement.SetTranslationPart(placement.TranslationPart() * scale_);
        return placement;
    }

    [[nodiscard]] TopoDS_Shape scaled(const TopoDS_Shape& shape) const
    {
        return std::abs(scale_ - 1.0) > 1e-12 ? detail::scaledAboutOrigin(shape, scale_) : shape;
    }

    // A part definition, minus any of its solids that were excluded one by one. Shared
    // between occurrences whenever it is copied whole, so instancing survives.
    std::optional<TDF_Label> copyPart(const TDF_Label& occurrence, const TDF_Label& definition)
    {
        const auto shape = XCAFDoc_ShapeTool::GetShape(definition);
        const auto occurrenceEntry = labelEntry(occurrence);
        std::vector<TopoDS_Shape> keptSolids;
        int solidCount = 0;
        int removedSolids = 0;
        for (TopExp_Explorer explorer(shape, TopAbs_SOLID); explorer.More(); explorer.Next(), ++solidCount) {
            if (excluded(domain::stableId(hash_, occurrenceEntry, "body-solid:" + std::to_string(solidCount)))) ++removedSolids;
            else keptSolids.push_back(explorer.Current());
        }
        if (solidCount > 1 && removedSolids == solidCount) {
            removed_ += removedSolids;
            return std::nullopt;
        }
        const auto key = labelEntry(definition);
        if (solidCount <= 1 || removedSolids == 0) {
            if (const auto known = parts_by_definition_.find(key); known != parts_by_definition_.end()) return known->second;
        }

        TopoDS_Shape kept = shape;
        if (solidCount > 1 && removedSolids > 0) {
            removed_ += removedSolids;
            TopoDS_Compound compound;
            BRep_Builder builder;
            builder.MakeCompound(compound);
            for (const auto& solid : keptSolids) builder.Add(compound, solid);
            kept = compound;
        }
        const auto label = shapes_->AddShape(scaled(kept), false, false);
        copyAttributes(definition, label);
        // Face and edge colours live on sub-shape labels. Scaling builds new geometry, so
        // they are found through the unscaled shape and re-attached by position in it.
        if (std::abs(scale_ - 1.0) <= 1e-12) {
            NCollection_Sequence<TDF_Label> subShapes;
            XCAFDoc_ShapeTool::GetSubShapes(definition, subShapes);
            for (int index = 1; index <= subShapes.Length(); ++index) {
                const auto source = subShapes.Value(index);
                const auto subShape = XCAFDoc_ShapeTool::GetShape(source);
                const auto target = shapes_->AddSubShape(label, subShape);
                if (!target.IsNull()) copyAttributes(source, target);
            }
        }
        if (solidCount <= 1 || removedSolids == 0) parts_by_definition_.emplace(key, label);
        return label;
    }

    // An assembly definition with its kept components; nothing if none are kept.
    std::optional<TDF_Label> copyAssembly(const TDF_Label& definition)
    {
        const auto key = labelEntry(definition);
        if (const auto known = assemblies_.find(key); known != assemblies_.end()) return known->second;

        NCollection_Sequence<TDF_Label> components;
        XCAFDoc_ShapeTool::GetComponents(definition, components);
        struct Kept final { TDF_Label component; TDF_Label child; };
        std::vector<Kept> kept;
        for (int index = 1; index <= components.Length(); ++index) {
            const auto component = components.Value(index);
            TDF_Label referred;
            XCAFDoc_ShapeTool::GetReferredShape(component, referred);
            std::optional<TDF_Label> child;
            if (XCAFDoc_ShapeTool::IsAssembly(referred)) {
                child = copyAssembly(referred);
            } else if (excluded(domain::stableId(hash_, labelEntry(component), "occurrence"))) {
                ++removed_;
            } else {
                child = copyPart(component, referred);
                if (child) ++parts_;
            }
            if (child) kept.push_back({component, *child});
        }
        if (kept.empty()) {
            assemblies_.emplace(key, std::nullopt);
            return std::nullopt;
        }
        const auto assembly = shapes_->NewShape();
        copyAttributes(definition, assembly);
        for (const auto& [component, child] : kept) {
            const auto placement = scaled(XCAFDoc_ShapeTool::GetLocation(component).Transformation());
            const auto added = shapes_->AddComponent(assembly, child, TopLoc_Location(placement));
            if (!added.IsNull()) copyAttributes(component, added);
        }
        assemblies_.emplace(key, assembly);
        return assembly;
    }

    std::string hash_;
    const AssemblyExportRequest& request_;
    double scale_;
    occ::handle<XCAFDoc_ColorTool> sourceColors_;
    occ::handle<XCAFDoc_ShapeTool> shapes_;
    occ::handle<XCAFDoc_ColorTool> colors_;
    std::unordered_map<std::string, TDF_Label> parts_by_definition_;
    std::unordered_map<std::string, std::optional<TDF_Label>> assemblies_;
    int parts_{};
    int removed_{};
};

} // namespace

AssemblyExportResult AssemblyExporter::write(const import::ImportResult& imported, const AssemblyExportRequest& request) const
{
    if (!imported.native || imported.native->document.IsNull()) throw std::runtime_error("import has no native XCAF document");
    occ::handle<TDocStd_Document> document;
    XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", document);
    TreeCopy copy(imported, request, document);
    NCollection_Sequence<TDF_Label> roots;
    XCAFDoc_DocumentTool::ShapeTool(imported.native->document->Main())->GetFreeShapes(roots);
    for (int index = 1; index <= roots.Length(); ++index) copy.copyRoot(roots.Value(index));
    auto result = copy.result();
    // A lone part file counts as one part when kept.
    NCollection_Sequence<TDF_Label> written;
    XCAFDoc_DocumentTool::ShapeTool(document->Main())->GetFreeShapes(written);
    if (written.IsEmpty()) throw std::runtime_error("every part is hidden, so there is nothing to export");
    if (result.partCount == 0) result.partCount = written.Length();
    XCAFDoc_DocumentTool::ShapeTool(document->Main())->UpdateAssemblies();

    // Geometry is in millimetres now; the writer converts to the requested unit.
    XCAFDoc_DocumentTool::SetLengthUnit(document, 1.0, UnitsMethods_LengthUnit_Millimeter);
    const bool inches = request.outputUnit == units::LengthUnit::Inch;
    std::filesystem::create_directories(request.destination.parent_path());
    detail::AtomicExportFile partial(request.destination);
    STEPCAFControl_Writer writer;
    writer.SetNameMode(true);
    writer.SetColorMode(true);
    DESTEP_Parameters parameters;
    parameters.WriteUnit = inches ? UnitsMethods_LengthUnit_Inch : UnitsMethods_LengthUnit_Millimeter;
    if (!writer.Perform(document, partial.partial().string().c_str(), parameters)) throw std::runtime_error("STEP writer failed");
    partial.finalize();
    return result;
}

} // namespace loupe::exporting
