#include "protocol/GeometryPayload.h"

#include "protocol/ProtocolTypes.h"

#include <QtEndian>

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <string>
#include <type_traits>

namespace loupe::protocol {
namespace {

constexpr quint32 kMagic = 0x4c504750; // LPGP
// 3: topology ranges carry a round face's axis.
constexpr quint16 kVersion = 3;
constexpr quint32 kMaximumStringBytes = 4 * 1024;
constexpr quint32 kMaximumVertices = 10 * 1000 * 1000;
constexpr quint32 kMaximumIndices = 30 * 1000 * 1000;
constexpr quint32 kMaximumTopologyRanges = 2 * 1000 * 1000;

enum class GeometryType : quint8 {
    Mesh = 1,
    Edges = 2,
    SharedMesh = 3,
    SharedEdges = 4,
    MeshInstance = 5,
    EdgeInstance = 6,
};

[[nodiscard]] bool isMeshType(const quint8 type) noexcept
{
    return type == static_cast<quint8>(GeometryType::Mesh) || type == static_cast<quint8>(GeometryType::SharedMesh);
}

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
    void skip(const qsizetype count)
    {
        ensure(count);
        offset_ += count;
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
        const char* reason = nullptr;
        if (range.topologyId == 0) reason = "topologyId is zero";
        else if (range.kind != expectedKind) reason = "kind mismatch";
        else if (range.indexCount == 0) reason = "indexCount is zero";
        else if (end > static_cast<quint64>(payload.indices.size())) reason = "range end past index count";
        else if (range.firstIndex < previousEnd) reason = "firstIndex overlaps previous range";
        else if (!std::isfinite(range.measureMm) || range.measureMm < 0.0F) reason = "measureMm invalid";
        else if (!std::isfinite(range.radiusMm) || range.radiusMm < 0.0F) reason = "radiusMm invalid";
        else if (!std::all_of(range.axisOrigin.begin(), range.axisOrigin.end(), [](float v) { return std::isfinite(v); })
                 || !std::all_of(range.axisDirection.begin(), range.axisDirection.end(), [](float v) { return std::isfinite(v); }))
            reason = "axis not finite";
        else if (range.hasAxis()
                 && std::abs(range.axisDirection[0] * range.axisDirection[0] + range.axisDirection[1] * range.axisDirection[1]
                             + range.axisDirection[2] * range.axisDirection[2] - 1.0F) > 1e-3F)
            reason = "axis direction not unit";
        if (reason) {
            throw ProtocolError(std::string("Geometry topology range is invalid: ") + reason
                                + " (topologyId=" + std::to_string(range.topologyId)
                                + " firstIndex=" + std::to_string(range.firstIndex)
                                + " indexCount=" + std::to_string(range.indexCount)
                                + " previousEnd=" + std::to_string(previousEnd)
                                + " indices=" + std::to_string(payload.indices.size())
                                + " measureMm=" + std::to_string(range.measureMm)
                                + " radiusMm=" + std::to_string(range.radiusMm) + ")");
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
        for (const auto value : range.axisOrigin) writer.number(value);
        for (const auto value : range.axisDirection) writer.number(value);
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
        TopologyRange range;
        range.topologyId = reader.u32();
        range.kind = static_cast<TopologyKind>(reader.u8());
        range.firstIndex = reader.u32();
        range.indexCount = reader.u32();
        range.measureMm = reader.number();
        range.radiusMm = reader.number();
        for (auto& value : range.axisOrigin) value = reader.number();
        for (auto& value : range.axisDirection) value = reader.number();
        payload.topology.append(range);
    }
    validate(payload);
}

} // namespace

void validatePlacement(const std::array<float, 12>& placement)
{
    for (const auto value : placement) {
        if (!std::isfinite(value)) throw ProtocolError("Geometry instance placement is not finite");
    }
}

QByteArray encodeGeometry(const GeometryPayload& payload)
{
    return std::visit([](const auto& value) {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, InstancePayload>) {
            validatePlacement(value.placement);
            if (value.nodeId.isEmpty() || value.sourceKey.isEmpty()) throw ProtocolError("Geometry instance names no node or source");
            Writer writer;
            writer.u32(kMagic);
            writer.u16(kVersion);
            writer.u8(static_cast<quint8>(value.edges ? GeometryType::EdgeInstance : GeometryType::MeshInstance));
            writer.u8(value.refinement);
            writer.u64(value.requestId);
            writer.u64(value.documentGeneration);
            writer.string(value.nodeId);
            writer.string(value.segmentKey);
            writer.string(value.sourceKey);
            writer.string(value.sourceColor);
            for (const auto entry : value.placement) writer.number(entry);
            return writer.take();
        } else {
            validate(value);
            if constexpr (std::is_same_v<Value, MeshPayload>) {
                if (value.normals.size() != value.vertices.size()) {
                    throw ProtocolError("Mesh normals do not match vertices");
                }
                Writer writer;
                writeHeader(writer, value.shared ? GeometryType::SharedMesh : GeometryType::Mesh, value);
                writeData(writer, value);
                return writer.take();
            } else {
                Writer writer;
                writeHeader(writer, value.shared ? GeometryType::SharedEdges : GeometryType::Edges, value);
                writeData(writer, value);
                return writer.take();
            }
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
    if (type == static_cast<quint8>(GeometryType::MeshInstance) || type == static_cast<quint8>(GeometryType::EdgeInstance)) {
        InstancePayload payload;
        payload.requestId = requestId;
        payload.documentGeneration = documentGeneration;
        payload.refinement = refinement;
        payload.edges = type == static_cast<quint8>(GeometryType::EdgeInstance);
        payload.nodeId = reader.string();
        payload.segmentKey = reader.string();
        payload.sourceKey = reader.string();
        payload.sourceColor = reader.string();
        for (auto& entry : payload.placement) entry = reader.number();
        reader.ensureRemaining();
        validatePlacement(payload.placement);
        if (payload.nodeId.isEmpty() || payload.sourceKey.isEmpty()) throw ProtocolError("Geometry instance names no node or source");
        return payload;
    }
    const auto definitionId = reader.string();
    const auto nodeId = reader.string();
    const auto segmentKey = reader.string();
    const auto sourceColor = reader.string();
    const auto vertexCount = reader.u32();
    const auto indexCount = reader.u32();
    const auto topologyCount = reader.u32();
    if (isMeshType(type)) {
        MeshPayload payload{requestId, documentGeneration, definitionId, nodeId, segmentKey, sourceColor, refinement, {}, {}, {}, {}};
        payload.shared = type == static_cast<quint8>(GeometryType::SharedMesh);
        readData(reader, payload, vertexCount, indexCount, topologyCount);
        reader.ensureRemaining();
        return payload;
    }
    if (type == static_cast<quint8>(GeometryType::Edges) || type == static_cast<quint8>(GeometryType::SharedEdges)) {
        if (!segmentKey.isEmpty() || !sourceColor.isEmpty()) {
            throw ProtocolError("Edge payload metadata is invalid");
        }
        EdgePayload payload{requestId, documentGeneration, definitionId, nodeId, refinement, {}, {}, {}};
        payload.shared = type == static_cast<quint8>(GeometryType::SharedEdges);
        readData(reader, payload, vertexCount, indexCount, topologyCount);
        reader.ensureRemaining();
        return payload;
    }
    throw ProtocolError("Geometry payload type is invalid");
}

double meshFaceArea(const QByteArray& bytes) noexcept
{
    try {
        Reader reader(bytes);
        if (reader.u32() != kMagic || reader.u16() != kVersion) return 0.0;
        if (!isMeshType(reader.u8())) return 0.0;
        static_cast<void>(reader.u8());
        static_cast<void>(reader.u64());
        static_cast<void>(reader.u64());
        for (int field = 0; field < 4; ++field) static_cast<void>(reader.string());
        const auto vertexCount = reader.u32();
        const auto indexCount = reader.u32();
        const auto topologyCount = reader.u32();
        if (vertexCount > kMaximumVertices || indexCount > kMaximumIndices || topologyCount > kMaximumTopologyRanges)
            return 0.0;
        // Vertices and normals (3 floats each per vertex), then the u32 indices.
        reader.skip(static_cast<qsizetype>(vertexCount) * 6 * 4 + static_cast<qsizetype>(indexCount) * 4);
        double area = 0.0;
        for (quint32 index = 0; index < topologyCount; ++index) {
            reader.skip(4 + 1 + 4 + 4); // topologyId, kind, firstIndex, indexCount
            const auto measure = reader.number();
            reader.skip(4 + 6 * 4); // radiusMm, axis origin and direction
            if (std::isfinite(measure) && measure > 0.0F) area += measure;
        }
        return area;
    } catch (...) {
        return 0.0;
    }
}

namespace {

std::optional<MeshBounds> boundsOf(const QByteArray& bytes, const std::array<float, 12>* placement) noexcept
{
    try {
        Reader reader(bytes);
        if (reader.u32() != kMagic || reader.u16() != kVersion) return std::nullopt;
        if (!isMeshType(reader.u8())) return std::nullopt;
        static_cast<void>(reader.u8());
        static_cast<void>(reader.u64());
        static_cast<void>(reader.u64());
        for (int field = 0; field < 4; ++field) static_cast<void>(reader.string());
        const auto vertexCount = reader.u32();
        static_cast<void>(reader.u32());
        static_cast<void>(reader.u32());
        if (vertexCount == 0 || vertexCount > kMaximumVertices) return std::nullopt;
        MeshBounds bounds;
        bool found = false;
        for (quint32 vertex = 0; vertex < vertexCount; ++vertex) {
            std::array<float, 3> point{reader.number(), reader.number(), reader.number()};
            if (!std::isfinite(point[0]) || !std::isfinite(point[1]) || !std::isfinite(point[2])) continue;
            if (placement) {
                const auto& m = *placement;
                point = {m[0] * point[0] + m[1] * point[1] + m[2] * point[2] + m[3],
                         m[4] * point[0] + m[5] * point[1] + m[6] * point[2] + m[7],
                         m[8] * point[0] + m[9] * point[1] + m[10] * point[2] + m[11]};
            }
            for (std::size_t axis = 0; axis < 3; ++axis) {
                bounds.minimum[axis] = found ? std::min(bounds.minimum[axis], point[axis]) : point[axis];
                bounds.maximum[axis] = found ? std::max(bounds.maximum[axis], point[axis]) : point[axis];
            }
            found = true;
        }
        if (!found) return std::nullopt;
        return bounds;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace

std::optional<MeshBounds> meshBounds(const QByteArray& bytes) noexcept
{
    return boundsOf(bytes, nullptr);
}

std::optional<MeshBounds> meshBounds(const QByteArray& bytes, const std::array<float, 12>& placement) noexcept
{
    return boundsOf(bytes, &placement);
}

} // namespace loupe::protocol
