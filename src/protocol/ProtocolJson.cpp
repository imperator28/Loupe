#include "protocol/ProtocolTypes.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <cmath>
#include <limits>
#include <type_traits>

namespace loupe::protocol {
namespace {

[[noreturn]] void fail(const char* message)
{
    throw ProtocolError(message);
}

QJsonObject versionObject()
{
    return {{QStringLiteral("major"), 1}, {QStringLiteral("minor"), 0}};
}

QJsonObject frame(const QByteArray& bytes)
{
    if (!bytes.endsWith('\n')) {
        fail("Protocol frame must end with newline");
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        fail("Protocol frame must be a JSON object");
    }
    return document.object();
}

void validateVersion(const QJsonObject& object)
{
    const auto version = object.value(QStringLiteral("version"));
    if (!version.isObject()) {
        fail("Protocol version missing");
    }
    const auto major = version.toObject().value(QStringLiteral("major"));
    if (!major.isDouble() || major.toInt(-1) != 1) {
        fail("Unsupported protocol major version");
    }
}

QString stringField(const QJsonObject& object, const QString& name)
{
    const auto value = object.value(name);
    if (!value.isString()) {
        fail("Protocol string field missing");
    }
    return value.toString();
}

std::uint64_t requestId(const QJsonObject& object)
{
    const auto value = object.value(QStringLiteral("requestId"));
    const auto number = value.toDouble(-1.0);
    if (!value.isDouble() || number < 0.0 || std::floor(number) != number
        || number > static_cast<double>(std::numeric_limits<std::uint64_t>::max())) {
        fail("Protocol request id invalid");
    }
    return static_cast<std::uint64_t>(number);
}

QByteArray serialize(QJsonObject object)
{
    object.insert(QStringLiteral("version"), versionObject());
    return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
}

} // namespace

QByteArray encode(const OpenFile& command)
{
    QJsonObject object{{QStringLiteral("type"), QStringLiteral("openFile")},
                       {QStringLiteral("requestId"), static_cast<qint64>(command.requestId)},
                       {QStringLiteral("path"), command.path}};
    if (command.unitOverride) {
        object.insert(QStringLiteral("unitOverride"),
                      QJsonObject{{QStringLiteral("unit"), command.unitOverride->unit},
                                  {QStringLiteral("customFactor"), command.unitOverride->customFactor},
                                  {QStringLiteral("reason"), command.unitOverride->reason}});
    }
    return serialize(std::move(object));
}

QByteArray encode(const Command& command)
{
    return std::visit([](const auto& value) -> QByteArray {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, OpenFile>) {
            return encode(value);
        } else if constexpr (std::is_same_v<Value, Cancel>) {
            return serialize({{QStringLiteral("type"), QStringLiteral("cancel")},
                              {QStringLiteral("requestId"), static_cast<qint64>(value.requestId)}});
        } else if constexpr (std::is_same_v<Value, MeasureAtPoint>) {
            return serialize({{QStringLiteral("type"), QStringLiteral("measureAtPoint")},
                              {QStringLiteral("requestId"), static_cast<qint64>(value.requestId)},
                              {QStringLiteral("nodeId"), value.nodeId}, {QStringLiteral("x"), value.x},
                              {QStringLiteral("y"), value.y}, {QStringLiteral("z"), value.z}, {QStringLiteral("mode"), value.mode}});
        } else {
            return serialize({{QStringLiteral("type"), QStringLiteral("setVisible")},
                              {QStringLiteral("nodeId"), value.nodeId},
                              {QStringLiteral("visible"), value.visible}});
        }
    }, command);
}

Command decodeCommand(const QByteArray& bytes)
{
    const auto object = frame(bytes);
    validateVersion(object);
    const auto type = stringField(object, QStringLiteral("type"));
    if (type == QStringLiteral("openFile")) {
        std::optional<UnitOverride> unitOverride;
        const auto value = object.value(QStringLiteral("unitOverride"));
        if (!value.isUndefined()) {
            if (!value.isObject()) {
                fail("Protocol unit override invalid");
            }
            const auto override = value.toObject();
            const auto factor = override.value(QStringLiteral("customFactor"));
            if (!factor.isDouble() || !std::isfinite(factor.toDouble())) {
                fail("Protocol unit factor invalid");
            }
            unitOverride = UnitOverride{stringField(override, QStringLiteral("unit")), factor.toDouble(),
                                        stringField(override, QStringLiteral("reason"))};
        }
        return OpenFile{requestId(object), stringField(object, QStringLiteral("path")), std::move(unitOverride)};
    }
    if (type == QStringLiteral("cancel")) {
        return Cancel{requestId(object)};
    }
    if (type == QStringLiteral("measureAtPoint")) {
        const auto x = object.value(QStringLiteral("x")); const auto y = object.value(QStringLiteral("y")); const auto z = object.value(QStringLiteral("z"));
        if (!x.isDouble() || !y.isDouble() || !z.isDouble() || !std::isfinite(x.toDouble()) || !std::isfinite(y.toDouble()) || !std::isfinite(z.toDouble())) fail("Protocol measurement point invalid");
        return MeasureAtPoint{requestId(object), stringField(object, QStringLiteral("nodeId")), x.toDouble(), y.toDouble(), z.toDouble(), stringField(object, QStringLiteral("mode"))};
    }
    if (type == QStringLiteral("setVisible")) {
        const auto visible = object.value(QStringLiteral("visible"));
        if (!visible.isBool()) {
            fail("Protocol visibility invalid");
        }
        return SetVisible{stringField(object, QStringLiteral("nodeId")), visible.toBool()};
    }
    fail("Unknown protocol command type");
}

Event decodeEvent(const QByteArray& bytes)
{
    const auto object = frame(bytes);
    validateVersion(object);
    const auto type = stringField(object, QStringLiteral("type"));
    if (type == QStringLiteral("ready")) {
        return Ready{};
    }
    if (type == QStringLiteral("progress")) {
        return Progress{requestId(object), stringField(object, QStringLiteral("stage")), object.value(QStringLiteral("fraction")).toDouble()};
    }
    if (type == QStringLiteral("snapshotReady")) {
        return SnapshotReady{requestId(object), QByteArray::fromBase64(stringField(object, QStringLiteral("snapshotBase64")).toLatin1())};
    }
    if (type == QStringLiteral("meshReady")) {
        return MeshReady{requestId(object), stringField(object, QStringLiteral("definitionId")), object.value(QStringLiteral("refinement")).toInt(),
                         stringField(object, QStringLiteral("segmentKey")),
                         QByteArray::fromBase64(stringField(object, QStringLiteral("meshBase64")).toLatin1())};
    }
    if (type == QStringLiteral("failed")) {
        const auto recoverable = object.value(QStringLiteral("recoverable"));
        if (!recoverable.isBool()) {
            fail("Protocol recoverable invalid");
        }
        return Failed{requestId(object), stringField(object, QStringLiteral("code")), stringField(object, QStringLiteral("message")),
                      recoverable.toBool()};
    }
    if (type == QStringLiteral("canceled")) {
        return Canceled{requestId(object)};
    }
    fail("Unknown protocol event type");
}

} // namespace loupe::protocol
