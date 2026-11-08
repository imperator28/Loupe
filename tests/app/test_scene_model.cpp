#include <QtTest/QTest>

#include <QGuiApplication>
#include <QVector2D>
#include <QVector3D>

#include <cmath>
#include <numbers>
#include "app/render/SceneModel.h"
#include "app/render/CadEdgeGeometry.h"
#include "app/render/SectionMeshBuilder.h"
#include "app/tools/MeasureGeometry.h"
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
    void sectionOverlaySkipsAPartThePlaneMisses();
    void sectionCapLeavesATubesBoreOpen();
    void sectionCapFillsAPlateAroundManyHoles();
    void sectionCapFillsAnIslandInsideABore();
    void sectionCapClosesAcrossASmallSeamGap();
    void sectionCapCompletesAContourWithLongStraightRuns();
    void sectionOverlayCapsAPartSplitAcrossColourSegments();
    void topologyRangesResolveAndCopyCompleteEntities();
    void measurementEntitiesCarryShapeAndSnapToVerticesAndCentres();
    void faceBoundaryExcludesInternalTessellationEdges();
    void faceFrameDistinguishesAFlatFaceFromACurvedOne();
    void faceViewLooksAlongAPlaneNormalOrACylinderAxis();
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

void SceneModelTest::sectionOverlaySkipsAPartThePlaneMisses()
{
    // A plane clear of the part: nothing to build, so the overlay is empty at once, with
    // no worker thread -- most parts of a large assembly, which used to be built one by one.
    QVERIFY(!loupe::app::render::sectionPlaneCrossesBox({-1, -1, -1}, {1, 1, 1}, {0, 0, 1}, 5.0));
    QVERIFY(!loupe::app::render::sectionPlaneCrossesBox({-1, -1, -1}, {1, 1, 1}, {0, 0, 1}, -1.5));
    QVERIFY(loupe::app::render::sectionPlaneCrossesBox({-1, -1, -1}, {1, 1, 1}, {0, 0, 1}, 0.5));
    QVERIFY(loupe::app::render::sectionPlaneCrossesBox({-1, -1, -1}, {1, 1, 1}, {0, 0, 1}, 1.0)); // touching a face
    // Tilted: the box's corner reaches sqrt(3) along the diagonal.
    const QVector3D diagonal(1, 1, 1);
    QVERIFY(loupe::app::render::sectionPlaneCrossesBox({-1, -1, -1}, {1, 1, 1}, diagonal, 1.7));
    QVERIFY(!loupe::app::render::sectionPlaneCrossesBox({-1, -1, -1}, {1, 1, 1}, diagonal, 1.8));

    loupe::app::render::MeshGeometry source;
    QVERIFY(source.appendWorkerMesh(QByteArrayLiteral(
        "{\"vertices\":[-1,-1,-1,1,-1,-1,1,1,-1,-1,1,-1,-1,-1,1,1,-1,1,1,1,1,-1,1,1],"
        "\"indices\":[0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7]}")));
    loupe::app::render::MeshGeometry overlay;
    QVERIFY(overlay.copySectionOverlayFrom(&source));
    // Cut first, so there is a cap to clear.
    overlay.configureSection(true, 0.0, 0.0, 1.0, 0.0, false, true, false, true, true, false, 0.1);
    QTRY_VERIFY_WITH_TIMEOUT(overlay.sectionCapTriangleCount() >= 2, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(!overlay.sectionBusy(), 3000);
    overlay.configureSection(true, 0.0, 0.0, 1.0, 5.0, false, true, false, true, true, false, 0.1);
    QVERIFY(!overlay.sectionBusy());
    QCOMPARE(overlay.triangleCount(), 0);
    QCOMPARE(overlay.sectionCapTriangleCount(), 0);
}

namespace {

using Ring = QVector<QVector2D>;

// The walls of a prism standing on z = -1..1, each wall its own quad with its own vertices,
// as the worker sends a part: one face's triangulation does not share nodes with the next.
QSharedPointer<loupe::app::render::SectionSourceData> prismWalls(const QVector<Ring>& rings,
                                                                 const QVector2D& seamShift = {})
{
    auto source = QSharedPointer<loupe::app::render::SectionSourceData>::create();
    for (const auto& ring : rings) {
        for (qsizetype index = 0; index < ring.size(); ++index) {
            auto first = ring.at(index);
            auto second = ring.at((index + 1) % ring.size());
            // The first wall of the first ring is moved a hair, as an unwelded seam would be.
            if (index == 0 && &ring == &rings.front()) {
                first += seamShift;
                second += seamShift;
            }
            const auto base = static_cast<quint32>(source->vertices.size() / 3);
            for (const auto& corner : {QVector3D(first, -1.0F), QVector3D(second, -1.0F),
                                       QVector3D(second, 1.0F), QVector3D(first, 1.0F)}) {
                source->vertices.append(corner.x());
                source->vertices.append(corner.y());
                source->vertices.append(corner.z());
            }
            source->indices.append({base, base + 1, base + 2, base, base + 2, base + 3});
        }
    }
    return source;
}

Ring square(const float half, const QVector2D& centre = {})
{
    return {centre + QVector2D(-half, -half), centre + QVector2D(half, -half),
            centre + QVector2D(half, half), centre + QVector2D(-half, half)};
}

Ring circle(const float radius, const int sides, const QVector2D& centre = {})
{
    Ring ring;
    for (int index = 0; index < sides; ++index) {
        const auto angle = 2.0 * std::numbers::pi * index / sides;
        ring.append(centre + QVector2D(static_cast<float>(std::cos(angle) * radius),
                                       static_cast<float>(std::sin(angle) * radius)));
    }
    return ring;
}

// The area the cap covers: its fill triangles, each drawn from both sides.
double capArea(const loupe::app::render::SectionBuildResult& result)
{
    double area{};
    for (qsizetype index = 0; index + 2 < result.fillIndexCount; index += 3) {
        const auto point = [&result](const quint32 vertex) {
            const auto offset = static_cast<qsizetype>(vertex) * 3;
            return QVector3D(result.vertices.at(offset), result.vertices.at(offset + 1), result.vertices.at(offset + 2));
        };
        const auto a = point(result.indices.at(index));
        const auto b = point(result.indices.at(index + 1));
        const auto c = point(result.indices.at(index + 2));
        area += 0.5 * static_cast<double>(QVector3D::crossProduct(b - a, c - a).length());
    }
    return area / 2.0;
}

double capAreaFor(const QSharedPointer<loupe::app::render::SectionSourceData>& source)
{
    loupe::app::render::SectionBuildRequest request;
    request.source = source;
    request.normal = {0.0F, 0.0F, 1.0F};
    request.offset = 0.0;
    request.capEnabled = true;
    return capArea(loupe::app::render::buildSectionOverlay(request));
}

double polygonArea(const Ring& ring)
{
    double area{};
    for (qsizetype index = 0; index < ring.size(); ++index) {
        const auto& a = ring.at(index);
        const auto& b = ring.at((index + 1) % ring.size());
        area += static_cast<double>(a.x()) * b.y() - static_cast<double>(b.x()) * a.y();
    }
    return std::abs(area) * 0.5;
}

} // namespace

void SceneModelTest::sectionCapLeavesATubesBoreOpen()
{
    // A square tube: the cut is a ring, and its bore must stay open. Filling every contour
    // on its own covered the bore over.
    QVERIFY(std::abs(capAreaFor(prismWalls({square(2.0F), square(1.0F)})) - 12.0) < 1.0e-3);
    // A round one, with the bore's contour wound the same way as the outside.
    const auto outer = circle(5.0F, 96);
    const auto inner = circle(3.0F, 64);
    const auto expected = polygonArea(outer) - polygonArea(inner);
    QVERIFY(std::abs(capAreaFor(prismWalls({outer, inner})) - expected) < expected * 1.0e-4);
}

void SceneModelTest::sectionCapFillsAPlateAroundManyHoles()
{
    QVector<Ring> rings{square(10.0F)};
    double holes{};
    for (int row = -2; row <= 2; ++row) {
        for (int column = -2; column <= 2; ++column) {
            rings.append(circle(1.0F, 24, QVector2D(column * 3.5F, row * 3.5F)));
            holes += polygonArea(rings.constLast());
        }
    }
    const auto expected = 400.0 - holes;
    QVERIFY2(std::abs(capAreaFor(prismWalls(rings)) - expected) < expected * 1.0e-4,
             qPrintable(QString::number(capAreaFor(prismWalls(rings)))));
}

void SceneModelTest::sectionCapFillsAnIslandInsideABore()
{
    // Wall, bore, and a pin standing in the bore: material, hole, material again.
    const auto expected = 64.0 - 16.0 + 4.0;
    QVERIFY(std::abs(capAreaFor(prismWalls({square(4.0F), square(2.0F), square(1.0F)})) - expected) < 1.0e-3);
}

void SceneModelTest::sectionCapClosesAcrossASmallSeamGap()
{
    // One wall a few microns off its neighbours: the cut's chain does not meet itself, and
    // an open chain was left unfilled -- the part showed hollow.
    const auto area = capAreaFor(prismWalls({square(10.0F)}, QVector2D(0.0F, 0.004F)));
    QVERIFY2(std::abs(area - 400.0) < 0.5, qPrintable(QString::number(area)));
}

void SceneModelTest::sectionCapCompletesAContourWithLongStraightRuns()
{
    // An L with every side split into many collinear points, as a mesher splits a long
    // edge: clipping stalled on the collinear points and gave up with the cap half-built.
    Ring ring;
    const auto run = [&ring](const QVector2D& from, const QVector2D& to) {
        for (int step = 0; step < 40; ++step) ring.append(from + (to - from) * (static_cast<float>(step) / 40.0F));
    };
    run({0, 0}, {6, 0});
    run({6, 0}, {6, 2});
    run({6, 2}, {2, 2});
    run({2, 2}, {2, 6});
    run({2, 6}, {0, 6});
    run({0, 6}, {0, 0});
    const auto area = capAreaFor(prismWalls({ring}));
    QVERIFY2(std::abs(area - 20.0) < 1.0e-3, qPrintable(QString::number(area)));
}

void SceneModelTest::sectionOverlayCapsAPartSplitAcrossColourSegments()
{
    // A part whose faces are not all one colour arrives as several meshes, one per colour.
    // Each alone is an open shell whose cut cannot close; the part's cap is built from all.
    loupe::app::render::MeshGeometry sides;
    QVERIFY(sides.appendWorkerMesh(QByteArrayLiteral(
        "{\"vertices\":[-1,-1,-1,1,-1,-1,1,1,-1,-1,1,-1,-1,-1,1,1,-1,1,1,1,1,-1,1,1],"
        "\"indices\":[0,1,5,0,5,4,1,2,6,1,6,5]}")));
    loupe::app::render::MeshGeometry otherSides;
    QVERIFY(otherSides.appendWorkerMesh(QByteArrayLiteral(
        "{\"vertices\":[-1,-1,-1,1,-1,-1,1,1,-1,-1,1,-1,-1,-1,1,1,-1,1,1,1,1,-1,1,1],"
        "\"indices\":[0,2,1,0,3,2,4,5,6,4,6,7,2,3,7,2,7,6,3,0,4,3,4,7]}")));

    loupe::app::render::MeshGeometry single;
    QVERIFY(single.copySectionOverlayFrom(&sides));
    single.configureSection(true, 0.0, 0.0, 1.0, 0.0, false, true, false, true, true, false, 0.1);
    QTRY_VERIFY_WITH_TIMEOUT(!single.sectionBusy(), 3000);
    QCOMPARE(single.sectionCapTriangleCount(), 0);

    loupe::app::render::MeshGeometry overlay;
    QVERIFY(overlay.copySectionOverlayFromSources({QVariant::fromValue<QObject*>(&sides),
                                                   QVariant::fromValue<QObject*>(&otherSides)}));
    overlay.configureSection(true, 0.0, 0.0, 1.0, 0.0, false, true, false, true, true, false, 0.1);
    QTRY_VERIFY_WITH_TIMEOUT(!overlay.sectionBusy(), 3000);
    QVERIFY(overlay.sectionCapTriangleCount() >= 2);
}

void SceneModelTest::measurementEntitiesCarryShapeAndSnapToVerticesAndCentres()
{
    using loupe::app::tools::MeasureEntity;
    // A flat square face, placed 10 up in the scene.
    loupe::protocol::MeshPayload meshPayload{1, 1, QStringLiteral("definition"), QStringLiteral("node"),
        QStringLiteral("segment"), QStringLiteral("#ffffff"), 1,
        {0, 0, 0, 2, 0, 0, 2, 2, 0, 0, 2, 0},
        {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1}, {0, 1, 2, 0, 2, 3},
        {{7, loupe::protocol::TopologyKind::Face, 0, 6, 4.0F, 0.0F}}};
    loupe::app::render::MeshGeometry mesh;
    QVERIFY(mesh.appendWorkerMesh(loupe::protocol::encodeGeometry(meshPayload)));
    QMatrix4x4 placed;
    placed.translate(0, 0, 10);
    const auto face = mesh.faceEntity(7, placed, {1, 1, 10}, {0, 0, 1}, true).value<MeasureEntity>();
    QCOMPARE(face.kind, MeasureEntity::Kind::Face);
    QVERIFY(face.planar);
    QCOMPARE(face.areaMm2, 4.0);
    QVERIFY((face.planePoint - QVector3D(1, 1, 10)).length() < 1e-5F);
    QVERIFY((face.planeNormal - QVector3D(0, 0, 1)).length() < 1e-5F);
    QCOMPARE(face.triangles.size(), 6);
    QVERIFY(mesh.faceEntity(7, placed, {}, {}, false).value<MeasureEntity>().triangles.isEmpty());

    // A straight edge from (0,0,0) to (4,0,0) in two segments, and a circle of radius 1 about (10,0,0).
    QVector<float> vertices{0, 0, 0, 2, 0, 0, 4, 0, 0};
    QVector<quint32> indices{0, 1, 1, 2};
    const auto circleStart = static_cast<quint32>(vertices.size() / 3);
    for (int step = 0; step < 32; ++step) {
        const auto angle = 2.0 * std::numbers::pi * step / 32;
        vertices.append({static_cast<float>(10 + std::cos(angle)), static_cast<float>(std::sin(angle)), 0.0F});
    }
    for (quint32 step = 0; step < 32; ++step) indices.append({circleStart + step, circleStart + (step + 1) % 32});
    loupe::protocol::EdgePayload edgePayload{1, 1, QStringLiteral("definition"), QStringLiteral("node"), 1, vertices, indices,
        {{3, loupe::protocol::TopologyKind::Edge, 0, 4, 4.0F, 0.0F},
         {5, loupe::protocol::TopologyKind::Edge, 4, 64, static_cast<float>(2 * std::numbers::pi), 1.0F}}};
    loupe::app::render::CadEdgeGeometry edges;
    QVERIFY(edges.replaceWorkerEdges(loupe::protocol::encodeGeometry(edgePayload)));
    const QMatrix4x4 identity;
    const auto line = edges.edgeEntity(3, identity, {3, 1, 0}).value<MeasureEntity>();
    QVERIFY(line.linear);
    QCOMPARE(line.polyline.size(), 3);
    QVERIFY((line.point - QVector3D(3, 0, 0)).length() < 1e-5F);
    const auto ring = edges.edgeEntity(5, identity, {}).value<MeasureEntity>();
    QVERIFY(ring.circular);
    QVERIFY((ring.centre - QVector3D(10, 0, 0)).length() < 1e-4F);

    // Near the line's end: its vertex. Along it: a point on it. Near the circle: its centre.
    const auto vertex = edges.snapPointAt(3.95, 0.02, 0, 0.1, 0.1, identity).value<MeasureEntity>();
    QCOMPARE(vertex.kind, MeasureEntity::Kind::Vertex);
    QVERIFY((vertex.point - QVector3D(4, 0, 0)).length() < 1e-5F);
    const auto onEdge = edges.snapPointAt(1.3, 0.02, 0, 0.1, 0.1, identity).value<MeasureEntity>();
    QCOMPARE(onEdge.kind, MeasureEntity::Kind::EdgePoint);
    QVERIFY((onEdge.point - QVector3D(1.3F, 0, 0)).length() < 1e-5F);
    // Near its middle, by length: the midpoint, exactly.
    const auto middle = edges.snapPointAt(2.06, 0.02, 0, 0.1, 0.1, identity).value<MeasureEntity>();
    QCOMPARE(middle.kind, MeasureEntity::Kind::Midpoint);
    QVERIFY((middle.point - QVector3D(2, 0, 0)).length() < 1e-5F);
    QVERIFY(middle.isPointLike());
    const auto centre = edges.snapPointAt(10, 1.02, 0, 0.1, 0.01, identity).value<MeasureEntity>();
    QCOMPARE(centre.kind, MeasureEntity::Kind::CircleCentre);
    QVERIFY((centre.point - QVector3D(10, 0, 0)).length() < 1e-4F);
    QVERIFY(!edges.snapPointAt(20, 20, 0, 0.1, 0.1, identity).isValid());
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

void SceneModelTest::faceViewLooksAlongAPlaneNormalOrACylinderAxis()
{
    // A 4 x 1 rectangle tilted 30 degrees about x: "normal to face" must give its exact
    // normal, aim at its centre, and offer its long side to square the view against.
    const auto angle = static_cast<float>(std::numbers::pi / 6.0);
    const auto c = std::cos(angle);
    const auto s = std::sin(angle);
    loupe::protocol::MeshPayload tiltedPayload{1, 1, QStringLiteral("definition"), QStringLiteral("node"),
        QStringLiteral("segment"), QStringLiteral("#ffffff"), 1,
        {0.0F, 0.0F, 0.0F, 4.0F, 0.0F, 0.0F, 4.0F, c, s, 0.0F, c, s},
        {0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F},
        {0, 1, 2, 0, 2, 3},
        {{31, loupe::protocol::TopologyKind::Face, 0, 6, 4.0F, 0.0F}}};
    loupe::app::render::MeshGeometry tilted;
    QVERIFY(tilted.appendWorkerMesh(loupe::protocol::encodeGeometry(tiltedPayload)));
    const auto tiltedView = tilted.faceViewFor(31);
    QCOMPARE(tiltedView.value(QStringLiteral("kind")).toString(), QStringLiteral("plane"));
    const auto normal = tiltedView.value(QStringLiteral("direction")).value<QVector3D>();
    QVERIFY(std::abs(normal.x()) < 1.0e-6F);
    QVERIFY(std::abs(normal.y() + s) < 1.0e-6F);
    QVERIFY(std::abs(normal.z() - c) < 1.0e-6F);
    const auto centre = tiltedView.value(QStringLiteral("centroid")).value<QVector3D>();
    QVERIFY(std::abs(centre.x() - 2.0F) < 1.0e-5F && std::abs(centre.y() - c / 2) < 1.0e-5F);
    const auto edge = tiltedView.value(QStringLiteral("edgeDirection")).value<QVector3D>();
    QVERIFY(std::abs(std::abs(edge.x()) - 1.0F) < 1.0e-6F);

    // The quarter cylinder from the frame test: no single normal, but an axis (y).
    QVector<float> vertices;
    QVector<float> normals;
    QVector<quint32> indices;
    for (int column = 0; column < 5; ++column) {
        const auto columnAngle = static_cast<float>(column) / 4.0F * static_cast<float>(std::numbers::pi / 2.0);
        for (const auto y : {0.0F, 1.0F}) {
            vertices << std::cos(columnAngle) << y << std::sin(columnAngle);
            normals << std::cos(columnAngle) << 0.0F << std::sin(columnAngle);
        }
    }
    for (quint32 column = 0; column + 1 < 5; ++column)
        indices << column * 2 << column * 2 + 1 << column * 2 + 3 << column * 2 << column * 2 + 3 << column * 2 + 2;
    loupe::protocol::MeshPayload curvedPayload{1, 1, QStringLiteral("definition"), QStringLiteral("node"),
        QStringLiteral("segment"), QStringLiteral("#ffffff"), 1, vertices, normals, indices,
        {{23, loupe::protocol::TopologyKind::Face, 0, static_cast<quint32>(indices.size()), 1.57F, 1.0F}}};
    loupe::app::render::MeshGeometry curved;
    QVERIFY(curved.appendWorkerMesh(loupe::protocol::encodeGeometry(curvedPayload)));
    const auto curvedView = curved.faceViewFor(23);
    QCOMPARE(curvedView.value(QStringLiteral("kind")).toString(), QStringLiteral("axis"));
    const auto axis = curvedView.value(QStringLiteral("direction")).value<QVector3D>();
    QVERIFY(std::abs(std::abs(axis.y()) - 1.0F) < 1.0e-5F);

    QVERIFY(tilted.faceViewFor(99).isEmpty());
}

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    SceneModelTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_scene_model.moc"
