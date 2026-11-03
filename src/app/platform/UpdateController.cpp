#include "app/platform/UpdateController.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUrl>

namespace loupe::app::platform {

namespace {

constexpr auto kReleasesUrl = "https://api.github.com/repos/imperator28/Loupe/releases?per_page=30";
constexpr auto kIncludePrereleasesKey = "updates/includePrereleases";
bool automaticChecks = false;

QString buildVersion()
{
#ifdef LOUPE_RELEASE_LABEL
    // A packaged build knows the tag or beta label it was built for; a dispatch without one
    // carries the branch name, which is no version.
    if (UpdateController::parseVersion(QStringLiteral(LOUPE_RELEASE_LABEL))) {
        auto label = QStringLiteral(LOUPE_RELEASE_LABEL);
        return label.startsWith(QLatin1Char('v')) ? label.mid(1) : label;
    }
#endif
#ifdef LOUPE_VERSION
    return QStringLiteral(LOUPE_VERSION);
#else
    return QStringLiteral("0.0.0");
#endif
}

QString curl()
{
#ifdef Q_OS_WIN
    // System32's, which every Windows 10 and later has, not one a PATH entry might shadow.
    const auto system = QDir(qEnvironmentVariable("SystemRoot", QStringLiteral("C:/Windows"))).filePath(QStringLiteral("System32/curl.exe"));
    return QFileInfo::exists(system) ? system : QStringLiteral("curl.exe");
#else
    return QFileInfo::exists(QStringLiteral("/usr/bin/curl")) ? QStringLiteral("/usr/bin/curl") : QStringLiteral("curl");
#endif
}

} // namespace

UpdateController::UpdateController(QObject* parent)
    : QObject(parent)
    , currentVersion_(buildVersion())
    , releasesUrl_(QString::fromLatin1(kReleasesUrl))
{
    const auto current = parseVersion(currentVersion_);
    const bool runningABeta = current && !current->prerelease.isEmpty();
    includePrereleases_ = QSettings().value(QString::fromLatin1(kIncludePrereleasesKey), runningABeta).toBool();
    progressTimer_.setInterval(200);
    connect(&progressTimer_, &QTimer::timeout, this, [this] {
        const auto size = QFileInfo(packagePath_).size();
        if (size != bytesReceived_) {
            bytesReceived_ = size;
            emit progressChanged();
        }
    });
    // A moment after launch, so opening the window and a file never waits on the network.
    if (automaticChecks && supported()) QTimer::singleShot(4000, this, [this] { checkForUpdates(false); });
}

void UpdateController::setAutomaticChecks(const bool enabled)
{
    automaticChecks = enabled;
}

UpdateController::~UpdateController()
{
    stopProcess();
}

std::optional<UpdateController::Version> UpdateController::parseVersion(const QString& text)
{
    static const QRegularExpression pattern(QStringLiteral(R"(^v?(\d+)\.(\d+)\.(\d+)(?:-([0-9A-Za-z.-]+))?$)"));
    const auto match = pattern.match(text.trimmed());
    if (!match.hasMatch()) return std::nullopt;
    Version version{match.captured(1).toInt(), match.captured(2).toInt(), match.captured(3).toInt(), {}};
    if (!match.captured(4).isEmpty()) version.prerelease = match.captured(4).split(QLatin1Char('.'));
    return version;
}

int UpdateController::compareVersions(const Version& a, const Version& b)
{
    for (const auto& [left, right] : {std::pair{a.major, b.major}, {a.minor, b.minor}, {a.patch, b.patch}}) {
        if (left != right) return left < right ? -1 : 1;
    }
    // A release outranks its own pre-releases.
    if (a.prerelease.isEmpty() || b.prerelease.isEmpty()) {
        if (a.prerelease.isEmpty() == b.prerelease.isEmpty()) return 0;
        return a.prerelease.isEmpty() ? 1 : -1;
    }
    for (qsizetype index = 0; index < std::min(a.prerelease.size(), b.prerelease.size()); ++index) {
        bool leftNumeric = false;
        bool rightNumeric = false;
        const auto left = a.prerelease.at(index).toLongLong(&leftNumeric);
        const auto right = b.prerelease.at(index).toLongLong(&rightNumeric);
        if (leftNumeric && rightNumeric) {
            if (left != right) return left < right ? -1 : 1;
        } else if (leftNumeric != rightNumeric) {
            return leftNumeric ? -1 : 1;
        } else if (const auto order = QString::compare(a.prerelease.at(index), b.prerelease.at(index)); order != 0) {
            return order < 0 ? -1 : 1;
        }
    }
    if (a.prerelease.size() == b.prerelease.size()) return 0;
    return a.prerelease.size() < b.prerelease.size() ? -1 : 1;
}

std::optional<UpdateController::Release> UpdateController::newestRelease(const QByteArray& releasesJson, const QString& current,
                                                                         const bool includePrereleases, const QString& assetSuffix)
{
    const auto currentVersion = parseVersion(current);
    if (!currentVersion || assetSuffix.isEmpty()) return std::nullopt;
    const auto document = QJsonDocument::fromJson(releasesJson);
    if (!document.isArray()) return std::nullopt;
    std::optional<Release> best;
    std::optional<Version> bestVersion;
    for (const auto& value : document.array()) {
        const auto release = value.toObject();
        if (release.value(QStringLiteral("draft")).toBool()) continue;
        if (release.value(QStringLiteral("prerelease")).toBool() && !includePrereleases) continue;
        const auto tag = release.value(QStringLiteral("tag_name")).toString();
        const auto version = parseVersion(tag);
        if (!version || compareVersions(*version, *currentVersion) <= 0) continue;
        if (!version->prerelease.isEmpty() && !includePrereleases) continue;
        if (bestVersion && compareVersions(*version, *bestVersion) <= 0) continue;
        for (const auto& assetValue : release.value(QStringLiteral("assets")).toArray()) {
            const auto asset = assetValue.toObject();
            if (!asset.value(QStringLiteral("name")).toString().endsWith(assetSuffix)) continue;
            best = Release{tag.startsWith(QLatin1Char('v')) ? tag.mid(1) : tag, tag,
                           asset.value(QStringLiteral("browser_download_url")).toString(),
                           static_cast<qint64>(asset.value(QStringLiteral("size")).toDouble()),
                           release.value(QStringLiteral("body")).toString(),
                           release.value(QStringLiteral("html_url")).toString()};
            bestVersion = version;
            break;
        }
    }
    return best;
}

QString UpdateController::assetSuffixForThisBuild()
{
#if defined(Q_OS_MACOS)
    return QSysInfo::buildCpuArchitecture() == QStringLiteral("arm64") ? QStringLiteral("macos-arm64.zip")
                                                                        : QStringLiteral("macos-x64.zip");
#elif defined(Q_OS_WIN)
    return QStringLiteral("windows-x64.zip");
#else
    return {};
#endif
}

QString UpdateController::installScript()
{
#if defined(Q_OS_WIN)
    // Waits for Loupe to exit, copies the new version over the old one (a few tries, while
    // Windows lets go of the files), and starts it. Settings live elsewhere and stay.
    return QStringLiteral(R"PS(param([int]$ProcessId, [string]$Old, [string]$New, [string]$Log)
Start-Transcript -Path $Log -Append | Out-Null
try { Wait-Process -Id $ProcessId -Timeout 60 -ErrorAction SilentlyContinue } catch {}
Start-Sleep -Milliseconds 400
$copied = $false
for ($attempt = 0; $attempt -lt 10 -and -not $copied; $attempt++) {
    robocopy $New $Old /E /R:2 /W:1 /NFL /NDL /NJH /NJS | Out-Null
    if ($LASTEXITCODE -lt 8) { $copied = $true } else { Start-Sleep -Seconds 1 }
}
Start-Process -FilePath (Join-Path $Old 'Loupe.exe')
Stop-Transcript | Out-Null
)PS");
#else
    // Waits for Loupe to exit, moves the old app aside, copies the new one into its place,
    // and opens it; if the copy fails, the old app goes back.
    return QStringLiteral(R"SH(#!/bin/bash
pid="$1"; old="$2"; new="$3"; log="$4"
exec >>"$log" 2>&1
for attempt in $(seq 1 300); do kill -0 "$pid" 2>/dev/null || break; sleep 0.2; done
backup="${old%.app}.previous.app"
rm -rf "$backup"
if mv "$old" "$backup"; then
    if ditto "$new" "$old"; then
        xattr -dr com.apple.quarantine "$old" 2>/dev/null
        rm -rf "$backup"
    else
        rm -rf "$old"
        mv "$backup" "$old"
    fi
fi
open "$old"
)SH");
#endif
}

bool UpdateController::supported() const noexcept
{
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    return true;
#else
    return false;
#endif
}

double UpdateController::progress() const noexcept
{
    const auto total = bytesTotal();
    return total > 0 ? std::clamp(static_cast<double>(bytesReceived_) / static_cast<double>(total), 0.0, 1.0) : 0.0;
}

void UpdateController::setIncludePrereleases(const bool include)
{
    if (include == includePrereleases_) return;
    includePrereleases_ = include;
    QSettings().setValue(QString::fromLatin1(kIncludePrereleasesKey), include);
    emit includePrereleasesChanged();
}

void UpdateController::setState(const State state, const QString& message)
{
    state_ = state;
    message_ = message;
    emit changed();
}

void UpdateController::fail(const QString& message)
{
    progressTimer_.stop();
    // A quiet check at launch that cannot reach GitHub says nothing: being offline is not
    // something to be told about every time the app opens.
    if (!userInitiated_ && state_ == State::Checking) {
        setState(State::Idle);
        return;
    }
    setState(State::Failed, message);
}

void UpdateController::stopProcess()
{
    if (!process_) return;
    process_->disconnect(this);
    if (process_->state() != QProcess::NotRunning) {
        process_->kill();
        process_->waitForFinished(2000);
    }
    process_->deleteLater();
    process_ = nullptr;
}

void UpdateController::checkForUpdates(const bool userInitiated)
{
    if (state_ == State::Checking || state_ == State::Downloading || state_ == State::Installing) return;
    userInitiated_ = userInitiated;
    if (userInitiated) dismissed_ = false;
    setState(State::Checking, tr("Checking for updates…"));
    const QUrl url(releasesUrl_);
    if (url.isLocalFile()) {
        QFile file(url.toLocalFile());
        if (!file.open(QIODevice::ReadOnly)) {
            fail(tr("Could not read the release list."));
            return;
        }
        const auto json = file.readAll();
        QMetaObject::invokeMethod(this, [this, json] {
            release_ = newestRelease(json, currentVersion_, includePrereleases_, assetSuffixForThisBuild());
            if (release_) setState(State::Available);
            else setState(State::UpToDate, tr("Loupe %1 is the latest version.").arg(currentVersion_));
        }, Qt::QueuedConnection);
        return;
    }
    stopProcess();
    process_ = new QProcess(this);
    connect(process_, &QProcess::finished, this, &UpdateController::finishCheck);
    connect(process_, &QProcess::errorOccurred, this, [this](const QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) fail(tr("Could not check for updates: curl is not available."));
    });
    process_->start(curl(), {QStringLiteral("-fsSL"), QStringLiteral("--max-time"), QStringLiteral("20"),
                             QStringLiteral("-H"), QStringLiteral("Accept: application/vnd.github+json"),
                             QStringLiteral("-H"), QStringLiteral("User-Agent: Loupe/%1").arg(currentVersion_), releasesUrl_});
}

