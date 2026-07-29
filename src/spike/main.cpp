#include "core/drawing/DrawingPreview.h"
#include "core/drawing/DrawingProjector.h"
#include "core/drawing/DrawingValidator.h"
#include "core/drawing/DxfWriter.h"
#include "core/drawing/PdfWriter.h"
#include "core/drawing/SvgWriter.h"
#include "core/export/ExportPlan.h"
#include "core/export/StepExporter.h"
#include "core/export/StlExporter.h"
#include "core/import/StepImporter.h"
#include "core/report/EvidenceWriter.h"
#include "core/units/UnitPolicy.h"
#include "core/validation/OutputValidator.h"

#include <nlohmann/json.hpp>

#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepLib.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <Bnd_Box2d.hxx>
#include <BndLib_Add2dCurve.hxx>
#include <Geom2dAdaptor_Curve.hxx>
#include <Geom2d_BSplineCurve.hxx>
#include <Geom_Plane.hxx>
#include <HLRAlgo_Projector.hxx>
#include <HLRBRep_Algo.hxx>
#include <HLRBRep_HLRToShape.hxx>
#include <Poly_Triangulation.hxx>
#include <TDF_Tool.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <chrono>
#include <array>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int success = 0;
constexpr int invalidArguments = 1;
constexpr int reviewRequired = 2;
constexpr int importFailure = 3;
constexpr int exportFailure = 4;
constexpr int validationFailure = 5;
constexpr int corpusContractFailure = 6;

struct CommandError { int code; std::string message; };
struct InspectOptions { std::filesystem::path file; std::optional<loupe::units::UnitOverride> overrideValue; };

void printJson(const nlohmann::json& value) { std::cout << value.dump() << '\n'; }

[[noreturn]] void fail(const int code, std::string message) { throw CommandError{code, std::move(message)}; }

loupe::units::LengthUnit parseUnit(const std::string_view value)
{
    if (value == "mm") return loupe::units::LengthUnit::Millimeter;
    if (value == "in") return loupe::units::LengthUnit::Inch;
    fail(invalidArguments, "unit must be mm or in");
}

std::string unitName(const loupe::units::LengthUnit value)
{
    switch (value) {
    case loupe::units::LengthUnit::Millimeter: return "mm";
    case loupe::units::LengthUnit::Inch: return "in";
    case loupe::units::LengthUnit::Unknown: return "unknown";
    case loupe::units::LengthUnit::Mixed: return "mixed";
    }
    return "unknown";
}

std::string confidenceName(const loupe::units::UnitConfidence value)
{
    switch (value) {
    case loupe::units::UnitConfidence::Confirmed: return "confirmed";
    case loupe::units::UnitConfidence::Suspicious: return "suspicious";
    case loupe::units::UnitConfidence::MissingOrMixed: return "missing_or_mixed";
    case loupe::units::UnitConfidence::UserOverride: return "user_override";
    }
    return "missing_or_mixed";
}

loupe::exporting::SelectionKind parseSelectionKind(const std::string_view value)
{
    if (value == "occurrence") return loupe::exporting::SelectionKind::Occurrence;
    if (value == "definition") return loupe::exporting::SelectionKind::Definition;
    fail(invalidArguments, "kind must be occurrence or definition");
}

loupe::exporting::Format parseFormat(const std::string_view value)
{
    if (value == "step") return loupe::exporting::Format::Step;
    if (value == "stl") return loupe::exporting::Format::Stl;
    fail(invalidArguments, "format must be step or stl");
}

loupe::exporting::Coordinates parseCoordinates(const std::string_view value)
{
    if (value == "local") return loupe::exporting::Coordinates::Local;
    if (value == "assembly") return loupe::exporting::Coordinates::Assembly;
    fail(invalidArguments, "coordinates must be local or assembly");
}

loupe::exporting::StepOutputUnit parseOutputUnit(const std::string_view value)
{
    if (value == "mm") return loupe::exporting::StepOutputUnit::Millimeter;
    if (value == "in") return loupe::exporting::StepOutputUnit::Inch;
    fail(invalidArguments, "output unit must be mm or in");
}

InspectOptions parseInspect(const std::vector<std::string>& args)
{
    if (args.size() < 3 || args[0] != "inspect") fail(invalidArguments, "usage: inspect <file> [--interpret-as mm|in] [--factor N] --json");
    InspectOptions options{args[1], std::nullopt};
    bool json{}; std::optional<loupe::units::LengthUnit> unit; double factor{1.0}; bool hasFactor{};
    for (std::size_t index = 2; index < args.size(); ++index) {
        if (args[index] == "--json") { json = true; continue; }
        if (args[index] == "--interpret-as" && index + 1 < args.size()) { unit = parseUnit(args[++index]); continue; }
        if (args[index] == "--factor" && index + 1 < args.size()) {
            try { factor = std::stod(args[++index]); } catch (const std::exception&) { fail(invalidArguments, "factor must be a number"); }
            hasFactor = true; continue;
        }
        fail(invalidArguments, "invalid inspect option");
    }
    if (!json || (hasFactor && !unit.has_value())) fail(invalidArguments, "inspect requires --json and a unit for --factor");
    if (unit.has_value()) options.overrideValue = loupe::units::UnitOverride{*unit, factor, "CLI override"};
    return options;
}

nlohmann::json inspectJson(const loupe::import::ImportResult& imported, const loupe::units::UnitDecision& decision)
{
    nlohmann::json declared = nlohmann::json::array();
    for (const auto unit : imported.unitEvidence.declaredRepresentationUnits) declared.push_back(unitName(unit));
    return {{"sourceHash", imported.snapshot.sourceHash}, {"status", decision.blocksExport() ? "unit_review_required" : "ready"},
            {"classification", static_cast<int>(imported.snapshot.classification)}, {"nodeCount", imported.snapshot.nodes.size()},
            {"declaredUnits", declared}, {"effectiveUnit", unitName(decision.effectiveUnit)},
            {"unitConfidence", confidenceName(decision.confidence)}, {"sourceToMillimeters", decision.sourceToMillimeters}, {"reason", decision.reason}};
}

std::pair<loupe::import::ImportResult, loupe::units::UnitDecision> importWithPolicy(const InspectOptions& options)
{
    try {
        loupe::import::ImportResult imported = loupe::import::StepImporter{}.read(options.file);
        const auto decision = loupe::units::decide(imported.unitEvidence, options.overrideValue);
        return {std::move(imported), decision};
    } catch (const std::invalid_argument& error) { fail(invalidArguments, error.what());
    } catch (const std::exception& error) { fail(importFailure, error.what()); }
}

int runInspect(const std::vector<std::string>& args)
{
    const auto options = parseInspect(args);
    nlohmann::json response; int code{};
    {
        const auto [imported, decision] = importWithPolicy(options);
        response = inspectJson(imported, decision);
        code = decision.blocksExport() ? reviewRequired : success;
    }
    printJson(response);
    return code;
}

