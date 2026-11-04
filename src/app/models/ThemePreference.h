#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

namespace loupe::app::models {

class ThemePreference : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
    // Automatic part colours, remembered across sessions with the rest of the appearance.
    Q_PROPERTY(QString partColorMode READ partColorMode WRITE setPartColorMode NOTIFY partColorsChanged)
    Q_PROPERTY(bool recolorFileColors READ recolorFileColors WRITE setRecolorFileColors NOTIFY partColorsChanged)
    Q_PROPERTY(int partColorSeed READ partColorSeed WRITE setPartColorSeed NOTIFY partColorsChanged)
    // The display style (0 Solid, 1 Solid + Edges, 2 Edges Only) newly opened files start in.
    Q_PROPERTY(int renderMode READ renderMode WRITE setRenderMode NOTIFY renderModeChanged)
    // The widths the user dragged the workspaces' side panels to, by panel key (e.g.
    // "inspectTree", "pickerPanel"). A key with no entry is at its default width.
    Q_PROPERTY(QVariantMap panelWidths READ panelWidths NOTIFY panelWidthsChanged)

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
    [[nodiscard]] int renderMode() const noexcept { return renderMode_; }
    void setRenderMode(int mode);
    [[nodiscard]] const QVariantMap& panelWidths() const noexcept { return panelWidths_; }
    Q_INVOKABLE void setPanelWidth(const QString& key, int width);
    // Back to the panel's default width.
    Q_INVOKABLE void clearPanelWidth(const QString& key);

signals:
    void modeChanged();
    void partColorsChanged();
    void panelWidthsChanged();
    void renderModeChanged();

private:
    QString mode_{QStringLiteral("System")};
    QString partColorMode_{QStringLiteral("file")};
    bool recolorFileColors_{false};
    int partColorSeed_{};
    QVariantMap panelWidths_;
    int renderMode_{1};
};

} // namespace loupe::app::models
