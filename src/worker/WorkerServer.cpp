#include "worker/WorkerServer.h"

#include "core/import/StepImporter.h"
#include "core/inspection/GeometryAnalysis.h"
#include "core/inspection/TopologyAnalysis.h"
#include "core/units/UnitPolicy.h"
#include "protocol/GeometryPayload.h"
#include "protocol/ProtocolFrame.h"
#include "protocol/ProtocolTypes.h"

#include <QFileInfo>
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
#include <BRepLib_ToolTriangulatedShape.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <GCPnts_TangentialDeflection.hxx>
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
};

struct EdgePayload final {
    QVector<float> vertices;
    QVector<quint32> indices;
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

QVector<MeshSegment> encodeMesh(const TopoDS_Shape& shape, const double sourceToMillimeters,
                                const occ::handle<XCAFDoc_ColorTool>& colors,
                                const TDF_Label& occurrence, const TDF_Label& definition)
{
    QHash<QString, int> segmentIndexes;
    QVector<MeshSegment> segments;
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next()) {
        const auto face = TopoDS::Face(explorer.Current());
        TopLoc_Location location;
        const auto triangulation = BRep_Tool::Triangulation(face, location);
        if (triangulation.IsNull()) continue;
        const auto color = colorFor(colors, occurrence, definition, face);
        const auto segmentIndex = segmentIndexes.value(color, -1);
        if (segmentIndex < 0) {
            segmentIndexes.insert(color, segments.size());
            segments.append({color, {}, {}, {}});
        }
        auto& segment = segments[segmentIndexes.value(color)];
        const int vertexOffset = segment.vertices.size() / 3;
        const auto transform = location.Transformation();
        BRepLib_ToolTriangulatedShape::ComputeNormals(face, triangulation);
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
    }
    return segments;
}

EdgePayload encodeEdges(const TopoDS_Shape& shape, const double sourceToMillimeters, const TessellationProfile& profile)
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
            for (int pointIndex = 1; pointIndex <= points.NbPoints(); ++pointIndex) {
                const auto point = points.Value(pointIndex);
                payload.vertices.append(static_cast<float>(point.X() * sourceToMillimeters));
                payload.vertices.append(static_cast<float>(point.Y() * sourceToMillimeters));
                payload.vertices.append(static_cast<float>(point.Z() * sourceToMillimeters));
                if (pointIndex > 1) {
                    payload.indices.append(static_cast<quint32>(firstVertex + pointIndex - 2));
                    payload.indices.append(static_cast<quint32>(firstVertex + pointIndex - 1));
                }
            }
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
    connect(socket_, &QLocalSocket::disconnected, this, [this] { socket_.clear(); });
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
    socket_->write(protocol::encodeFrame(protocol::FrameType::Geometry, protocol::encodeGeometry(payload)));
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
    if (!activeSessions_.isEmpty()) {
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
                const auto shapeCount = std::min({sessionImport.native->shapes.size(), sessionImport.native->shapeNodeIds.size(),
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
                        const auto segments = encodeMesh(shape, unitDecision.sourceToMillimeters, colors,
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
                                                               profile.refinement, segment.vertices, segment.normals, segment.indices});
                        }
                        const auto edges = encodeEdges(shape, unitDecision.sourceToMillimeters, profile);
                        if (!edges.indices.isEmpty()) {
                            postGeometry(protocol::EdgePayload{requestId, requestId, nodeId, nodeId, profile.refinement,
                                                               edges.vertices, edges.indices});
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

void WorkerServer::cancel(const std::uint64_t requestId)
{
    if (const auto job = activeSessions_.value(requestId)) job->canceled.store(true);
    send({{QStringLiteral("type"), QStringLiteral("canceled")}, {QStringLiteral("requestId"), static_cast<qint64>(requestId)}});
}

} // namespace loupe::worker
