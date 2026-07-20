#include "protocol/GeometryPayload.h"

#include "protocol/ProtocolTypes.h"

#include <QtEndian>

#include <bit>
#include <cmath>
#include <limits>
#include <type_traits>

namespace loupe::protocol {
namespace {

constexpr quint32 kMagic = 0x4c504750; // LPGP
constexpr quint16 kVersion = 2;
constexpr quint32 kMaximumStringBytes = 4 * 1024;
constexpr quint32 kMaximumVertices = 10 * 1000 * 1000;
constexpr quint32 kMaximumIndices = 30 * 1000 * 1000;
constexpr quint32 kMaximumTopologyRanges = 2 * 1000 * 1000;

enum class GeometryType : quint8 {
    Mesh = 1,
    Edges = 2,
};

class Writer final {
public:
    void u8(const quint8 value) { bytes_.append(static_cast<char>(value)); }
    void u16(const quint16 value) { append(qToBigEndian(value)); }
    void u32(const quint32 value) { append(qToBigEndian(value)); }
    void u64(const quint64 value) { append(qToBigEndian(value)); }
    void number(const float value) { u32(std::bit_cast<quint32>(value)); }
    void string(const QString& value)
    {
        const auto utf8 = value.toUtf8();
        if (utf8.size() > static_cast<qsizetype>(kMaximumStringBytes)) {
            throw ProtocolError("Geometry string exceeds maximum length");
        }
        u32(static_cast<quint32>(utf8.size()));
        bytes_.append(utf8);
    }
    [[nodiscard]] QByteArray take() { return std::move(bytes_); }

private:
    template<typename Value>
    void append(const Value value)
    {
        bytes_.append(reinterpret_cast<const char*>(&value), sizeof(Value));
    }

    QByteArray bytes_;
};

class Reader final {
public:
    explicit Reader(const QByteArray& bytes)
        : bytes_(bytes)
    {
    }

    [[nodiscard]] quint8 u8()
    {
        ensure(sizeof(quint8));
        return static_cast<quint8>(bytes_.at(offset_++));
    }
    [[nodiscard]] quint16 u16() { return qFromBigEndian(read<quint16>()); }
    [[nodiscard]] quint32 u32() { return qFromBigEndian(read<quint32>()); }
    [[nodiscard]] quint64 u64() { return qFromBigEndian(read<quint64>()); }
    [[nodiscard]] float number() { return std::bit_cast<float>(u32()); }
    [[nodiscard]] QString string()
    {
        const auto length = u32();
        if (length > kMaximumStringBytes) {
            throw ProtocolError("Geometry string length is invalid");
        }
        ensure(length);
        const auto value = QString::fromUtf8(bytes_.constData() + offset_, static_cast<qsizetype>(length));
        offset_ += static_cast<qsizetype>(length);
        return value;
    }
    void ensureRemaining() const
    {
        if (offset_ != bytes_.size()) {
            throw ProtocolError("Geometry payload has trailing bytes");
        }
    }

private:
    template<typename Value>
    [[nodiscard]] Value read()
    {
        ensure(sizeof(Value));
        Value value{};
        std::memcpy(&value, bytes_.constData() + offset_, sizeof(Value));
        offset_ += sizeof(Value);
        return value;
    }
    void ensure(const qsizetype count) const
    {
        if (count < 0 || offset_ > bytes_.size() - count) {
            throw ProtocolError("Geometry payload is truncated");
        }
    }

