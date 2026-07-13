#pragma once

#include <QObject>
#include <QString>

#include "app/tools/CaptureController.h"
#include "app/tools/MeasurementController.h"
#include "app/tools/SectionController.h"

namespace loupe::app {

Q_NAMESPACE

enum class Workspace { Inspect, Export };
Q_ENUM_NS(Workspace)

enum class InspectorMode { Document, Component };
Q_ENUM_NS(InspectorMode)

class ApplicationController : public QObject {
    Q_OBJECT
    Q_PROPERTY(Workspace workspace READ workspace WRITE setWorkspace NOTIFY workspaceChanged)
    Q_PROPERTY(InspectorMode inspectorMode READ inspectorMode NOTIFY inspectorModeChanged)
    Q_PROPERTY(QString activeNodeId READ activeNodeId WRITE setActiveNodeId NOTIFY activeNodeIdChanged)
    Q_PROPERTY(QObject* measurement READ measurementController CONSTANT)
    Q_PROPERTY(QObject* section READ sectionController CONSTANT)
    Q_PROPERTY(QObject* capture READ captureController CONSTANT)

public:
    explicit ApplicationController(QObject* parent = nullptr);

    [[nodiscard]] Workspace workspace() const noexcept { return workspace_; }
    [[nodiscard]] InspectorMode inspectorMode() const noexcept;
    [[nodiscard]] const QString& activeNodeId() const noexcept { return activeNodeId_; }
    [[nodiscard]] QObject* measurementController() noexcept { return &measurementController_; }
    [[nodiscard]] QObject* sectionController() noexcept { return &sectionController_; }
    [[nodiscard]] QObject* captureController() noexcept { return &captureController_; }

    Q_INVOKABLE void setWorkspace(Workspace workspace);
    Q_INVOKABLE void setActiveNodeId(const QString& activeNodeId);

signals:
    void workspaceChanged();
    void inspectorModeChanged();
    void activeNodeIdChanged();

private:
    Workspace workspace_{Workspace::Inspect};
    QString activeNodeId_;
    tools::MeasurementController measurementController_{this};
    tools::SectionController sectionController_{this};
    tools::CaptureController captureController_{this};
};

} // namespace loupe::app
