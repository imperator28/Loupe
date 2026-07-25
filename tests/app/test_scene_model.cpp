#include <QtTest/QTest>

#include <QGuiApplication>
#include <QVector3D>

#include <cmath>
#include <numbers>
#include "app/render/SceneModel.h"
#include "app/render/CadEdgeGeometry.h"
#include "app/render/SectionMeshBuilder.h"
#include "protocol/GeometryPayload.h"

class SceneModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void repeatedDefinitionSharesOneGeometry();
    void selectionMapsPickInstanceToOccurrence();
    void meshGeometryAppendsWorkerPayload();
    void meshGeometryReplacesPreviewPayload();
    void cadEdgeGeometryUsesOnlyWorkerCurveLines();
    void meshGeometryClipsTrianglesAgainstSectionPlane();
    void meshGeometryClipsTrianglesAgainstArbitraryPlane();
    void meshGeometryBuildsAPlanarCapForAClosedSection();
    void meshGeometryCanRenderOnlyThePlanarSlice();
    void meshGeometrySliceRendersOpenIntersectionContours();
    void meshGeometrySliceSuppressesGeometryAwayFromPlane();
    void meshGeometryPreviewOmitsCapUntilCommit();
    void meshGeometryBuildsSectionOverlayWithoutBodyFaces();
    void sectionOverlayBuildsFilledContoursWithRoundedJoins();
    void topologyRangesResolveAndCopyCompleteEntities();
    void faceBoundaryExcludesInternalTessellationEdges();
    void faceFrameDistinguishesAFlatFaceFromACurvedOne();
};

void SceneModelTest::repeatedDefinitionSharesOneGeometry()
{
    loupe::app::render::SceneModel scene;
    scene.applySnapshot({
        {QStringLiteral("def-box"), QStringLiteral("occ-box-1")},
        {QStringLiteral("def-box"), QStringLiteral("occ-box-2")},
    });
    scene.applyMesh({QStringLiteral("def-box"), {0.0F, 0.0F, 0.0F}, {0U, 0U, 0U}});

    QCOMPARE(scene.geometryCount(), 1);
    QCOMPARE(scene.instanceCount(QStringLiteral("def-box")), 2);
}

void SceneModelTest::selectionMapsPickInstanceToOccurrence()
{
    loupe::app::render::SceneModel scene;
    scene.applySnapshot({
        {QStringLiteral("def-box"), QStringLiteral("occ-box-1")},
        {QStringLiteral("def-box"), QStringLiteral("occ-box-2")},
    });

    QCOMPARE(scene.nodeIdForPick(QStringLiteral("def-box"), 1), QStringLiteral("occ-box-2"));
}

void SceneModelTest::meshGeometryAppendsWorkerPayload()
{
    loupe::app::render::MeshGeometry geometry;

    QVERIFY(geometry.appendWorkerMesh(QByteArrayLiteral("{\"vertices\":[0,0,0,1,0,0,0,1,0],\"indices\":[0,1,2]}")));
    QCOMPARE(geometry.vertexCount(), 3);
    QCOMPARE(geometry.triangleCount(), 1);
    geometry.clearMesh();
    QCOMPARE(geometry.vertexCount(), 0);
}

void SceneModelTest::meshGeometryReplacesPreviewPayload()
{
    loupe::app::render::MeshGeometry geometry;
    QVERIFY(geometry.appendWorkerMesh(QByteArrayLiteral(
        "{\"vertices\":[0,0,0,1,0,0,0,1,0],\"indices\":[0,1,2]}")));

    QVERIFY(geometry.replaceWorkerMesh(QByteArrayLiteral(
        "{\"vertices\":[0,0,0,2,0,0,2,2,0,0,2,0],\"indices\":[0,1,2,0,2,3]}")));

    QCOMPARE(geometry.vertexCount(), 4);
    QCOMPARE(geometry.triangleCount(), 2);
}

