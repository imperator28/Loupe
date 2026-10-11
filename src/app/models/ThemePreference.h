#pragma once

#include <QObject>
#include <QString>

namespace loupe::app::models {

class ThemePreference : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
    // Automatic part colours, remembered across sessions with the rest of the appearance.
    Q_PROPERTY(QString partColorMode READ partColorMode WRITE setPartColorMode NOTIFY partColorsChanged)
    Q_PROPERTY(bool recolorFileColors READ recolorFileColors WRITE setRecolorFileColors NOTIFY partColorsChanged)
    Q_PROPERTY(int partColorSeed READ partColorSeed WRITE setPartColorSeed NOTIFY partColorsChanged)

public:
    explicit ThemePreference(QObject* parent = nullptr);

    [[nodiscard]] const QString& mode() const noexcept { return mode_; }
    void setMode(const QString& mode);
    [[nodiscard]] const QString& partColorMode() const noexcept { return partColorMode_; }
    void setPartColorMode(const QString& mode);
    [[nodiscard]] bool recolorFileColors() const noexcept { return recolorFileColors_; }
    void setRecolorFileColors(bool recolor);
    [[nodiscard]] int partColorSeed() const noexcept { return partColorSeed_; }
    void setPartColorSeed(int seed);

signals:
    void modeChanged();
    void partColorsChanged();

private:
    QString mode_{QStringLiteral("System")};
    QString partColorMode_{QStringLiteral("file")};
    bool recolorFileColors_{false};
    int partColorSeed_{};
};

} // namespace loupe::app::models
