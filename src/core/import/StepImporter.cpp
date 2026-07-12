#include "core/import/StepImporter.h"
#include "core/import/StepImporterTestSupport.h"

#include "core/domain/StableId.h"

#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <NCollection_Sequence.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <TCollection_AsciiString.hxx>
#include <TDF_Tool.hxx>
#include <TDocStd_Document.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS_Shape.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <gp_Trsf.hxx>

#include <fstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace loupe::import {

namespace {

using domain::AssemblyNode;
using domain::InputClass;
using domain::NodeKind;

std::string labelEntry(const TDF_Label& label)
{
    TCollection_AsciiString entry;
    TDF_Tool::Entry(label, entry);
    return entry.ToCString();
}

units::LengthUnit unitForName(const std::string_view name)
{
    std::string normalized(name);
    std::ranges::transform(normalized, normalized.begin(), [](const unsigned char value) { return static_cast<char>(std::toupper(value)); });
    if (normalized == "MM" || normalized == "MILLIMETRE" || normalized == "MILLIMETER") return units::LengthUnit::Millimeter;
    if (normalized == "INCH" || normalized == "IN") return units::LengthUnit::Inch;
    return units::LengthUnit::Unknown;
}

units::LengthUnit unitForMeters(const double meters)
{
    if (std::abs(meters - 0.001) < 1.0e-12) return units::LengthUnit::Millimeter;
    if (std::abs(meters - 0.0254) < 1.0e-12) return units::LengthUnit::Inch;
    return units::LengthUnit::Unknown;
}

double millimetersPerDocumentUnit(const double meters, const units::LengthUnit fallback)
{
    if (std::isfinite(meters) && meters > 0.0) return meters * 1000.0;
    if (fallback == units::LengthUnit::Millimeter || fallback == units::LengthUnit::Inch) {
        return units::millimetersPerUnit(fallback);
    }
    return 1.0;
}

std::string sourceHash(const std::filesystem::path& file)
{
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::runtime_error("unable to open STEP file");
    std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    return domain::stableId("step", bytes, "source");
}

domain::Transform identityTransform()
{
    domain::Transform transform;
    transform.columnMajor[0] = transform.columnMajor[5] = transform.columnMajor[10] = transform.columnMajor[15] = 1.0;
    return transform;
}

domain::Transform transformFor(const gp_Trsf& occtTransform)
{
    auto transform = identityTransform();
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            transform.columnMajor[static_cast<std::size_t>(column * 4 + row)] = occtTransform.Value(row + 1, column + 1);
        }
    }
    const gp_XYZ translation = occtTransform.TranslationPart();
    transform.columnMajor[12] = translation.X();
    transform.columnMajor[13] = translation.Y();
    transform.columnMajor[14] = translation.Z();
    return transform;
}

gp_Trsf composedLocation(const TDF_Label& label, const gp_Trsf& parent)
{
    if (!XCAFDoc_ShapeTool::IsComponent(label)) return parent;
    return parent.Multiplied(XCAFDoc_ShapeTool::GetLocation(label).Transformation());
}

void addDefinition(const TDF_Label& label, const std::string& hash, ImportResult& result,
                   std::unordered_map<std::string, std::string>& definitions)
{
    const auto entry = labelEntry(label);
    if (definitions.contains(entry)) return;
    const auto id = domain::stableId(hash, entry, "definition");
    definitions.emplace(entry, id);
    result.snapshot.nodes.push_back({id, NodeKind::Definition, "definition", entry, std::nullopt, std::nullopt, identityTransform()});
    ++result.definitionCount;
}

void visit(const TDF_Label& label, const std::optional<std::string>& parentId, const std::string& hash,
           const occ::handle<XCAFDoc_ShapeTool>& shapes, ImportResult& result,
           std::unordered_map<std::string, std::string>& definitions, const gp_Trsf& parentLocation)
{
    const auto entry = labelEntry(label);
    if (XCAFDoc_ShapeTool::IsExternRef(label)) {
        result.snapshot.classification = InputClass::ExternalReferences;
    }
    const bool component = XCAFDoc_ShapeTool::IsComponent(label);
    TDF_Label referred = label;
    if (component) XCAFDoc_ShapeTool::GetReferredShape(label, referred);
    if (XCAFDoc_ShapeTool::IsAssembly(referred)) {
        const gp_Trsf location = composedLocation(label, parentLocation);
        const bool root = !component && !parentId;
        const auto id = domain::stableId(hash, entry, root ? "root" : "subassembly");
        result.snapshot.nodes.push_back({id, root ? NodeKind::Root : NodeKind::Subassembly, "assembly", entry, parentId, std::nullopt, transformFor(location)});
        if (root) result.snapshot.rootIds.push_back(id);
        NCollection_Sequence<TDF_Label> components;
        XCAFDoc_ShapeTool::GetComponents(referred, components);
        for (int i = 1; i <= components.Length(); ++i) visit(components.Value(i), id, hash, shapes, result, definitions, location);
        return;
    }

    TDF_Label definition = referred;
    addDefinition(definition, hash, result, definitions);
    const auto definitionId = definitions.at(labelEntry(definition));
    const auto id = domain::stableId(hash, entry, component ? "occurrence" : "body");
    result.snapshot.nodes.push_back({id, component ? NodeKind::Occurrence : NodeKind::Body, component ? "occurrence" : "body", entry, parentId, definitionId, transformFor(composedLocation(label, parentLocation))});
    if (!parentId) result.snapshot.rootIds.push_back(id);
    if (component) ++result.occurrenceCount;
    auto native = std::const_pointer_cast<NativeDocument>(result.native);
    native->labels.push_back(label);
    native->shapes.push_back(XCAFDoc_ShapeTool::GetShape(label));
}

} // namespace