void SceneModelTest::cadEdgeGeometryUsesOnlyWorkerCurveLines()
{
    loupe::app::render::CadEdgeGeometry geometry;
    QVERIFY(geometry.replaceWorkerEdges(QByteArrayLiteral(
        "{\"vertices\":[0,0,0,1,0,0,1,1,0,0,1,0],\"indices\":[0,1,1,2,2,3,3,0]}")));

    QCOMPARE(geometry.lineCount(), 4);
    geometry.setSection(true, 0, 0.5, false);
    QVERIFY(geometry.lineCount() > 0);
    geometry.setSectionOptions(true, true);
    QCOMPARE(geometry.lineCount(), 0);
}

void SceneModelTest::meshGeometryClipsTrianglesAgainstSectionPlane()
{
    loupe::app::render::MeshGeometry geometry;
    QVERIFY(geometry.appendWorkerMesh(QByteArrayLiteral("{\"vertices\":[0,0,0,2,0,0,0,2,0],\"indices\":[0,1,2]}")));

    geometry.setSection(true, 0, 1.0, false);

    QCOMPARE(geometry.triangleCount(), 1);
    QVERIFY(geometry.minimumCoordinate(0) >= 0.999F);
    QVERIFY(geometry.maximumCoordinate(0) <= 2.001F);
    geometry.setSection(false, 0, 0.0, false);
    QCOMPARE(geometry.triangleCount(), 1);
    QVERIFY(geometry.minimumCoordinate(0) <= 0.001F);
}

void SceneModelTest::meshGeometryClipsTrianglesAgainstArbitraryPlane()
{
    loupe::app::render::MeshGeometry geometry;
    QVERIFY(geometry.appendWorkerMesh(QByteArrayLiteral("{\"vertices\":[0,0,0,2,0,0,0,2,0],\"indices\":[0,1,2]}")));

    geometry.setSectionPlane(true, 0.0, 1.0, 0.0, 1.0, false);

    QCOMPARE(geometry.triangleCount(), 1);
    QVERIFY(geometry.minimumCoordinate(1) >= 0.999F);
}

void SceneModelTest::meshGeometryBuildsAPlanarCapForAClosedSection()
{
    loupe::app::render::MeshGeometry geometry;
    QVERIFY(geometry.appendWorkerMesh(QByteArrayLiteral(
        "{\"vertices\":[-1,-1,-1,1,-1,-1,1,1,-1,-1,1,-1,-1,-1,1,1,-1,1,1,1,1,-1,1,1],"
        "\"indices\":[0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7]}")));

    geometry.setSection(true, 2, 0.0, false);
    geometry.setSectionOptions(true, false);

    QVERIFY(geometry.sectionCapTriangleCount() >= 2);
    QVERIFY(geometry.triangleCount() > geometry.sectionCapTriangleCount());
}

void SceneModelTest::meshGeometryCanRenderOnlyThePlanarSlice()
{
    loupe::app::render::MeshGeometry geometry;
    QVERIFY(geometry.appendWorkerMesh(QByteArrayLiteral(
        "{\"vertices\":[-1,-1,-1,1,-1,-1,1,1,-1,-1,1,-1,-1,-1,1,1,-1,1,1,1,1,-1,1,1],"
        "\"indices\":[0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7]}")));

    geometry.setSection(true, 2, 0.0, false);
    geometry.setSectionOptions(true, true);

    QCOMPARE(geometry.triangleCount(), geometry.sectionCapTriangleCount());
    QVERIFY(geometry.triangleCount() >= 2);
}

void SceneModelTest::meshGeometrySliceRendersOpenIntersectionContours()
{
    loupe::app::render::MeshGeometry geometry;
    QVERIFY(geometry.appendWorkerMesh(QByteArrayLiteral(
        "{\"vertices\":[-1,-1,-1,1,-1,1,0,1,1],\"indices\":[0,1,2]}")));

    geometry.setSection(true, 2, 0.0, false);
    geometry.setSectionOptions(true, true);

    QVERIFY(geometry.triangleCount() >= 4);
    QCOMPARE(geometry.triangleCount(), geometry.sectionCapTriangleCount());
    QVERIFY(qAbs(geometry.minimumCoordinate(2)) < 0.01F);
    QVERIFY(qAbs(geometry.maximumCoordinate(2)) < 0.01F);
}

