#include "app/models/VisibilityModel.h"

namespace loupe::app::models {

VisibilityModel::VisibilityModel(QObject* parent)
    : QObject(parent)
{
}

void VisibilityModel::setNodes(const QStringList& nodeIds)
{
    current_.clear();
    previous_.clear();
    for (const auto& nodeId : nodeIds) {
        current_.insert(nodeId, {});
    }
    emit presentationChanged();
}

void VisibilityModel::hide(const QString& nodeId)
{
    if (current_.contains(nodeId)) {
        current_[nodeId].visible = false;
        emit presentationChanged();
    }
}

void VisibilityModel::show(const QString& nodeId)
{
    if (current_.contains(nodeId)) {
        current_[nodeId].visible = true;
        emit presentationChanged();
    }
}

void VisibilityModel::isolate(const QString& nodeId)
{
    if (!current_.contains(nodeId)) {
        return;
    }
    previous_ = current_;
    for (auto it = current_.begin(); it != current_.end(); ++it) {
        it->visible = it.key() == nodeId;
        it->ghosted = false;
    }
    emit presentationChanged();
}

void VisibilityModel::ghostComplements(const QString& nodeId)
{
    if (!current_.contains(nodeId)) {
        return;
    }
    previous_ = current_;
    for (auto it = current_.begin(); it != current_.end(); ++it) {
        it->visible = true;
        it->ghosted = it.key() != nodeId;
    }
    emit presentationChanged();
}

void VisibilityModel::restorePrevious()
{
    if (previous_.isEmpty()) {
        return;
    }
    current_ = previous_;
    previous_.clear();
    emit presentationChanged();
}

bool VisibilityModel::isVisible(const QString& nodeId) const
{
    return current_.value(nodeId).visible;
}

bool VisibilityModel::isGhosted(const QString& nodeId) const
{
    return current_.value(nodeId).ghosted;
}

} // namespace loupe::app::models
