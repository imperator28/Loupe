#include "app/ApplicationController.h"

namespace loupe::app {

ApplicationController::ApplicationController(QObject* parent)
    : QObject(parent)
{
}

InspectorMode ApplicationController::inspectorMode() const noexcept
{
    return activeNodeId_.isEmpty() ? InspectorMode::Document : InspectorMode::Component;
}

void ApplicationController::setWorkspace(const Workspace workspace)
{
    if (workspace_ == workspace) {
        return;
    }
    workspace_ = workspace;
    emit workspaceChanged();
}

void ApplicationController::setActiveNodeId(const QString& activeNodeId)
{
    if (activeNodeId_ == activeNodeId) {
        return;
    }
    const auto previousMode = inspectorMode();
    activeNodeId_ = activeNodeId;
    emit activeNodeIdChanged();
    if (previousMode != inspectorMode()) {
        emit inspectorModeChanged();
    }
}

} // namespace loupe::app
