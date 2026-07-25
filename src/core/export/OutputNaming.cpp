#include "core/export/OutputNaming.h"

#include <algorithm>
#include <cctype>
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

} // namespace loupe::exporting::detail