ImportResult StepImporter::read(const std::filesystem::path& file) const
{
    STEPCAFControl_Reader reader;
    reader.SetNameMode(true);
    reader.SetColorMode(true);
    if (reader.ReadFile(file.string().c_str()) != IFSelect_RetDone) throw std::runtime_error("STEP read failed");

    NCollection_Sequence<TCollection_AsciiString> lengthNames, angleNames, solidAngleNames;
    reader.ChangeReader().FileUnits(lengthNames, angleNames, solidAngleNames);

    auto native = std::make_shared<NativeDocument>();
    XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", native->document);
    if (!reader.Transfer(native->document)) throw std::runtime_error("STEP transfer failed");

    ImportResult result;
    result.native = native;
    result.snapshot.sourceHash = sourceHash(file);
    result.snapshot.stage = domain::LoadStage::TreeReady;
    result.snapshot.classification = InputClass::SinglePart;
    for (int i = 1; i <= lengthNames.Length(); ++i) result.unitEvidence.declaredRepresentationUnits.push_back(unitForName(lengthNames.Value(i).ToCString()));
    double xcafMeters = 0.0;
    const bool hasXcafUnit = XCAFDoc_DocumentTool::GetLengthUnit(native->document, xcafMeters);
    result.unitEvidence.xcafUnit = hasXcafUnit ? unitForMeters(xcafMeters) : units::LengthUnit::Unknown;

    const auto shapes = XCAFDoc_DocumentTool::ShapeTool(native->document->Main());
    NCollection_Sequence<TDF_Label> freeShapes;
    shapes->GetFreeShapes(freeShapes);
    std::unordered_map<std::string, std::string> definitions;
    bool hasAssembly = false;
    const gp_Trsf identity;
    for (int i = 1; i <= freeShapes.Length(); ++i) {
        hasAssembly = hasAssembly || XCAFDoc_ShapeTool::IsAssembly(freeShapes.Value(i));
        visit(freeShapes.Value(i), std::nullopt, result.snapshot.sourceHash, shapes, result, definitions, identity);
    }
    Bnd_Box bounds;
    const auto mutableNative = std::const_pointer_cast<NativeDocument>(result.native);
    for (const auto& shape : mutableNative->shapes) BRepBndLib::Add(shape, bounds);
    if (!bounds.IsVoid()) {
        double minX, minY, minZ, maxX, maxY, maxZ;
        bounds.Get(minX, minY, minZ, maxX, maxY, maxZ);
        const auto largestSourceExtent = std::max({maxX - minX, maxY - minY, maxZ - minZ});
        const auto fallback = result.unitEvidence.declaredRepresentationUnits.empty()
            ? units::LengthUnit::Unknown : result.unitEvidence.declaredRepresentationUnits.front();
        result.unitEvidence.normalizedLongestExtentMm = largestSourceExtent * millimetersPerDocumentUnit(xcafMeters, fallback);
    }
    if (result.snapshot.classification != InputClass::ExternalReferences) {
        result.snapshot.classification = hasAssembly ? InputClass::StructuredAssembly
            : freeShapes.Length() > 1 ? InputClass::FlatMultiSolid : InputClass::SinglePart;
    }
    return result;
}

domain::AssemblySnapshot detail::snapshotForTesting(const occ::handle<TDocStd_Document>& document)
{
    ImportResult result;
    auto native = std::make_shared<NativeDocument>();
    native->document = document;
    result.native = native;
    result.snapshot.sourceHash = "in-memory-xcaf";
    result.snapshot.stage = domain::LoadStage::TreeReady;
    result.snapshot.classification = domain::InputClass::SinglePart;
    const auto shapes = XCAFDoc_DocumentTool::ShapeTool(document->Main());
    NCollection_Sequence<TDF_Label> freeShapes;
    shapes->GetFreeShapes(freeShapes);
    std::unordered_map<std::string, std::string> definitions;
    const gp_Trsf identity;
    for (int index = 1; index <= freeShapes.Length(); ++index) {
        visit(freeShapes.Value(index), std::nullopt, result.snapshot.sourceHash, shapes, result, definitions, identity);
    }
    return result.snapshot;
}

domain::AssemblySnapshot detail::snapshotForTesting(const occ::handle<TDocStd_Document>& document, const TDF_Label& root)
{
    ImportResult result;
    auto native = std::make_shared<NativeDocument>();
    native->document = document;
    result.native = native;
    result.snapshot.sourceHash = "in-memory-xcaf";
    result.snapshot.stage = domain::LoadStage::TreeReady;
    result.snapshot.classification = domain::InputClass::StructuredAssembly;
    std::unordered_map<std::string, std::string> definitions;
    const auto shapes = XCAFDoc_DocumentTool::ShapeTool(document->Main());
    const gp_Trsf identity;
    visit(root, std::nullopt, result.snapshot.sourceHash, shapes, result, definitions, identity);
    return result.snapshot;
}

} // namespace loupe::import