    const QByteArray& bytes_;
    qsizetype offset_{};
};

template<typename Payload>
void validate(const Payload& payload)
{
    if (payload.vertices.isEmpty() || payload.vertices.size() % 3 != 0
        || payload.vertices.size() / 3 > static_cast<qsizetype>(kMaximumVertices)
        || payload.indices.isEmpty() || payload.indices.size() > static_cast<qsizetype>(kMaximumIndices)) {
        throw ProtocolError("Geometry counts are invalid");
    }
    for (const auto index : payload.indices) {
        if (index >= static_cast<quint32>(payload.vertices.size() / 3)) {
            throw ProtocolError("Geometry index is outside the vertex range");
        }
    }
    quint64 previousEnd = 0;
    for (const auto& range : payload.topology) {
        const auto expectedKind = std::is_same_v<Payload, MeshPayload> ? TopologyKind::Face : TopologyKind::Edge;
        const auto end = static_cast<quint64>(range.firstIndex) + range.indexCount;
        if (range.topologyId == 0 || range.kind != expectedKind || range.indexCount == 0
            || end > static_cast<quint64>(payload.indices.size()) || range.firstIndex < previousEnd
            || !std::isfinite(range.measureMm) || range.measureMm < 0.0F
            || !std::isfinite(range.radiusMm) || range.radiusMm < 0.0F) {
            throw ProtocolError("Geometry topology range is invalid");
        }
        previousEnd = end;
    }
}

template<typename Payload>
void writeHeader(Writer& writer, const GeometryType type, const Payload& payload)
{
    writer.u32(kMagic);
    writer.u16(kVersion);
    writer.u8(static_cast<quint8>(type));
    writer.u8(payload.refinement);
    writer.u64(payload.requestId);
    writer.u64(payload.documentGeneration);
    writer.string(payload.definitionId);
    writer.string(payload.nodeId);
    if constexpr (std::is_same_v<Payload, MeshPayload>) {
        writer.string(payload.segmentKey);
        writer.string(payload.sourceColor);
    } else {
        writer.string({});
        writer.string({});
    }
    writer.u32(static_cast<quint32>(payload.vertices.size() / 3));
    writer.u32(static_cast<quint32>(payload.indices.size()));
    if (payload.topology.size() > static_cast<qsizetype>(kMaximumTopologyRanges)) {
        throw ProtocolError("Geometry topology count is invalid");
    }
    writer.u32(static_cast<quint32>(payload.topology.size()));
}

template<typename Payload>
void writeData(Writer& writer, const Payload& payload)
{
    for (const auto value : payload.vertices) writer.number(value);
    if constexpr (std::is_same_v<Payload, MeshPayload>) {
        for (const auto value : payload.normals) writer.number(value);
    }
    for (const auto value : payload.indices) writer.u32(value);
    for (const auto& range : payload.topology) {
        writer.u32(range.topologyId);
        writer.u8(static_cast<quint8>(range.kind));
        writer.u32(range.firstIndex);
        writer.u32(range.indexCount);
        writer.number(range.measureMm);
        writer.number(range.radiusMm);
    }
}

template<typename Payload>
void readData(Reader& reader, Payload& payload, const quint32 vertexCount, const quint32 indexCount,
              const quint32 topologyCount)
{
    if (vertexCount == 0 || vertexCount > kMaximumVertices || indexCount == 0 || indexCount > kMaximumIndices) {
        throw ProtocolError("Geometry counts are invalid");
    }
    payload.vertices.reserve(static_cast<qsizetype>(vertexCount) * 3);
    for (quint32 index = 0; index < vertexCount * 3; ++index) payload.vertices.append(reader.number());
    if constexpr (std::is_same_v<Payload, MeshPayload>) {
        payload.normals.reserve(static_cast<qsizetype>(vertexCount) * 3);
        for (quint32 index = 0; index < vertexCount * 3; ++index) payload.normals.append(reader.number());
    }
    payload.indices.reserve(static_cast<qsizetype>(indexCount));
    for (quint32 index = 0; index < indexCount; ++index) payload.indices.append(reader.u32());
    if (topologyCount > kMaximumTopologyRanges) {
        throw ProtocolError("Geometry topology count is invalid");
    }
    payload.topology.reserve(topologyCount);
    for (quint32 index = 0; index < topologyCount; ++index) {
        payload.topology.append({reader.u32(), static_cast<TopologyKind>(reader.u8()), reader.u32(), reader.u32(),
                                 reader.number(), reader.number()});
    }
    validate(payload);
}

} // namespace

QByteArray encodeGeometry(const GeometryPayload& payload)
{
    return std::visit([](const auto& value) {
        validate(value);
        if constexpr (std::is_same_v<std::decay_t<decltype(value)>, MeshPayload>) {
            if (value.normals.size() != value.vertices.size()) {
                throw ProtocolError("Mesh normals do not match vertices");
            }
            Writer writer;
            writeHeader(writer, GeometryType::Mesh, value);
            writeData(writer, value);
            return writer.take();
        } else {
            Writer writer;
            writeHeader(writer, GeometryType::Edges, value);
            writeData(writer, value);
            return writer.take();
        }
    }, payload);
}

GeometryPayload decodeGeometry(const QByteArray& bytes)
{
    Reader reader(bytes);
    if (reader.u32() != kMagic || reader.u16() != kVersion) {
        throw ProtocolError("Unsupported geometry payload version");
    }
    const auto type = reader.u8();
    const auto refinement = reader.u8();
    const auto requestId = reader.u64();
    const auto documentGeneration = reader.u64();
    const auto definitionId = reader.string();
    const auto nodeId = reader.string();
    const auto segmentKey = reader.string();
    const auto sourceColor = reader.string();
    const auto vertexCount = reader.u32();
    const auto indexCount = reader.u32();
    const auto topologyCount = reader.u32();
    if (type == static_cast<quint8>(GeometryType::Mesh)) {
        MeshPayload payload{requestId, documentGeneration, definitionId, nodeId, segmentKey, sourceColor, refinement, {}, {}, {}, {}};
        readData(reader, payload, vertexCount, indexCount, topologyCount);
        reader.ensureRemaining();
        return payload;
    }
    if (type == static_cast<quint8>(GeometryType::Edges)) {
        if (!segmentKey.isEmpty() || !sourceColor.isEmpty()) {
            throw ProtocolError("Edge payload metadata is invalid");
        }
        EdgePayload payload{requestId, documentGeneration, definitionId, nodeId, refinement, {}, {}, {}};
        readData(reader, payload, vertexCount, indexCount, topologyCount);
        reader.ensureRemaining();
        return payload;
    }
    throw ProtocolError("Geometry payload type is invalid");
}

} // namespace loupe::protocol
