#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QVariantList>
#include <QPointer>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <QVariantMap>

#include <optional>

namespace loupe::app::platform {

// Keeps Loupe up to date from its GitHub releases. Each launch it asks GitHub for the newest
// release, offers it when it is newer than this build, downloads the package for this
// platform with progress, and on request swaps it in place of this install and starts it.
//
// It talks to GitHub through the system's curl -- on every macOS and on Windows 10 and later
// -- so the certificates checked are the system's own and no TLS support has to be built
// into Qt. The new version is unpacked and run with --self-check before anything is
// replaced; the swap itself runs in a small script once this process has exited, and puts
// the old version back if the copy fails. Replacing the app in place keeps the user's
// settings, and a package downloaded this way carries no quarantine flag, so macOS does not
// ask again whether to open it.
class UpdateController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(bool supported READ supported CONSTANT)
    Q_PROPERTY(State state READ state NOTIFY changed)
    Q_PROPERTY(QString availableVersion READ availableVersion NOTIFY changed)
    Q_PROPERTY(QString releaseNotes READ releaseNotes NOTIFY changed)
    Q_PROPERTY(QString releaseUrl READ releaseUrl NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(qint64 bytesReceived READ bytesReceived NOTIFY progressChanged)
    Q_PROPERTY(qint64 bytesTotal READ bytesTotal NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(bool includePrereleases READ includePrereleases WRITE setIncludePrereleases NOTIFY includePrereleasesChanged)
    Q_PROPERTY(bool dismissed READ dismissed NOTIFY changed)
    // The last check came from Help > Check for Updates, so its result is worth showing
    // even when it is "up to date".
    Q_PROPERTY(bool userInitiated READ userInitiated NOTIFY changed)
    // What's New: this version's highlights (icon, title, body), and whether they are still
    // to be shown -- the first launch after an update, once per version.
    Q_PROPERTY(QVariantList whatsNewItems READ whatsNewItems CONSTANT)
    Q_PROPERTY(QString whatsNewVersion READ whatsNewVersion CONSTANT)
    Q_PROPERTY(bool whatsNewPending READ whatsNewPending NOTIFY changed)

public:
    enum class State { Idle, Checking, UpToDate, Available, Downloading, Ready, Installing, Failed };
    Q_ENUM(State)

    struct Version {
        int major{};
        int minor{};
        int patch{};
        QStringList prerelease; // "beta", "7" for -beta.7; empty for a release
    };

    struct Release {
        QString version;
        QString tag;
        QString assetUrl;
        qint64 assetSize{};
        QString notes;
        QString htmlUrl;
    };

    explicit UpdateController(QObject* parent = nullptr);
    ~UpdateController() override;

    // "v0.1.5-beta.7", "0.1.5": a version, or nothing for anything else (a branch name).
    [[nodiscard]] static std::optional<Version> parseVersion(const QString& text);
    // <0, 0, >0 as semantic versioning orders them: 0.1.5-beta.7 < 0.1.5-beta.10 < 0.1.5.
    [[nodiscard]] static int compareVersions(const Version& a, const Version& b);
    // The newest release in GitHub's releases JSON that is newer than `current` and carries a
    // package ending in `assetSuffix`; betas only when `includePrereleases`.
    [[nodiscard]] static std::optional<Release> newestRelease(const QByteArray& releasesJson, const QString& current,
                                                              bool includePrereleases, const QString& assetSuffix);
    // The package name ending for this build: "macos-arm64.zip", "windows-x64.zip", ...
    [[nodiscard]] static QString assetSuffixForThisBuild();
    // The script that swaps the new version in, for this platform.
    [[nodiscard]] static QString installScript();

    [[nodiscard]] QString currentVersion() const { return currentVersion_; }
    [[nodiscard]] bool supported() const noexcept;
    [[nodiscard]] State state() const noexcept { return state_; }
    [[nodiscard]] QString availableVersion() const { return release_ ? release_->version : QString{}; }
    [[nodiscard]] QString releaseNotes() const { return release_ ? release_->notes : QString{}; }
    [[nodiscard]] QString releaseUrl() const { return release_ ? release_->htmlUrl : QString{}; }
    [[nodiscard]] double progress() const noexcept;
    [[nodiscard]] qint64 bytesReceived() const noexcept { return bytesReceived_; }
    [[nodiscard]] qint64 bytesTotal() const noexcept { return release_ ? release_->assetSize : 0; }
    [[nodiscard]] QString message() const { return message_; }
    [[nodiscard]] bool includePrereleases() const noexcept { return includePrereleases_; }
    void setIncludePrereleases(bool include);
    [[nodiscard]] bool dismissed() const noexcept { return dismissed_; }
    [[nodiscard]] bool userInitiated() const noexcept { return userInitiated_; }

    // The app (not a test) turns this on: every controller then checks once, shortly
    // after it is made, which is each launch.
    static void setAutomaticChecks(bool enabled);

    // What's New. The highlights for a version ("0.1.5", or a beta of it) from the bundled
    // whats-new.json; empty when it has none.
    [[nodiscard]] static QVariantList whatsNewFor(const QByteArray& whatsNewJson, const QString& version);
    // "0.1.5" for "v0.1.5-beta.7": the release a build belongs to, which its notes are for.
    [[nodiscard]] static QString releaseVersionOf(const QString& version);
    // Whether to show them on this launch: there are some, and this release's were not
    // shown before.
    [[nodiscard]] static bool shouldShowWhatsNew(const QString& lastShown, const QString& release, bool hasItems);
    // The app turns this on, as for automatic checks; tests and the self-check leave it off.
    static void setShowWhatsNewOnLaunch(bool enabled);
    [[nodiscard]] QVariantList whatsNewItems() const { return whatsNewItems_; }
    [[nodiscard]] QString whatsNewVersion() const { return releaseVersionOf(currentVersion_); }
    [[nodiscard]] bool whatsNewPending() const noexcept { return whatsNewPending_; }
    // Shown: not again for this release.
    Q_INVOKABLE void markWhatsNewSeen();

    // Where GitHub is asked; a test points it at a file.
    void setReleasesUrl(const QString& url) { releasesUrl_ = url; }

    // userInitiated: say "up to date" or what went wrong, rather than staying quiet.
    Q_INVOKABLE void checkForUpdates(bool userInitiated = false);
    Q_INVOKABLE void download();
    Q_INVOKABLE void cancel();
    // Unpack, check, and hand over to the install script, then quit.
    Q_INVOKABLE void installAndRestart();
    // "Later": keep quiet about this version for the rest of the session.
    Q_INVOKABLE void dismiss();

signals:
    void changed();
    void progressChanged();
    void includePrereleasesChanged();
    // The app should close now; the install script waits for it.
    void quitRequested();

private:
    void setState(State state, const QString& message = {});
    void fail(const QString& message);
    void finishCheck(int exitCode, QProcess::ExitStatus status);
    void finishDownload(int exitCode, QProcess::ExitStatus status);
    void finishExtract(int exitCode, QProcess::ExitStatus status);
    void finishSelfCheck(int exitCode, QProcess::ExitStatus status);
    [[nodiscard]] QString installPath() const;
    [[nodiscard]] QString workDirectory() const;
    [[nodiscard]] QString findNewInstall() const;
    void stopProcess();

    QString currentVersion_;
    QString releasesUrl_;
    State state_{State::Idle};
    std::optional<Release> release_;
    QString message_;
    bool includePrereleases_{false};
    bool userInitiated_{false};
    QVariantList whatsNewItems_;
    bool whatsNewPending_{false};
    bool dismissed_{false};
    qint64 bytesReceived_{};
    QString packagePath_;
    QString newInstallPath_;
    QPointer<QProcess> process_;
    QTimer progressTimer_;
};

} // namespace loupe::app::platform
