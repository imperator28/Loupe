#pragma once

#include <QObject>
#include <QString>

namespace loupe::app::models {

class ThemePreference : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)

public:
    explicit ThemePreference(QObject* parent = nullptr);

    [[nodiscard]] const QString& mode() const noexcept { return mode_; }
    void setMode(const QString& mode);

signals:
    void modeChanged();

private:
    QString mode_{QStringLiteral("System")};
};

} // namespace loupe::app::models
