#pragma once

#include <QObject>
#include <QString>

namespace loupe::app::models {

class SelectionModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString activeNodeId READ activeNodeId WRITE setActiveNodeId NOTIFY activeNodeChanged)

public:
    explicit SelectionModel(QObject* parent = nullptr);
    [[nodiscard]] const QString& activeNodeId() const noexcept { return activeNodeId_; }
    Q_INVOKABLE void setActiveNodeId(const QString& activeNodeId);

signals:
    void activeNodeChanged();

private:
    QString activeNodeId_;
};

} // namespace loupe::app::models
