#pragma once

#include <QByteArray>
#include <QString>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <variant>

namespace loupe::protocol {

struct Version { std::uint16_t major{1}; std::uint16_t minor{0}; };
struct UnitOverride { QString unit; double customFactor{1.0}; QString reason; };
struct OpenFile { std::uint64_t requestId{}; QString path; std::optional<UnitOverride> unitOverride; };
struct Cancel { std::uint64_t requestId{}; };
struct SetVisible { QString nodeId; bool visible{}; };
struct Ready { Version version; };
struct Progress { std::uint64_t requestId{}; QString stage; double fraction{}; };
struct SnapshotReady { std::uint64_t requestId{}; QByteArray snapshotJson; };
struct MeshReady { std::uint64_t requestId{}; QString definitionId; int refinement{}; QString segmentKey; QByteArray meshJson; };
struct Failed { std::uint64_t requestId{}; QString code; QString message; bool recoverable{}; };
struct Canceled { std::uint64_t requestId{}; };

using Command = std::variant<OpenFile, Cancel, SetVisible>;
using Event = std::variant<Ready, Progress, SnapshotReady, MeshReady, Failed, Canceled>;

class ProtocolError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

QByteArray encode(const OpenFile& command);
QByteArray encode(const Command& command);
Command decodeCommand(const QByteArray& bytes);
Event decodeEvent(const QByteArray& bytes);

} // namespace loupe::protocol
