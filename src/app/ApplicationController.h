#pragma once

#include <QObject>
#include <QProcess>
#include <QHash>
#include <QString>

#include <cstdint>

#include "app/tools/CaptureController.h"
#include "app/models/AssemblyTreeModel.h"
#include "app/tools/MeasurementController.h"
#include "app/tools/SectionController.h"
#include "app/worker/WorkerClient.h"

namespace loupe::app {

Q_NAMESPACE

enum class Workspace { Inspect, Export };
Q_ENUM_NS(Workspace)

enum class InspectorMode { Document, Component };
Q_ENUM_NS(InspectorMode)

enum class DocumentState { Empty, Opening, TreeReady, WorkerFailed, Invalid };
Q_ENUM_NS(DocumentState)

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
    Q_PROPERTY(QObject* measurement READ measurementController CONSTANT)
    Q_PROPERTY(QObject* section READ sectionController CONSTANT)
    Q_PROPERTY(QObject* capture READ captureController CONSTANT)

public:
    explicit ApplicationController(QObject* parent = nullptr);
    explicit ApplicationController(const QString& workerExecutable, QObject* parent = nullptr);
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
    [[nodiscard]] QObject* measurementController() noexcept { return &measurementController_; }
    [[nodiscard]] QObject* sectionController() noexcept { return &sectionController_; }
    [[nodiscard]] QObject* captureController() noexcept { return &captureController_; }

    Q_INVOKABLE void setWorkspace(Workspace workspace);
    Q_INVOKABLE void setActiveNodeId(const QString& activeNodeId);
    Q_INVOKABLE void openFile(const QUrl& file);
    Q_INVOKABLE bool assignActiveMaterial(const QString& materialId);

signals:
    void workspaceChanged();
    void inspectorModeChanged();
    void activeNodeIdChanged();
    void documentStateChanged();
    void snapshotChanged();
    void componentPropertiesChanged();

private:
    void connectWorker();
    void applySnapshotToTree(const QByteArray& snapshot);

    struct ComponentGeometry final { double surfaceAreaMm2{}; double volumeMm3{}; };

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
    QHash<QString, ComponentGeometry> geometryByNode_;
    QHash<QString, QString> materialByNode_;
    tools::MeasurementController measurementController_{this};
    tools::SectionController sectionController_{this};
    tools::CaptureController captureController_{this};
};

} // namespace loupe::app
