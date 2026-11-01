#pragma once

#include "app/ApplicationController.h"

#include <QAbstractListModel>
#include <QPointer>

#include <vector>

namespace loupe::app {

// The open documents, one per tab. Each tab is a whole ApplicationController -- its own
// worker process, selection, camera and Export/Drawing state -- so switching tabs keeps
// everything as it was left. There is always at least one tab, so the window always has
// an active document to bind to.
class DocumentTabs : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int activeIndex READ activeIndex WRITE setActiveIndex NOTIFY activeChanged)
    Q_PROPERTY(loupe::app::ApplicationController* active READ active NOTIFY activeChanged)

public:
    enum Role { DocumentRole = Qt::UserRole + 1, NameRole };

    explicit DocumentTabs(QObject* parent = nullptr);
    // For tests: every tab's controller starts its worker from this executable.
    explicit DocumentTabs(const QString& workerExecutable, QObject* parent = nullptr);
    ~DocumentTabs() override;

    [[nodiscard]] int count() const noexcept { return static_cast<int>(documents_.size()); }
    [[nodiscard]] int activeIndex() const noexcept { return activeIndex_; }
    void setActiveIndex(int index);
    [[nodiscard]] ApplicationController* active() const;

    Q_INVOKABLE [[nodiscard]] loupe::app::ApplicationController* documentAt(int index) const;
    // A new, empty tab, made active.
    Q_INVOKABLE loupe::app::ApplicationController* add();
    // Where a file being opened should go: the active tab if nothing is open in it (or it
    // only shows an error), else a new tab.
    Q_INVOKABLE loupe::app::ApplicationController* documentForOpening();
    // Close a tab, stopping its worker. Closing the last tab leaves one empty tab.
    Q_INVOKABLE void close(int index);
    // Drag-reorder (#15): move a tab, the active one staying active.
    Q_INVOKABLE void move(int from, int to);
    // Drag a tab out (#15): remove the tab without closing its document, which keeps its
    // worker and state, for another window to adopt. Refused for the last tab.
    Q_INVOKABLE loupe::app::ApplicationController* take(int index);
    // Add a document taken from another window, as the active tab.
    Q_INVOKABLE void adopt(loupe::app::ApplicationController* document);
    // Whether closing the tab would interrupt an export or drawing run.
    Q_INVOKABLE [[nodiscard]] bool isBusy(int index) const;

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

signals:
    void countChanged();
    void activeChanged();
    // A tab was created, for settings that every document should start from.
    void documentAdded(loupe::app::ApplicationController* document);

private:
    ApplicationController* create();
    void track(ApplicationController* document);

    QString workerExecutable_;
    std::vector<QPointer<ApplicationController>> documents_;
    int activeIndex_{0};
};

} // namespace loupe::app
