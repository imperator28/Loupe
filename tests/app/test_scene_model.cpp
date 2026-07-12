#include <QtTest/QTest>

#include <QGuiApplication>

#include "app/render/SceneModel.h"

class SceneModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void repeatedDefinitionSharesOneGeometry();
    void selectionMapsPickInstanceToOccurrence();
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

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    SceneModelTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_scene_model.moc"