void SceneModelTest::meshGeometrySliceSuppressesGeometryAwayFromPlane()
{
    loupe::app::render::MeshGeometry geometry;
    QVERIFY(geometry.appendWorkerMesh(QByteArrayLiteral(
        "{\"vertices\":[0,0,1,1,0,1,0,1,1],\"indices\":[0,1,2]}")));

    geometry.setSection(true, 2, 0.0, false);
    geometry.setSectionOptions(true, true);

    QCOMPARE(geometry.triangleCount(), 0);
    QCOMPARE(geometry.sectionCapTriangleCount(), 0);
}

void SceneModelTest::meshGeometryPreviewOmitsCapUntilCommit()
{
    loupe::app::render::MeshGeometry geometry;
    QVERIFY(geometry.appendWorkerMesh(QByteArrayLiteral(
        "{\"vertices\":[-1,-1,-1,1,-1,-1,1,1,-1,-1,1,-1,-1,-1,1,1,-1,1,1,1,1,-1,1,1],"
        "\"indices\":[0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7]}")));

    geometry.configureSection(true, 0.0, 0.0, 1.0, 0.0, false, true, false, true, true, true);
    QCOMPARE(geometry.sectionCapTriangleCount(), 0);

    geometry.configureSection(true, 0.0, 0.0, 1.0, 0.0, false, true, false, true, true, false);
    QVERIFY(geometry.sectionCapTriangleCount() >= 2);
}

