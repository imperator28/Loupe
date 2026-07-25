#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

// Output-name rules shared by every plan builder.
//
// These encode hard-won constraints about what is safe to write to disk on Windows
// -- reserved device names, trailing dots and spaces, control characters, malformed
// UTF-8 -- and how two paths that differ textually can still collide once case and
// Unicode normalisation are applied. They live here rather than inside one plan
// builder so a second builder cannot drift from them.
//
// They report failure with their own neutral error type: each plan builder catches
// it and maps it onto whatever error code that plan reports, since a shared helper
// cannot know about any particular plan's error enum.
namespace loupe::exporting::detail {

class OutputNamingError : public std::runtime_error {
public:
    enum class Code { UnsafeName, InvalidUtf8 };

    OutputNamingError(Code code, std::string message);
    [[nodiscard]] Code code() const noexcept { return code_; }

private:
    Code code_;
};

// Last path segment, stripped of characters that are unsafe in a filename.
// Throws OutputNamingError when no safe name can be produced at all.
[[nodiscard]] std::string sanitizedLeaf(std::string_view hierarchyPath);

// A form in which two paths compare equal exactly when Windows would treat them as
// the same file: separators unified, NFC-normalised, case-folded.
[[nodiscard]] std::u16string windowsComparablePath(std::string path);

} // namespace loupe::exporting::detail
