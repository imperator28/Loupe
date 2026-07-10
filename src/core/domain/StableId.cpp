#include "core/domain/StableId.h"

#include <xxhash.h>

#include <string>

namespace loupe::domain {
namespace {

void appendHex(std::string& result, const unsigned long long value)
{
    constexpr char digits[] = "0123456789abcdef";

    for (int shift = 60; shift >= 0; shift -= 4) {
        result.push_back(digits[(value >> shift) & 0x0fU]);
    }
}

} // namespace

std::string stableId(
    const std::string_view sourceHash,
    const std::string_view canonicalPath,
    const std::string_view nodeKind)
{
    std::string input;
    input.reserve(sourceHash.size() + canonicalPath.size() + nodeKind.size() + 2);
    input.append(sourceHash);
    input.push_back('\n');
    input.append(canonicalPath);
    input.push_back('\n');
    input.append(nodeKind);

    const auto hash = XXH3_128bits(input.data(), input.size());
    std::string result;
    result.reserve(32);
    appendHex(result, hash.high64);
    appendHex(result, hash.low64);
    return result;
}

} // namespace loupe::domain
