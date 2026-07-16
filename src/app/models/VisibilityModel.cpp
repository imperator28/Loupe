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
    if (current_.contains(nodeId) && current_[nodeId].visible) {
        previous_ = current_;
        current_[nodeId].visible = false;
        emit presentationChanged();
    }
}

void VisibilityModel::hide(const QStringList& nodeIds)
{
    bool changed = false;
    const auto before = current_;
    for (const auto& nodeId : nodeIds) {
        if (current_.contains(nodeId) && current_[nodeId].visible) {
            current_[nodeId].visible = false;
            changed = true;
        }
    }
    if (changed) {
        previous_ = before;
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
    isolate(QStringList{nodeId});
}

void VisibilityModel::isolate(const QStringList& nodeIds)
{
    if (nodeIds.isEmpty()) return;
    bool hasKnownNode = false;
    for (const auto& nodeId : nodeIds) {
        hasKnownNode = hasKnownNode || current_.contains(nodeId);
    }
    if (!hasKnownNode) return;
    previous_ = current_;
    for (auto it = current_.begin(); it != current_.end(); ++it) {
        it->visible = nodeIds.contains(it.key());
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

void VisibilityModel::showAll()
{
    const auto before = current_;
    bool changed = false;
    for (auto it = current_.begin(); it != current_.end(); ++it) {
        if (!it->visible || it->ghosted) {
            it->visible = true;
            it->ghosted = false;
            changed = true;
        }
    }
    if (changed) {
        previous_ = before;
        emit presentationChanged();
    }
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
