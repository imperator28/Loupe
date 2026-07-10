#pragma once

#include <string>
#include <string_view>

namespace loupe::domain {

[[nodiscard]] std::string stableId(
    std::string_view sourceHash,
    std::string_view canonicalPath,
    std::string_view nodeKind);

} // namespace loupe::domain
