#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace loupe::inspection {

struct Material final {
    std::string id;
    std::string name;
    double densityGPerCm3{};
};

class MaterialLibrary final {
public:
    [[nodiscard]] static std::optional<Material> find(std::string_view id);
};

[[nodiscard]] std::optional<double> estimateMassKg(double volumeMm3, const Material& material);

} // namespace loupe::inspection
