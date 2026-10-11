#pragma once

#include <Quantity_Color.hxx>

#include <filesystem>
#include <optional>

namespace loupe::tests {

enum class FixtureUnit { Millimeter, Inch };

[[nodiscard]] std::filesystem::path writeRepeatedBoxAssembly(
    const std::filesystem::path& file,
    FixtureUnit unit);
[[nodiscard]] std::filesystem::path writeTwoDefinitionAssembly(const std::filesystem::path& file);
// Both parts given the same colour, as CAD tools often stamp their default on every body.
[[nodiscard]] std::filesystem::path writeTwoDefinitionAssembly(const std::filesystem::path& file,
                                                               const std::optional<Quantity_Color>& bothPartsColor);
[[nodiscard]] std::filesystem::path writeNestedBoxAssembly(const std::filesystem::path& file);
[[nodiscard]] std::filesystem::path writeFlatTwoSolidStep(const std::filesystem::path& file);
[[nodiscard]] std::filesystem::path writeSingleCylinderStep(const std::filesystem::path& file);
[[nodiscard]] std::filesystem::path writeMultiSolidBodyStep(const std::filesystem::path& file);

} // namespace loupe::tests
