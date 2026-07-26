#include "worker/WorkerServer.h"

#include "core/import/StepImporter.h"
#include "core/drawing/DrawingExporter.h"
#include "core/drawing/DrawingPlan.h"
#include "core/drawing/DrawingPreview.h"
#include "core/drawing/DrawingProjector.h"
#include "core/export/ExportPlan.h"
#include "core/export/ShapeSelection.h"
#include "core/export/StepExporter.h"
#include "core/export/StlExporter.h"
#include "core/inspection/GeometryAnalysis.h"
#include "core/inspection/TopologyAnalysis.h"
#include "core/units/UnitPolicy.h"
#include "core/validation/OutputValidator.h"
#include "protocol/GeometryPayload.h"
#include "protocol/ProtocolFrame.h"
#include "protocol/ProtocolTypes.h"

#include <QFileInfo>
#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QMetaObject>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <QElapsedTimer>

#include <BRep_Tool.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <BRepLib_ToolTriangulatedShape.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <GCPnts_TangentialDeflection.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Poly_Triangulation.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>

#include <gp_Pnt.hxx>
#include <Quantity_ColorRGBA.hxx>

#include <exception>
#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <type_traits>

namespace loupe::worker {
namespace {

std::optional<loupe::units::UnitOverride> toUnitOverride(const QString& unit, const double factor, const QString& reason)
{
    if (unit.isEmpty()) return std::nullopt;
    const auto interpretAs = unit == QStringLiteral("mm") ? loupe::units::LengthUnit::Millimeter
                           : unit == QStringLiteral("in") ? loupe::units::LengthUnit::Inch
                                                           : loupe::units::LengthUnit::Unknown;
    return loupe::units::UnitOverride{interpretAs, factor, reason.toStdString()};
}

struct DecodedExportPlan final {
    loupe::exporting::PlanRequest request;
    std::vector<std::string> reviewedNodeOrder;
};

DecodedExportPlan decodeExportPlan(const QByteArray& bytes)
{
    const auto document = QJsonDocument::fromJson(bytes);
    if (!document.isObject()) throw std::runtime_error("reviewed export plan is not valid JSON");
    const auto object = document.object();
    if (object.value(QStringLiteral("schemaVersion")).toInt() != 1) {
        throw std::runtime_error("reviewed export plan version is not supported");
    }

    DecodedExportPlan decoded;
    auto& request = decoded.request;
    request.destination = object.value(QStringLiteral("destination")).toString().toUtf8().toStdString();
    const auto format = object.value(QStringLiteral("format")).toString();
    request.format = format == QStringLiteral("STL") ? loupe::exporting::Format::Stl : loupe::exporting::Format::Step;
    const auto coordinates = object.value(QStringLiteral("coordinates")).toString();
    request.coordinates = coordinates == QStringLiteral("Local")
        ? loupe::exporting::Coordinates::Local : loupe::exporting::Coordinates::Assembly;
    request.grouping = loupe::exporting::Grouping::SeparateFiles;
    request.stepOutputUnit = loupe::exporting::StepOutputUnit::Millimeter;
    request.requestedUnitToMillimeters = 1.0;
    const auto effectiveUnit = object.value(QStringLiteral("effectiveUnit")).toString();
    const auto sourceScale = object.value(QStringLiteral("sourceToMillimeters")).toDouble(0.0);
    request.unitDecision = {effectiveUnit == QStringLiteral("in") ? loupe::units::LengthUnit::Inch
                                                                   : loupe::units::LengthUnit::Millimeter,
                            loupe::units::UnitConfidence::Confirmed, sourceScale, "reviewed in Loupe"};

    const auto selections = object.value(QStringLiteral("selections"));
    if (!selections.isArray()) throw std::runtime_error("reviewed export selections are missing");
    for (const auto& value : selections.toArray()) {
        const auto selection = value.toObject();
        const auto nodeId = selection.value(QStringLiteral("nodeId")).toString().toUtf8().toStdString();
        const auto hierarchyPath = selection.value(QStringLiteral("hierarchyPath")).toString().toUtf8().toStdString();
        const auto leafName = selection.value(QStringLiteral("leafName")).toString().toUtf8().toStdString();
        const auto kind = selection.value(QStringLiteral("kind"));
        if (nodeId.empty() || hierarchyPath.empty() || leafName.empty() || !kind.isDouble()) {
            throw std::runtime_error("reviewed export selection is incomplete");
        }
        request.selections.push_back({nodeId, static_cast<loupe::exporting::SelectionKind>(kind.toInt(-1))});
        request.hierarchyPaths.emplace(nodeId, hierarchyPath);
        request.outputLeafNames.emplace(nodeId, leafName);
        decoded.reviewedNodeOrder.push_back(nodeId);
    }
    return decoded;
}

QByteArray encodeSnapshot(const loupe::import::ImportResult& imported, const loupe::units::UnitDecision& unitDecision)
{
    QJsonArray nodes;
    for (const auto& node : imported.snapshot.nodes) {
        nodes.append(QJsonObject{
            {QStringLiteral("id"), QString::fromStdString(node.id)},
            {QStringLiteral("name"), QString::fromStdString(node.name)},
            {QStringLiteral("kind"), static_cast<int>(node.kind)},
            {QStringLiteral("parentId"), node.parentId ? QString::fromStdString(*node.parentId) : QString{}},
            {QStringLiteral("definitionId"), node.definitionId ? QString::fromStdString(*node.definitionId) : QString{}},
        });
    }
    QJsonArray geometry;
    for (std::size_t index = 0; index < imported.native->shapes.size() && index < imported.native->shapeNodeIds.size(); ++index) {
        geometry.append(QJsonObject{
            {QStringLiteral("nodeId"), QString::fromStdString(imported.native->shapeNodeIds[index])},
        });
    }
    return QJsonDocument(QJsonObject{
        {QStringLiteral("sourceHash"), QString::fromStdString(imported.snapshot.sourceHash)},
        {QStringLiteral("classification"), static_cast<int>(imported.snapshot.classification)},
        {QStringLiteral("definitionCount"), static_cast<qint64>(imported.definitionCount)},
        {QStringLiteral("occurrenceCount"), static_cast<qint64>(imported.occurrenceCount)},
        {QStringLiteral("nodes"), nodes},
        {QStringLiteral("geometry"), geometry},
        {QStringLiteral("effectiveUnit"), unitDecision.effectiveUnit == loupe::units::LengthUnit::Inch ? QStringLiteral("in") : QStringLiteral("mm")},
        {QStringLiteral("sourceToMillimeters"), unitDecision.sourceToMillimeters},
    }).toJson(QJsonDocument::Compact);
}

struct MeshSegment final {
    QString color;
    QVector<float> vertices;
    QVector<float> normals;
    QVector<quint32> indices;
    QVector<protocol::TopologyRange> topology;
};

struct EdgePayload final {
    QVector<float> vertices;
    QVector<quint32> indices;
    QVector<protocol::TopologyRange> topology;
};

struct TessellationProfile final {
    QString progressStage;
    double linearDeflectionMm{};
    double angularDeflectionRadians{};
    quint8 refinement{};
};

const TessellationProfile kPreviewProfile{
    QStringLiteral("Showing preview geometry"), 0.35, 0.42, 0};
const TessellationProfile kRefinedProfile{
    QStringLiteral("Refining viewport geometry"), 0.025, 0.12, 1};

QString colorString(const Quantity_ColorRGBA& color)
{
    const auto& rgb = color.GetRGB();
    const auto channel = [](const double value) {
        return QStringLiteral("%1").arg(std::clamp(qRound(value * 255.0), 0, 255), 2, 16, QLatin1Char('0'));
    };
    return (QStringLiteral("#") + channel(rgb.Red()) + channel(rgb.Green()) + channel(rgb.Blue())).toUpper();
}

QString colorFor(const occ::handle<XCAFDoc_ColorTool>& colors, const TDF_Label& occurrence,
                 const TDF_Label& definition, const TopoDS_Face& face)
{
    Quantity_ColorRGBA color;
    if (colors && (colors->GetColor(face, XCAFDoc_ColorSurf, color)
                   || colors->GetColor(face, XCAFDoc_ColorGen, color)
                   || XCAFDoc_ColorTool::GetColor(occurrence, XCAFDoc_ColorSurf, color)
                   || XCAFDoc_ColorTool::GetColor(occurrence, XCAFDoc_ColorGen, color)
                   || XCAFDoc_ColorTool::GetColor(definition, XCAFDoc_ColorSurf, color)
                   || XCAFDoc_ColorTool::GetColor(definition, XCAFDoc_ColorGen, color))) {
        return colorString(color);
    }
    return QStringLiteral("#67D5C0");
}

void prepareTessellation(const TopoDS_Shape& shape, const double sourceToMillimeters,
                         const TessellationProfile& profile)
{
    const auto linearDeflection = std::clamp(profile.linearDeflectionMm / std::max(sourceToMillimeters, 1.0e-9), 1.0e-5, 1.0);
    BRepMesh_IncrementalMesh mesher(shape, linearDeflection, false, profile.angularDeflectionRadians, true);
}

// OCCT's GProp mass (a face area or edge length) can come back as a tiny
// negative value from floating-point roundoff on near-degenerate geometry.
// The wire protocol requires every topology measure to be finite and
// non-negative, so clamp numerical noise to zero here rather than emit a
// schema-invalid payload — which fails validation in encodeGeometry and, since
// that runs on the main thread's event loop, would terminate the whole worker.
float sanitizeMeasure(const double value)
{
    return (std::isfinite(value) && value > 0.0) ? static_cast<float>(value) : 0.0F;
}

QVector<MeshSegment> encodeMesh(const TopoDS_Shape& shape, const gp_Trsf& placement, const double sourceToMillimeters,
                                const occ::handle<XCAFDoc_ColorTool>& colors,
                                const TDF_Label& occurrence, const TDF_Label& definition)
{
    QHash<QString, int> segmentIndexes;
    QVector<MeshSegment> segments;
    TopTools_IndexedMapOfShape faceMap;
    TopExp::MapShapes(shape, TopAbs_FACE, faceMap);
    for (int faceIndex = 1; faceIndex <= faceMap.Extent(); ++faceIndex) {
        const auto face = TopoDS::Face(faceMap(faceIndex));
        TopLoc_Location location;
        const auto triangulation = BRep_Tool::Triangulation(face, location);
        if (triangulation.IsNull()) continue;
        const auto color = colorFor(colors, occurrence, definition, face);
        const auto segmentIndex = segmentIndexes.value(color, -1);
        if (segmentIndex < 0) {
            segmentIndexes.insert(color, segments.size());
            segments.append({color, {}, {}, {}, {}});
        }
        auto& segment = segments[segmentIndexes.value(color)];
        const int vertexOffset = segment.vertices.size() / 3;
        const auto transform = placement.Multiplied(location.Transformation());
        BRepLib_ToolTriangulatedShape::ComputeNormals(face, triangulation);
        const auto firstIndex = static_cast<quint32>(segment.indices.size());
        for (int node = 1; node <= triangulation->NbNodes(); ++node) {
            const auto point = triangulation->Node(node).Transformed(transform);
            segment.vertices.append(static_cast<float>(point.X() * sourceToMillimeters));
            segment.vertices.append(static_cast<float>(point.Y() * sourceToMillimeters));
            segment.vertices.append(static_cast<float>(point.Z() * sourceToMillimeters));
            auto normal = triangulation->HasNormals() ? triangulation->Normal(node) : gp_Dir{0.0, 0.0, 1.0};
            normal.Transform(transform);
            if (face.Orientation() == TopAbs_REVERSED) normal.Reverse();
            segment.normals.append(static_cast<float>(normal.X()));
            segment.normals.append(static_cast<float>(normal.Y()));
            segment.normals.append(static_cast<float>(normal.Z()));
        }
        for (int triangle = 1; triangle <= triangulation->NbTriangles(); ++triangle) {
            int first = 0;
            int second = 0;
            int third = 0;
            triangulation->Triangle(triangle).Get(first, second, third);
            if (face.Orientation() == TopAbs_REVERSED) std::swap(second, third);
            segment.indices.append(static_cast<quint32>(vertexOffset + first - 1));
            segment.indices.append(static_cast<quint32>(vertexOffset + second - 1));
            segment.indices.append(static_cast<quint32>(vertexOffset + third - 1));
        }
        GProp_GProps properties;
        BRepGProp::SurfaceProperties(face, properties);
        float radiusMm = 0.0F;
        try {
            const BRepAdaptor_Surface surface(face);
            if (surface.GetType() == GeomAbs_Cylinder) {
                radiusMm = static_cast<float>(surface.Cylinder().Radius() * sourceToMillimeters);
            } else if (surface.GetType() == GeomAbs_Sphere) {
                radiusMm = static_cast<float>(surface.Sphere().Radius() * sourceToMillimeters);
            }
        } catch (const Standard_Failure&) {
        }
        segment.topology.append({static_cast<quint32>(faceIndex), protocol::TopologyKind::Face,
                                 firstIndex, static_cast<quint32>(segment.indices.size()) - firstIndex,
                                 sanitizeMeasure(properties.Mass() * sourceToMillimeters * sourceToMillimeters),
                                 sanitizeMeasure(radiusMm)});
    }
    return segments;
}

EdgePayload encodeEdges(const TopoDS_Shape& shape, const gp_Trsf& placement, const double sourceToMillimeters, const TessellationProfile& profile)
{
    EdgePayload payload;
    TopTools_IndexedMapOfShape edgeMap;
    TopExp::MapShapes(shape, TopAbs_EDGE, edgeMap);
    const auto linearDeflection = std::clamp(profile.linearDeflectionMm / std::max(sourceToMillimeters, 1.0e-9), 1.0e-5, 1.0);
    for (int edgeIndex = 1; edgeIndex <= edgeMap.Extent(); ++edgeIndex) {
        const auto edge = TopoDS::Edge(edgeMap(edgeIndex));
        if (BRep_Tool::Degenerated(edge)) continue;
        try {
            const BRepAdaptor_Curve curve(edge);
            const auto first = curve.FirstParameter();
            const auto last = curve.LastParameter();
            if (!std::isfinite(first) || !std::isfinite(last) || qFuzzyCompare(first, last)) continue;
            const GCPnts_TangentialDeflection points(curve, first, last, profile.angularDeflectionRadians, linearDeflection, 2);
            if (points.NbPoints() < 2) continue;
            const auto firstVertex = payload.vertices.size() / 3;
            const auto firstIndex = static_cast<quint32>(payload.indices.size());
            for (int pointIndex = 1; pointIndex <= points.NbPoints(); ++pointIndex) {
                const auto point = points.Value(pointIndex).Transformed(placement);
                payload.vertices.append(static_cast<float>(point.X() * sourceToMillimeters));
                payload.vertices.append(static_cast<float>(point.Y() * sourceToMillimeters));
                payload.vertices.append(static_cast<float>(point.Z() * sourceToMillimeters));
                if (pointIndex > 1) {
                    payload.indices.append(static_cast<quint32>(firstVertex + pointIndex - 2));
                    payload.indices.append(static_cast<quint32>(firstVertex + pointIndex - 1));
                }
            }
            GProp_GProps properties;
            BRepGProp::LinearProperties(edge, properties);
            const auto radiusMm = curve.GetType() == GeomAbs_Circle
                ? static_cast<float>(curve.Circle().Radius() * sourceToMillimeters) : 0.0F;
            payload.topology.append({static_cast<quint32>(edgeIndex), protocol::TopologyKind::Edge,
                                     firstIndex, static_cast<quint32>(payload.indices.size()) - firstIndex,
                                     sanitizeMeasure(properties.Mass() * sourceToMillimeters),
                                     sanitizeMeasure(radiusMm)});
        } catch (const Standard_Failure&) {
            continue;
        }
    }
    return payload;
}

} // namespace

struct WorkerServer::DocumentSession final {
    import::ImportResult imported;
    units::UnitDecision unitDecision;
};

WorkerServer::WorkerServer(QObject* parent)
    : QObject(parent)
{
    connect(&server_, &QLocalServer::newConnection, this, &WorkerServer::acceptConnection);
}

bool WorkerServer::listen(const QString& serverName)
{
    QLocalServer::removeServer(serverName);
    return server_.listen(serverName);
}

void WorkerServer::acceptConnection()
{
    if (socket_) {
        server_.nextPendingConnection()->disconnectFromServer();
        return;
    }
    socket_ = server_.nextPendingConnection();
    commandFrames_.clear();
    connect(socket_, &QLocalSocket::readyRead, this, &WorkerServer::readCommands);
    connect(socket_, &QLocalSocket::disconnected, this, [this] {
        for (const auto& job : std::as_const(activeSessions_)) job->canceled.store(true);
        for (const auto& job : std::as_const(activeExports_)) job->canceled.store(true);
        socket_.clear();
        server_.close();
        QCoreApplication::quit();
    });
    send({{QStringLiteral("type"), QStringLiteral("ready")}});
}

void WorkerServer::readCommands()
{
    if (!socket_) return;
    try {
        commandFrames_.append(socket_->readAll());
    } catch (const protocol::ProtocolError&) {
        fail(0, QStringLiteral("protocol_error"), QStringLiteral("Worker command exceeds frame bounds"));
        socket_->disconnectFromServer();
        return;
    }
    for (;;) {
        try {
            const auto frame = commandFrames_.take();
            if (!frame) return;
            if (frame->type != protocol::FrameType::ControlJson) {
                throw protocol::ProtocolError("Worker commands must be control frames");
            }
            const auto command = protocol::decodeCommand(frame->payload);
            std::visit([this](const auto& value) {
                using Value = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, protocol::OpenFile>) {
                    open(value.requestId, value.path, value.unitOverride);
                } else if constexpr (std::is_same_v<Value, protocol::Cancel>) {
                    cancel(value.requestId);
                } else if constexpr (std::is_same_v<Value, protocol::ExecuteExportPlan>) {
                    executeExportPlan(value.requestId, value.planJson, value.fingerprint);
                } else if constexpr (std::is_same_v<Value, protocol::ExecuteDrawingPlan>) {
                    executeDrawingPlan(value.requestId, value.planJson, value.fingerprint);
                } else if constexpr (std::is_same_v<Value, protocol::RequestDrawingPreview>) {
                    requestDrawingPreview(value.requestId, value.requestJson, value.revision);
                }
            }, command);
        } catch (const protocol::ProtocolError&) {
            fail(0, QStringLiteral("protocol_error"), QStringLiteral("Invalid worker command"));
        }
    }
}

