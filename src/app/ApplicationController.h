#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QProcess>
#include <QHash>
#include <QString>
#include <QTimer>
#include <QVector3D>

#include <cstdint>
#include <memory>
#include <optional>

#include "app/tools/CaptureController.h"
#include "app/models/AssemblyTreeModel.h"
#include "app/models/MaterialLibraryModel.h"
#include "app/models/VisibilityModel.h"
#include "app/cache/OverrideStore.h"
#include "app/export/ExportWorkspaceController.h"
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
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString snapshotJson READ snapshotJson NOTIFY snapshotChanged)
    Q_PROPERTY(QObject* assemblyTree READ assemblyTreeModelObject CONSTANT)
    Q_PROPERTY(QObject* materials READ materialLibraryObject CONSTANT)
    Q_PROPERTY(double activeSurfaceAreaMm2 READ activeSurfaceAreaMm2 NOTIFY componentPropertiesChanged)
    Q_PROPERTY(double activeVolumeMm3 READ activeVolumeMm3 NOTIFY componentPropertiesChanged)
    Q_PROPERTY(double estimatedMassKg READ estimatedMassKg NOTIFY componentPropertiesChanged)
    Q_PROPERTY(QString activeMaterialId READ activeMaterialId NOTIFY componentPropertiesChanged)
    Q_PROPERTY(QString activeAppearanceColor READ activeAppearanceColor NOTIFY componentPropertiesChanged)
    Q_PROPERTY(QString activeResolvedAppearanceColor READ activeResolvedAppearanceColor NOTIFY componentPropertiesChanged)
    Q_PROPERTY(bool cacheHit READ cacheHit NOTIFY cacheHitChanged)
    Q_PROPERTY(double modelExtentMm READ modelExtentMm NOTIFY modelExtentChanged)
    Q_PROPERTY(ViewerPresentation viewerPresentation READ viewerPresentation NOTIFY viewerPresentationChanged)
    Q_PROPERTY(bool canRestoreVisibility READ canRestoreVisibility NOTIFY visibilityChanged)
    Q_PROPERTY(QString importStage READ importStage NOTIFY importProgressChanged)
    Q_PROPERTY(double importProgress READ importProgress NOTIFY importProgressChanged)
    Q_PROPERTY(qint64 importElapsedSeconds READ importElapsedSeconds NOTIFY importProgressChanged)
    Q_PROPERTY(qint64 importEtaSeconds READ importEtaSeconds NOTIFY importProgressChanged)
    Q_PROPERTY(bool importInProgress READ importInProgress NOTIFY importProgressChanged)
    Q_PROPERTY(bool previewReady READ previewReady NOTIFY importProgressChanged)
    Q_PROPERTY(QString effectiveUnit READ effectiveUnit NOTIFY effectiveUnitChanged)
    Q_PROPERTY(QObject* measurement READ measurementController CONSTANT)
    Q_PROPERTY(QObject* section READ sectionController CONSTANT)
    Q_PROPERTY(QObject* capture READ captureController CONSTANT)
    Q_PROPERTY(QObject* exportWorkspace READ exportWorkspaceController CONSTANT)