void validateCase(const nlohmann::json& value)
{
    if (!value.is_object() || !value.contains("id") || !value["id"].is_string() || !value.contains("file") || !value["file"].is_string()) {
        fail(corpusContractFailure, "each corpus case needs string id and file");
    }
    const std::string id = value["id"].get<std::string>();
    const std::filesystem::path idPath(id);
    if (id.empty() || idPath.has_parent_path() || idPath.is_absolute() || id == "." || id == "..") fail(corpusContractFailure, "case id must be a safe leaf name");
    if (!value.contains("requiredFullFlow") || !value["requiredFullFlow"].is_boolean() || !value["requiredFullFlow"].get<bool>()) {
        fail(corpusContractFailure, "every corpus case must explicitly require the full export proof");
    }
}

nlohmann::json loadCases(const std::filesystem::path& file)
{
    try {
        std::ifstream input(file);
        if (!input) fail(corpusContractFailure, "unable to open corpus cases JSON");
        nlohmann::json document; input >> document;
        if (!document.is_object() || document.value("schemaVersion", 0) != 1 || !document.contains("cases") || !document["cases"].is_array()) {
            fail(corpusContractFailure, "cases JSON must have schemaVersion 1 and cases array");
        }
        for (const auto& value : document["cases"]) validateCase(value);
        return document;
    } catch (const nlohmann::json::exception& error) { fail(corpusContractFailure, error.what()); }
}

std::filesystem::path optionValue(const std::vector<std::string>& args, const std::string_view name)
{
    if (args.size() != 4 || args[2] != name) fail(invalidArguments, "invalid command arguments");
    return args[3];
}

std::string csvCell(std::string value)
{
    if (!value.empty() && (value.front() == '=' || value.front() == '+' || value.front() == '-' || value.front() == '@')) value.insert(value.begin(), '\'');
    if (value.find_first_of(",\"\r\n") != std::string::npos) {
        std::string escaped{"\""};
        for (const char character : value) escaped += character == '\"' ? "\"\"" : std::string(1, character);
        return escaped + "\"";
    }
    return value;
}

loupe::report::EvidenceMetadata metadataFor(const loupe::import::ImportResult& imported, const loupe::units::UnitDecision& decision)
{
    return {imported.snapshot.sourceHash, "loupe-spike", unitName(decision.effectiveUnit) + ":" + confidenceName(decision.confidence), "local"};
}

std::string className(const loupe::domain::InputClass value)
{
    switch (value) {
    case loupe::domain::InputClass::StructuredAssembly: return "structured_assembly";
    case loupe::domain::InputClass::FlatMultiSolid: return "flat_multi_solid";
    case loupe::domain::InputClass::SinglePart: return "single_part";
    case loupe::domain::InputClass::Partial: return "partial";
    case loupe::domain::InputClass::Invalid: return "invalid";
    case loupe::domain::InputClass::ExternalReferences: return "external_references";
    }
    return "invalid";
}

bool sameImport(const loupe::import::ImportResult& left, const loupe::units::UnitDecision& leftDecision,
                const loupe::import::ImportResult& right, const loupe::units::UnitDecision& rightDecision)
{
    if (left.snapshot.classification != right.snapshot.classification || leftDecision.effectiveUnit != rightDecision.effectiveUnit
        || leftDecision.confidence != rightDecision.confidence || leftDecision.sourceToMillimeters != rightDecision.sourceToMillimeters
        || left.snapshot.nodes.size() != right.snapshot.nodes.size()) return false;
    for (std::size_t index = 0; index < left.snapshot.nodes.size(); ++index) {
        if (left.snapshot.nodes[index].id != right.snapshot.nodes[index].id || left.snapshot.nodes[index].hierarchyPath != right.snapshot.nodes[index].hierarchyPath) return false;
    }
    return true;
}

const loupe::domain::AssemblyNode& representative(const loupe::import::ImportResult& imported, const loupe::domain::NodeKind kind)
{
    const loupe::domain::AssemblyNode* selected{};
    for (const auto& node : imported.snapshot.nodes) {
        if (node.kind == kind && (!selected || std::pair{node.hierarchyPath, node.id} < std::pair{selected->hierarchyPath, selected->id})) selected = &node;
    }
    if (!selected) fail(corpusContractFailure, "required representative selection is missing");
    return *selected;
}

loupe::exporting::OutputRow makeRow(const loupe::import::ImportResult& imported, const loupe::units::UnitDecision& decision,
                                    const loupe::domain::AssemblyNode& node, const loupe::exporting::SelectionKind kind,
                                    const loupe::exporting::Format format, const loupe::exporting::Coordinates coordinates,
                                    const std::filesystem::path& destination)
{
    loupe::exporting::PlanRequest request;
    request.selections.push_back({node.id, kind}); request.destination = destination.string(); request.format = format;
    request.coordinates = coordinates; request.grouping = loupe::exporting::Grouping::SeparateFiles;
    request.stepOutputUnit = loupe::exporting::StepOutputUnit::Millimeter; request.requestedUnitToMillimeters = 1.0; request.unitDecision = decision;
    for (const auto& value : imported.snapshot.nodes) request.hierarchyPaths.emplace(value.id, value.hierarchyPath);
    const auto plan = loupe::exporting::buildPlan(request);
    return plan.outputs().front();
}

TopoDS_Shape shapeForExpectation(const loupe::import::ImportResult& imported, const loupe::exporting::OutputRow& row)
{
    const auto node = std::ranges::find_if(imported.snapshot.nodes, [&row](const auto& value) { return value.id == row.nodeId(); });
    if (node == imported.snapshot.nodes.end()) fail(exportFailure, "selected node disappeared before export");
    TDF_Label label; TDF_Tool::Label(imported.native->document->GetData(), node->hierarchyPath.c_str(), label);
    const auto shapes = XCAFDoc_DocumentTool::ShapeTool(imported.native->document->Main());
    if (XCAFDoc_ShapeTool::IsComponent(label)) XCAFDoc_ShapeTool::GetReferredShape(label, label);
    TopoDS_Shape shape = shapes->GetShape(label); if (shape.IsNull()) fail(exportFailure, "selected shape is empty");
    if (row.coordinates() == loupe::exporting::Coordinates::Assembly) {
        gp_Trsf transform; transform.SetValues(node->placement.columnMajor[0], node->placement.columnMajor[4], node->placement.columnMajor[8], node->placement.columnMajor[12], node->placement.columnMajor[1], node->placement.columnMajor[5], node->placement.columnMajor[9], node->placement.columnMajor[13], node->placement.columnMajor[2], node->placement.columnMajor[6], node->placement.columnMajor[10], node->placement.columnMajor[14]);
        shape = BRepBuilderAPI_Transform(shape, transform, true).Shape();
    }
    double nativeMeters = 0.001; XCAFDoc_DocumentTool::GetLengthUnit(imported.native->document, nativeMeters);
    const auto declared = imported.unitEvidence.declaredRepresentationUnits.empty() ? loupe::units::LengthUnit::Millimeter : imported.unitEvidence.declaredRepresentationUnits.front();
    const double scale = row.sourceToOutputScale() * (nativeMeters * 1000.0 / loupe::units::millimetersPerUnit(declared));
    if (scale != 1.0) { gp_Trsf transform; transform.SetScale(gp_Pnt(0.0, 0.0, 0.0), scale); shape = BRepBuilderAPI_Transform(shape, transform, true).Shape(); }
    return shape;
}

