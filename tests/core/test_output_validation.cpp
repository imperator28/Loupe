#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/export/ExportPlan.h"
#include "core/export/StlExporter.h"
#include "core/export/StepExporter.h"
#include "core/import/StepImporter.h"
#include "core/report/EvidenceWriter.h"
#include "core/validation/OutputValidator.h"
#include "fixtures/FixtureFactory.h"

#include <filesystem>
#include <fstream>
#include <array>
#include <cstdint>
#include <limits>
#include <nlohmann/json.hpp>

namespace {

class ScopedDirectory {
public:
    explicit ScopedDirectory(const std::string_view name)
        : path_(std::filesystem::temp_directory_path() / ("loupe-validation-" + std::string(name)))
    {
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }
    ~ScopedDirectory() { std::filesystem::remove_all(path_); }
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }
private:
    std::filesystem::path path_;
};

loupe::exporting::OutputRow makeOutput(const loupe::import::ImportResult& imported,
                                       const loupe::exporting::Format format,
                                       const std::filesystem::path& directory,
                                       const loupe::exporting::StepOutputUnit unit = loupe::exporting::StepOutputUnit::Millimeter)
{
    const auto definition = std::ranges::find_if(imported.snapshot.nodes, [](const auto& node) {
        return node.kind == loupe::domain::NodeKind::Definition;
    });
    REQUIRE(definition != imported.snapshot.nodes.end());
    loupe::exporting::PlanRequest request;
    request.selections = {{definition->id, loupe::exporting::SelectionKind::Definition}};
    request.hierarchyPaths = {{definition->id, definition->hierarchyPath}};
    request.destination = directory.string();
    request.format = format;
    request.coordinates = loupe::exporting::Coordinates::Local;
    request.grouping = loupe::exporting::Grouping::SeparateFiles;
    request.stepOutputUnit = unit;
    request.requestedUnitToMillimeters = unit == loupe::exporting::StepOutputUnit::Inch ? 25.4 : 1.0;
    request.unitDecision = {unit == loupe::exporting::StepOutputUnit::Inch ? loupe::units::LengthUnit::Inch : loupe::units::LengthUnit::Millimeter,
                            loupe::units::UnitConfidence::Confirmed,
                            unit == loupe::exporting::StepOutputUnit::Inch ? 25.4 : 1.0,
                            "test fixture"};
    const auto plan = loupe::exporting::buildPlan(request);
    return plan.outputs().front();
}

loupe::validation::ExpectedOutput expected(const std::filesystem::path& path, const loupe::validation::OutputUnit unit)
{
    return {path, unit, 1, {{0.0, 0.0, 0.0}, {10.0, 10.0, 10.0}}, {5.0, 5.0, 5.0}, 1.0e-4};
}

bool hasIssue(const std::vector<loupe::validation::ValidationIssue>& issues, const std::string_view code)
{
    return std::ranges::any_of(issues, [code](const auto& issue) { return issue.code == code; });
}

std::filesystem::path writeReversedBinaryStl(const std::filesystem::path& path)
{
    std::ofstream output(path, std::ios::binary);
    std::array<char, 80> header{};
    const std::uint32_t count = 1;
    const std::array<float, 12> data{{0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F, 0.0F}};
    const std::uint16_t attribute = 0;
    output.write(header.data(), static_cast<std::streamsize>(header.size()));
    output.write(reinterpret_cast<const char*>(&count), sizeof(count));
    output.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(sizeof(data)));
    output.write(reinterpret_cast<const char*>(&attribute), sizeof(attribute));
    return path;
}

TEST_CASE("STEP validation detects wrong output unit", "[validation]")
{
    const ScopedDirectory directory{"wrong-unit"};
    const auto source = loupe::tests::writeRepeatedBoxAssembly(directory.path() / "source.step", loupe::tests::FixtureUnit::Inch);
    const auto imported = loupe::import::StepImporter {}.read(source);
    const auto output = makeOutput(imported, loupe::exporting::Format::Step, directory.path(), loupe::exporting::StepOutputUnit::Inch);
    loupe::exporting::StepExporter {}.write(imported, output);

    const auto result = loupe::validation::OutputValidator {}.validate(expected(output.finalPath(), loupe::validation::OutputUnit::Millimeter));

    REQUIRE_FALSE(result.passed);
    REQUIRE(hasIssue(result.errors, "unit_mismatch"));
}

