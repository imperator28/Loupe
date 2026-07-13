#pragma once

#include <QObject>
#include <QProcess>
#include <QHash>
#include <QString>
#include <QVector3D>

#include <cstdint>
#include <memory>
#include <optional>

#include "app/tools/CaptureController.h"
#include "app/models/AssemblyTreeModel.h"
#include "app/cache/OverrideStore.h"
#include "app/tools/MeasurementController.h"
#include "app/tools/SectionController.h"
#include "app/worker/WorkerClient.h"

namespace loupe::app {

namespace cache { class CacheStore; }

Q_NAMESPACE

enum class Workspace { Inspect, Export };
Q_ENUM_NS(Workspace)

enum class InspectorMode { Document, Component };
Q_ENUM_NS(InspectorMode)

enum class DocumentState { Empty, Opening, TreeReady, WorkerFailed, Invalid };
Q_ENUM_NS(DocumentState)

enum class ViewerPresentation { Full, Isolate, Ghost };
Q_ENUM_NS(ViewerPresentation)

class ApplicationController : public QObject {
    Q_OBJECT
    Q_PROPERTY(Workspace workspace READ workspace WRITE setWorkspace NOTIFY workspaceChanged)
    Q_PROPERTY(InspectorMode inspectorMode READ inspectorMode NOTIFY inspectorModeChanged)
    Q_PROPERTY(QString activeNodeId READ activeNodeId WRITE setActiveNodeId NOTIFY activeNodeIdChanged)
    Q_PROPERTY(DocumentState documentState READ documentState NOTIFY documentStateChanged)
    Q_PROPERTY(QString snapshotJson READ snapshotJson NOTIFY snapshotChanged)
    Q_PROPERTY(QObject* assemblyTree READ assemblyTreeModelObject CONSTANT)
    Q_PROPERTY(double activeSurfaceAreaMm2 READ activeSurfaceAreaMm2 NOTIFY componentPropertiesChanged)
    Q_PROPERTY(double activeVolumeMm3 READ activeVolumeMm3 NOTIFY componentPropertiesChanged)
    Q_PROPERTY(double estimatedMassKg READ estimatedMassKg NOTIFY componentPropertiesChanged)
    Q_PROPERTY(QString activeMaterialId READ activeMaterialId NOTIFY componentPropertiesChanged)
    Q_PROPERTY(bool cacheHit READ cacheHit NOTIFY cacheHitChanged)
    Q_PROPERTY(double modelExtentMm READ modelExtentMm NOTIFY modelExtentChanged)
    Q_PROPERTY(ViewerPresentation viewerPresentation READ viewerPresentation NOTIFY viewerPresentationChanged)
    Q_PROPERTY(QString effectiveUnit READ effectiveUnit NOTIFY effectiveUnitChanged)
    Q_PROPERTY(QObject* measurement READ measurementController CONSTANT)
    Q_PROPERTY(QObject* section READ sectionController CONSTANT)
    Q_PROPERTY(QObject* capture READ captureController CONSTANT)

public:
    explicit ApplicationController(QObject* parent = nullptr);
    explicit ApplicationController(const QString& workerExecutable, QObject* parent = nullptr);
    ApplicationController(const QString& workerExecutable, const QString& cacheRoot, QObject* parent = nullptr);
    ~ApplicationController() override;

    [[nodiscard]] Workspace workspace() const noexcept { return workspace_; }
    [[nodiscard]] InspectorMode inspectorMode() const noexcept;
    [[nodiscard]] const QString& activeNodeId() const noexcept { return activeNodeId_; }
    [[nodiscard]] DocumentState documentState() const noexcept { return documentState_; }
    [[nodiscard]] const QString& snapshotJson() const noexcept { return snapshotJson_; }
    [[nodiscard]] models::AssemblyTreeModel* assemblyTreeModel() noexcept { return &assemblyTreeModel_; }
    [[nodiscard]] QObject* assemblyTreeModelObject() noexcept { return &assemblyTreeModel_; }
    [[nodiscard]] double activeSurfaceAreaMm2() const noexcept;
    [[nodiscard]] double activeVolumeMm3() const noexcept;
    [[nodiscard]] double estimatedMassKg() const noexcept;
    [[nodiscard]] QString activeMaterialId() const;
    [[nodiscard]] bool cacheHit() const noexcept { return cacheHit_; }
    [[nodiscard]] double modelExtentMm() const noexcept { return modelExtentMm_; }
    [[nodiscard]] ViewerPresentation viewerPresentation() const noexcept { return viewerPresentation_; }
    [[nodiscard]] const QString& effectiveUnit() const noexcept { return effectiveUnit_; }
    [[nodiscard]] QObject* measurementController() noexcept { return &measurementController_; }
    [[nodiscard]] QObject* sectionController() noexcept { return &sectionController_; }
    [[nodiscard]] QObject* captureController() noexcept { return &captureController_; }

    Q_INVOKABLE void setWorkspace(Workspace workspace);
    Q_INVOKABLE void setActiveNodeId(const QString& activeNodeId);
    Q_INVOKABLE void openFile(const QUrl& file);
    Q_INVOKABLE void fitView();
    Q_INVOKABLE void isolateActiveNode();
    Q_INVOKABLE void ghostActiveNode();
    Q_INVOKABLE void restoreFullAssembly();
    Q_INVOKABLE bool assignActiveMaterial(const QString& materialId);
    Q_INVOKABLE bool setUnitOverride(const QString& unit);

signals:
    void workspaceChanged();
    void inspectorModeChanged();
    void activeNodeIdChanged();
    void documentStateChanged();
    void snapshotChanged();
    void componentPropertiesChanged();
    void cacheHitChanged();
    void modelExtentChanged();
    void fitRequested();
    void viewerPresentationChanged();
    void effectiveUnitChanged();
    void meshReady(const QString& nodeId, const QByteArray& meshJson);

private:
    void connectWorker();
    void applySnapshotToTree(const QByteArray& snapshot);
    void applyActiveGeometryToMeasurement();

    struct ComponentGeometry final { double surfaceAreaMm2{}; double volumeMm3{}; QVector3D boundsMm; };

    Workspace workspace_{Workspace::Inspect};
    QString activeNodeId_;
    QString workerExecutable_;
    QString serverName_;
    QString pendingPath_;
    QString snapshotJson_;
    DocumentState documentState_{DocumentState::Empty};
    QProcess workerProcess_;
    worker::WorkerClient workerClient_{this};
    int connectionAttempts_{};
    std::uint64_t activeRequestId_{};
    models::AssemblyTreeModel assemblyTreeModel_{this};
    std::unique_ptr<cache::CacheStore> cacheStore_;
    std::unique_ptr<cache::OverrideStore> overrideStore_;
    std::optional<cache::SourceIdentity> pendingSource_;
    std::optional<QString> pendingUnitOverride_;
    bool cacheHit_{false};
    double modelExtentMm_{};
    ViewerPresentation viewerPresentation_{ViewerPresentation::Full};
    QString effectiveUnit_{QStringLiteral("mm")};
    QHash<QString, ComponentGeometry> geometryByNode_;
    QHash<QString, QString> materialByNode_;
    tools::MeasurementController measurementController_{this};
    tools::SectionController sectionController_{this};
    tools::CaptureController captureController_{this};
};

} // namespace loupe::app
