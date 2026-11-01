#pragma once

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QUrl>

#include <functional>

class QLocalServer;

namespace loupe::app::platform {

// Gets STEP files the operating system asks Loupe to open into the window, whichever way
// they arrive (#8):
//   * paths on the command line, which is how Windows "Open with" and a default-app
//     double-click launch an app;
//   * a QFileOpenEvent, which is how macOS Finder delivers them, possibly before the
//     window exists;
//   * a second launch while Loupe is already running, which hands its files to the
//     running copy and exits, so they open as new tabs instead of a second window.
class FileOpenRouter final : public QObject {
    Q_OBJECT

public:
    explicit FileOpenRouter(QObject* parent = nullptr);
    ~FileOpenRouter() override;

    // STEP files among command-line arguments (options and other files ignored).
    [[nodiscard]] static QStringList stepFilesIn(const QStringList& arguments);
    // Send files to a running Loupe. True if one took them.
    [[nodiscard]] static bool forwardToRunningInstance(const QStringList& files);

    // Accept files from later launches.
    void listenForOtherInstances();
    // Where files go: the window's openStepFile(url). Files that arrived before the window
    // existed are delivered now.
    void setWindow(QObject* window);
    // With several windows, the one to use at each delivery (the one used last); the
    // window set above is the fallback.
    void setWindowProvider(std::function<QObject*()> provider);
    void open(const QStringList& files);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void deliver(const QUrl& file);
    [[nodiscard]] static QString serverName();

    QPointer<QObject> window_;
    std::function<QObject*()> windowProvider_;
    QList<QUrl> pending_;
    QLocalServer* server_{nullptr};
};

} // namespace loupe::app::platform
