#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace loupe::validation {

enum class OutputUnit { Millimeter, Inch, Unknown };

struct Vec3 {
    double x{};
    double y{};
    double z{};
};

struct Bounds {
    Vec3 minimum{};
    Vec3 maximum{};
};

struct ExpectedOutput {
    std::filesystem::path path;
    OutputUnit unit{OutputUnit::Unknown};
    std::size_t bodyCount{};
    Bounds boundsMm{};
    Vec3 centroidMm{};
    double relativeTolerance{1.0e-5};
};

struct ValidationIssue { std::string code; std::string message; };

struct ValidationResult {
    std::filesystem::path path;
    bool reopened{};
    bool shapeValid{};
    bool passed{};
    std::size_t actualBodyCount{};
    Bounds actualBoundsMm{};
    Vec3 actualCentroidMm{};
    OutputUnit actualUnit{OutputUnit::Unknown};
    std::vector<ValidationIssue> warnings;
    std::vector<ValidationIssue> errors;
};

class OutputValidator {
public:
    [[nodiscard]] ValidationResult validate(const ExpectedOutput& expected) const;
    [[nodiscard]] std::vector<ValidationResult> validateBatch(const std::vector<ExpectedOutput>& expected) const;
};

} // namespace loupe::validation
