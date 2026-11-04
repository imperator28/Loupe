#include "core/export/AssemblyExporter.h"
#include "core/domain/StableId.h"
#include "core/drawing/DrawingExporter.h"
#include "core/export/AtomicExportFile.h"
#include "core/export/ShapeSelection.h"
#include "core/export/StlExporter.h"

#include <BRep_Builder.hxx>
#include <DESTEP_Parameters.hxx>
#include <Quantity_ColorRGBA.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <Message_ProgressIndicator.hxx>
#include <Message_ProgressScope.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_StepModelType.hxx>
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
#include <algorithm>
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
            copyAssembly(root, gp_Trsf{}, moveOf(domain::stableId(hash_, labelEntry(root), "root")));
            return;
        }
        // A single part file: its one body is the whole export.
        const auto id = domain::stableId(hash_, labelEntry(root), "body");
        if (excluded(id)) {
            ++removed_;
            return;
        }
        copyPart(root, root, gp_Trsf{}, moveOf(id));
    }

    [[nodiscard]] AssemblyExportResult result() const { return {parts_, removed_}; }

private:
    [[nodiscard]] bool excluded(const std::string& id) const { return request_.excludedNodeIds.contains(id); }

    // The node's own exploded-view move, or none.
    [[nodiscard]] gp_Trsf moveOf(const std::string& id) const
    {
        const auto found = request_.moved.find(id);
        return found == request_.moved.end() ? gp_Trsf{} : found->second;
    }

    [[nodiscard]] bool exploded() const { return !request_.moved.empty(); }

    // A move given in assembly coordinates, expressed in the frame `frame` places things in:
    // what a placement inside that frame has to gain for the part to land where it was moved.
    [[nodiscard]] static gp_Trsf inFrame(const gp_Trsf& move, const gp_Trsf& frame)
    {
        return frame.Inverted().Multiplied(move).Multiplied(frame);
    }

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
    // between occurrences whenever it is copied whole, so instancing survives. `world` is
    // where the occurrence sits in the assembly (millimetres); `wholeMove` moves a lone part
    // file's whole shape, which has no placement of its own to carry it.
    std::optional<TDF_Label> copyPart(const TDF_Label& occurrence, const TDF_Label& definition,
                                      const gp_Trsf& world = gp_Trsf{}, const gp_Trsf& wholeMove = gp_Trsf{})
    {
        const auto shape = XCAFDoc_ShapeTool::GetShape(definition);
        const auto occurrenceEntry = labelEntry(occurrence);
        std::vector<TopoDS_Shape> keptSolids;
        std::vector<gp_Trsf> solidMoves;
        int solidCount = 0;
        int removedSolids = 0;
        bool solidsMoved = false;
        for (TopExp_Explorer explorer(shape, TopAbs_SOLID); explorer.More(); explorer.Next(), ++solidCount) {
            const auto bodyId = domain::stableId(hash_, occurrenceEntry, "body-solid:" + std::to_string(solidCount));
            if (excluded(bodyId)) {
                ++removedSolids;
                continue;
            }
            keptSolids.push_back(explorer.Current());
            // One body of a multi-body part moved on its own: the move happens inside the
            // part, so this occurrence gets a definition of its own.
            const auto move = moveOf(bodyId);
            solidMoves.push_back(inFrame(move, world));
            if (move.Form() != gp_Identity) solidsMoved = true;
        }
        if (solidCount <= 1) solidsMoved = false;
        const bool wholeMoved = wholeMove.Form() != gp_Identity;
        if (solidCount > 1 && removedSolids == solidCount) {
            removed_ += removedSolids;
            return std::nullopt;
        }
        const auto key = labelEntry(definition);
        const bool shared = (solidCount <= 1 || removedSolids == 0) && !solidsMoved && !wholeMoved;
        if (shared) {
            if (const auto known = parts_by_definition_.find(key); known != parts_by_definition_.end()) return known->second;
        }

        TopoDS_Shape kept = scaled(shape);
        if (solidCount > 1 && (removedSolids > 0 || solidsMoved)) {
            removed_ += removedSolids;
            TopoDS_Compound compound;
            BRep_Builder builder;
            builder.MakeCompound(compound);
            for (std::size_t index = 0; index < keptSolids.size(); ++index) {
                auto solid = scaled(keptSolids[index]);
                if (solidMoves[index].Form() != gp_Identity) solid = solid.Moved(TopLoc_Location(solidMoves[index]));
                builder.Add(compound, solid);
            }
            kept = compound;
        }
        if (wholeMoved) kept = kept.Moved(TopLoc_Location(wholeMove));
        const auto label = shapes_->AddShape(kept, false, false);
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
        if (shared) parts_by_definition_.emplace(key, label);
        return label;
    }

    // An assembly definition with its kept components; nothing if none are kept. `world` is
    // where this assembly sits (millimetres); `rootMove` is the top assembly's own move,
    // which has no placement of its own to carry it, so its components take it.
    std::optional<TDF_Label> copyAssembly(const TDF_Label& definition, const gp_Trsf& world, const gp_Trsf& rootMove)
    {
        // Exploded, two occurrences of one sub-assembly can be taken apart differently, so
        // each gets its own copy; the parts in it are still shared where they can be.
        const auto key = labelEntry(definition);
        if (!exploded()) {
            if (const auto known = assemblies_.find(key); known != assemblies_.end()) return known->second;
        }

        NCollection_Sequence<TDF_Label> components;
        XCAFDoc_ShapeTool::GetComponents(definition, components);
        struct Kept final { TDF_Label component; TDF_Label child; gp_Trsf placement; };
        std::vector<Kept> kept;
        for (int index = 1; index <= components.Length(); ++index) {
            const auto component = components.Value(index);
            TDF_Label referred;
            XCAFDoc_ShapeTool::GetReferredShape(component, referred);
            const bool isAssembly = XCAFDoc_ShapeTool::IsAssembly(referred);
            const auto id = domain::stableId(hash_, labelEntry(component), isAssembly ? "subassembly" : "occurrence");
            const auto placement = scaled(XCAFDoc_ShapeTool::GetLocation(component).Transformation());
            const auto placed = world.Multiplied(placement);
            // The component lands where it was moved: its move, given in assembly space,
            // brought into this assembly's frame, and the top assembly's own move on top.
            const auto movedPlacement = rootMove.Multiplied(inFrame(moveOf(id), world)).Multiplied(placement);
            std::optional<TDF_Label> child;
            if (isAssembly) {
                child = copyAssembly(referred, placed, gp_Trsf{});
            } else if (excluded(id)) {
                ++removed_;
            } else {
                child = copyPart(component, referred, placed);
                if (child) ++parts_;
            }
            if (child) kept.push_back({component, *child, movedPlacement});
        }
        if (kept.empty()) {
            assemblies_.emplace(key, std::nullopt);
            return std::nullopt;
        }
        const auto assembly = shapes_->NewShape();
        copyAttributes(definition, assembly);
        for (const auto& [component, child, placement] : kept) {
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

namespace {

// OCCT's progress through the STEP translation, mapped into [from, to] of the whole.
class StageProgress final : public Message_ProgressIndicator {
public:
    StageProgress(const AssemblyExportRequest& request, double from, double to) : request_(request), from_(from), to_(to) {}

    void Show(const Message_ProgressScope&, const bool isForce) override
    {
        const auto position = std::clamp(GetPosition(), 0.0, 1.0);
        if (!isForce && position - lastReported_ < 0.01) return;
        lastReported_ = position;
        request_.progress("translate", from_ + (to_ - from_) * position);
    }

private:
    const AssemblyExportRequest& request_;
    double from_;
    double to_;
    double lastReported_{-1.0};
};

void report(const AssemblyExportRequest& request, const std::string& stage, const double fraction)
{
    if (request.progress) request.progress(stage, fraction);
}

} // namespace

AssemblyExportResult AssemblyExporter::write(const import::ImportResult& imported, const AssemblyExportRequest& request) const
{
    report(request, "copy", 0.0);
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

    report(request, request.output == AssemblyOutput::Step ? "translate"
                    : request.output == AssemblyOutput::Stl ? "mesh" : "draw", 0.15);
    if (request.output != AssemblyOutput::Step) {
        // One shape of everything kept, each part where it was placed (or moved), in mm.
        const auto shapes = XCAFDoc_DocumentTool::ShapeTool(document->Main());
        TopoDS_Compound everything;
        BRep_Builder builder;
        builder.MakeCompound(everything);
        for (int index = 1; index <= written.Length(); ++index) builder.Add(everything, shapes->GetShape(written.Value(index)));
        if (request.output == AssemblyOutput::Stl) {
            const auto unitScale = request.outputUnit == units::LengthUnit::Inch ? 1.0 / 25.4 : 1.0;
            detail::writeStlShape(detail::scaledAboutOrigin(everything, unitScale), request.destination);
            report(request, "write", 1.0);
            return result;
        }
        drawing::ShapeDrawingRequest drawingRequest;
        drawingRequest.destination = request.destination;
        drawingRequest.format = request.output == AssemblyOutput::Dxf ? drawing::DrawingFormat::Dxf
                              : request.output == AssemblyOutput::Svg ? drawing::DrawingFormat::Svg
                                                                      : drawing::DrawingFormat::Pdf;
        drawingRequest.viewDirection = request.viewDirection;
        drawingRequest.upDirection = request.upDirection;
        try {
            static_cast<void>(drawing::DrawingExporter{}.writeShape(everything, drawingRequest));
        } catch (...) {
            std::error_code ignored;
            std::filesystem::remove(request.destination, ignored);
            throw;
        }
        report(request, "write", 1.0);
        return result;
    }

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
    // Translating is nearly all of the time; writing the file the rest.
    bool transferred = false;
    if (request.progress) {
        const occ::handle<StageProgress> indicator = new StageProgress(request, 0.15, 0.85);
        transferred = writer.Transfer(document, parameters, STEPControl_AsIs, nullptr, indicator->Start());
    } else {
        transferred = writer.Transfer(document, parameters);
    }
    if (!transferred) throw std::runtime_error("STEP writer could not translate the assembly");
    report(request, "write", 0.85);
    if (writer.Write(partial.partial().string().c_str()) != IFSelect_RetDone) throw std::runtime_error("STEP writer failed");
    partial.finalize();
    report(request, "write", 1.0);
    return result;
}

} // namespace loupe::exporting