loupe::validation::ExpectedOutput expectedBeforeExport(const loupe::import::ImportResult& imported, const loupe::exporting::OutputRow& row)
{
    const TopoDS_Shape shape = shapeForExpectation(imported, row);
    if (row.format() == loupe::exporting::Format::Stl) {
        BRepMesh_IncrementalMesh mesher(shape, 0.1); if (!mesher.IsDone()) fail(exportFailure, "STL expectation triangulation failed");
        loupe::validation::Bounds result{}; bool any{};
        for (TopExp_Explorer faces(shape, TopAbs_FACE); faces.More(); faces.Next()) {
            TopLoc_Location location;
            const auto triangulation = BRep_Tool::Triangulation(TopoDS::Face(faces.Current()), location);
            if (triangulation.IsNull()) continue;
            const gp_Trsf transform = location.Transformation();
            for (int index = 1; index <= triangulation->NbNodes(); ++index) {
                const gp_Pnt point = triangulation->Node(index).Transformed(transform);
                const loupe::validation::Vec3 vertex{point.X(), point.Y(), point.Z()};
                if (!any) { result.minimum = vertex; result.maximum = vertex; any = true; }
                else { result.minimum.x = std::min(result.minimum.x, vertex.x); result.minimum.y = std::min(result.minimum.y, vertex.y); result.minimum.z = std::min(result.minimum.z, vertex.z); result.maximum.x = std::max(result.maximum.x, vertex.x); result.maximum.y = std::max(result.maximum.y, vertex.y); result.maximum.z = std::max(result.maximum.z, vertex.z); }
            }
        }
        if (!any) fail(validationFailure, "STL expectation has no source mesh vertices");
        return {row.finalPath(), loupe::validation::OutputUnit::Millimeter, 1, result, {(result.minimum.x+result.maximum.x)/2.0,(result.minimum.y+result.maximum.y)/2.0,(result.minimum.z+result.maximum.z)/2.0}, 1.0e-5};
    }
    Bnd_Box bounds; BRepBndLib::Add(shape, bounds);
    double minX{}, minY{}, minZ{}, maxX{}, maxY{}, maxZ{}; bounds.Get(minX, minY, minZ, maxX, maxY, maxZ);
    const double toMillimeters = row.format() == loupe::exporting::Format::Step && row.stepOutputUnit() == loupe::exporting::StepOutputUnit::Inch ? 25.4 : 1.0;
    loupe::validation::Bounds result{{minX * toMillimeters, minY * toMillimeters, minZ * toMillimeters}, {maxX * toMillimeters, maxY * toMillimeters, maxZ * toMillimeters}};
    return {row.finalPath(), row.format() == loupe::exporting::Format::Stl ? loupe::validation::OutputUnit::Millimeter : row.stepOutputUnit() == loupe::exporting::StepOutputUnit::Inch ? loupe::validation::OutputUnit::Inch : loupe::validation::OutputUnit::Millimeter,
            1, result, {(result.minimum.x + result.maximum.x) / 2.0, (result.minimum.y + result.maximum.y) / 2.0, (result.minimum.z + result.maximum.z) / 2.0}, row.format() == loupe::exporting::Format::Stl ? 1.0e-3 : 1.0e-5};
}

int runCorpus(const std::vector<std::string>& args)
{
    if (args.size() != 4 || args[0] != "corpus") fail(invalidArguments, "usage: corpus <cases.json> --evidence <dir>");
    const std::filesystem::path caseFile(args[1]); const std::filesystem::path evidence = optionValue(args, "--evidence");
    const nlohmann::json document = loadCases(caseFile); nlohmann::json results = nlohmann::json::array();
    loupe::report::EvidenceWriter writer;
    bool requiredFailure{};
    for (const auto& value : document["cases"]) {
        const std::string id = value["id"].get<std::string>();
        const std::filesystem::path source = std::filesystem::path(value["file"].get<std::string>()).is_relative()
            ? caseFile.parent_path() / value["file"].get<std::string>() : std::filesystem::path(value["file"].get<std::string>());
        try {
            const auto [imported, decision] = importWithPolicy({source, std::nullopt});
            nlohmann::json caseResult{{"id", id}, {"sourceHash", imported.snapshot.sourceHash}, {"validationRows", nlohmann::json::array()}};
            std::map<std::string, std::string> details{{"status", "failed"}};
            const auto started = std::chrono::steady_clock::now();
            try {
                const auto [secondImport, secondDecision] = importWithPolicy({source, std::nullopt});
                if (!sameImport(imported, decision, secondImport, secondDecision)) fail(corpusContractFailure, "non_deterministic_import");
                if (decision.blocksExport()) fail(reviewRequired, "unit_review_required");
                if (value.contains("expectedSourceHash") && value["expectedSourceHash"].get<std::string>() != imported.snapshot.sourceHash) fail(corpusContractFailure, "source_hash_mismatch");
                if (value.value("expectedClass", className(imported.snapshot.classification)) != className(imported.snapshot.classification)) fail(corpusContractFailure, "classification_mismatch");
                if (value.contains("expectedDeclaredUnits")) {
                    nlohmann::json declared = nlohmann::json::array(); for (const auto unit : imported.unitEvidence.declaredRepresentationUnits) declared.push_back(unitName(unit));
                    if (declared != value["expectedDeclaredUnits"]) fail(corpusContractFailure, "declared_units_mismatch");
                }
                const auto& definition = representative(imported, loupe::domain::NodeKind::Definition);
                const auto& occurrence = representative(imported, loupe::domain::NodeKind::Occurrence);
                caseResult["selected"] = {{"definition", {{"id", definition.id}, {"path", definition.hierarchyPath}}}, {"occurrence", {{"id", occurrence.id}, {"path", occurrence.hierarchyPath}}}};
                struct Proof { const loupe::domain::AssemblyNode* node; loupe::exporting::SelectionKind kind; loupe::exporting::Format format; loupe::exporting::Coordinates coordinates; const char* name; };
                const std::array<Proof, 4> proofs{{{&definition, loupe::exporting::SelectionKind::Definition, loupe::exporting::Format::Step, loupe::exporting::Coordinates::Local, "definition_step_local"}, {&occurrence, loupe::exporting::SelectionKind::Occurrence, loupe::exporting::Format::Step, loupe::exporting::Coordinates::Assembly, "occurrence_step_assembly"}, {&definition, loupe::exporting::SelectionKind::Definition, loupe::exporting::Format::Stl, loupe::exporting::Coordinates::Local, "definition_stl_local"}, {&occurrence, loupe::exporting::SelectionKind::Occurrence, loupe::exporting::Format::Stl, loupe::exporting::Coordinates::Assembly, "occurrence_stl_assembly"}}};
                std::vector<loupe::validation::ValidationResult> validationRows;
                for (const auto& proof : proofs) {
                    const auto row = makeRow(imported, decision, *proof.node, proof.kind, proof.format, proof.coordinates, evidence / "exports" / id);
                    const auto expected = expectedBeforeExport(imported, row);
                    if (proof.format == loupe::exporting::Format::Step) static_cast<void>(loupe::exporting::StepExporter{}.write(imported, row)); else static_cast<void>(loupe::exporting::StlExporter{}.write(imported, row));
                    const auto validation = loupe::validation::OutputValidator{}.validate(expected); validationRows.push_back(validation);
                    nlohmann::json errors = nlohmann::json::array(); for (const auto& issue : validation.errors) errors.push_back(issue.code);
                    caseResult["validationRows"].push_back({{"name", proof.name}, {"passed", validation.passed}, {"path", std::filesystem::path(row.finalPath()).filename().string()}, {"errors", errors}});
                    if (!validation.passed) fail(validationFailure, "validation_failed");
                }
                details["status"] = "passed"; details["definitionId"] = definition.id; details["definitionPath"] = definition.hierarchyPath; details["occurrenceId"] = occurrence.id; details["occurrencePath"] = occurrence.hierarchyPath;
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
                details["elapsedMilliseconds"] = std::to_string(elapsed);
                static_cast<void>(writer.write(evidence, id, imported.snapshot, validationRows, {{{"caseId", id}, {"sourceHash", imported.snapshot.sourceHash}, {"elapsedMilliseconds", std::to_string(elapsed)}, {"status", "passed"}}}, metadataFor(imported, decision), details));
                caseResult["status"] = "passed"; results.push_back(caseResult);
            } catch (const CommandError& error) {
                requiredFailure = true; const std::string stable = error.message; details["failureCode"] = stable; details["exitCode"] = std::to_string(error.code);
                static_cast<void>(writer.write(evidence, id, imported.snapshot, {}, {}, metadataFor(imported, decision), details));
                caseResult["status"] = "failed"; caseResult["failureCode"] = stable; results.push_back(caseResult);
            } catch (const std::exception& error) {
                requiredFailure = true; details["failureCode"] = "export_failure"; details["error"] = error.what(); static_cast<void>(writer.write(evidence, id, imported.snapshot, {}, {}, metadataFor(imported, decision), details));
                caseResult["status"] = "failed"; caseResult["failureCode"] = "export_failure"; caseResult["error"] = error.what(); results.push_back(caseResult);
            }
        } catch (const CommandError& error) {
            if (error.code == importFailure) {
                results.push_back({{"id", id}, {"status", "import_failed"}});
                requiredFailure = true;
                continue;
            }
            throw;
        }
    }
    printJson({{"caseCount", document["cases"].size()}, {"cases", results}});
    return requiredFailure ? corpusContractFailure : success;
}