void WorkerServer::send(QJsonObject event)
{
    if (!socket_) {
        return;
    }
    event.insert(QStringLiteral("version"), QJsonObject{{QStringLiteral("major"), 2}, {QStringLiteral("minor"), 0}});
    socket_->write(protocol::encodeFrame(protocol::FrameType::ControlJson, QJsonDocument(event).toJson(QJsonDocument::Compact) + '\n'));
    socket_->flush();
}

void WorkerServer::sendGeometry(const protocol::GeometryPayload& payload)
{
    if (!socket_) return;
    // encodeGeometry validates the payload and throws ProtocolError on any
    // malformed body. This runs on the main thread via a queued call from the
    // tessellation thread, so an escaping exception has no handler and would
    // terminate the whole worker (observed as WER 0xC0000409). Contain it: drop
    // the offending body and keep the import alive rather than crashing.
    QByteArray frame;
    try {
        frame = protocol::encodeFrame(protocol::FrameType::Geometry, protocol::encodeGeometry(payload));
    } catch (const std::exception& error) {
        qWarning("loupe-worker: dropping geometry payload that failed to encode: %s", error.what());
        return;
    }
    socket_->write(frame);
    socket_->flush();
}

void WorkerServer::fail(const std::uint64_t requestId, const QString& code, const QString& message)
{
    send({{QStringLiteral("type"), QStringLiteral("failed")}, {QStringLiteral("requestId"), static_cast<qint64>(requestId)},
          {QStringLiteral("code"), code}, {QStringLiteral("message"), message}, {QStringLiteral("recoverable"), true}});
}

