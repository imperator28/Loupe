#include "app/render/SceneModel.h"

namespace loupe::app::render {

SceneModel::SceneModel(QQuick3DObject* parent)
    : QQuick3DObject(parent)
{
}

void SceneModel::applySnapshot(const QVector<Occurrence>& occurrences)
{
    occurrencesByDefinition_.clear();
    for (const auto& occurrence : occurrences) {
        occurrencesByDefinition_[occurrence.definitionId].append(occurrence.occurrenceId);
    }
}

void SceneModel::applyMesh(const MeshData& mesh)
{
    auto* geometry = geometryByDefinition_.value(mesh.definitionId);
    if (!geometry) {
        geometry = new MeshGeometry(this);
        geometryByDefinition_.insert(mesh.definitionId, geometry);
    }
    geometry->replaceMesh(mesh);
}

int SceneModel::instanceCount(const QString& definitionId) const
{
    return occurrencesByDefinition_.value(definitionId).size();
}

QString SceneModel::nodeIdForPick(const QString& definitionId, const int instanceIndex) const
{
    const auto occurrences = occurrencesByDefinition_.value(definitionId);
    return instanceIndex >= 0 && instanceIndex < occurrences.size() ? occurrences.at(instanceIndex) : QString{};
}

} // namespace loupe::app::render
