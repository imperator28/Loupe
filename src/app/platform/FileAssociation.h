#pragma once

#include <QObject>

namespace loupe::app::platform {

// Windows: make Loupe an app that can open .step / .stp files (#8). Loupe ships as a zip
// with no installer, so nothing else registers it. This writes per-user keys only (no
// administrator rights): a ProgID, the extensions' OpenWithProgids, and the Capabilities
// entry Default Apps lists. Windows 10 and 11 do not let an app make itself the default;
// the user confirms that in Settings, which openDefaultAppsSettings() opens.
// Elsewhere it does nothing: macOS reads the bundle's Info.plist.
class FileAssociation : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool supported READ supported CONSTANT)
    Q_PROPERTY(bool registered READ registered NOTIFY registeredChanged)

public:
    explicit FileAssociation(QObject* parent = nullptr);

    [[nodiscard]] bool supported() const noexcept;
    [[nodiscard]] bool registered() const;

    Q_INVOKABLE bool registerForStepFiles();
    Q_INVOKABLE bool unregisterForStepFiles();
    Q_INVOKABLE void openDefaultAppsSettings() const;

signals:
    void registeredChanged();
};

} // namespace loupe::app::platform
