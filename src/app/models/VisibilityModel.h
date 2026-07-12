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
    void show(const QString& nodeId);
    void isolate(const QString& nodeId);
    void ghostComplements(const QString& nodeId);
    void restorePrevious();

    [[nodiscard]] bool isVisible(const QString& nodeId) const;
    [[nodiscard]] bool isGhosted(const QString& nodeId) const;

signals:
    void presentationChanged();

private:
    struct State { bool visible{true}; bool ghosted{false}; };
    QHash<QString, State> current_;
    QHash<QString, State> previous_;
};

} // namespace loupe::app::models
