#include "core/export/OutputNaming.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <set>
#include <utility>

#include <QByteArrayView>
#include <QString>
#include <QStringDecoder>

namespace loupe::exporting::detail {
namespace {

[[nodiscard]] std::string_view leafOf(const std::string_view hierarchyPath)
{
    const auto separator = hierarchyPath.find_last_of("/\\");
    return separator == std::string_view::npos ? hierarchyPath : hierarchyPath.substr(separator + 1);
}

[[nodiscard]] std::string upperAscii(const std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    for (const unsigned char character : value) {
        result.push_back(static_cast<char>(std::toupper(character)));
    }
    return result;
}

[[nodiscard]] bool isReservedWindowsDeviceName(const std::string_view leaf)
{
    const std::string_view base = leaf.substr(0, leaf.find('.'));
    const std::string upper = upperAscii(base);
    if (upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL") {
        return true;
    }
    if (upper.size() == 4 && (upper.starts_with("COM") || upper.starts_with("LPT"))) {
        return upper.back() >= '1' && upper.back() <= '9';
    }
    return false;
}

[[nodiscard]] std::string anonymousSanitizedLeaf(const std::string_view hierarchyPath)
{
    const std::string_view leaf = leafOf(hierarchyPath);
    const auto validUtf8 = [](const std::string_view value) {
        for (std::size_t index = 0; index < value.size();) {
            const auto first = static_cast<unsigned char>(value[index]);
            if (first < 0x80U) {
                ++index;
                continue;
            }
            int continuationCount{};
            unsigned int codePoint{};
            if (first >= 0xc2U && first <= 0xdfU) {
                continuationCount = 1;
                codePoint = first & 0x1fU;
            } else if (first >= 0xe0U && first <= 0xefU) {
                continuationCount = 2;
                codePoint = first & 0x0fU;
            } else if (first >= 0xf0U && first <= 0xf4U) {
                continuationCount = 3;
                codePoint = first & 0x07U;
            } else {
                return false;
            }
            if (index + static_cast<std::size_t>(continuationCount) >= value.size()) {
                return false;
            }
            for (int offset = 1; offset <= continuationCount; ++offset) {
                const auto next = static_cast<unsigned char>(value[index + static_cast<std::size_t>(offset)]);
                if ((next & 0xc0U) != 0x80U) {
                    return false;
                }
                codePoint = (codePoint << 6U) | (next & 0x3fU);
            }
            if ((continuationCount == 2 && codePoint < 0x800U)
                || (continuationCount == 3 && codePoint < 0x10000U)
                || (codePoint >= 0xd800U && codePoint <= 0xdfffU) || codePoint > 0x10ffffU) {
                return false;
            }
            index += static_cast<std::size_t>(continuationCount + 1);
        }
        return true;
    };
    if (!validUtf8(leaf) || leaf.empty() || leaf.back() == '.' || leaf.back() == ' '
        || isReservedWindowsDeviceName(leaf)) {
        throw OutputNamingError(OutputNamingError::Code::UnsafeName, "output name is unsafe on Windows");
    }

    std::string result;
    result.reserve(leaf.size());
    for (const unsigned char character : leaf) {
        if (character < 0x20U || (character < 0x80U && std::string_view{"<>:\"/\\|?*"}.contains(character))) {
            result.push_back('_');
        } else {
            result.push_back(static_cast<char>(character));
        }
    }
    if (result.empty() || result.back() == '.' || result.back() == ' ') {
        throw OutputNamingError(OutputNamingError::Code::UnsafeName, "output name is unsafe on Windows");
    }
    return result;
}

[[nodiscard]] std::u16string anonymousWindowsComparablePath(std::string path)
{
    for (char& character : path) {
        if (character == '/') {
            character = '\\';
        }
    }

    QStringDecoder decoder(QStringDecoder::Utf8);
    QString comparable = decoder(QByteArrayView(path.data(), static_cast<qsizetype>(path.size())));
    if (decoder.hasError()) {
        throw OutputNamingError(OutputNamingError::Code::InvalidUtf8, "output path is not valid UTF-8");
    }
    comparable = comparable.normalized(QString::NormalizationForm_C).toCaseFolded();
    return comparable.toStdU16String();
}

} // namespace

OutputNamingError::OutputNamingError(const Code code, std::string message)
    : std::runtime_error(std::move(message))
    , code_(code)
{
}

std::string sanitizedLeaf(const std::string_view hierarchyPath)
{
    return anonymousSanitizedLeaf(hierarchyPath);
}

std::u16string windowsComparablePath(std::string path)
{
    return anonymousWindowsComparablePath(std::move(path));
}

std::vector<std::string> uniqueLeafNames(const std::vector<LeafNameRequest>& requests)
{
    struct Entry {
        std::string leaf;
        std::optional<std::u16string> comparable;
        bool named{};
    };
    std::vector<Entry> entries;
    entries.reserve(requests.size());
    for (const auto& request : requests) {
        Entry entry{request.name, std::nullopt, false};
        try {
            entry.leaf = anonymousSanitizedLeaf(request.name);
            entry.comparable = anonymousWindowsComparablePath(entry.leaf);
        } catch (const OutputNamingError&) {
            // Left as given: the plan that receives it reports the unsafe name.
            entry.leaf = request.name;
            entry.comparable.reset();
        }
        entries.push_back(std::move(entry));
    }

    std::set<std::u16string> taken;
    // Typed names first, so a generated name never takes one from the user.
    for (std::size_t index = 0; index < requests.size(); ++index) {
        auto& entry = entries[index];
        if (!requests[index].typedByUser || !entry.comparable) {
            entry.named = !entry.comparable.has_value();
            continue;
        }
        taken.insert(*entry.comparable);
        entry.named = true;
    }
    // Then the first holder of every generated name, so a name that appears once is never
    // displaced by another name's sequence number.
    for (auto& entry : entries) {
        if (entry.named) continue;
        if (taken.insert(*entry.comparable).second) entry.named = true;
    }
    // The rest are copies of a name already taken.
    for (auto& entry : entries) {
        if (entry.named) continue;
        for (int sequence = 2;; ++sequence) {
            auto candidate = entry.leaf + "-" + std::to_string(sequence);
            auto comparable = anonymousWindowsComparablePath(candidate);
            if (!taken.insert(comparable).second) continue;
            entry.leaf = std::move(candidate);
            entry.named = true;
            break;
        }
    }

    std::vector<std::string> result;
    result.reserve(entries.size());
    for (auto& entry : entries) result.push_back(std::move(entry.leaf));
    return result;
}

} // namespace loupe::exporting::detail