int runBenchmark(const std::vector<std::string>& args)
{
    if (args.size() != 4 || args[0] != "benchmark" || args[2] != "--out") fail(invalidArguments, "usage: benchmark <cases.json> --out <directory>");
    const std::filesystem::path caseFile(args[1]); const std::filesystem::path outputDirectory = optionValue(args, "--out"); const nlohmann::json document = loadCases(caseFile);
    std::filesystem::create_directories(outputDirectory);
    const auto csvPath = outputDirectory / "metrics.csv";
    std::ofstream output(csvPath); if (!output) fail(corpusContractFailure, "unable to write benchmark CSV");
    output << "caseId,sourceHash,shellReadyMs,fileAcknowledgementMs,treeReadyMs,coarseViewMs,firstInteractionMs,cachedReopenMs,selectionLatencyP50Ms,selectionLatencyP95Ms,frameTimeP50Ms,frameTimeP95Ms,peakMemoryBytes,idleCpuPercent,status\n";
    nlohmann::json results = nlohmann::json::array();
    const nlohmann::json unavailable = nlohmann::json::array({"shellReadyMs", "fileAcknowledgementMs", "coarseViewMs", "firstInteractionMs", "cachedReopenMs", "selectionLatencyP50Ms", "selectionLatencyP95Ms", "frameTimeP50Ms", "frameTimeP95Ms", "peakMemoryBytes", "idleCpuPercent"});
    for (const auto& value : document["cases"]) {
        const std::string id = value["id"].get<std::string>(); const std::filesystem::path source = std::filesystem::path(value["file"].get<std::string>()).is_relative() ? caseFile.parent_path() / value["file"].get<std::string>() : std::filesystem::path(value["file"].get<std::string>());
        nlohmann::json row{{"caseId", id}, {"sourceHash", nullptr}, {"shellReadyMs", nullptr}, {"fileAcknowledgementMs", nullptr}, {"treeReadyMs", nullptr}, {"coarseViewMs", nullptr}, {"firstInteractionMs", nullptr}, {"cachedReopenMs", nullptr}, {"selectionLatencyP50Ms", nullptr}, {"selectionLatencyP95Ms", nullptr}, {"frameTimeP50Ms", nullptr}, {"frameTimeP95Ms", nullptr}, {"peakMemoryBytes", nullptr}, {"idleCpuPercent", nullptr}, {"status", "import_failed"}, {"unavailableMetrics", unavailable}};
        const auto started = std::chrono::steady_clock::now();
        try {
            const auto [imported, decision] = importWithPolicy({source, std::nullopt});
            row["sourceHash"] = imported.snapshot.sourceHash;
            row["treeReadyMs"] = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count());
            row["status"] = decision.blocksExport() ? "unit_review_required" : "ready";
        } catch (const CommandError&) { }
        output << csvCell(id) << ',' << (row["sourceHash"].is_string() ? csvCell(row["sourceHash"].get<std::string>()) : std::string{})
               << ",,," << (row["treeReadyMs"].is_number() ? std::to_string(row["treeReadyMs"].get<std::uint64_t>()) : std::string{})
               << ",,,,,,,,,," << csvCell(row["status"].get<std::string>()) << '\n';
        results.push_back(std::move(row));
    }
    if (!output) fail(corpusContractFailure, "unable to write benchmark CSV");
    std::ofstream report(outputDirectory / "metrics.json"); if (!report) fail(corpusContractFailure, "unable to write benchmark JSON");
    report << nlohmann::json{{"schemaVersion", 1}, {"cases", results}}.dump(2) << '\n';
    if (!report) fail(corpusContractFailure, "unable to write benchmark JSON");
    printJson({{"caseCount", document["cases"].size()}, {"directory", outputDirectory.filename().string()}}); return success;
}

int runExport(const std::vector<std::string>& args)
{
    if (args.size() != 14 || args[0] != "export" || args[2] != "--selection" || args[4] != "--kind" || args[6] != "--format" || args[8] != "--coordinates" || args[10] != "--output-unit" || args[12] != "--out") fail(invalidArguments, "invalid export arguments");
    const auto [imported, decision] = importWithPolicy({args[1], std::nullopt}); if (decision.blocksExport()) { printJson(inspectJson(imported, decision)); return reviewRequired; }
    const auto kind = parseSelectionKind(args[5]);
    const auto format = parseFormat(args[7]);
    const auto coordinates = parseCoordinates(args[9]);
    const auto outputUnit = parseOutputUnit(args[11]);
    loupe::exporting::PlanRequest request; request.selections.push_back({args[3], kind}); request.destination = args[13]; request.format = format; request.coordinates = coordinates; request.grouping = loupe::exporting::Grouping::SeparateFiles; request.stepOutputUnit = outputUnit; request.requestedUnitToMillimeters = outputUnit == loupe::exporting::StepOutputUnit::Inch ? 25.4 : 1.0; request.unitDecision = decision;
    for (const auto& node : imported.snapshot.nodes) request.hierarchyPaths.emplace(node.id, node.hierarchyPath);
    try {
        const auto plan = loupe::exporting::buildPlan(request);
        const auto& row = plan.outputs().front();
        const auto expected = expectedBeforeExport(imported, row);
        if (format == loupe::exporting::Format::Step) static_cast<void>(loupe::exporting::StepExporter{}.write(imported, row));
        else static_cast<void>(loupe::exporting::StlExporter{}.write(imported, row));
        const std::filesystem::path output(row.finalPath());
        loupe::validation::OutputValidator validator;
        const auto validation = validator.validate(expected);
        if (!validation.passed) fail(validationFailure, "export read-back validation failed");
        printJson({{"status", "exported"}, {"path", output.filename().string()}, {"fingerprint", plan.fingerprint()}, {"validationPassed", true}});
        return success;
    } catch (const std::exception& error) { fail(exportFailure, error.what()); }
}

