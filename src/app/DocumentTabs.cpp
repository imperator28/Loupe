#include "app/DocumentTabs.h"

#include "app/ApplicationController.h"

#include <algorithm>

namespace loupe::app {

DocumentTabs::DocumentTabs(QObject* parent)
    : DocumentTabs(QString{}, parent)
{
}

DocumentTabs::DocumentTabs(const QString& workerExecutable, QObject* parent)
    : QAbstractListModel(parent)
    , workerExecutable_(workerExecutable)
{
    documents_.emplace_back(create());
}

DocumentTabs::~DocumentTabs()
{
    for (const auto& document : documents_) delete document.data();
}

ApplicationController* DocumentTabs::create()
{
    auto* document = workerExecutable_.isEmpty() ? new ApplicationController(this)
                                                 : new ApplicationController(workerExecutable_, this);
    track(document);
    return document;
}

void DocumentTabs::track(ApplicationController* document)
{
    connect(document, &ApplicationController::documentNameChanged, this, [this, document] {
        const auto it = std::find(documents_.begin(), documents_.end(), document);
        if (it == documents_.end()) return;
        const auto row = index(static_cast<int>(std::distance(documents_.begin(), it)));
        emit dataChanged(row, row, {NameRole});
    });
}

void DocumentTabs::move(const int from, const int to)
{
    if (from < 0 || from >= count() || to < 0 || to >= count() || from == to) return;
    auto* active = this->active();
    // beginMoveRows wants the destination as it is before the move.
    beginMoveRows({}, from, from, {}, to > from ? to + 1 : to);
    auto moving = documents_.at(static_cast<std::size_t>(from));
    documents_.erase(documents_.begin() + from);
    documents_.insert(documents_.begin() + to, moving);
    endMoveRows();
    const auto it = std::find(documents_.begin(), documents_.end(), active);
    const auto activeIndex = it == documents_.end() ? 0 : static_cast<int>(std::distance(documents_.begin(), it));
    if (activeIndex != activeIndex_) {
        activeIndex_ = activeIndex;
        emit activeChanged();
    }
}

ApplicationController* DocumentTabs::take(const int index)
{
    if (index < 0 || index >= count() || count() == 1) return nullptr;
    auto* document = documents_.at(static_cast<std::size_t>(index)).data();
    beginRemoveRows({}, index, index);
    documents_.erase(documents_.begin() + index);
    endRemoveRows();
    emit countChanged();
    if (activeIndex_ > index || activeIndex_ >= count()) activeIndex_ = std::max(0, activeIndex_ - 1);
    emit activeChanged();
    if (document) {
        disconnect(document, nullptr, this, nullptr);
        document->setParent(nullptr);
    }
    return document;
}

void DocumentTabs::adopt(ApplicationController* document, const int index)
{
    if (!document || std::find(documents_.begin(), documents_.end(), document) != documents_.end()) return;
    document->setParent(this);
    track(document);
    const auto row = index >= 0 && index <= count() ? index : count();
    beginInsertRows({}, row, row);
    documents_.insert(documents_.begin() + row, document);
    endInsertRows();
    emit countChanged();
    activeIndex_ = row;
    emit activeChanged();
}

void DocumentTabs::setActiveIndex(const int index)
{
    if (index < 0 || index >= count() || index == activeIndex_) return;
    activeIndex_ = index;
    emit activeChanged();
}

ApplicationController* DocumentTabs::active() const { return documentAt(activeIndex_); }

ApplicationController* DocumentTabs::documentAt(const int index) const
{
    return index >= 0 && index < count() ? documents_.at(static_cast<std::size_t>(index)).data() : nullptr;
}

ApplicationController* DocumentTabs::add()
{
    const auto row = count();
    beginInsertRows({}, row, row);
    auto* document = create();
    documents_.emplace_back(document);
    endInsertRows();
    emit countChanged();
    emit documentAdded(document);
    activeIndex_ = row;
    emit activeChanged();
    return document;
}

ApplicationController* DocumentTabs::documentForOpening()
{
    auto* current = active();
    if (current) {
        const auto state = current->documentState();
        if (state == DocumentState::Empty || state == DocumentState::Invalid || state == DocumentState::WorkerFailed) return current;
    }
    return add();
}

void DocumentTabs::close(const int index)
{
    if (index < 0 || index >= count()) return;
    if (count() == 1) {
        // The window always has a document; the last tab is replaced with an empty one.
        add();
    }
    beginRemoveRows({}, index, index);
    auto* closing = documents_.at(static_cast<std::size_t>(index)).data();
    documents_.erase(documents_.begin() + index);
    endRemoveRows();
    emit countChanged();
    // Keep the same tab active where possible; closing the active one moves to its
    // neighbour, as browsers do.
    if (activeIndex_ > index || activeIndex_ >= count()) activeIndex_ = std::max(0, activeIndex_ - 1);
    emit activeChanged();
    // Deferred: QML bound to it is torn down first. The destructor stops its worker.
    if (closing) closing->deleteLater();
}

bool DocumentTabs::isBusy(const int index) const
{
    const auto* document = documentAt(index);
    if (!document) return false;
    const auto exporting = [](const QObject* workspace) { return workspace && workspace->property("exporting").toBool(); };
    return exporting(document->property("exportWorkspace").value<QObject*>())
        || exporting(document->property("drawingWorkspace").value<QObject*>())
        || document->assemblyExportInProgress();
}

int DocumentTabs::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : count(); }

QVariant DocumentTabs::data(const QModelIndex& index, const int role) const
{
    auto* document = documentAt(index.row());
    if (!index.isValid() || !document) return {};
    if (role == DocumentRole) return QVariant::fromValue(static_cast<QObject*>(document));
    if (role == NameRole || role == Qt::DisplayRole) return document->documentName();
    return {};
}

QHash<int, QByteArray> DocumentTabs::roleNames() const
{
    return {{DocumentRole, "document"}, {NameRole, "name"}};
}

} // namespace loupe::app
