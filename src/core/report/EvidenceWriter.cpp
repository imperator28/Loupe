#include "core/report/EvidenceWriter.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

namespace loupe::report {
namespace {

nlohmann::json metadataJson(const EvidenceMetadata& metadata)
{
    return {{"schemaVersion", 1}, {"sourceHash", metadata.sourceHash}, {"importerVersion", metadata.importerVersion},
            {"effectiveUnitDecision", metadata.effectiveUnitDecision}, {"generatedAtUtc", metadata.generatedAtUtc}};
}

nlohmann::json point(const validation::Vec3& value) { return {{"x", value.x}, {"y", value.y}, {"z", value.z}}; }

const char* outputUnitName(const validation::OutputUnit unit)
{
    switch (unit) {
    case validation::OutputUnit::Millimeter: return "millimeter";
    case validation::OutputUnit::Inch: return "inch";
    case validation::OutputUnit::Unknown: return "unknown";
    }
    return "unknown";
}

nlohmann::json issues(const std::vector<validation::ValidationIssue>& values)
{
    nlohmann::json result = nlohmann::json::array();
    for (const auto& value : values) result.push_back({{"code", value.code}, {"message", value.message}});
    return result;
}

void writeJson(const std::filesystem::path& file, const nlohmann::json& value)
{
    std::ofstream output(file);
    if (!output) throw std::runtime_error("unable to open evidence JSON output");
    output << value.dump(2) << '\n';
    output.flush();
    if (!output) throw std::runtime_error("unable to write evidence JSON output");
}

std::string csvCell(std::string value)
{
    if (!value.empty() && (value.front() == '=' || value.front() == '+' || value.front() == '-' || value.front() == '@')) value.insert(value.begin(), '\'');
    if (value.find_first_of(",\"\r\n") != std::string::npos) {
        std::string escaped{"\""};
        for (const char character : value) { if (character == '\"') escaped += "\"\""; else escaped += character; }
        return escaped + "\"";
    }
    return value;
}

void validateCaseId(const std::string& caseId)
{
    const std::filesystem::path value(caseId);
    if (caseId.empty() || value.is_absolute() || value.has_parent_path() || caseId == "." || caseId == ".."
        || caseId.find('/') != std::string::npos || caseId.find('\\') != std::string::npos) {
        throw std::invalid_argument("evidence case id must be a safe leaf name");
    }
}

} // namespace

EvidencePaths EvidenceWriter::write(const std::filesystem::path& root, const std::string& caseId,
                                    const domain::AssemblySnapshot& snapshot,
                                    const std::vector<validation::ValidationResult>& validations,
                                    const std::vector<std::map<std::string, std::string>>& benchmarkRows,
                                    const EvidenceMetadata& metadata,
                                    const std::map<std::string, std::string>& failure) const
{
    validateCaseId(caseId);
    std::filesystem::create_directories(root);
    const auto normalizedCallerRoot = std::filesystem::weakly_canonical(root);
    const auto evidenceRoot = root / "evidence";
    std::filesystem::create_directories(evidenceRoot);
    const auto normalizedRoot = std::filesystem::weakly_canonical(evidenceRoot);
    if (normalizedRoot.parent_path() != normalizedCallerRoot) throw std::invalid_argument("evidence root escapes caller root");
    const auto directory = evidenceRoot / caseId;
    std::filesystem::create_directories(directory);
    const auto normalizedDirectory = std::filesystem::weakly_canonical(directory);
    if (normalizedDirectory.parent_path() != normalizedRoot) throw std::invalid_argument("evidence directory escapes root");
    EvidencePaths paths{directory / "assembly-snapshot.json", directory / "validation.json", directory / "benchmark.csv", directory / "failure.json"};
    auto snapshotJson = metadataJson(metadata);
    snapshotJson["classification"] = static_cast<int>(snapshot.classification);
    snapshotJson["nodeCount"] = snapshot.nodes.size();
    snapshotJson["warningCount"] = snapshot.warnings.size();
    writeJson(paths.assemblySnapshot, snapshotJson);

    auto validationJson = metadataJson(metadata); validationJson["results"] = nlohmann::json::array();
    for (const auto& value : validations) validationJson["results"].push_back({
        {"path", value.path.filename().string()}, {"reopened", value.reopened}, {"shapeValid", value.shapeValid}, {"passed", value.passed},
        {"actualBodyCount", value.actualBodyCount}, {"actualBoundsMm", {{"minimum", point(value.actualBoundsMm.minimum)}, {"maximum", point(value.actualBoundsMm.maximum)}}},
        {"actualCentroidMm", point(value.actualCentroidMm)}, {"actualUnit", outputUnitName(value.actualUnit)},
        {"warnings", issues(value.warnings)}, {"errors", issues(value.errors)}});
    writeJson(paths.validation, validationJson);

    auto failureJson = metadataJson(metadata); failureJson["failure"] = failure; writeJson(paths.failure, failureJson);
    std::ofstream csv(paths.benchmark);
    if (!csv) throw std::runtime_error("unable to open evidence benchmark output");
    if (!benchmarkRows.empty()) {
        bool first = true; for (const auto& [key, ignored] : benchmarkRows.front()) { if (!first) csv << ','; csv << csvCell(key); first = false; } csv << '\n';
        for (const auto& row : benchmarkRows) { first = true; for (const auto& [ignored, value] : row) { if (!first) csv << ','; csv << csvCell(value); first = false; } csv << '\n'; }
    }
    csv.flush();
    if (!csv) throw std::runtime_error("unable to write evidence benchmark output");
    return paths;
}

} // namespace loupe::report
