#pragma once

#include "app/render/MeshGeometry.h"

#include <QHash>
#include <QQuick3DObject>
#include <QStringList>

namespace loupe::app::render {

class SceneModel final : public QQuick3DObject {
    Q_OBJECT
public:
    struct Occurrence { QString definitionId; QString occurrenceId; };

    explicit SceneModel(QQuick3DObject* parent = nullptr);

    void applySnapshot(const QVector<Occurrence>& occurrences);
    void applyMesh(const MeshData& mesh);
    [[nodiscard]] int geometryCount() const noexcept { return geometryByDefinition_.size(); }
    [[nodiscard]] int instanceCount(const QString& definitionId) const;
    [[nodiscard]] QString nodeIdForPick(const QString& definitionId, int instanceIndex) const;

private:
    QHash<QString, QStringList> occurrencesByDefinition_;
    QHash<QString, MeshGeometry*> geometryByDefinition_;
};

} // namespace loupe::app::render
