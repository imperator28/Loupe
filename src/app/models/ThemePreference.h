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
    // The display style (0 Solid, 1 Solid + Edges, 2 Edges Only) newly opened files start in.
    Q_PROPERTY(int renderMode READ renderMode WRITE setRenderMode NOTIFY renderModeChanged)
    // Width of Inspect's assembly tree panel, which the user can drag.
    Q_PROPERTY(int treePanelWidth READ treePanelWidth WRITE setTreePanelWidth NOTIFY treePanelWidthChanged)

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
    [[nodiscard]] int treePanelWidth() const noexcept { return treePanelWidth_; }
    void setTreePanelWidth(int width);

signals:
    void modeChanged();
    void partColorsChanged();
    void treePanelWidthChanged();
    void renderModeChanged();

private:
    QString mode_{QStringLiteral("System")};
    QString partColorMode_{QStringLiteral("file")};
    bool recolorFileColors_{false};
    int partColorSeed_{};
    int treePanelWidth_{248};
    int renderMode_{1};
};

} // namespace loupe::app::models
