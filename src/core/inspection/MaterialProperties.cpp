#include "core/inspection/MaterialProperties.h"

#include <array>
#include <cmath>

namespace loupe::inspection {
namespace {

const std::array kStandardMaterials{
    Material{"aluminum-6061", "Aluminum 6061", 2.70},
    Material{"aluminum-casting", "Aluminum casting", 2.68},
    Material{"steel-carbon", "Carbon steel", 7.85},
    Material{"stainless-304", "Stainless steel 304", 8.00},
    Material{"zinc-alloy", "Zinc alloy", 6.60},
    Material{"copper", "Copper", 8.96},
    Material{"abs", "ABS", 1.04},
    Material{"pp", "Polypropylene (PP)", 0.90},
    Material{"hdpe", "HDPE", 0.95},
    Material{"pc", "Polycarbonate (PC)", 1.20},
    Material{"pa", "Polyamide (PA / nylon)", 1.14},
    Material{"pmma", "PMMA (acrylic)", 1.18},
    Material{"fr4", "PCBA (FR-4)", 1.85},
    Material{"rubber-hnbr", "Rubber (HNBR)", 1.20},
    Material{"silicone", "Silicone rubber", 1.10},
};

} // namespace

std::optional<Material> MaterialLibrary::find(const std::string_view id)
{
    for (const auto& material : kStandardMaterials) {
        if (material.id == id) return material;
    }
    return std::nullopt;
}

std::optional<double> estimateMassKg(const double volumeMm3, const Material& material)
{
    if (!std::isfinite(volumeMm3) || volumeMm3 < 0.0 || !std::isfinite(material.densityGPerCm3) || material.densityGPerCm3 <= 0.0) {
        return std::nullopt;
    }
    return volumeMm3 * material.densityGPerCm3 / 1'000'000.0;
}

} // namespace loupe::inspection