void WorkerServer::open(const std::uint64_t requestId, const QString& path, const std::optional<protocol::UnitOverride>& unitOverride)
{
    if (!QFileInfo::exists(path)) {
        fail(requestId, QStringLiteral("read_failed"), QStringLiteral("The requested file does not exist"));
        return;
    }
    if (!activeSessions_.isEmpty() || !activeExports_.isEmpty()) {
        fail(requestId, QStringLiteral("busy"), QStringLiteral("A request is already active"));
        return;
    }
    documentSession_.reset();
    const auto job = std::make_shared<ImportJob>();
    activeSessions_.insert(requestId, job);
    const auto unit = unitOverride ? unitOverride->unit : QString{};
    const auto factor = unitOverride ? unitOverride->customFactor : 1.0;
    const auto reason = unitOverride ? unitOverride->reason : QString{};
    QTimer::singleShot(25, this, [this, requestId, path, unit, factor, reason, job] {
        if (job->canceled.load()) {
            activeSessions_.remove(requestId);
            return;
        }
        QPointer<WorkerServer> server(this);
        auto post = [server](QJsonObject event) {
            if (!server) return;
            QMetaObject::invokeMethod(server, [server, event = std::move(event)] {
                if (server) server->send(event);
            }, Qt::QueuedConnection);
        };
        auto finish = [server, requestId, job] {
            if (!server) return;
            QMetaObject::invokeMethod(server, [server, requestId, job] {
                if (server && server->activeSessions_.value(requestId) == job) server->activeSessions_.remove(requestId);
            }, Qt::QueuedConnection);
        };
        auto retain = [server, requestId, job](const std::shared_ptr<DocumentSession>& session) {
            if (!server) return;
            QMetaObject::invokeMethod(server, [server, requestId, job, session] {
                if (server && server->activeSessions_.value(requestId) == job) server->documentSession_ = session;
            }, Qt::QueuedConnection);
        };
        auto postGeometry = [server](protocol::GeometryPayload payload) {
            if (!server) return;
            QMetaObject::invokeMethod(server, [server, payload = std::move(payload)] {
                if (server) server->sendGeometry(payload);
            }, Qt::QueuedConnection);
        };
        auto* thread = QThread::create([requestId, path, unit, factor, reason, job, post, postGeometry, finish, retain] {
            QElapsedTimer elapsed;
            elapsed.start();
            qint64 treeReadyMs = 0;
            qint64 firstGeometryMs = 0;
            qint64 previewReadyMs = 0;
            qint64 previewTriangleCount = 0;
            qint64 refinedTriangleCount = 0;
            const auto progress = [requestId, &post](const QString& stage, const double fraction) {
                post({{QStringLiteral("type"), QStringLiteral("progress")}, {QStringLiteral("requestId"), static_cast<qint64>(requestId)},
                      {QStringLiteral("stage"), stage}, {QStringLiteral("fraction"), fraction}});
            };
            try {
                progress(QStringLiteral("Reading STEP file"), 0.01);
                if (job->canceled.load()) { finish(); return; }
                progress(QStringLiteral("Translating XCAF assembly"), 0.10);
                auto imported = import::StepImporter{}.read(path.toStdString());
                if (job->canceled.load()) { finish(); return; }
                progress(QStringLiteral("Building assembly tree and colors"), 0.55);
                const auto unitDecision = loupe::units::decide(imported.unitEvidence, toUnitOverride(unit, factor, reason));
                auto documentSession = std::make_shared<DocumentSession>(DocumentSession{std::move(imported), unitDecision});
                const auto& sessionImport = documentSession->imported;
                const auto snapshot = encodeSnapshot(sessionImport, unitDecision);
                if (job->canceled.load()) { finish(); return; }
                progress(QStringLiteral("Preparing viewport geometry"), 0.65);
                post({{QStringLiteral("type"), QStringLiteral("snapshotReady")}, {QStringLiteral("requestId"), static_cast<qint64>(requestId)},
                      {QStringLiteral("snapshotBase64"), QString::fromLatin1(snapshot.toBase64())}});
                treeReadyMs = elapsed.elapsed();
                const auto shapeCount = std::min({sessionImport.native->shapes.size(), sessionImport.native->shapePlacements.size(), sessionImport.native->shapeNodeIds.size(),
                                                  sessionImport.native->definitionLabels.size(), sessionImport.native->definitionIds.size()});
                const auto colors = XCAFDoc_DocumentTool::ColorTool(sessionImport.native->document->Main());
                const auto sendProfile = [&](const TessellationProfile& profile, const double start, const double end) {
                    QSet<QString> preparedDefinitions;
                    for (std::size_t index = 0; index < shapeCount; ++index) {
                        if (job->canceled.load()) return false;
                        const auto& shape = sessionImport.native->shapes[index];
                        const auto nodeId = QString::fromStdString(sessionImport.native->shapeNodeIds[index]);
                        const auto definitionId = QString::fromStdString(sessionImport.native->definitionIds[index]);
                        if (!preparedDefinitions.contains(definitionId)) {
                            const auto definitionShape = XCAFDoc_ShapeTool::GetShape(sessionImport.native->definitionLabels[index]);
                            prepareTessellation(definitionShape, unitDecision.sourceToMillimeters, profile);
                            preparedDefinitions.insert(definitionId);
                        }
                        const auto segments = encodeMesh(shape, sessionImport.native->shapePlacements[index], unitDecision.sourceToMillimeters, colors,
                                                         sessionImport.native->labels[index], sessionImport.native->definitionLabels[index]);
                        if (job->canceled.load()) return false;
                        for (qsizetype segmentIndex = 0; segmentIndex < segments.size(); ++segmentIndex) {
                            if (job->canceled.load()) return false;
                            const auto& segment = segments[segmentIndex];
                            if (segment.indices.isEmpty()) continue;
                            if (firstGeometryMs == 0) firstGeometryMs = elapsed.elapsed();
                            const auto triangleCount = static_cast<qint64>(segment.indices.size() / 3);
                            if (profile.refinement == kPreviewProfile.refinement) previewTriangleCount += triangleCount;
                            else refinedTriangleCount += triangleCount;
                            postGeometry(protocol::MeshPayload{requestId, requestId, nodeId, nodeId,
                                                               QStringLiteral("%1:%2").arg(nodeId).arg(segmentIndex), segment.color,
                                                               profile.refinement, segment.vertices, segment.normals, segment.indices,
                                                               segment.topology});
                        }
                        const auto edges = encodeEdges(shape, sessionImport.native->shapePlacements[index], unitDecision.sourceToMillimeters, profile);
                        if (!edges.indices.isEmpty()) {
                            postGeometry(protocol::EdgePayload{requestId, requestId, nodeId, nodeId, profile.refinement,
                                                               edges.vertices, edges.indices, edges.topology});
                        }
                        if (profile.refinement == kRefinedProfile.refinement) {
                            const auto analysis = inspection::analyze(shape, unitDecision.sourceToMillimeters);
                            const auto topology = inspection::analyzeTopology(shape, unitDecision.sourceToMillimeters);
                            if (analysis.valid) {
                                post({{QStringLiteral("type"), QStringLiteral("componentMetadata")},
                                      {QStringLiteral("requestId"), static_cast<qint64>(requestId)},
                                      {QStringLiteral("nodeId"), nodeId},
                                      {QStringLiteral("surfaceAreaMm2"), analysis.surfaceAreaMm2},
                                      {QStringLiteral("volumeMm3"), analysis.volumeMm3},
                                      {QStringLiteral("boundsMm"), QJsonObject{{QStringLiteral("width"), analysis.boundsMm.width},
                                                                              {QStringLiteral("height"), analysis.boundsMm.height},
                                                                              {QStringLiteral("depth"), analysis.boundsMm.depth}}},
                                      {QStringLiteral("longestEdgeMm"), topology.longestEdgeMm},
                                      {QStringLiteral("circularRadiusMm"), topology.circularRadiusMm},
                                      {QStringLiteral("planarFaceCount"), topology.planarFaceCount}});
                            }
                        }
                        const auto fraction = shapeCount == 0 ? end
                            : start + (end - start) * static_cast<double>(index + 1) / static_cast<double>(shapeCount);
                        progress(profile.progressStage, fraction);
                    }
                    return true;
                };
                if (!sendProfile(kPreviewProfile, 0.65, 0.80)) { finish(); return; }
                previewReadyMs = elapsed.elapsed();
                progress(QStringLiteral("Preview ready - refining geometry"), 0.80);
                if (!sendProfile(kRefinedProfile, 0.80, 0.99)) { finish(); return; }
                if (!job->canceled.load()) {
                    retain(documentSession);
                    post({{QStringLiteral("type"), QStringLiteral("importMetrics")},
                          {QStringLiteral("requestId"), static_cast<qint64>(requestId)},
                          {QStringLiteral("sourceName"), QFileInfo(path).fileName()},
                          {QStringLiteral("sourceHash"), QString::fromStdString(sessionImport.snapshot.sourceHash)},
                          {QStringLiteral("stepReadMs"), static_cast<qint64>(sessionImport.phaseTimes.stepReadMs)},
                          {QStringLiteral("xcafTransferMs"), static_cast<qint64>(sessionImport.phaseTimes.xcafTransferMs)},
                          {QStringLiteral("snapshotBuildMs"), static_cast<qint64>(sessionImport.phaseTimes.snapshotBuildMs)},
                          {QStringLiteral("treeReadyMs"), treeReadyMs},
                          {QStringLiteral("firstGeometryMs"), firstGeometryMs},
                          {QStringLiteral("previewReadyMs"), previewReadyMs},
                          {QStringLiteral("finalReadyMs"), elapsed.elapsed()},
                          {QStringLiteral("previewTriangleCount"), previewTriangleCount},
                          {QStringLiteral("refinedTriangleCount"), refinedTriangleCount},
                          {QStringLiteral("bodyCount"), static_cast<int>(shapeCount)}});
                    progress(QStringLiteral("Ready"), 1.0);
                }
            } catch (const std::exception&) {
                if (!job->canceled.load()) post({{QStringLiteral("type"), QStringLiteral("failed")}, {QStringLiteral("requestId"), static_cast<qint64>(requestId)},
                                                  {QStringLiteral("code"), QStringLiteral("import_failed")}, {QStringLiteral("message"), QStringLiteral("The STEP file could not be imported")},
                                                  {QStringLiteral("recoverable"), true}});
            }
            finish();
        });
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        thread->start();
    });
}