void UpdateController::finishCheck(const int exitCode, const QProcess::ExitStatus status)
{
    if (!process_) return;
    const auto json = process_->readAllStandardOutput();
    process_->deleteLater();
    process_ = nullptr;
    if (status != QProcess::NormalExit || exitCode != 0) {
        fail(tr("Could not reach GitHub to check for updates."));
        return;
    }
    release_ = newestRelease(json, currentVersion_, includePrereleases_, assetSuffixForThisBuild());
    if (release_) setState(State::Available);
    else setState(State::UpToDate, tr("Loupe %1 is the latest version.").arg(currentVersion_));
}

QString UpdateController::workDirectory() const
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath(QStringLiteral("loupe-update"));
}

void UpdateController::download()
{
    if (!release_ || state_ == State::Downloading || state_ == State::Installing) return;
    stopProcess();
    QDir(workDirectory()).removeRecursively();
    QDir().mkpath(workDirectory());
    packagePath_ = QDir(workDirectory()).filePath(QStringLiteral("update.zip"));
    bytesReceived_ = 0;
    setState(State::Downloading, tr("Downloading Loupe %1…").arg(release_->version));
    emit progressChanged();
    process_ = new QProcess(this);
    connect(process_, &QProcess::finished, this, &UpdateController::finishDownload);
    connect(process_, &QProcess::errorOccurred, this, [this](const QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) fail(tr("Could not download the update: curl is not available."));
    });
    process_->start(curl(), {QStringLiteral("-fL"), QStringLiteral("--retry"), QStringLiteral("2"),
                             QStringLiteral("--silent"), QStringLiteral("--show-error"),
                             QStringLiteral("-H"), QStringLiteral("User-Agent: Loupe/%1").arg(currentVersion_),
                             QStringLiteral("-o"), packagePath_, release_->assetUrl});
    progressTimer_.start();
}

