#pragma once

#include "core/domain/AssemblyTypes.h"
#include "core/validation/OutputValidator.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace loupe::report {

struct EvidenceMetadata {
    std::string sourceHash;
    std::string importerVersion;
    std::string effectiveUnitDecision;
    std::string generatedAtUtc;
};

struct EvidencePaths {
    std::filesystem::path assemblySnapshot;
    std::filesystem::path validation;
    std::filesystem::path benchmark;
    std::filesystem::path failure;
};

class EvidenceWriter {
public:
    [[nodiscard]] EvidencePaths write(const std::filesystem::path& root, const std::string& caseId,
                                      const domain::AssemblySnapshot& snapshot,
                                      const std::vector<validation::ValidationResult>& validations,
                                      const std::vector<std::map<std::string, std::string>>& benchmarkRows,
                                      const EvidenceMetadata& metadata,
                                      const std::map<std::string, std::string>& failure = {}) const;
};

} // namespace loupe::report