void SceneModelTest::meshGeometryBuildsSectionOverlayWithoutBodyFaces()
{
    loupe::app::render::MeshGeometry source;
    QVERIFY(source.appendWorkerMesh(QByteArrayLiteral(
        "{\"vertices\":[-1,-1,-1,1,-1,-1,1,1,-1,-1,1,-1,-1,-1,1,1,-1,1,1,1,1,-1,1,1],"
        "\"indices\":[0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7]}")));

    loupe::app::render::MeshGeometry overlay;
    QVERIFY(overlay.copySectionOverlayFrom(&source));
    QCOMPARE(overlay.triangleCount(), 0);
    overlay.configureSection(true, 0.0, 0.0, 1.0, 0.0, false, true, true,
                             true, true, false, 0.1);
    QVERIFY(overlay.sectionBusy());
    QTRY_VERIFY_WITH_TIMEOUT(overlay.sectionCapTriangleCount() >= 2, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(!overlay.sectionBusy(), 3000);
    QCOMPARE(overlay.triangleCount(), overlay.sectionCapTriangleCount());
    QCOMPARE(overlay.subsetCount(), 2);
    QCOMPARE(overlay.subsetName(0), QStringLiteral("section-fill"));
    QCOMPARE(overlay.subsetName(1), QStringLiteral("section-outline"));
    QCOMPARE(source.triangleCount(), 12);
}

void SceneModelTest::sectionOverlayBuildsFilledContoursWithRoundedJoins()
{
    auto source = QSharedPointer<loupe::app::render::SectionSourceData>::create();
    source->vertices = {-1.0F, -1.0F, -1.0F, 1.0F, -1.0F, -1.0F, 1.0F, 1.0F, -1.0F, -1.0F, 1.0F, -1.0F,
                        -1.0F, -1.0F, 1.0F, 1.0F, -1.0F, 1.0F, 1.0F, 1.0F, 1.0F, -1.0F, 1.0F, 1.0F};
    source->indices = {0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4,
                       1, 2, 6, 1, 6, 5, 2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7};

    loupe::app::render::SectionBuildRequest request;
    request.source = source;
    request.sliceOnly = true;
    request.sliceFill = true;
    request.sliceOutline = true;
    request.outlineWidth = 0.1F;
    const auto result = loupe::app::render::buildSectionOverlay(request);

    QVERIFY(result.fillIndexCount >= 12);
    const auto outlineIndexCount = result.indices.size() - result.fillIndexCount;
    QVERIFY(outlineIndexCount >= 240);
    QCOMPARE(result.capTriangleCount, result.indices.size() / 3);
}

void SceneModelTest::topologyRangesResolveAndCopyCompleteEntities()
{
    loupe::protocol::MeshPayload meshPayload{1, 1, QStringLiteral("definition"), QStringLiteral("node"),
        QStringLiteral("segment"), QStringLiteral("#ffffff"), 1,
        {0.0F, 0.0F, 0.0F, 2.0F, 0.0F, 0.0F, 0.0F, 2.0F, 0.0F},
        {0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F}, {0, 1, 2},
        {{7, loupe::protocol::TopologyKind::Face, 0, 3, 2.0F, 0.0F}}};
    loupe::app::render::MeshGeometry mesh;
    QVERIFY(mesh.appendWorkerMesh(loupe::protocol::encodeGeometry(meshPayload)));
    const auto face = mesh.topologyAtPoint(0.25, 0.25, 0.0);
    QCOMPARE(face.value(QStringLiteral("topologyId")).toUInt(), 7U);
    QCOMPARE(face.value(QStringLiteral("entityKind")).toString(), QStringLiteral("face"));
    loupe::app::render::MeshGeometry faceHighlight;
    QVERIFY(faceHighlight.copyTopologyFrom(&mesh, 7));
    QCOMPARE(faceHighlight.triangleCount(), 1);

    loupe::protocol::EdgePayload edgePayload{1, 1, QStringLiteral("definition"), QStringLiteral("node"), 1,
        {0.0F, 0.0F, 0.0F, 2.0F, 0.0F, 0.0F}, {0, 1},
        {{9, loupe::protocol::TopologyKind::Edge, 0, 2, 2.0F, 1.0F}}};
    loupe::app::render::CadEdgeGeometry edges;
    QVERIFY(edges.replaceWorkerEdges(loupe::protocol::encodeGeometry(edgePayload)));
    const auto edge = edges.topologyAtPoint(1.0, 0.05, 0.0, 0.1);
    QCOMPARE(edge.value(QStringLiteral("topologyId")).toUInt(), 9U);
    loupe::app::render::CadEdgeGeometry edgeHighlight;
    QVERIFY(edgeHighlight.copyTopologyFrom(&edges, 9));
    QCOMPARE(edgeHighlight.lineCount(), 1);
}

void SceneModelTest::faceBoundaryExcludesInternalTessellationEdges()
{
    loupe::protocol::MeshPayload payload{1, 1, QStringLiteral("definition"), QStringLiteral("node"),
        QStringLiteral("segment"), QStringLiteral("#ffffff"), 1,
        {0.0F, 0.0F, 0.0F, 2.0F, 0.0F, 0.0F, 2.0F, 2.0F, 0.0F, 0.0F, 2.0F, 0.0F},
        {0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F},
        {0, 1, 2, 0, 2, 3},
        {{17, loupe::protocol::TopologyKind::Face, 0, 6, 4.0F, 0.0F}}};
    loupe::app::render::MeshGeometry mesh;
    QVERIFY(mesh.appendWorkerMesh(loupe::protocol::encodeGeometry(payload)));

    loupe::app::render::CadEdgeGeometry boundary;
    QVERIFY(boundary.copyFaceBoundaryFrom(&mesh, 17));
    QCOMPARE(boundary.lineCount(), 4);
    QVERIFY(!boundary.copyFaceBoundaryFrom(&mesh, 99));

    loupe::app::render::CadEdgeGeometry wrongSource;
    QVERIFY(!boundary.copyFaceBoundaryFrom(&wrongSource, 17));
}

void SceneModelTest::faceFrameDistinguishesAFlatFaceFromACurvedOne()
{
    // Two coplanar triangles forming a 2x2 square in z=0, and a four-triangle strip
    // wrapped around a quarter cylinder. "Normal to this face" is only a meaningful view
    // for the first, so the frame has to tell them apart from the tessellation alone.
    //
    // The square's shading normals deliberately disagree with its geometry: this is what
    // a shared border vertex looks like once neighbouring faces have been averaged in,
    // and the flatness verdict must not depend on them.
    loupe::protocol::MeshPayload flatPayload{1, 1, QStringLiteral("definition"), QStringLiteral("node"),
        QStringLiteral("segment"), QStringLiteral("#ffffff"), 1,
        {0.0F, 0.0F, 0.0F, 2.0F, 0.0F, 0.0F, 2.0F, 2.0F, 0.0F, 0.0F, 2.0F, 0.0F},
        {0.0F, 0.7F, 0.7F, 0.7F, 0.0F, 0.7F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F},
        {0, 1, 2, 0, 2, 3},
        {{17, loupe::protocol::TopologyKind::Face, 0, 6, 4.0F, 0.0F}}};
    loupe::app::render::MeshGeometry flat;
    QVERIFY(flat.appendWorkerMesh(loupe::protocol::encodeGeometry(flatPayload)));

    const auto flatFrame = flat.faceFrameFor(17);
    QVERIFY(!flatFrame.isEmpty());
    const auto flatNormal = flatFrame.value(QStringLiteral("normal")).value<QVector3D>();
    QVERIFY(qFuzzyCompare(flatNormal.z(), 1.0F));
    QVERIFY(qFuzzyIsNull(flatNormal.x()));
    QVERIFY(qFuzzyIsNull(flatNormal.y()));
    QVERIFY(flatFrame.value(QStringLiteral("maximumDeviationDegrees")).toDouble() < 0.01);
    const auto flatCentroid = flatFrame.value(QStringLiteral("centroid")).value<QVector3D>();
    QVERIFY(qFuzzyCompare(flatCentroid.x(), 1.0F));
    QVERIFY(qFuzzyCompare(flatCentroid.y(), 1.0F));
    QCOMPARE(flatFrame.value(QStringLiteral("areaMm2")).toDouble(), 4.0);

    // A quarter cylinder of radius 1 about the y axis, as five columns at 0/22.5/45/
    // 67.5/90 degrees extruded 1 mm. Its triangles span the full quarter turn.
    QVector<float> curvedVertices;
    QVector<float> curvedNormals;
    QVector<quint32> curvedIndices;
    constexpr int columns = 5;
    for (int column = 0; column < columns; ++column) {
        const auto angle = static_cast<float>(column) / (columns - 1) * static_cast<float>(std::numbers::pi / 2.0);
        const auto x = std::cos(angle);
        const auto z = std::sin(angle);
        for (const auto y : {0.0F, 1.0F}) {
            curvedVertices << x << y << z;
            curvedNormals << x << 0.0F << z;
        }
    }
    for (int column = 0; column + 1 < columns; ++column) {
        const auto base = static_cast<quint32>(column) * 2;
        curvedIndices << base << base + 1 << base + 3 << base << base + 3 << base + 2;
    }
    loupe::protocol::MeshPayload curvedPayload{1, 1, QStringLiteral("definition"), QStringLiteral("node"),
        QStringLiteral("segment"), QStringLiteral("#ffffff"), 1, curvedVertices, curvedNormals, curvedIndices,
        {{23, loupe::protocol::TopologyKind::Face, 0, static_cast<quint32>(curvedIndices.size()), 1.57F, 1.0F}}};
    loupe::app::render::MeshGeometry curved;
    QVERIFY(curved.appendWorkerMesh(loupe::protocol::encodeGeometry(curvedPayload)));

    const auto curvedFrame = curved.faceFrameFor(23);
    QVERIFY(!curvedFrame.isEmpty());
    // Each facet sits 11.25 degrees off its neighbour and the mean points at 45 degrees,
    // so the outermost facets tilt roughly 39 degrees away from it. The exact figure
    // matters less than it being nowhere near flat.
    QVERIFY(curvedFrame.value(QStringLiteral("maximumDeviationDegrees")).toDouble() > 30.0);

    // An unknown face reports nothing rather than a default frame, which would silently
    // become a wrong view direction. The mirror case -- an edge range asked for as a face
    // -- is unreachable: the protocol refuses a non-Face range inside a mesh payload, so
    // the kind check in faceFrameFor is belt-and-braces and has no payload to test with.
    QVERIFY(flat.faceFrameFor(99).isEmpty());
}

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    SceneModelTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_scene_model.moc"