public:
    explicit ApplicationController(QObject* parent = nullptr);
    explicit ApplicationController(const QString& workerExecutable, QObject* parent = nullptr);
    ApplicationController(const QString& workerExecutable, const QString& cacheRoot, QObject* parent = nullptr);
    ~ApplicationController() override;

    [[nodiscard]] Workspace workspace() const noexcept { return workspace_; }
    [[nodiscard]] InspectorMode inspectorMode() const noexcept;
    [[nodiscard]] const QString& activeNodeId() const noexcept { return activeNodeId_; }
    [[nodiscard]] DocumentState documentState() const noexcept { return documentState_; }
    [[nodiscard]] const QString& errorMessage() const noexcept { return errorMessage_; }
    [[nodiscard]] const QString& snapshotJson() const noexcept { return snapshotJson_; }
    [[nodiscard]] models::AssemblyTreeModel* assemblyTreeModel() noexcept { return &assemblyTreeModel_; }
    [[nodiscard]] QObject* assemblyTreeModelObject() noexcept { return &assemblyTreeModel_; }
    [[nodiscard]] QObject* materialLibraryObject() noexcept { return &materialLibrary_; }
    [[nodiscard]] double activeSurfaceAreaMm2() const noexcept;
    [[nodiscard]] double activeVolumeMm3() const noexcept;
    [[nodiscard]] double estimatedMassKg() const noexcept;
    [[nodiscard]] QString activeMaterialId() const;
    [[nodiscard]] QString activeAppearanceColor() const;
    [[nodiscard]] QString activeResolvedAppearanceColor() const;
    [[nodiscard]] bool cacheHit() const noexcept { return cacheHit_; }
    [[nodiscard]] double modelExtentMm() const noexcept { return modelExtentMm_; }
    [[nodiscard]] ViewerPresentation viewerPresentation() const noexcept { return viewerPresentation_; }
    [[nodiscard]] bool canRestoreVisibility() const noexcept { return visibilityModel_.canRestore(); }
    [[nodiscard]] const QString& importStage() const noexcept { return importStage_; }
    [[nodiscard]] double importProgress() const noexcept { return importProgress_; }
    [[nodiscard]] qint64 importElapsedSeconds() const noexcept { return importElapsedSeconds_; }
    [[nodiscard]] qint64 importEtaSeconds() const noexcept;
    [[nodiscard]] bool importInProgress() const noexcept { return importInProgress_; }
    [[nodiscard]] bool previewReady() const noexcept { return previewGeometryReceived_; }
    [[nodiscard]] const QString& effectiveUnit() const noexcept { return effectiveUnit_; }
    [[nodiscard]] QObject* measurementController() noexcept { return &measurementController_; }
    [[nodiscard]] QObject* sectionController() noexcept { return &sectionController_; }
    [[nodiscard]] QObject* captureController() noexcept { return &captureController_; }
    [[nodiscard]] QObject* exportWorkspaceController() noexcept { return &exportWorkspaceController_; }

    Q_INVOKABLE void setWorkspace(Workspace workspace);
    Q_INVOKABLE void setActiveNodeId(const QString& activeNodeId);
    Q_INVOKABLE void openFile(const QUrl& file);
    Q_INVOKABLE void cancelImport();
    Q_INVOKABLE void fitView();
    Q_INVOKABLE void replayGeometry();
    Q_INVOKABLE void isolateActiveNode();
    Q_INVOKABLE void toggleIsolateActiveNode();
    Q_INVOKABLE void hideActiveNode();
    Q_INVOKABLE void hideOtherNodes();
    Q_INVOKABLE void showAllNodes();
    Q_INVOKABLE void restoreVisibility();
    Q_INVOKABLE void restoreFullAssembly();
    Q_INVOKABLE bool isNodeVisible(const QString& nodeId) const;
    Q_INVOKABLE bool assignActiveMaterial(const QString& materialId);
    Q_INVOKABLE bool assignActiveMaterial(const QString& materialId, const QString& scope);
    Q_INVOKABLE bool setActiveAppearanceColor(const QString& color, const QString& scope);
    Q_INVOKABLE bool clearActiveAppearanceColor(const QString& scope);
    Q_INVOKABLE QString resolvedAppearanceColor(const QString& nodeId, const QString& sourceColor) const;
    Q_INVOKABLE bool setUnitOverride(const QString& unit);
    Q_INVOKABLE void acceptViewSelection(const QString& nodeId, double x, double y, double z, double normalX, double normalY, double normalZ);
    Q_INVOKABLE void acceptViewPick(const QString& nodeId, double x, double y, double z, double normalX, double normalY, double normalZ);
    Q_INVOKABLE bool acceptTopologyPick(const QString& nodeId, const QString& entityKind, quint32 topologyId,
                                        double x, double y, double z, double normalX, double normalY, double normalZ,
                                        double measureMm, double radiusMm);

signals:
    void workspaceChanged();
    void inspectorModeChanged();
    void activeNodeIdChanged();
    void documentStateChanged();
    void errorMessageChanged();
    void snapshotChanged();
    void componentPropertiesChanged();
    void cacheHitChanged();
    void modelExtentChanged();
    void fitRequested();
    void viewerPresentationChanged();
    void visibilityChanged();
    void importProgressChanged();
    void effectiveUnitChanged();
    void meshReady(const QString& nodeId, const QString& segmentKey, const QString& sourceColor, const QByteArray& meshJson);
    void edgeReady(const QString& nodeId, const QByteArray& edgeJson);

