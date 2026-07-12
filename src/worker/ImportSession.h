#pragma once

#include <QObject>

#include <atomic>
#include <cstdint>

namespace loupe::worker {

class ImportSession final : public QObject {
    Q_OBJECT
public:
    explicit ImportSession(std::uint64_t requestId, QObject* parent = nullptr);

    [[nodiscard]] std::uint64_t requestId() const noexcept { return requestId_; }
    void cancel() noexcept { canceled_.store(true); }
    [[nodiscard]] bool canceled() const noexcept { return canceled_.load(); }

private:
    std::uint64_t requestId_{};
    std::atomic_bool canceled_{false};
};

} // namespace loupe::worker