TEST_CASE("STEP validation accepts its declared inch output unit despite XCAF normalization", "[validation]")
{
    const ScopedDirectory directory{"declared-inch"};
    const auto source = loupe::tests::writeRepeatedBoxAssembly(directory.path() / "source.step", loupe::tests::FixtureUnit::Inch);
    const auto imported = loupe::import::StepImporter {}.read(source);
    const auto output = makeOutput(imported, loupe::exporting::Format::Step, directory.path(), loupe::exporting::StepOutputUnit::Inch);
    (void)loupe::exporting::StepExporter {}.write(imported, output);

    auto reviewed = expected(output.finalPath(), loupe::validation::OutputUnit::Inch);
    reviewed.boundsMm.maximum = {254.0, 254.0, 254.0};
    reviewed.centroidMm = {127.0, 127.0, 127.0};
    const auto result = loupe::validation::OutputValidator {}.validate(reviewed);

    REQUIRE(result.passed);
    REQUIRE(result.actualUnit == loupe::validation::OutputUnit::Inch);
}

TEST_CASE("validation reports each batch row independently", "[validation]")
{
    const ScopedDirectory directory{"batch"};
    const auto source = loupe::tests::writeRepeatedBoxAssembly(directory.path() / "source.step", loupe::tests::FixtureUnit::Millimeter);
    const auto imported = loupe::import::StepImporter {}.read(source);
    const auto output = makeOutput(imported, loupe::exporting::Format::Step, directory.path());
    loupe::exporting::StepExporter {}.write(imported, output);
    const auto missing = expected(directory.path() / "missing.step", loupe::validation::OutputUnit::Millimeter);

    const auto results = loupe::validation::OutputValidator {}.validateBatch({expected(output.finalPath(), loupe::validation::OutputUnit::Millimeter), missing});

    REQUIRE(results.size() == 2);
    REQUIRE(results[0].passed);
    REQUIRE_FALSE(results[1].passed);
    REQUIRE(hasIssue(results[1].errors, "missing_file"));
}

TEST_CASE("validation reports a body-count mismatch independently", "[validation]")
{
    const ScopedDirectory directory{"body-count"};
    const auto source = loupe::tests::writeRepeatedBoxAssembly(directory.path() / "source.step", loupe::tests::FixtureUnit::Millimeter);
    const auto imported = loupe::import::StepImporter {}.read(source);
    const auto output = makeOutput(imported, loupe::exporting::Format::Step, directory.path());
    (void)loupe::exporting::StepExporter {}.write(imported, output);
    auto reviewed = expected(output.finalPath(), loupe::validation::OutputUnit::Millimeter);
    reviewed.bodyCount = 2;

    const auto result = loupe::validation::OutputValidator {}.validate(reviewed);

    REQUIRE_FALSE(result.passed);
    REQUIRE(hasIssue(result.errors, "body_count_mismatch"));
}

TEST_CASE("validation reports bounds and centroid mismatches independently", "[validation]")
{
    const ScopedDirectory directory{"spatial-mismatch"};
    const auto source = loupe::tests::writeRepeatedBoxAssembly(directory.path() / "source.step", loupe::tests::FixtureUnit::Millimeter);
    const auto imported = loupe::import::StepImporter {}.read(source);
    const auto output = makeOutput(imported, loupe::exporting::Format::Step, directory.path());
    (void)loupe::exporting::StepExporter {}.write(imported, output);
    auto boundsReviewed = expected(output.finalPath(), loupe::validation::OutputUnit::Millimeter);
    boundsReviewed.boundsMm.maximum.x = 11.0;
    const auto boundsResult = loupe::validation::OutputValidator {}.validate(boundsReviewed);
    auto centroidReviewed = expected(output.finalPath(), loupe::validation::OutputUnit::Millimeter);
    centroidReviewed.centroidMm.x = 6.0;
    const auto centroidResult = loupe::validation::OutputValidator {}.validate(centroidReviewed);

    REQUIRE(hasIssue(boundsResult.errors, "bounds_mismatch"));
    REQUIRE_FALSE(hasIssue(boundsResult.errors, "centroid_mismatch"));
    REQUIRE(hasIssue(centroidResult.errors, "centroid_mismatch"));
    REQUIRE_FALSE(hasIssue(centroidResult.errors, "bounds_mismatch"));
}