private:
    void connectWorker();
    void setErrorMessage(const QString& errorMessage);
    void applySnapshotToTree(const QByteArray& snapshot);
    void applyActiveGeometryToMeasurement();
    [[nodiscard]] QStringList geometryNodesForAction(const QString& nodeId) const;
    [[nodiscard]] QStringList geometryNodesForScope(const QString& scope) const;
    [[nodiscard]] QStringList appearanceTargetsForScope(const QString& scope) const;
    void loadAppearanceOverrides();
    void applyAppearanceOverride(const cache::AppearanceOverride& record);
    [[nodiscard]] bool saveAppearanceOverride(const cache::AppearanceOverride& record);
    void resetImportProgress();
    void setImportProgress(const QString& stage, double fraction);
    void appendCachedMesh(const QString& nodeId, const QString& segmentKey, const QString& sourceColor, const QByteArray& payload);
    void appendCachedEdges(const QString& nodeId, const QByteArray& payload);
    void replayCachedGeometry(const QByteArray& archive);
    void replayGeometryChunk();
    void saveGeometryCache();

    struct ComponentGeometry final { double surfaceAreaMm2{}; double volumeMm3{}; QVector3D boundsMm; double longestEdgeMm{}; double circularRadiusMm{}; int planarFaceCount{}; };
    struct GeometryReplayEntry final {
        quint8 kind{};
        QString nodeId;
        QString segmentKey;
        QString sourceColor;
        QByteArray payload;
    };
    [[nodiscard]] static std::optional<QVector<GeometryReplayEntry>> decodeGeometryArchive(const QByteArray& archive);

    Workspace workspace_{Workspace::Inspect};
    QString activeNodeId_;
    QString workerExecutable_;
    QString stagedWorkerPath_;
    QString serverName_;
    QString pendingPath_;
    QString snapshotJson_;
    QString errorMessage_;
    DocumentState documentState_{DocumentState::Empty};
    QProcess workerProcess_;
    bool stoppingWorker_{false};
    worker::WorkerClient workerClient_{this};
    int connectionAttempts_{};
    std::uint64_t activeRequestId_{};
    std::uint64_t activeExportRequestId_{};
    models::AssemblyTreeModel assemblyTreeModel_{this};
    models::MaterialLibraryModel materialLibrary_{this};
    models::VisibilityModel visibilityModel_{this};
    std::unique_ptr<cache::CacheStore> cacheStore_;
    std::unique_ptr<cache::OverrideStore> overrideStore_;
    std::optional<cache::SourceIdentity> pendingSource_;
    std::optional<QString> pendingUnitOverride_;
    bool cacheHit_{false};
    QString importStage_;
    double importProgress_{};
    qint64 importElapsedSeconds_{};
    bool importInProgress_{false};
    bool preservePreviewOnCancel_{false};
    bool previewGeometryReceived_{false};
    bool cachedGeometryLoaded_{false};
    QByteArray geometryCacheArchive_;
    QVector<GeometryReplayEntry> geometryReplayQueue_;
    qsizetype geometryReplayIndex_{};
    quint64 geometryReplayGeneration_{};
    QTimer geometryReplayTimer_{this};
    QElapsedTimer importElapsed_;
    QTimer importProgressTimer_{this};
    double modelExtentMm_{};
    ViewerPresentation viewerPresentation_{ViewerPresentation::Full};
    QString effectiveUnit_{QStringLiteral("mm")};
    QHash<QString, ComponentGeometry> geometryByNode_;
    QHash<QString, QString> parentByNode_;
    QHash<QString, QString> definitionByNode_;
    QHash<QString, QString> materialByNode_;
    QHash<QString, QString> appearanceByNode_;
    QHash<QString, QString> importedColorByNode_;
    QHash<QString, QString> displayNameByNode_;
    QHash<QString, cache::AppearanceOverride> persistedAppearanceByKey_;
    tools::MeasurementController measurementController_{this};
    tools::SectionController sectionController_{this};
    tools::CaptureController captureController_{this};
    exporting::ExportWorkspaceController exportWorkspaceController_{this};
};

} // namespace loupe::app
