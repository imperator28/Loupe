#include "fixtures/FixtureFactory.h"
#include "core/import/StepImporter.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class ScopedDirectory {
public:
    explicit ScopedDirectory(const std::string& name)
        : path_(std::filesystem::temp_directory_path() / ("loupe-spike-" + name + "-" + std::to_string(GetCurrentProcessId())))
    {
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }
    ~ScopedDirectory() { std::filesystem::remove_all(path_); }
    [[nodiscard]] const std::filesystem::path& path() const { return path_; }
private:
    std::filesystem::path path_;
};

struct SpikeResult { int exitCode{}; nlohmann::json json; };

std::wstring quote(const std::wstring& value)
{
    return L"\"" + value + L"\"";
}

SpikeResult runSpike(const std::vector<std::filesystem::path>& arguments)
{
    std::wstring command = quote(std::filesystem::path(LOUPE_SPIKE_PATH).wstring());
    for (const auto& argument : arguments) command += L" " + quote(argument.wstring());
    std::vector<wchar_t> buffer(command.begin(), command.end()); buffer.push_back(L'\0');
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), nullptr, TRUE};
    HANDLE readPipe{}; HANDLE writePipe{};
    if (!CreatePipe(&readPipe, &writePipe, &attributes, 0)) throw std::runtime_error("CreatePipe failed");
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW startup{}; startup.cb = sizeof(startup); startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writePipe; startup.hStdError = writePipe; startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, buffer.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
        CloseHandle(readPipe); CloseHandle(writePipe); throw std::runtime_error("CreateProcess failed");
    }
    CloseHandle(writePipe); CloseHandle(process.hThread);
    std::string output; char bytes[512]; DWORD count{};
    while (ReadFile(readPipe, bytes, sizeof(bytes), &count, nullptr) && count != 0) output.append(bytes, count);
    CloseHandle(readPipe); WaitForSingleObject(process.hProcess, INFINITE); DWORD exitCode{}; GetExitCodeProcess(process.hProcess, &exitCode); CloseHandle(process.hProcess);
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) output.pop_back();
    const auto jsonStart = output.find_last_of('\n');
    return {static_cast<int>(exitCode), nlohmann::json::parse(jsonStart == std::string::npos ? output : output.substr(jsonStart + 1))};
}

std::filesystem::path textArg(const char* value) { return std::filesystem::path(value); }

} // namespace