// ---------------------------------------------------------------------------
// Drawing spike: Gate A of the 2D drawing export plan.
//
// Answers the four questions that gate the feature, on real geometry:
//   1. Is orthographic hidden-line removal exactly 1:1?
//   2. How long does it take on a real assembly?
//   3. How often does OCCT silently coarsen a curve to its 15-point,
//      degree-1 fallback (which has no tolerance control)?
//   4. Do circles survive as circles, or arrive pre-tessellated?
// ---------------------------------------------------------------------------

std::string curveTypeName(const GeomAbs_CurveType type)
{
    switch (type) {
    case GeomAbs_Line: return "line";
    case GeomAbs_Circle: return "circle";
    case GeomAbs_Ellipse: return "ellipse";
    case GeomAbs_Hyperbola: return "hyperbola";
    case GeomAbs_Parabola: return "parabola";
    case GeomAbs_BezierCurve: return "bezier";
    case GeomAbs_BSplineCurve: return "bspline";
    case GeomAbs_OffsetCurve: return "offset";
    case GeomAbs_OtherCurve: return "other";
    }
    return "unknown";
}

gp_Dir parseViewAxis(const std::string_view value)
{
    if (value == "X") return {1.0, 0.0, 0.0};
    if (value == "-X") return {-1.0, 0.0, 0.0};
    if (value == "Y") return {0.0, 1.0, 0.0};
    if (value == "-Y") return {0.0, -1.0, 0.0};
    if (value == "Z") return {0.0, 0.0, 1.0};
    if (value == "-Z") return {0.0, 0.0, -1.0};
    fail(invalidArguments, "axis must be one of X, -X, Y, -Y, Z, -Z");
}

// gp_Ax2's main direction is the view direction; its X direction sets the
// in-plane orientation. Pick a reference up vector that cannot be parallel to
// the view direction, otherwise the cross product is degenerate and OCCT throws.
gp_Ax2 projectorAxes(const gp_Dir& viewDirection)
{
    const gp_Dir reference = std::abs(viewDirection.Z()) > 0.9
        ? gp_Dir(0.0, 1.0, 0.0) : gp_Dir(0.0, 0.0, 1.0);
    return {gp_Pnt(0.0, 0.0, 0.0), viewDirection, reference.Crossed(viewDirection)};
}

struct HlrStats {
    std::map<std::string, int> curveTypes;
    int edges{};
    int nullPcurves{};
    int coarseFallback{};
};

