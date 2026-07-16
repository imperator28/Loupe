#pragma once

#include <QByteArray>

#include <optional>

namespace loupe::protocol {

enum class FrameType : quint8 {
    ControlJson = 1,
    Geometry = 2,
};

struct Frame final {
    FrameType type{};
    QByteArray payload;
};

class FrameDecoder final {
public:
    static constexpr qsizetype MaximumPayloadBytes = 64 * 1024 * 1024;

    void append(QByteArray bytes);
    [[nodiscard]] std::optional<Frame> take();
    void clear() noexcept;

private:
    QByteArray buffer_;
};

[[nodiscard]] QByteArray encodeFrame(FrameType type, const QByteArray& payload);

} // namespace loupe::protocol