void UpdateController::finishDownload(const int exitCode, const QProcess::ExitStatus status)
{
    progressTimer_.stop();
    if (!process_) return;
    const auto error = QString::fromUtf8(process_->readAllStandardError()).trimmed();
    process_->deleteLater();
    process_ = nullptr;
    bytesReceived_ = QFileInfo(packagePath_).size();
    emit progressChanged();
    if (status != QProcess::NormalExit || exitCode != 0) {
        fail(error.isEmpty() ? tr("The download did not finish.") : tr("The download did not finish: %1").arg(error));
        return;
    }
    if (release_ && release_->assetSize > 0 && bytesReceived_ != release_->assetSize) {
        fail(tr("The download is incomplete. Try again."));
        return;
    }
    setState(State::Ready, tr("Loupe %1 is ready. Restart to finish updating.").arg(release_->version));
}

void UpdateController::cancel()
{
    if (state_ != State::Downloading && state_ != State::Installing) return;
    stopProcess();
    progressTimer_.stop();
    setState(release_ ? State::Available : State::Idle);
}

void UpdateController::dismiss()
{
    dismissed_ = true;
    emit changed();
}

QString UpdateController::installPath() const
{
    QDir directory(QCoreApplication::applicationDirPath());
#if defined(Q_OS_MACOS)
    // .../Loupe.app/Contents/MacOS
    directory.cdUp();
    directory.cdUp();
#endif
    return directory.absolutePath();
}

