#pragma once

#include <QObject>
#include <QHash>
#include <QStringList>

namespace loupe::app::models {

class VisibilityModel final : public QObject {
    Q_OBJECT
public:
    explicit VisibilityModel(QObject* parent = nullptr);

    void setNodes(const QStringList& nodeIds);
    void hide(const QString& nodeId);
    void hide(const QStringList& nodeIds);
    void show(const QString& nodeId);
    void isolate(const QString& nodeId);
    void isolate(const QStringList& nodeIds);
    void ghostComplements(const QString& nodeId);
    // Everything but these drawn semi-transparent: the selection in focus, with what is
    // around and in front of it still in view as context.
    void ghostComplements(const QStringList& nodeIds);
    // Semi-transparent rather than hidden, so a part stays in view as context while what
    // is behind it can be seen and picked. Restorable, like hide.
    void setGhosted(const QStringList& nodeIds, bool ghosted);
    void showAll();
    void restorePrevious();

    [[nodiscard]] bool isVisible(const QString& nodeId) const;
    [[nodiscard]] bool isGhosted(const QString& nodeId) const;
    [[nodiscard]] bool canRestore() const noexcept { return !previous_.isEmpty(); }

signals:
    void presentationChanged();

private:
    struct State { bool visible{true}; bool ghosted{false}; };
    QHash<QString, State> current_;
    QHash<QString, State> previous_;
};

} // namespace loupe::app::models
