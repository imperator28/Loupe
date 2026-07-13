#include <QtTest/QTest>

#include <QGuiApplication>

#include "app/render/SceneModel.h"

class SceneModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void repeatedDefinitionSharesOneGeometry();
    void selectionMapsPickInstanceToOccurrence();
    void meshGeometryAppendsWorkerPayload();
    void meshGeometryClipsTrianglesAgainstSectionPlane();
    void meshGeometryClipsTrianglesAgainstArbitraryPlane();
    void meshGeometryBuildsAPlanarCapForAClosedSection();
    void meshGeometryCanRenderOnlyThePlanarSlice();
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

void SceneModelTest::meshGeometryClipsTrianglesAgainstSectionPlane()
{
    loupe::app::render::MeshGeometry geometry;
    QVERIFY(geometry.appendWorkerMesh(QByteArrayLiteral("{\"vertices\":[0,0,0,2,0,0,0,2,0],\"indices\":[0,1,2]}")));

    geometry.setSection(true, 0, 1.0, false);

    QCOMPARE(geometry.triangleCount(), 1);
    QVERIFY(geometry.minimumCoordinate(0) >= 0.999F);
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

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    SceneModelTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_scene_model.moc"
