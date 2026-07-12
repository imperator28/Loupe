#include "app/models/SelectionModel.h"

namespace loupe::app::models {

SelectionModel::SelectionModel(QObject* parent)
    : QObject(parent)
{
}

void SelectionModel::setActiveNodeId(const QString& activeNodeId)
{
    if (activeNodeId_ == activeNodeId) {
        return;
    }
    activeNodeId_ = activeNodeId;
    emit activeNodeChanged();
}

} // namespace loupe::app::models