TEST_CASE("validation detects reversed binary STL orientation", "[validation]")
{
    const ScopedDirectory directory{"reversed-stl"};
    const auto file = writeReversedBinaryStl(directory.path() / "reversed.stl");
    const loupe::validation::ExpectedOutput reviewed{
        file, loupe::validation::OutputUnit::Millimeter, 1, {{0.0, 0.0, 0.0}, {1.0, 1.0, 0.0}}, {0.5, 0.5, 0.0}, 1.0e-4};

    const auto result = loupe::validation::OutputValidator {}.validate(reviewed);

    REQUIRE_FALSE(result.passed);
    REQUIRE(hasIssue(result.errors, "mirrored_triangle_orientation"));
}

TEST_CASE("validation rejects invalid tolerances before comparison", "[validation]")
{
    const ScopedDirectory directory{"invalid-tolerance"};
    const auto source = loupe::tests::writeRepeatedBoxAssembly(directory.path() / "source.step", loupe::tests::FixtureUnit::Millimeter);
    const auto imported = loupe::import::StepImporter {}.read(source);
    const auto output = makeOutput(imported, loupe::exporting::Format::Step, directory.path());
    (void)loupe::exporting::StepExporter {}.write(imported, output);
    for (const double tolerance : {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        auto reviewed = expected(output.finalPath(), loupe::validation::OutputUnit::Millimeter);
        reviewed.relativeTolerance = tolerance;
        const auto result = loupe::validation::OutputValidator {}.validate(reviewed);
        REQUIRE_FALSE(result.passed);
        REQUIRE(hasIssue(result.errors, "invalid_tolerance"));
    }
}

TEST_CASE("STL validation rejects hostile byte payloads", "[validation]")
{
    const ScopedDirectory directory{"hostile-stl"};
    const auto base = writeReversedBinaryStl(directory.path() / "base.stl");
    const loupe::validation::ExpectedOutput reviewed{
        base, loupe::validation::OutputUnit::Millimeter, 1, {{0.0, 0.0, 0.0}, {1.0, 1.0, 0.0}}, {0.5, 0.5, 0.0}, 1.0e-4};
    std::ifstream sourceBytes(base, std::ios::binary);
    auto bytes = std::vector<char>(std::istreambuf_iterator<char>(sourceBytes), {});
    auto malformed = [&](const std::string_view name, auto mutate) {
        auto altered = bytes; mutate(altered);
        const auto file = directory.path() / std::string(name);
        std::ofstream output(file, std::ios::binary); output.write(altered.data(), static_cast<std::streamsize>(altered.size())); output.close();
        auto candidate = reviewed; candidate.path = file;
        return loupe::validation::OutputValidator {}.validate(candidate);
    };
    const auto truncated = malformed("truncated.stl", [](auto& value) { value.pop_back(); });
    const auto trailing = malformed("trailing.stl", [](auto& value) { value.push_back('x'); });
    const auto badCount = malformed("bad-count.stl", [](auto& value) { value[80] = 2; });
    const auto nanVertex = malformed("nan.stl", [](auto& value) { value[96] = static_cast<char>(0); value[97] = static_cast<char>(0); value[98] = static_cast<char>(0xc0); value[99] = static_cast<char>(0x7f); });
    REQUIRE(hasIssue(truncated.errors, "invalid_shape"));
    REQUIRE(hasIssue(trailing.errors, "invalid_shape"));
    REQUIRE(hasIssue(badCount.errors, "invalid_shape"));
    REQUIRE(hasIssue(nanVertex.errors, "invalid_shape"));
}

TEST_CASE("read-back validation verifies exported STEP and mirrored STL geometry", "[validation]")
{
    const ScopedDirectory directory{"exporters"};
    const auto source = loupe::tests::writeRepeatedBoxAssembly(directory.path() / "source.step", loupe::tests::FixtureUnit::Millimeter);
    auto imported = loupe::import::StepImporter {}.read(source);
    const auto step = makeOutput(imported, loupe::exporting::Format::Step, directory.path());
    loupe::exporting::StepExporter {}.write(imported, step);
    const auto stepResult = loupe::validation::OutputValidator {}.validate(expected(step.finalPath(), loupe::validation::OutputUnit::Millimeter));
    REQUIRE(stepResult.passed);
    REQUIRE(stepResult.shapeValid);
    REQUIRE(stepResult.actualBodyCount == 1);
    REQUIRE(stepResult.actualBoundsMm.maximum.x == Catch::Approx(10.0));

    const auto occurrence = std::ranges::find_if(imported.snapshot.nodes, [](const auto& node) {
        return node.kind == loupe::domain::NodeKind::Occurrence && node.hierarchyPath.ends_with(":2");
    });
    REQUIRE(occurrence != imported.snapshot.nodes.end());
    occurrence->placement.columnMajor[0] = -1.0;
    occurrence->placement.columnMajor[12] = 10.0;
    loupe::exporting::PlanRequest request;
    request.selections = {{occurrence->id, loupe::exporting::SelectionKind::Occurrence}};
    request.hierarchyPaths = {{occurrence->id, occurrence->hierarchyPath}};
    request.destination = directory.path().string(); request.format = loupe::exporting::Format::Stl;
    request.coordinates = loupe::exporting::Coordinates::Assembly; request.grouping = loupe::exporting::Grouping::SeparateFiles;
    request.unitDecision = {loupe::units::LengthUnit::Millimeter, loupe::units::UnitConfidence::Confirmed, 1.0, "test fixture"};
    const auto stlPlan = loupe::exporting::buildPlan(request);
    const auto stl = stlPlan.outputs().front();
    loupe::exporting::StlExporter {}.write(imported, stl);
    const auto stlResult = loupe::validation::OutputValidator {}.validate(
        {stl.finalPath(), loupe::validation::OutputUnit::Millimeter, 1, {{0.0, 0.0, 0.0}, {10.0, 10.0, 10.0}}, {5.0, 5.0, 5.0}, 1.0e-4});
    REQUIRE(stlResult.passed);
    REQUIRE_FALSE(hasIssue(stlResult.errors, "mirrored_triangle_orientation"));
}

TEST_CASE("evidence reports parse and carry required metadata", "[evidence]")
{
    const ScopedDirectory directory{"evidence"};
    loupe::domain::AssemblySnapshot snapshot;
    snapshot.sourceHash = "abc123";
    auto validation = loupe::validation::ValidationResult {};
    validation.actualUnit = loupe::validation::OutputUnit::Millimeter;
    const loupe::report::EvidenceMetadata metadata{"abc123", "test-importer", "millimeter", "2026-07-10T00:00:00Z"};

    const auto paths = loupe::report::EvidenceWriter {}.write(
        directory.path(), "case-a", snapshot, {validation},
        std::vector<std::map<std::string, std::string>> {{{"elapsed_ms", "12"}}}, metadata,
        std::map<std::string, std::string> {{"reason", "none"}});

    for (const auto& file : {paths.assemblySnapshot, paths.validation, paths.failure}) {
        std::ifstream input(file);
        const auto json = nlohmann::json::parse(input);
        REQUIRE(json.at("schemaVersion") == 1);
        REQUIRE(json.at("sourceHash") == "abc123");
        REQUIRE(json.at("importerVersion") == "test-importer");
        REQUIRE(json.at("effectiveUnitDecision") == "millimeter");
        REQUIRE(json.at("generatedAtUtc") == "2026-07-10T00:00:00Z");
    }
    {
        std::ifstream input(paths.validation);
        const auto json = nlohmann::json::parse(input);
        REQUIRE(json.at("results").at(0).at("actualUnit") == "millimeter");
    }
    REQUIRE(std::filesystem::exists(paths.benchmark));
}

TEST_CASE("evidence rejects unsafe case IDs and protects CSV and paths", "[evidence]")
{
    const ScopedDirectory directory{"evidence-security"};
    loupe::domain::AssemblySnapshot snapshot; snapshot.sourceHash = "hash";
    loupe::validation::ValidationResult validation; validation.path = "C:/private/source.step";
    const loupe::report::EvidenceMetadata metadata{"hash", "importer", "millimeter", "2026-07-10T00:00:00Z"};
    const auto writer = loupe::report::EvidenceWriter {};
    REQUIRE_THROWS(writer.write(directory.path(), "../escape", snapshot, {validation}, {}, metadata));
    REQUIRE_THROWS(writer.write(directory.path(), "nested/case", snapshot, {validation}, {}, metadata));
    const auto paths = writer.write(directory.path(), "safe-case", snapshot, {validation},
        std::vector<std::map<std::string, std::string>> {{{"formula", "=1+1"}, {"quoted", "a,\"b\""}}}, metadata);
    std::ifstream jsonInput(paths.validation); const auto json = nlohmann::json::parse(jsonInput);
    REQUIRE(json.at("results").at(0).at("path") == "source.step");
    std::ifstream csv(paths.benchmark); std::string line; std::getline(csv, line); std::getline(csv, line);
    REQUIRE(line.find("'=1+1") != std::string::npos);
    REQUIRE(line.find("\"a,\"\"b\"\"\"") != std::string::npos);
}

TEST_CASE("evidence rejects a safe-looking case symlink that escapes its root", "[evidence]")
{
    const ScopedDirectory directory{"evidence-symlink"};
    const auto external = directory.path() / "external";
    const auto evidenceRoot = directory.path() / "evidence";
    const auto link = evidenceRoot / "safe-case";
    std::filesystem::create_directories(external);
    std::filesystem::create_directories(evidenceRoot);
    std::error_code error;
    std::filesystem::create_directory_symlink(external, link, error);
    if (error == std::make_error_code(std::errc::permission_denied)
        || error == std::make_error_code(std::errc::operation_not_permitted)
        // ERROR_PRIVILEGE_NOT_HELD (1314): Windows reports this when symlink creation needs Developer Mode/admin rights.
        || (error.category() == std::system_category() && error.value() == 1314)) {
        SKIP("directory symlinks require permissions unavailable in this environment");
    }
    if (error) FAIL("unable to create directory symlink: " << error.message());
    loupe::domain::AssemblySnapshot snapshot; snapshot.sourceHash = "hash";
    const loupe::report::EvidenceMetadata metadata{"hash", "importer", "millimeter", "2026-07-10T00:00:00Z"};

    REQUIRE_THROWS(loupe::report::EvidenceWriter {}.write(directory.path(), "safe-case", snapshot, {}, {}, metadata));
    REQUIRE_FALSE(std::filesystem::exists(external / "assembly-snapshot.json"));
}

TEST_CASE("evidence rejects a pre-existing evidence-root symlink escape", "[evidence]")
{
    const ScopedDirectory directory{"evidence-root-symlink"};
    const auto root = directory.path() / "root";
    const auto external = directory.path() / "external";
    std::filesystem::create_directories(root); std::filesystem::create_directories(external);
    std::error_code error;
    std::filesystem::create_directory_symlink(external, root / "evidence", error);
    if (error == std::make_error_code(std::errc::permission_denied)
        || error == std::make_error_code(std::errc::operation_not_permitted)
        || (error.category() == std::system_category() && error.value() == 1314)) SKIP("directory symlinks require permissions unavailable in this environment");
    if (error) FAIL("unable to create directory symlink: " << error.message());
    loupe::domain::AssemblySnapshot snapshot; snapshot.sourceHash = "hash";
    const loupe::report::EvidenceMetadata metadata{"hash", "importer", "millimeter", "2026-07-10T00:00:00Z"};

    REQUIRE_THROWS(loupe::report::EvidenceWriter {}.write(root, "safe-case", snapshot, {}, {}, metadata));
    REQUIRE_FALSE(std::filesystem::exists(external / "safe-case" / "assembly-snapshot.json"));
}

} // namespace
