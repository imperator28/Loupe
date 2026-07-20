#include <QtTest/QTest>

#include <QGuiApplication>
#include "app/render/SceneModel.h"
#include "app/render/CadEdgeGeometry.h"
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
    void topologyRangesResolveAndCopyCompleteEntities();
    void faceBoundaryExcludesInternalTessellationEdges();
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
    overlay.configureSection(true, 0.0, 0.0, 1.0, 0.0, false, true, false, true, true, false);
    QVERIFY(overlay.sectionBusy());
    QTRY_VERIFY_WITH_TIMEOUT(overlay.sectionCapTriangleCount() >= 2, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(!overlay.sectionBusy(), 3000);
    QCOMPARE(overlay.triangleCount(), overlay.sectionCapTriangleCount());
    QCOMPARE(source.triangleCount(), 12);
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

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    SceneModelTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_scene_model.moc"
