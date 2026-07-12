#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#endif

namespace loupe::exporting::detail {

class AtomicExportFile {
public:
    explicit AtomicExportFile(const std::filesystem::path& final)
        : final_(final)
    {
        static std::atomic_uint64_t sequence{};
        const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
        partial_ = final_;
        partial_ += ".partial." + std::to_string(
#ifdef _WIN32
            GetCurrentProcessId()
#else
            0
#endif
            ) + "." + std::to_string(tick) + "." + std::to_string(sequence.fetch_add(1));
    }

    ~AtomicExportFile() { std::error_code ignored; std::filesystem::remove(partial_, ignored); }

    [[nodiscard]] const std::filesystem::path& partial() const noexcept { return partial_; }

    void finalize()
    {
#ifdef _WIN32
        if (!MoveFileExW(partial_.c_str(), final_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            throw std::runtime_error("could not atomically finalize export: " + std::to_string(GetLastError()));
        }
#else
        std::error_code error;
        std::filesystem::rename(partial_, final_, error);
        if (error) throw std::runtime_error("could not atomically finalize export: " + error.message());
#endif
        partial_.clear();
    }

private:
    std::filesystem::path final_;
    std::filesystem::path partial_;
};

} // namespace loupe::exporting::detail
