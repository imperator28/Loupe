#include "protocol/ProtocolFrame.h"

#include "protocol/ProtocolTypes.h"

#include <QtEndian>

namespace loupe::protocol {
namespace {

constexpr qsizetype kHeaderSize = sizeof(quint32) + sizeof(quint8);

void appendSize(QByteArray& bytes, const quint32 value)
{
    const auto bigEndian = qToBigEndian(value);
    bytes.append(reinterpret_cast<const char*>(&bigEndian), sizeof(bigEndian));
}

quint32 readSize(const QByteArray& bytes)
{
    quint32 bigEndian{};
    std::memcpy(&bigEndian, bytes.constData(), sizeof(bigEndian));
    return qFromBigEndian(bigEndian);
}

} // namespace

QByteArray encodeFrame(const FrameType type, const QByteArray& payload)
{
    if (payload.size() > FrameDecoder::MaximumPayloadBytes) {
        throw ProtocolError("Protocol frame exceeds maximum payload size");
    }
    QByteArray frame;
    frame.reserve(kHeaderSize + payload.size());
    appendSize(frame, static_cast<quint32>(payload.size()) + 1U);
    frame.append(static_cast<char>(type));
    frame.append(payload);
    return frame;
}

void FrameDecoder::append(QByteArray bytes)
{
    if (bytes.size() > MaximumPayloadBytes + kHeaderSize
        || buffer_.size() > MaximumPayloadBytes + kHeaderSize - bytes.size()) {
        throw ProtocolError("Protocol receive buffer exceeds maximum size");
    }
    buffer_.append(std::move(bytes));
}

std::optional<Frame> FrameDecoder::take()
{
    if (buffer_.size() < kHeaderSize) {
        return std::nullopt;
    }
    const auto encodedSize = readSize(buffer_);
    if (encodedSize < sizeof(quint8) || encodedSize > MaximumPayloadBytes + sizeof(quint8)) {
        throw ProtocolError("Protocol frame length is invalid");
    }
    const auto frameSize = static_cast<qsizetype>(sizeof(quint32) + encodedSize);
    if (buffer_.size() < frameSize) {
        return std::nullopt;
    }
    const auto typeValue = static_cast<quint8>(buffer_.at(sizeof(quint32)));
    FrameType type{};
    if (typeValue == static_cast<quint8>(FrameType::ControlJson)) {
        type = FrameType::ControlJson;
    } else if (typeValue == static_cast<quint8>(FrameType::Geometry)) {
        type = FrameType::Geometry;
    } else {
        throw ProtocolError("Protocol frame type is invalid");
    }
    Frame frame{type, buffer_.mid(kHeaderSize, static_cast<qsizetype>(encodedSize - sizeof(quint8)))};
    buffer_.remove(0, frameSize);
    return frame;
}

void FrameDecoder::clear() noexcept
{
    buffer_.clear();
}

} // namespace loupe::protocol