void WorkerServer::executeExportPlan(const std::uint64_t requestId, const QByteArray& planJson,
                                     const QString& fingerprint)
{
    if (!documentSession_) {
        fail(requestId, QStringLiteral("document_not_ready"),
             QStringLiteral("Wait for geometry refinement to finish before exporting"));
        return;
    }
    if (!activeSessions_.isEmpty() || !activeExports_.isEmpty()) {
        fail(requestId, QStringLiteral("busy"), QStringLiteral("Another worker request is already active"));
        return;
    }

    std::optional<loupe::exporting::ExportPlan> reviewedPlan;
    std::vector<std::string> reviewedNodeOrder;
    try {
        auto decoded = decodeExportPlan(planJson);
        reviewedNodeOrder = std::move(decoded.reviewedNodeOrder);
        reviewedPlan.emplace(loupe::exporting::buildPlan(decoded.request));
        if (QString::fromStdString(reviewedPlan->fingerprint()) != fingerprint) {
            throw std::runtime_error("reviewed export plan changed before execution");
        }
        const QFileInfo destination(QString::fromStdString(decoded.request.destination));
        if (!destination.exists() || !destination.isDir() || !destination.isWritable()) {
            throw std::runtime_error("export destination is not a writable folder");
        }
    } catch (const std::exception& error) {
        fail(requestId, QStringLiteral("export_gate_failed"), QString::fromUtf8(error.what()));
        return;
    }

    const auto document = documentSession_;
    std::vector<int> reviewedRowIndexes;
    reviewedRowIndexes.reserve(reviewedPlan->outputs().size());
    for (const auto& output : reviewedPlan->outputs()) {
        const auto found = std::ranges::find(reviewedNodeOrder, output.nodeId());
        if (found == reviewedNodeOrder.end()) {
            fail(requestId, QStringLiteral("export_gate_failed"),
                 QStringLiteral("reviewed output order is incomplete"));
            return;
        }
        reviewedRowIndexes.push_back(static_cast<int>(std::distance(reviewedNodeOrder.begin(), found)));
    }
    const auto job = std::make_shared<ExportJob>();
    activeExports_.insert(requestId, job);
    QPointer<WorkerServer> server(this);
    auto post = [server](QJsonObject event) {
        if (!server) return;
        QMetaObject::invokeMethod(server, [server, event = std::move(event)] {
            if (server) server->send(event);
        }, Qt::QueuedConnection);
    };
    auto finish = [server, requestId, job] {
        if (!server) return;
        QMetaObject::invokeMethod(server, [server, requestId, job] {
            if (server && server->activeExports_.value(requestId) == job) server->activeExports_.remove(requestId);
        }, Qt::QueuedConnection);
    };

    auto* thread = QThread::create([requestId, plan = std::move(*reviewedPlan),
                                    reviewedRowIndexes = std::move(reviewedRowIndexes), document, job, post, finish] {
        int succeeded{};
        int failed{};
        const auto& outputs = plan.outputs();
        const int rowCount = static_cast<int>(outputs.size());
        for (int outputIndex = 0; outputIndex < rowCount; ++outputIndex) {
            if (job->canceled.load()) {
                post({{QStringLiteral("type"), QStringLiteral("canceled")},
                      {QStringLiteral("requestId"), static_cast<qint64>(requestId)}});
                finish();
                return;
            }
            const auto& output = outputs.at(static_cast<std::size_t>(outputIndex));
            const auto rowIndex = reviewedRowIndexes.at(static_cast<std::size_t>(outputIndex));
            const auto nodeId = QString::fromStdString(output.nodeId());
            const auto path = QString::fromStdString(output.finalPath());
            const auto fraction = static_cast<double>(outputIndex) / static_cast<double>(rowCount);
            post({{QStringLiteral("type"), QStringLiteral("exportProgress")},
                  {QStringLiteral("requestId"), static_cast<qint64>(requestId)},
                  {QStringLiteral("rowIndex"), rowIndex}, {QStringLiteral("rowCount"), rowCount},
                  {QStringLiteral("stage"), QStringLiteral("Writing %1").arg(QFileInfo(path).fileName())},
                  {QStringLiteral("fraction"), fraction}});
            try {
                if (output.format() == loupe::exporting::Format::Step) {
                    static_cast<void>(loupe::exporting::StepExporter{}.write(document->imported, output));
                } else {
                    static_cast<void>(loupe::exporting::StlExporter{}.write(document->imported, output));
                }
                post({{QStringLiteral("type"), QStringLiteral("exportProgress")},
                      {QStringLiteral("requestId"), static_cast<qint64>(requestId)},
                      {QStringLiteral("rowIndex"), rowIndex}, {QStringLiteral("rowCount"), rowCount},
                      {QStringLiteral("stage"), QStringLiteral("Validating %1").arg(QFileInfo(path).fileName())},
                      {QStringLiteral("fraction"), (static_cast<double>(outputIndex) + 0.75) / static_cast<double>(rowCount)}});
                const auto unit = output.format() == loupe::exporting::Format::Stl
                    || output.stepOutputUnit() == loupe::exporting::StepOutputUnit::Millimeter
                    ? loupe::validation::OutputUnit::Millimeter : loupe::validation::OutputUnit::Inch;
                const auto validation = loupe::validation::OutputValidator{}.validate(
                    {std::filesystem::path(output.finalPath()), unit, 1, {}, {}, 1.0e-5, false, false});
                if (!validation.passed) {
                    const auto message = validation.errors.empty() ? std::string{"output validation failed"}
                                                                    : validation.errors.front().message;
                    std::error_code ignored;
                    std::filesystem::remove(std::filesystem::path(output.finalPath()), ignored);
                    throw std::runtime_error(message);
                }
                ++succeeded;
                post({{QStringLiteral("type"), QStringLiteral("exportRowResult")},
                      {QStringLiteral("requestId"), static_cast<qint64>(requestId)},
                      {QStringLiteral("rowIndex"), rowIndex}, {QStringLiteral("nodeId"), nodeId},
                      {QStringLiteral("path"), path}, {QStringLiteral("passed"), true},
                      {QStringLiteral("message"), QStringLiteral("Exported and validated")}});
            } catch (const std::exception& error) {
                ++failed;
                post({{QStringLiteral("type"), QStringLiteral("exportRowResult")},
                      {QStringLiteral("requestId"), static_cast<qint64>(requestId)},
                      {QStringLiteral("rowIndex"), rowIndex}, {QStringLiteral("nodeId"), nodeId},
                      {QStringLiteral("path"), path}, {QStringLiteral("passed"), false},
                      {QStringLiteral("message"), QString::fromUtf8(error.what())}});
            }
        }
        post({{QStringLiteral("type"), QStringLiteral("exportCompleted")},
              {QStringLiteral("requestId"), static_cast<qint64>(requestId)},
              {QStringLiteral("succeededCount"), succeeded}, {QStringLiteral("failedCount"), failed}});
        finish();
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

namespace {

struct DecodedDrawingPlan final {
    loupe::drawing::DrawingPlanRequest request;
    // Reviewed queue order, so every rowIndex reported back indexes the row the user is
    // looking at rather than the plan's canonical order.
    std::vector<std::string> reviewedDrawingOrder;
    bool includeScaleFiducial{};
};

[[nodiscard]] loupe::drawing::DrawingContent drawingContentFor(const QString& mode)
{
    if (mode == QStringLiteral("Outer contour only")) return loupe::drawing::DrawingContent::OuterContourOnly;
    if (mode == QStringLiteral("Technical view")) return loupe::drawing::DrawingContent::TechnicalView;
    if (mode == QStringLiteral("Cut contours")) return loupe::drawing::DrawingContent::CutContours;
    throw std::runtime_error("reviewed drawing content mode is not recognised");
}

[[nodiscard]] loupe::drawing::DrawingFormat drawingFormatFor(const QString& format)
{
    if (format == QStringLiteral("DXF")) return loupe::drawing::DrawingFormat::Dxf;
    if (format == QStringLiteral("SVG")) return loupe::drawing::DrawingFormat::Svg;
    if (format == QStringLiteral("PDF")) return loupe::drawing::DrawingFormat::Pdf;
    throw std::runtime_error("reviewed drawing format is not recognised");
}

[[nodiscard]] double finiteNumber(const QJsonObject& object, const QString& name)
{
    const auto value = object.value(name);
    if (!value.isDouble() || !std::isfinite(value.toDouble())) {
        throw std::runtime_error("reviewed drawing selection has a non-numeric field");
    }
    return value.toDouble();
}

DecodedDrawingPlan decodeDrawingPlan(const QByteArray& bytes)
{
    const auto document = QJsonDocument::fromJson(bytes);
    if (!document.isObject()) throw std::runtime_error("reviewed drawing plan is not valid JSON");
    const auto object = document.object();
    if (object.value(QStringLiteral("schemaVersion")).toInt() != 1) {
        throw std::runtime_error("reviewed drawing plan version is not supported");
    }

    DecodedDrawingPlan decoded;
    auto& request = decoded.request;
    request.destination = object.value(QStringLiteral("destination")).toString().toUtf8().toStdString();
    request.format = drawingFormatFor(object.value(QStringLiteral("format")).toString());
    decoded.includeScaleFiducial = object.value(QStringLiteral("includeScaleFiducial")).toBool();
    request.includeScaleFiducial = decoded.includeScaleFiducial;
    const auto sourceScale = object.value(QStringLiteral("sourceToMillimeters")).toDouble(0.0);
    request.unitDecision = {loupe::units::LengthUnit::Millimeter, loupe::units::UnitConfidence::Confirmed,
                            sourceScale, "reviewed in Loupe"};

    const auto selections = object.value(QStringLiteral("selections"));
    if (!selections.isArray()) throw std::runtime_error("reviewed drawing selections are missing");
    for (const auto& value : selections.toArray()) {
        const auto selection = value.toObject();
        const auto drawingId = selection.value(QStringLiteral("drawingId")).toString().toUtf8().toStdString();
        const auto nodeId = selection.value(QStringLiteral("nodeId")).toString().toUtf8().toStdString();
        const auto hierarchyPath = selection.value(QStringLiteral("hierarchyPath")).toString().toUtf8().toStdString();
        if (drawingId.empty() || nodeId.empty() || hierarchyPath.empty()) {
            throw std::runtime_error("reviewed drawing selection is incomplete");
        }
        const auto numerator = selection.value(QStringLiteral("scaleNumerator"));
        const auto denominator = selection.value(QStringLiteral("scaleDenominator"));
        if (!numerator.isDouble() || !denominator.isDouble()) {
            throw std::runtime_error("reviewed drawing scale is missing");
        }
        loupe::drawing::DrawingSelection decodedSelection;
        decodedSelection.drawingId = drawingId;
        decodedSelection.nodeId = nodeId;
        decodedSelection.viewLabel = selection.value(QStringLiteral("viewLabel")).toString().toUtf8().toStdString();
        decodedSelection.viewDirection = {finiteNumber(selection, QStringLiteral("viewX")),
                                          finiteNumber(selection, QStringLiteral("viewY")),
                                          finiteNumber(selection, QStringLiteral("viewZ"))};
        decodedSelection.upDirection = {finiteNumber(selection, QStringLiteral("upX")),
                                        finiteNumber(selection, QStringLiteral("upY")),
                                        finiteNumber(selection, QStringLiteral("upZ"))};
        decodedSelection.content = drawingContentFor(selection.value(QStringLiteral("contentMode")).toString());
        decodedSelection.scaleNumerator = numerator.toInt();
        decodedSelection.scaleDenominator = denominator.toInt();
        request.selections.push_back(std::move(decodedSelection));
        request.hierarchyPaths.emplace(nodeId, hierarchyPath);
        const auto leafName = selection.value(QStringLiteral("leafName")).toString();
        if (!leafName.isEmpty()) request.outputLeafNames.emplace(drawingId, leafName.toUtf8().toStdString());
        decoded.reviewedDrawingOrder.push_back(drawingId);
    }
    return decoded;
}

} // namespace

void WorkerServer::executeDrawingPlan(const std::uint64_t requestId, const QByteArray& planJson,
                                      const QString& fingerprint)
{
    if (!documentSession_) {
        fail(requestId, QStringLiteral("document_not_ready"),
             QStringLiteral("Wait for geometry refinement to finish before exporting drawings"));
        return;
    }
    if (!activeSessions_.isEmpty() || !activeExports_.isEmpty()) {
        fail(requestId, QStringLiteral("busy"), QStringLiteral("Another worker request is already active"));
        return;
    }

    std::optional<loupe::drawing::DrawingPlan> reviewedPlan;
    std::vector<std::string> reviewedDrawingOrder;
    bool includeScaleFiducial{};
    try {
        auto decoded = decodeDrawingPlan(planJson);
        reviewedDrawingOrder = std::move(decoded.reviewedDrawingOrder);
        includeScaleFiducial = decoded.includeScaleFiducial;
        // Rebuilt here rather than trusted: the plan the worker executes has to be the one
        // the user reviewed, and the fingerprint is the only thing that can prove it.
        reviewedPlan.emplace(loupe::drawing::buildDrawingPlan(decoded.request));
        if (QString::fromStdString(reviewedPlan->fingerprint()) != fingerprint) {
            throw std::runtime_error("reviewed drawing plan changed before execution");
        }
        const QFileInfo destination(QString::fromStdString(decoded.request.destination));
        if (!destination.exists() || !destination.isDir() || !destination.isWritable()) {
            throw std::runtime_error("drawing destination is not a writable folder");
        }
    } catch (const std::exception& error) {
        fail(requestId, QStringLiteral("drawing_gate_failed"), QString::fromUtf8(error.what()));
        return;
    }

    std::vector<int> reviewedRowIndexes;
    reviewedRowIndexes.reserve(reviewedPlan->outputs().size());
    for (const auto& output : reviewedPlan->outputs()) {
        const auto found = std::ranges::find(reviewedDrawingOrder, output.drawingId());
        if (found == reviewedDrawingOrder.end()) {
            fail(requestId, QStringLiteral("drawing_gate_failed"),
                 QStringLiteral("reviewed drawing order is incomplete"));
            return;
        }
        reviewedRowIndexes.push_back(static_cast<int>(std::distance(reviewedDrawingOrder.begin(), found)));
    }

    const auto document = documentSession_;
    const auto job = std::make_shared<ExportJob>();
    activeExports_.insert(requestId, job);
    QPointer<WorkerServer> server(this);
    auto post = [server](QJsonObject event) {
        if (!server) return;
        QMetaObject::invokeMethod(server, [server, event = std::move(event)] {
            if (server) server->send(event);
        }, Qt::QueuedConnection);
    };
    auto finish = [server, requestId, job] {
        if (!server) return;
        QMetaObject::invokeMethod(server, [server, requestId, job] {
            if (server && server->activeExports_.value(requestId) == job) server->activeExports_.remove(requestId);
        }, Qt::QueuedConnection);
    };

    // One thread, and rows are projected one at a time on it. BRepLib::Plane() is a mutable
    // global static that hidden-line removal reads, so HLR jobs cannot be parallelised.
    auto* thread = QThread::create([requestId, plan = std::move(*reviewedPlan),
                                    reviewedRowIndexes = std::move(reviewedRowIndexes), document, job,
                                    includeScaleFiducial, post, finish] {
        int succeeded{};
        int failed{};
        const auto& outputs = plan.outputs();
        const int rowCount = static_cast<int>(outputs.size());
        for (int outputIndex = 0; outputIndex < rowCount; ++outputIndex) {
            if (job->canceled.load()) {
                post({{QStringLiteral("type"), QStringLiteral("canceled")},
                      {QStringLiteral("requestId"), static_cast<qint64>(requestId)}});
                finish();
                return;
            }
            const auto& output = outputs.at(static_cast<std::size_t>(outputIndex));
            const auto rowIndex = reviewedRowIndexes.at(static_cast<std::size_t>(outputIndex));
            const auto drawingId = QString::fromStdString(output.drawingId());
            const auto path = QString::fromStdString(output.finalPath());
            post({{QStringLiteral("type"), QStringLiteral("drawingProgress")},
                  {QStringLiteral("requestId"), static_cast<qint64>(requestId)},
                  {QStringLiteral("rowIndex"), rowIndex}, {QStringLiteral("rowCount"), rowCount},
                  {QStringLiteral("stage"), QStringLiteral("Projecting %1").arg(QFileInfo(path).fileName())},
                  {QStringLiteral("fraction"), static_cast<double>(outputIndex) / static_cast<double>(rowCount)}});
            try {
                const auto result = loupe::drawing::DrawingExporter{}.write(document->imported, output,
                                                                           includeScaleFiducial);
                ++succeeded;
                auto message = QStringLiteral("Written and validated");
                if (result.approximate) {
                    // The exact algorithm could not project this view, so the file is not
                    // strictly 1:1. Said out loud on the row that produced it.
                    message = QStringLiteral("Written from a tilted view; not strictly 1:1");
                }
                if (result.openContours > 0) {
                    // Said out loud rather than passed silently: an open contour will not cut.
                    message = QStringLiteral("Written, but %1 contour(s) are not closed")
                                  .arg(result.openContours);
                }
                post({{QStringLiteral("type"), QStringLiteral("drawingRowResult")},
                      {QStringLiteral("requestId"), static_cast<qint64>(requestId)},
                      {QStringLiteral("rowIndex"), rowIndex}, {QStringLiteral("drawingId"), drawingId},
                      {QStringLiteral("path"), path}, {QStringLiteral("passed"), true},
                      {QStringLiteral("message"), message}});
            } catch (const std::exception& error) {
                ++failed;
                // A half-written file is worse than no file: it looks cuttable.
                std::error_code ignored;
                std::filesystem::remove(std::filesystem::path(output.finalPath()), ignored);
                post({{QStringLiteral("type"), QStringLiteral("drawingRowResult")},
                      {QStringLiteral("requestId"), static_cast<qint64>(requestId)},
                      {QStringLiteral("rowIndex"), rowIndex}, {QStringLiteral("drawingId"), drawingId},
                      {QStringLiteral("path"), path}, {QStringLiteral("passed"), false},
                      {QStringLiteral("message"), QString::fromUtf8(error.what())}});
            }
        }
        post({{QStringLiteral("type"), QStringLiteral("drawingCompleted")},
              {QStringLiteral("requestId"), static_cast<qint64>(requestId)},
              {QStringLiteral("succeededCount"), succeeded}, {QStringLiteral("failedCount"), failed}});
        finish();
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void WorkerServer::requestDrawingPreview(const std::uint64_t requestId, const QByteArray& requestJson,
                                         const int revision)
{
    if (!documentSession_) {
        fail(requestId, QStringLiteral("document_not_ready"),
             QStringLiteral("Wait for geometry refinement to finish before previewing a drawing"));
        return;
    }
    const auto document = QJsonDocument::fromJson(requestJson);
    if (!document.isObject()) {
        fail(requestId, QStringLiteral("drawing_preview_failed"),
             QStringLiteral("drawing preview request is not valid JSON"));
        return;
    }
    const auto object = document.object();
    const auto nodeId = object.value(QStringLiteral("nodeId")).toString();
    if (nodeId.isEmpty()) {
        fail(requestId, QStringLiteral("drawing_preview_failed"),
             QStringLiteral("drawing preview request names no part"));
        return;
    }

    // A superseded preview is dropped here rather than computed and thrown away: the
    // newest revision is the only one worth the projection cost.
    if (revision < latestPreviewRevision_) {
        send({{QStringLiteral("type"), QStringLiteral("canceled")},
              {QStringLiteral("requestId"), static_cast<qint64>(requestId)}});
        return;
    }
    latestPreviewRevision_ = revision;

    const auto session = documentSession_;
    const auto contentMode = object.value(QStringLiteral("contentMode")).toString();
    const auto numerator = object.value(QStringLiteral("scaleNumerator")).toInt(1);
    const auto denominator = object.value(QStringLiteral("scaleDenominator")).toInt(1);
    const auto sourceToMillimeters = object.value(QStringLiteral("sourceToMillimeters")).toDouble(1.0);
    const auto view = std::array<double, 3>{object.value(QStringLiteral("viewX")).toDouble(),
                                            object.value(QStringLiteral("viewY")).toDouble(),
                                            object.value(QStringLiteral("viewZ")).toDouble()};
    const auto up = std::array<double, 3>{object.value(QStringLiteral("upX")).toDouble(),
                                          object.value(QStringLiteral("upY")).toDouble(),
                                          object.value(QStringLiteral("upZ")).toDouble()};

    QPointer<WorkerServer> server(this);
    auto post = [server](QJsonObject event) {
        if (!server) return;
        QMetaObject::invokeMethod(server, [server, event = std::move(event)] {
            if (server) server->send(event);
        }, Qt::QueuedConnection);
    };

    auto* thread = QThread::create([requestId, revision, nodeId, contentMode, numerator, denominator,
                                    sourceToMillimeters, view, up, session, post, server] {
        try {
            const auto& node = loupe::exporting::detail::selectedNode(session->imported,
                                                                     nodeId.toUtf8().toStdString());
            auto shape = loupe::exporting::detail::localShape(session->imported, node);
            shape = loupe::exporting::detail::placedInAssembly(shape, node);

            loupe::drawing::ProjectionRequest projection;
            projection.shape = shape;
            projection.viewDirection = gp_Dir(view[0], view[1], view[2]);
            projection.upDirection = gp_Dir(up[0], up[1], up[2]);
            projection.mode = contentMode == QStringLiteral("Outer contour only")
                ? loupe::drawing::ContentMode::OuterContourOnly
                : contentMode == QStringLiteral("Technical view")
                    ? loupe::drawing::ContentMode::TechnicalView
                    : loupe::drawing::ContentMode::CutContours;
            projection.sourceToMillimeters = sourceToMillimeters
                * loupe::exporting::detail::nativeUnitRebase(session->imported)
                * (static_cast<double>(numerator) / static_cast<double>(denominator));
            const auto projected = loupe::drawing::project(projection);

            // The preview is the exact projection, so what is reviewed is what is written.
            // Gate A measured this fast enough to stay interactive on the corpus.
            post({{QStringLiteral("type"), QStringLiteral("drawingPreviewReady")},
                  {QStringLiteral("requestId"), static_cast<qint64>(requestId)},
                  {QStringLiteral("revision"), revision},
                  {QStringLiteral("previewBase64"),
                   QString::fromLatin1(QByteArray::fromStdString(loupe::drawing::encodePreview(projected)).toBase64())},
                  {QStringLiteral("approximate"), projected.approximate}});
        } catch (const std::exception& error) {
            post({{QStringLiteral("type"), QStringLiteral("failed")},
                  {QStringLiteral("requestId"), static_cast<qint64>(requestId)},
                  {QStringLiteral("code"), QStringLiteral("drawing_preview_failed")},
                  {QStringLiteral("message"), QString::fromUtf8(error.what())},
                  {QStringLiteral("recoverable"), true}});
        }
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void WorkerServer::cancel(const std::uint64_t requestId)
{
    if (const auto job = activeSessions_.value(requestId)) job->canceled.store(true);
    if (const auto job = activeExports_.value(requestId)) {
        job->canceled.store(true);
        return;
    }
    send({{QStringLiteral("type"), QStringLiteral("canceled")}, {QStringLiteral("requestId"), static_cast<qint64>(requestId)}});
}

} // namespace loupe::worker