QString UpdateController::findNewInstall() const
{
    const auto extracted = QDir(workDirectory()).filePath(QStringLiteral("extracted"));
#if defined(Q_OS_MACOS)
    QDirIterator it(extracted, {QStringLiteral("*.app")}, QDir::Dirs, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const auto path = it.next();
        if (QFileInfo::exists(QDir(path).filePath(QStringLiteral("Contents/MacOS/Loupe")))) return path;
    }
#else
    QDirIterator it(extracted, {QStringLiteral("Loupe.exe")}, QDir::Files, QDirIterator::Subdirectories);
    if (it.hasNext()) return QFileInfo(it.next()).absolutePath();
#endif
    return {};
}

void UpdateController::installAndRestart()
{
    if (state_ != State::Ready || !supported()) return;
    const auto target = installPath();
#if defined(Q_OS_MACOS)
    if (!target.endsWith(QStringLiteral(".app"))) {
        fail(tr("Loupe is not running from an app bundle, so it cannot replace itself."));
        return;
    }
    if (target.contains(QStringLiteral("/AppTranslocation/"))) {
        // macOS runs a quarantined app from a hidden read-only copy; it cannot be replaced.
        fail(tr("Move Loupe into Applications (or any folder) and open it from there, then update."));
        return;
    }
    const auto writableDirectory = QFileInfo(target).absolutePath();
#else
    const auto writableDirectory = target;
#endif
    if (!QFileInfo(writableDirectory).isWritable()) {
        fail(tr("Loupe cannot write to the folder it is in (%1). Download the update from the release page instead.")
                 .arg(QDir::toNativeSeparators(writableDirectory)));
        return;
    }
    const auto extracted = QDir(workDirectory()).filePath(QStringLiteral("extracted"));
    QDir(extracted).removeRecursively();
    QDir().mkpath(extracted);
    setState(State::Installing, tr("Preparing the update…"));
    stopProcess();
    process_ = new QProcess(this);
    connect(process_, &QProcess::finished, this, &UpdateController::finishExtract);
#if defined(Q_OS_MACOS)
    process_->start(QStringLiteral("/usr/bin/ditto"), {QStringLiteral("-x"), QStringLiteral("-k"), packagePath_, extracted});
#else
    const auto tar = QDir(qEnvironmentVariable("SystemRoot", QStringLiteral("C:/Windows"))).filePath(QStringLiteral("System32/tar.exe"));
    process_->start(QFileInfo::exists(tar) ? tar : QStringLiteral("tar"), {QStringLiteral("-xf"), packagePath_, QStringLiteral("-C"), extracted});
#endif
}