// Accumulate curve-type counts and the 2D extent of one HLR result compound.
//
// The result edges carry NO 3D curve -- only a stored pcurve on BRepLib's global
// plane -- so BRepAdaptor_Curve fails on them.
//
// Use the CurveOnSurface overload that hands back the pcurve, its surface, and
// its location, i.e. ask the edge what it actually stored. Do NOT use
// CurveOnPlane: despite the name it does not read a stored planar pcurve, it
// *projects the edge's 3D curve* onto the plane you supply, so on an HLR result
// edge (which has no 3D curve) it returns null for every single edge. Measured:
// 171 of 171 null.
void accumulateHlrCompound(const TopoDS_Shape& compound,
                           HlrStats& stats,
                           Bnd_Box2d& extent)
{
    if (compound.IsNull()) return;
    for (TopExp_Explorer explorer(compound, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
        ++stats.edges;
        double first = 0.0;
        double last = 0.0;
        occ::handle<Geom2d_Curve> curve;
        occ::handle<Geom_Surface> surface;
        TopLoc_Location location;
        BRep_Tool::CurveOnSurface(edge, curve, surface, location, first, last);
        if (curve.IsNull()) {
            ++stats.nullPcurves;
            continue;
        }
        const Geom2dAdaptor_Curve adaptor(curve, first, last);
        const auto type = adaptor.GetType();
        stats.curveTypes[curveTypeName(type)] += 1;
        // OCCT's HLRBRep::MakeEdge falls back to a 15-pole, degree-1 B-spline
        // for curve types it cannot classify -- a 14-segment polyline with no
        // tolerance control. On a cut path that is a silent accuracy cliff.
        if (type == GeomAbs_BSplineCurve) {
            const auto bspline = adaptor.BSpline();
            if (!bspline.IsNull() && bspline->Degree() == 1 && bspline->NbPoles() == 15) {
                ++stats.coarseFallback;
            }
        }
        BndLib_Add2dCurve::Add(adaptor, 0.0, extent);
    }
}

struct HlrRun {
    HlrStats sharp;
    HlrStats outline;
    Bnd_Box2d extent;
    double milliseconds{};
};

HlrRun runHiddenLineRemoval(const TopoDS_Shape& shape, const gp_Dir& viewDirection)
{
    const auto plane = BRepLib::Plane();
    if (plane.IsNull()) fail(exportFailure, "BRepLib plane is null");
    // BRepLib::Plane() is a mutable global static that HLR results are expressed
    // on. Assert it is still XOY: if any other code has replaced it, every
    // coordinate below would be silently measured against a different plane.
    if (!plane->Position().Direction().IsEqual(gp_Dir(0.0, 0.0, 1.0), 1.0e-12)) {
        fail(exportFailure, "BRepLib plane is not XOY; HLR output frame is not the expected plane");
    }

    const auto started = std::chrono::steady_clock::now();
    occ::handle<HLRBRep_Algo> algo = new HLRBRep_Algo();
    // nbIso = 0: no isoparametric lines. A drawing wants edges, not surface grids.
    algo->Add(shape, 0);
    algo->Projector(HLRAlgo_Projector(projectorAxes(viewDirection)));
    algo->Update();
    algo->Hide();
    HLRBRep_HLRToShape toShape(algo);
    const TopoDS_Shape sharp = toShape.VCompound();
    const TopoDS_Shape outline = toShape.OutLineVCompound();
    const auto finished = std::chrono::steady_clock::now();

    HlrRun run;
    run.milliseconds = std::chrono::duration<double, std::milli>(finished - started).count();
    accumulateHlrCompound(sharp, run.sharp, run.extent);
    accumulateHlrCompound(outline, run.outline, run.extent);
    return run;
}

std::array<double, 2> sortedExtent(const Bnd_Box2d& box)
{
    if (box.IsVoid()) return {0.0, 0.0};
    double xMin = 0.0;
    double yMin = 0.0;
    double xMax = 0.0;
    double yMax = 0.0;
    box.Get(xMin, yMin, xMax, yMax);
    std::array<double, 2> extent{xMax - xMin, yMax - yMin};
    if (extent[0] > extent[1]) std::swap(extent[0], extent[1]);
    return extent;
}

// A rigid orthographic projection of an axis-aligned box must reproduce two of
// its three dimensions exactly. This is the 1:1 claim, asserted rather than
// assumed: the projector forces its own scale factor to 1, so any deviation is
// a bug in our transform handling, not an inherent approximation.
nlohmann::json verifyExactScale()
{
    constexpr double length = 100.0;
    constexpr double width = 50.0;
    constexpr double height = 10.0;
    constexpr double tolerance = 1.0e-9;
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(length, width, height).Shape();

    const std::array<std::pair<std::string, std::array<double, 2>>, 3> cases{{
        {"Z", {width, length}},
        {"Y", {height, length}},
        {"X", {height, width}},
    }};

    nlohmann::json results = nlohmann::json::array();
    bool allPassed = true;
    for (const auto& [axis, expectedUnsorted] : cases) {
        auto expected = expectedUnsorted;
        if (expected[0] > expected[1]) std::swap(expected[0], expected[1]);
        const auto run = runHiddenLineRemoval(box, parseViewAxis(axis));
        const auto measured = sortedExtent(run.extent);
        const double errorA = std::abs(measured[0] - expected[0]);
        const double errorB = std::abs(measured[1] - expected[1]);
        const bool passed = errorA <= tolerance && errorB <= tolerance;
        allPassed = allPassed && passed;
        results.push_back({{"axis", axis},
                           {"expectedMm", {expected[0], expected[1]}},
                           {"measuredMm", {measured[0], measured[1]}},
                           {"maxErrorMm", std::max(errorA, errorB)},
                           {"passed", passed}});
    }
    return {{"toleranceMm", tolerance}, {"passed", allPassed}, {"cases", results}};
}

// Does an analytic circle survive projection as a circle, or does HLR hand back
// a tessellated B-spline? This decides whether DXF can emit exact CIRCLE/ARC
// entities and polyline bulges, or whether every curved feature has to be
// approximated. A cylinder viewed down its own axis is the controlled case:
// its rim is a circle exactly parallel to the view plane.
nlohmann::json probeCurveFidelity()
{
    const TopoDS_Shape cylinder = BRepPrimAPI_MakeCylinder(10.0, 20.0).Shape();
    const auto down = runHiddenLineRemoval(cylinder, gp_Dir(0.0, 0.0, 1.0));
    const auto side = runHiddenLineRemoval(cylinder, gp_Dir(1.0, 0.0, 0.0));

    std::map<std::string, int> axialTypes = down.sharp.curveTypes;
    for (const auto& [name, count] : down.outline.curveTypes) axialTypes[name] += count;
    std::map<std::string, int> sideTypes = side.sharp.curveTypes;
    for (const auto& [name, count] : side.outline.curveTypes) sideTypes[name] += count;

    const bool circlePreserved = axialTypes.contains("circle");
    return {{"axialViewTypes", axialTypes},
            {"sideViewTypes", sideTypes},
            {"circlePreservedAnalytically", circlePreserved},
            {"axialExtentMm", sortedExtent(down.extent)[1]}};
}

nlohmann::json curveTypeJson(const HlrStats& stats)
{
    return {{"edges", stats.edges},
            {"nullPcurves", stats.nullPcurves},
            {"coarseFallbackEdges", stats.coarseFallback},
            {"curveTypes", stats.curveTypes}};
}

// Per-body hidden-line removal, for isolating a body that poisons a whole-assembly
// projection. Whole-assembly HLR reports nothing on failure rather than raising, so the only
// way to find the offender is to project each body on its own and compare.
int runDrawingBodies(const std::vector<std::string>& args)
{
    if (args.size() < 2) fail(invalidArguments, "usage: drawing-bodies <file.step> [axis]");
    const std::filesystem::path file(args[1]);
    // Accepts a named axis or "x,y,z", so an exactly-axis-aligned direction can be compared
    // against one nudged a fraction off it. Hidden-line removal is known to struggle when
    // every face is exactly parallel or perpendicular to the view, and that hypothesis is
    // only testable with an off-axis direction.
    const auto axisArgument = args.size() > 2 ? args[2] : std::string("Z");
    gp_Dir viewDirection(0.0, 0.0, 1.0);
    if (axisArgument.find(',') != std::string::npos) {
        std::istringstream stream(axisArgument);
        std::string part;
        std::vector<double> components;
        while (std::getline(stream, part, ',')) components.push_back(std::stod(part));
        if (components.size() != 3) fail(invalidArguments, "direction needs three components");
        viewDirection = gp_Dir(components[0], components[1], components[2]);
    } else {
        viewDirection = parseViewAxis(axisArgument);
    }

    loupe::import::ImportResult imported;
    try {
        imported = loupe::import::StepImporter{}.read(file);
    } catch (const std::exception& error) { fail(importFailure, error.what()); }

    const auto decision = loupe::units::decide(imported.unitEvidence, std::nullopt);
    const auto& native = *imported.native;
    const auto bodyCount = std::min(native.shapes.size(), native.shapePlacements.size());

    const auto scaled = [&decision](const TopoDS_Shape& input) {
        if (decision.sourceToMillimeters == 1.0) return input;
        gp_Trsf scale;
        scale.SetScale(gp_Pnt(0.0, 0.0, 0.0), decision.sourceToMillimeters);
        return BRepBuilderAPI_Transform(input, scale, true).Shape();
    };

    nlohmann::json bodies = nlohmann::json::array();
    for (std::size_t index = 0; index < bodyCount; ++index) {
        nlohmann::json entry{{"index", index}};
        if (native.shapes[index].IsNull()) {
            entry["skipped"] = "null shape";
            bodies.push_back(entry);
            continue;
        }
        if (index < native.shapeNodeIds.size()) entry["nodeId"] = native.shapeNodeIds[index];
        const auto located = native.shapes[index].Located(TopLoc_Location(native.shapePlacements[index]));
        int solids = 0;
        int faces = 0;
        for (TopExp_Explorer explorer(located, TopAbs_SOLID); explorer.More(); explorer.Next()) ++solids;
        for (TopExp_Explorer explorer(located, TopAbs_FACE); explorer.More(); explorer.Next()) ++faces;
        entry["solids"] = solids;
        entry["faces"] = faces;
        try {
            const auto run = runHiddenLineRemoval(scaled(located), viewDirection);
            const auto extent = sortedExtent(run.extent);
            entry["edges"] = run.sharp.edges + run.outline.edges;
            entry["extentMm"] = {extent[0], extent[1]};
            entry["milliseconds"] = run.milliseconds;
        } catch (const CommandError& error) {
            entry["error"] = error.message;
        } catch (const std::exception& error) {
            entry["error"] = error.what();
        }
        bodies.push_back(entry);
    }

    printJson({{"status", "ok"},
               {"axis", args.size() > 2 ? args[2] : "Z"},
               {"effectiveUnit", unitName(decision.effectiveUnit)},
               {"sourceToMillimeters", decision.sourceToMillimeters},
               {"bodies", bodies}});
    return success;
}

// Sweeps every standard view in both content modes and cross-checks them against each other.
//
// The invariant that makes this worth automating: a silhouette is a *filter over the same
// projection* as the cut outline, so its bounding box can never exceed the cut outline's. Any
// view where it does is a defect, and eyeballing previews one at a time will not find them all.
int runDrawingAudit(const std::vector<std::string>& args)
{
    if (args.size() < 2) fail(invalidArguments, "usage: drawing-audit <file.step>");
    const std::filesystem::path file(args[1]);

    loupe::import::ImportResult imported;
    try {
        imported = loupe::import::StepImporter{}.read(file);
    } catch (const std::exception& error) { fail(importFailure, error.what()); }

    const auto decision = loupe::units::decide(imported.unitEvidence, std::nullopt);
    const auto& native = *imported.native;
    const auto bodyCount = std::min(native.shapes.size(), native.shapePlacements.size());

    BRep_Builder builder;
    TopoDS_Compound assembly;
    builder.MakeCompound(assembly);
    for (std::size_t index = 0; index < bodyCount; ++index) {
        if (native.shapes[index].IsNull()) continue;
        builder.Add(assembly, native.shapes[index].Located(TopLoc_Location(native.shapePlacements[index])));
    }
    TopoDS_Shape shape = assembly;
    if (decision.sourceToMillimeters != 1.0) {
        gp_Trsf scale;
        scale.SetScale(gp_Pnt(0.0, 0.0, 0.0), decision.sourceToMillimeters);
        shape = BRepBuilderAPI_Transform(assembly, scale, true).Shape();
    }

    const std::vector<std::pair<std::string, gp_Dir>> views{
        {"Top", {0.0, 0.0, 1.0}},   {"Bottom", {0.0, 0.0, -1.0}},
        {"Front", {0.0, -1.0, 0.0}}, {"Back", {0.0, 1.0, 0.0}},
        {"Right", {1.0, 0.0, 0.0}}, {"Left", {-1.0, 0.0, 0.0}},
    };

    nlohmann::json rows = nlohmann::json::array();
    int oversizedSilhouettes = 0;
    // A silhouette that produced nothing where the cut outline has geometry. Counted
    // separately because it is the failure a fix for oversizing can accidentally
    // create: drop the offending edges and the view goes empty, which the oversize
    // check alone would report as success.
    int emptySilhouettes = 0;
    for (const auto& [name, direction] : views) {
        const gp_Dir up = std::abs(direction.Z()) > 0.9 ? gp_Dir(0.0, 1.0, 0.0) : gp_Dir(0.0, 0.0, 1.0);
        nlohmann::json row{{"view", name}};
        const auto run = [&](const loupe::drawing::ContentMode mode) {
            loupe::drawing::ProjectionRequest request;
            request.shape = shape;
            request.viewDirection = direction;
            request.upDirection = up;
            request.mode = mode;
            request.sourceToMillimeters = 1.0;
            return loupe::drawing::project(request);
        };
        double cutWidth = 0.0;
        double cutHeight = 0.0;
        bool cutOk = false;
        try {
            const auto cut = run(loupe::drawing::ContentMode::CutContours);
            const auto bounds = cut.drawing.bounds();
            cutWidth = bounds.width();
            cutHeight = bounds.height();
            cutOk = true;
            row["cut"] = {{"widthMm", cutWidth}, {"heightMm", cutHeight},
                          {"closed", cut.statistics.closedContours},
                          {"open", cut.statistics.openContours},
                          {"approximate", cut.approximate}};
        } catch (const std::exception& error) { row["cut"] = {{"error", error.what()}}; }
        try {
            const auto silhouette = run(loupe::drawing::ContentMode::OuterContourOnly);
            const auto bounds = silhouette.drawing.bounds();
            row["silhouette"] = {{"widthMm", bounds.width()}, {"heightMm", bounds.height()},
                                 {"closed", silhouette.statistics.closedContours},
                                 {"open", silhouette.statistics.openContours},
                                 {"approximate", silhouette.approximate}};
            if (cutOk) {
                // A tolerance, not equality: the two modes tessellate curves independently.
                const double slack = 0.05;
                const bool oversized = bounds.width() > cutWidth + slack
                    || bounds.height() > cutHeight + slack;
                row["silhouetteOversized"] = oversized;
                if (oversized) ++oversizedSilhouettes;
                const bool empty = !bounds.valid
                    || (bounds.width() <= slack && bounds.height() <= slack);
                const bool cutHasGeometry = cutWidth > slack || cutHeight > slack;
                row["silhouetteEmpty"] = empty && cutHasGeometry;
                if (empty && cutHasGeometry) ++emptySilhouettes;
            }
        } catch (const std::exception& error) { row["silhouette"] = {{"error", error.what()}}; }
        rows.push_back(row);
    }

    printJson({{"status", "ok"},
               {"bodies", bodyCount},
               {"effectiveUnit", unitName(decision.effectiveUnit)},
               {"oversizedSilhouettes", oversizedSilhouettes},
               {"emptySilhouettes", emptySilhouettes},
               {"views", rows}});
    return oversizedSilhouettes == 0 && emptySilhouettes == 0 ? success : validationFailure;
}

int runDrawingSpike(const std::vector<std::string>& args)
{
    if (args.size() < 2) fail(invalidArguments, "usage: drawing-spike <file.step> [axis]");
    const std::filesystem::path file(args[1]);
    const gp_Dir viewDirection = parseViewAxis(args.size() > 2 ? args[2] : "Z");

    const auto exactScale = verifyExactScale();
    const auto curveFidelity = probeCurveFidelity();

    loupe::import::ImportResult imported;
    try {
        imported = loupe::import::StepImporter{}.read(file);
    } catch (const std::exception& error) { fail(importFailure, error.what()); }

    const auto decision = loupe::units::decide(imported.unitEvidence, std::nullopt);

    // Compose every body at its assembly placement into one shape. HLR must be
    // fed a single pre-located shape: adding N located sub-shapes and hiding
    // them pairwise is quadratic and far slower.
    BRep_Builder builder;
    TopoDS_Compound assembly;
    builder.MakeCompound(assembly);
    const auto& native = *imported.native;
    const auto bodyCount = std::min(native.shapes.size(), native.shapePlacements.size());
    for (std::size_t index = 0; index < bodyCount; ++index) {
        if (native.shapes[index].IsNull()) continue;
        builder.Add(assembly, native.shapes[index].Located(TopLoc_Location(native.shapePlacements[index])));
    }

    // Scale the shape, not the projector: HLRAlgo_Projector forces its own
    // transform's scale factor to 1, so a scale baked into the projector is
    // silently discarded for orthographic views.
    TopoDS_Shape shape = assembly;
    if (decision.sourceToMillimeters != 1.0) {
        gp_Trsf scale;
        scale.SetScale(gp_Pnt(0.0, 0.0, 0.0), decision.sourceToMillimeters);
        shape = BRepBuilderAPI_Transform(assembly, scale, true).Shape();
    }

    HlrRun run;
    try {
        run = runHiddenLineRemoval(shape, viewDirection);
    } catch (const Standard_Failure& error) {
        fail(exportFailure, std::string("hidden line removal failed: ") + error.GetMessageString());
    } catch (const std::exception& error) { fail(exportFailure, error.what()); }

    const auto extent = sortedExtent(run.extent);
    const int totalEdges = run.sharp.edges + run.outline.edges;
    const int totalFallback = run.sharp.coarseFallback + run.outline.coarseFallback;

    // Optional third argument: write real DXF/SVG/PDF from this part so the output
    // can be opened in the software that will actually consume it. Gate C cannot
    // be closed by unit tests alone -- only a real reader proves a DXF loads
    // without a repair prompt.
    nlohmann::json written = nlohmann::json::array();
    if (args.size() > 3) {
        const std::filesystem::path outputDirectory(args[3]);
        std::filesystem::create_directories(outputDirectory);
        loupe::drawing::ProjectionRequest projection;
        projection.shape = shape;
        projection.viewDirection = viewDirection;
        projection.upDirection = std::abs(viewDirection.Z()) > 0.9 ? gp_Dir(0.0, 1.0, 0.0)
                                                                  : gp_Dir(0.0, 0.0, 1.0);
        projection.sourceToMillimeters = 1.0; // shape is already in millimetres here
        // 6th argument selects the silhouette mode, so the outer-contour filter can
        // be measured against the same part in both modes.
        if (args.size() > 5 && args[5] == "silhouette") {
            projection.mode = loupe::drawing::ContentMode::OuterContourOnly;
        }
        try {
            const auto projected = loupe::drawing::project(projection);
            loupe::drawing::DrawingWriteOptions options;
            // Off by default, matching the product decision: the fiducial is a
            // deliberate opt-in, and leaving it on put an unexplained magenta line
            // across every sample drawing. Pass a 5th argument to enable it.
            // Check the value, not merely that an argument exists: gating on
            // arity alone meant passing "no" switched the fiducial ON.
            options.includeScaleFiducial = args.size() > 4 && args[4] == "fiducial";
            const auto emit = [&](const std::string& name, const auto& writer) {
                const auto path = outputDirectory / name;
                const auto result = writer.write(projected.drawing, path, options);
                const auto validation = loupe::drawing::validateDrawing(
                    {path, result.pageWidthMm, result.pageHeightMm, 0.01});
                written.push_back({{"file", name},
                                   {"widthMm", result.pageWidthMm},
                                   {"heightMm", result.pageHeightMm},
                                   {"contours", result.contoursWritten},
                                   {"validated", validation.passed}});
            };
            // Also render what the on-screen 2D preview draws, from encodePreview's own
            // flattened polylines rather than from the exact primitives the writers use.
            // The preview has a separate failure mode -- it once reported correct extents
            // while drawing nothing -- and only this path exercises it outside the app.
            {
                const auto encoded = loupe::drawing::encodePreview(projected);
                const auto document = nlohmann::json::parse(encoded);
                const auto width = document.at("widthMm").get<double>();
                const auto height = document.at("heightMm").get<double>();
                const auto originX = document.at("minX").get<double>();
                const auto originY = document.at("minY").get<double>();
                std::ostringstream svg;
                svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width
                    << "mm\" height=\"" << height << "mm\" viewBox=\"0 0 " << width << ' ' << height
                    << "\">\n";
                int polylines = 0;
                int points = 0;
                for (const auto& layer : document.at("layers")) {
                    for (const auto& contour : layer.at("contours")) {
                        const auto closed = contour.at("closed").get<bool>();
                        const auto& coordinates = contour.at("points");
                        if (coordinates.size() < 4) continue;
                        svg << "  <polyline fill=\"none\" stroke=\""
                            << (closed ? "#3b3bd6" : "#c07000") << "\" stroke-width=\"0.2\" points=\"";
                        for (std::size_t index = 0; index + 1 < coordinates.size(); index += 2) {
                            // Y flipped exactly as the QML preview flips it, so this image and
                            // the on-screen preview cannot disagree about orientation.
                            svg << (coordinates[index].get<double>() - originX) << ','
                                << (height - (coordinates[index + 1].get<double>() - originY)) << ' ';
                            ++points;
                        }
                        svg << "\"/>\n";
                        ++polylines;
                    }
                }
                svg << "</svg>\n";
                const auto previewPath = outputDirectory / "preview.svg";
                std::ofstream out(previewPath, std::ios::binary);
                out << svg.str();
                if (!out) throw std::runtime_error("unable to write preview.svg");
                written.push_back({{"file", "preview.svg"},
                                   {"widthMm", width},
                                   {"heightMm", height},
                                   {"approximate", document.at("approximate").get<bool>()},
                                   {"previewPolylines", polylines},
                                   {"previewPoints", points},
                                   {"closedContours", document.at("closedContours").get<int>()},
                                   {"openContours", document.at("openContours").get<int>()}});
            }
            emit("drawing.dxf", loupe::drawing::DxfWriter{});
            emit("drawing.svg", loupe::drawing::SvgWriter{});
            emit("drawing.pdf", loupe::drawing::PdfWriter{});
            const auto& stats = projected.statistics;
            written.push_back({{"projection",
                                {{"edges", stats.edges},
                                 {"interiorEdgesRemoved", stats.interiorEdgesRemoved},
                                 {"analyticLines", stats.analyticLines},
                                 {"analyticCircles", stats.analyticCircles},
                                 {"recoveredArcs", stats.recoveredArcs},
                                 {"exactCubics", stats.exactCubics},
                                 {"tessellatedCurves", stats.tessellatedCurves},
                                 {"coarseFallbackEdges", stats.coarseFallbackEdges},
                                 {"duplicatesRemoved", stats.duplicatesRemoved},
                                 {"closedContours", stats.closedContours},
                                 {"openContours", stats.openContours}}}});
        } catch (const std::exception& error) {
            written.push_back({{"error", error.what()}});
        }
    }

    printJson({{"status", "ok"},
               {"exactScale", exactScale},
               {"curveFidelity", curveFidelity},
               {"bodies", bodyCount},
               {"effectiveUnit", unitName(decision.effectiveUnit)},
               {"sourceToMillimeters", decision.sourceToMillimeters},
               {"hlrMilliseconds", run.milliseconds},
               {"visibleSharp", curveTypeJson(run.sharp)},
               {"visibleOutline", curveTypeJson(run.outline)},
               {"totalEdges", totalEdges},
               {"coarseFallbackEdges", totalFallback},
               {"coarseFallbackRatio", totalEdges > 0 ? static_cast<double>(totalFallback) / totalEdges : 0.0},
               {"extentMm", {extent[0], extent[1]}},
               {"written", written}});
    return exactScale.at("passed").get<bool>() ? success : validationFailure;
}

} // namespace

int main(int argc, char* argv[])
{
    try {
        std::vector<std::string> args; for (int index = 1; index < argc; ++index) args.emplace_back(argv[index]);
        if (args.empty()) fail(invalidArguments, "choose inspect, export, corpus, benchmark, drawing-spike, or drawing-bodies");
        if (args[0] == "inspect") return runInspect(args); if (args[0] == "export") return runExport(args); if (args[0] == "corpus") return runCorpus(args); if (args[0] == "benchmark") return runBenchmark(args);
        if (args[0] == "drawing-spike") return runDrawingSpike(args);
        if (args[0] == "drawing-bodies") return runDrawingBodies(args);
        if (args[0] == "drawing-audit") return runDrawingAudit(args);
        fail(invalidArguments, "unknown command");
    } catch (const CommandError& error) { printJson({{"status", "error"}, {"code", error.code}, {"message", error.message}}); return error.code;
    } catch (const std::exception& error) { printJson({{"status", "error"}, {"code", importFailure}, {"message", error.what()}}); return importFailure; }
}
