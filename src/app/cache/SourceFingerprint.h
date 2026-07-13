#pragma once

#include "app/cache/OverrideStore.h"

#include <QString>

#include <optional>

namespace loupe::app::cache {

class SourceFingerprint final {
public:
    [[nodiscard]] static std::optional<SourceIdentity> fromFile(const QString& path);
};

} // namespace loupe::app::cache
