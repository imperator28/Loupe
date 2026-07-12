#pragma once

#include <QObject>
#include <QString>

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

public:
    explicit ApplicationController(QObject* parent = nullptr);

    [[nodiscard]] Workspace workspace() const noexcept { return workspace_; }
    [[nodiscard]] InspectorMode inspectorMode() const noexcept;
    [[nodiscard]] const QString& activeNodeId() const noexcept { return activeNodeId_; }

    Q_INVOKABLE void setWorkspace(Workspace workspace);
    Q_INVOKABLE void setActiveNodeId(const QString& activeNodeId);

signals:
    void workspaceChanged();
    void inspectorModeChanged();
    void activeNodeIdChanged();

private:
    Workspace workspace_{Workspace::Inspect};
    QString activeNodeId_;
};

} // namespace loupe::app