void UpdateController::finishExtract(const int exitCode, const QProcess::ExitStatus status)
{
    if (!process_) return;
    process_->deleteLater();
    process_ = nullptr;
    newInstallPath_ = findNewInstall();
    if (status != QProcess::NormalExit || exitCode != 0 || newInstallPath_.isEmpty()) {
        fail(tr("The downloaded update could not be unpacked."));
        return;
    }
    // Start the new version once, as CI does, before replacing anything with it.
#if defined(Q_OS_MACOS)
    const auto executable = QDir(newInstallPath_).filePath(QStringLiteral("Contents/MacOS/Loupe"));
#else
    const auto executable = QDir(newInstallPath_).filePath(QStringLiteral("Loupe.exe"));
#endif
    process_ = new QProcess(this);
    connect(process_, &QProcess::finished, this, &UpdateController::finishSelfCheck);
    connect(process_, &QProcess::errorOccurred, this, [this](const QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) fail(tr("The downloaded update would not start."));
    });
    process_->start(executable, {QStringLiteral("--self-check")});
}

void UpdateController::finishSelfCheck(const int exitCode, const QProcess::ExitStatus status)
{
    if (!process_) return;
    process_->deleteLater();
    process_ = nullptr;
    if (status != QProcess::NormalExit || exitCode != 0) {
        fail(tr("The downloaded update did not pass its start-up check, so nothing was replaced."));
        return;
    }
#if defined(Q_OS_WIN)
    const auto scriptPath = QDir(workDirectory()).filePath(QStringLiteral("install.ps1"));
#else
    const auto scriptPath = QDir(workDirectory()).filePath(QStringLiteral("install.sh"));
#endif
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fail(tr("Could not prepare the update."));
        return;
    }
    script.write(installScript().toUtf8());
    script.close();
    const auto log = QDir(workDirectory()).filePath(QStringLiteral("install.log"));
    const auto pid = QString::number(QCoreApplication::applicationPid());
    bool started = false;
#if defined(Q_OS_WIN)
    started = QProcess::startDetached(QStringLiteral("powershell.exe"),
                                      {QStringLiteral("-NoProfile"), QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                                       QStringLiteral("-WindowStyle"), QStringLiteral("Hidden"), QStringLiteral("-File"), scriptPath,
                                       pid, QDir::toNativeSeparators(installPath()), QDir::toNativeSeparators(newInstallPath_),
                                       QDir::toNativeSeparators(log)});
#else
    started = QProcess::startDetached(QStringLiteral("/bin/bash"), {scriptPath, pid, installPath(), newInstallPath_, log});
#endif
    if (!started) {
        fail(tr("Could not start the update."));
        return;
    }
    setState(State::Installing, tr("Restarting into Loupe %1…").arg(release_ ? release_->version : QString{}));
    emit quitRequested();
}

} // namespace loupe::app::platform