TEST_CASE("spike exits 2 for unresolved units", "[cli]")
{
    const ScopedDirectory directory{"missing-unit"};
    const auto fixture = directory.path() / "missing-unit.step";
    static_cast<void>(loupe::tests::writeSingleCylinderStep(fixture));
    std::ifstream input(fixture);
    std::string step((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    const auto unit = step.find("SI_UNIT(.MILLI.,.METRE.)");
    REQUIRE(unit != std::string::npos);
    step.replace(unit, std::string("SI_UNIT(.MILLI.,.METRE.)").size(), "SI_UNIT($,.METRE.)");
    std::ofstream(fixture, std::ios::trunc) << step;

    const auto result = runSpike({textArg("inspect"), fixture, textArg("--json")});
    REQUIRE(result.exitCode == 2);
    REQUIRE(result.json.at("status") == "unit_review_required");
}

TEST_CASE("spike export validates a generated selected definition", "[cli]")
{
    const ScopedDirectory directory{"export"};
    const auto fixture = loupe::tests::writeSingleCylinderStep(directory.path() / "part.step");
    const auto imported = loupe::import::StepImporter{}.read(fixture);
    const auto selected = std::ranges::find_if(imported.snapshot.nodes, [](const auto& node) {
        return node.kind == loupe::domain::NodeKind::Definition;
    });
    REQUIRE(selected != imported.snapshot.nodes.end());

    const auto result = runSpike({textArg("export"), fixture, textArg("--selection"), selected->id,
                                  textArg("--kind"), textArg("definition"), textArg("--format"), textArg("step"),
                                  textArg("--coordinates"), textArg("local"), textArg("--output-unit"), textArg("mm"),
                                  textArg("--out"), directory.path() / "out"});
    REQUIRE(result.exitCode == 0);
    REQUIRE(result.json.at("validationPassed") == true);
}

TEST_CASE("spike corpus command reports every generated full-flow assembly without copying geometry", "[cli]")
{
    const ScopedDirectory directory{"corpus"};
    const auto one = loupe::tests::writeRepeatedBoxAssembly(directory.path() / "one.step", loupe::tests::FixtureUnit::Millimeter);
    const auto two = loupe::tests::writeTwoDefinitionAssembly(directory.path() / "two.step");
    const auto cases = directory.path() / "cases.json";
    nlohmann::json document{{"schemaVersion", 1}, {"cases", nlohmann::json::array()}};
    for (const auto& file : {one, two}) document["cases"].push_back({{"id", file.stem().string()}, {"file", file.string()}, {"requiredFullFlow", true}, {"expectedOutcome", "supported"}});
    std::ofstream(cases) << document.dump(2);
    const auto evidence = directory.path() / "evidence";

    const auto result = runSpike({textArg("corpus"), cases, textArg("--evidence"), evidence});
    INFO(result.json.dump());
    REQUIRE(result.exitCode == 0);
    REQUIRE(result.json.at("caseCount") == 2);
    for (const auto& id : {"one", "two"}) {
        REQUIRE(std::filesystem::exists(evidence / "evidence" / id / "assembly-snapshot.json"));
        REQUIRE_FALSE(std::filesystem::exists(evidence / "evidence" / id / (std::string(id) + ".step")));
    }
}

TEST_CASE("required corpus case performs deterministic four-row proof without private source data", "[cli]")
{
    const ScopedDirectory directory{"required-corpus"};
    const auto fixture = loupe::tests::writeRepeatedBoxAssembly(directory.path() / "source.step", loupe::tests::FixtureUnit::Millimeter);
    const auto cases = directory.path() / "cases.json";
    nlohmann::json document{{"schemaVersion", 1}, {"cases", nlohmann::json::array()}};
    document["cases"].push_back({{"id", "required-case"}, {"file", fixture.string()}, {"requiredFullFlow", true},
                                  {"expectedClass", "structured_assembly"}, {"expectedDeclaredUnits", nlohmann::json::array({"mm"})}});
    std::ofstream(cases) << document.dump(2);
    const auto evidence = directory.path() / "evidence";

    const auto result = runSpike({textArg("corpus"), cases, textArg("--evidence"), evidence});
    INFO(result.json.dump());
    REQUIRE(result.exitCode == 0);
    const auto& caseResult = result.json.at("cases").at(0);
    REQUIRE(caseResult.at("status") == "passed");
    REQUIRE(caseResult.at("validationRows").size() == 4);
    REQUIRE(caseResult.at("selected").at("definition").contains("id"));
    REQUIRE(caseResult.at("selected").at("occurrence").contains("id"));
    const auto summary = evidence / "evidence" / "required-case" / "failure.json";
    std::ifstream input(summary); std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    REQUIRE(contents.find(fixture.string()) == std::string::npos);
    REQUIRE(contents.find("ISO-10303-21") == std::string::npos);
}

TEST_CASE("required corpus hash mismatch is a contract failure", "[cli]")
{
    const ScopedDirectory directory{"required-hash"};
    const auto fixture = loupe::tests::writeRepeatedBoxAssembly(directory.path() / "source.step", loupe::tests::FixtureUnit::Millimeter);
    const auto cases = directory.path() / "cases.json";
    nlohmann::json document{{"schemaVersion", 1}, {"cases", nlohmann::json::array()}};
    document["cases"].push_back({{"id", "bad-hash"}, {"file", fixture.string()}, {"requiredFullFlow", true},
                                  {"expectedClass", "structured_assembly"}, {"expectedDeclaredUnits", nlohmann::json::array({"mm"})}, {"expectedSourceHash", "wrong"}});
    std::ofstream(cases) << document.dump(2);
    const auto result = runSpike({textArg("corpus"), cases, textArg("--evidence"), directory.path() / "evidence"});
    REQUIRE(result.exitCode == 6);
    REQUIRE(result.json.at("cases").at(0).at("failureCode") == "source_hash_mismatch");
}

TEST_CASE("corpus rejects a case that omits the mandatory full-flow contract", "[cli]")
{
    const ScopedDirectory directory{"missing-full-flow-contract"};
    const auto fixture = loupe::tests::writeRepeatedBoxAssembly(directory.path() / "source.step", loupe::tests::FixtureUnit::Millimeter);
    const auto cases = directory.path() / "cases.json";
    nlohmann::json document{{"schemaVersion", 1}, {"cases", nlohmann::json::array()}};
    document["cases"].push_back({{"id", "missing-contract"}, {"file", fixture.string()},
                                  {"expectedClass", "structured_assembly"}, {"expectedDeclaredUnits", nlohmann::json::array({"mm"})}});
    std::ofstream(cases) << document.dump(2);

    const auto result = runSpike({textArg("corpus"), cases, textArg("--evidence"), directory.path() / "evidence"});
    REQUIRE(result.exitCode == 6);
}

TEST_CASE("spike benchmark records a privacy-safe complete metric manifest", "[cli]")
{
    const ScopedDirectory directory{"benchmark"};
    const auto fixture = loupe::tests::writeSingleCylinderStep(directory.path() / "source.step");
    const auto cases = directory.path() / "cases.json";
    std::ofstream(cases) << nlohmann::json{{"schemaVersion", 1}, {"cases", nlohmann::json::array({{{"id", "benchmark-case"}, {"file", fixture.string()}, {"requiredFullFlow", true}}})}}.dump(2);
    const auto output = directory.path() / "benchmark";

    const auto result = runSpike({textArg("benchmark"), cases, textArg("--out"), output});

    REQUIRE(result.exitCode == 0);
    REQUIRE(std::filesystem::exists(output / "metrics.json"));
    REQUIRE(std::filesystem::exists(output / "metrics.csv"));
    nlohmann::json report;
    std::ifstream(output / "metrics.json") >> report;
    const auto& row = report.at("cases").at(0);
    REQUIRE(row.at("sourceHash").is_string());
    REQUIRE(row.at("treeReadyMs").is_number_unsigned());
    for (const auto* field : {"shellReadyMs", "fileAcknowledgementMs", "coarseViewMs", "firstInteractionMs", "cachedReopenMs", "selectionLatencyP50Ms", "selectionLatencyP95Ms", "frameTimeP50Ms", "frameTimeP95Ms", "peakMemoryBytes", "idleCpuPercent"}) {
        REQUIRE(row.at(field).is_null());
    }
    REQUIRE(row.at("unavailableMetrics").is_array());
}
