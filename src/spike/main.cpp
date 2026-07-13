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
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <Poly_Triangulation.hxx>
#include <TDF_Tool.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <chrono>
#include <array>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <fstream>
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

} // namespace

int main(int argc, char* argv[])
{
    try {
        std::vector<std::string> args; for (int index = 1; index < argc; ++index) args.emplace_back(argv[index]);
        if (args.empty()) fail(invalidArguments, "choose inspect, export, corpus, or benchmark");
        if (args[0] == "inspect") return runInspect(args); if (args[0] == "export") return runExport(args); if (args[0] == "corpus") return runCorpus(args); if (args[0] == "benchmark") return runBenchmark(args);
        fail(invalidArguments, "unknown command");
    } catch (const CommandError& error) { printJson({{"status", "error"}, {"code", error.code}, {"message", error.message}}); return error.code;
    } catch (const std::exception& error) { printJson({{"status", "error"}, {"code", importFailure}, {"message", error.what()}}); return importFailure; }
}
