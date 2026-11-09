#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QScopeGuard>
#include <QElapsedTimer>

#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QUrl>
#include <QProcessEnvironment>
#include <QProcess>
#include <QStandardPaths>

#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "app/platform/UpdateController.h"
#include "app/platform/TempHygiene.h"

using loupe::app::platform::UpdateController;

class UpdateControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void versionsParseFromTagsAndLabels();
    void versionsOrderAsSemanticVersioning();
    void theNewestNewerReleaseForThisPlatformIsOffered();
    void betasAreOfferedOnlyWhenAskedFor();
    void aCheckAgainstAReleaseListReportsWhatItFound();
    void theBuildKnowsItsBetaLabel();
    void aSwapThatCannotCopyAsksForAnAdministratorAndSaysSo();
    void anUpdateThatCouldNotBeInstalledSaysWhyInsteadOfOfferingItAgain();
#ifdef Q_OS_WIN
    void theWindowsSwapLeavesAResultWhenTheFilesCannotBeReplaced();
    void theWindowsSwapAsksForAnAdministratorAtOnceWhenTheFolderIsClosedToTheUser();
#endif
    void whatsNewIsReadForTheReleaseAndShownOncePerRelease();
    void whatsNewListsEveryReleaseSinceTheOneUpdatedFrom();
    void everyReleaseShipsWhatsNewWithIconsThatExist();
    void launchSweepsTheTempFilesOfLoupesThatAreGone();
};

namespace {

int compare(const char* a, const char* b)
{
    return UpdateController::compareVersions(*UpdateController::parseVersion(QString::fromLatin1(a)),
                                             *UpdateController::parseVersion(QString::fromLatin1(b)));
}

QByteArray releases()
{
    return QByteArrayLiteral(R"([
        {"tag_name":"v0.1.6","draft":false,"prerelease":false,"body":"Final notes","html_url":"https://example/v0.1.6",
         "assets":[{"name":"loupe-v0.1.6-windows-x64.zip","browser_download_url":"https://example/win.zip","size":100},
                   {"name":"loupe-v0.1.6-macos-arm64.zip","browser_download_url":"https://example/mac.zip","size":200},
                   {"name":"loupe-v0.1.6-macos-x64.zip","browser_download_url":"https://example/mac-intel.zip","size":250}]},
        {"tag_name":"v0.1.7-beta.1","draft":false,"prerelease":true,"body":"Beta notes","html_url":"https://example/beta",
         "assets":[{"name":"loupe-v0.1.7-beta.1-windows-x64.zip","browser_download_url":"https://example/beta-win.zip","size":300}]},
        {"tag_name":"v0.2.0","draft":true,"prerelease":false,"body":"","html_url":"",
         "assets":[{"name":"loupe-v0.2.0-windows-x64.zip","browser_download_url":"https://example/draft.zip","size":1}]},
        {"tag_name":"v0.1.5","draft":false,"prerelease":false,"body":"","html_url":"",
         "assets":[{"name":"loupe-v0.1.5-windows-x64.zip","browser_download_url":"https://example/old.zip","size":1}]},
        {"tag_name":"nightly","draft":false,"prerelease":true,"body":"","html_url":"","assets":[]}
    ])");
}

} // namespace

void UpdateControllerTest::versionsParseFromTagsAndLabels()
{
    const auto beta = UpdateController::parseVersion(QStringLiteral("v0.1.5-beta.7"));
    QVERIFY(beta.has_value());
    QCOMPARE(beta->major, 0);
    QCOMPARE(beta->minor, 1);
    QCOMPARE(beta->patch, 5);
    QCOMPARE(beta->prerelease, (QStringList{QStringLiteral("beta"), QStringLiteral("7")}));
    QVERIFY(UpdateController::parseVersion(QStringLiteral("1.2.3"))->prerelease.isEmpty());
    QVERIFY(!UpdateController::parseVersion(QStringLiteral("claude/keen-tesla")).has_value());
    QVERIFY(!UpdateController::parseVersion(QStringLiteral("v1.2")).has_value());
}

void UpdateControllerTest::versionsOrderAsSemanticVersioning()
{
    QVERIFY(compare("0.1.5-beta.7", "0.1.5-beta.10") < 0); // numerically, not as text
    QVERIFY(compare("0.1.5-beta.10", "0.1.5") < 0);         // a release outranks its betas
    QVERIFY(compare("0.1.5", "0.1.6-beta.1") < 0);
    QVERIFY(compare("0.2.0", "0.10.0") < 0);
    QVERIFY(compare("0.1.5-alpha", "0.1.5-beta") < 0);
    QVERIFY(compare("0.1.5-beta", "0.1.5-beta.1") < 0);
    QCOMPARE(compare("v1.0.0", "1.0.0"), 0);
}

void UpdateControllerTest::theNewestNewerReleaseForThisPlatformIsOffered()
{
    const auto release = UpdateController::newestRelease(releases(), QStringLiteral("0.1.5"), false, QStringLiteral("windows-x64.zip"));
    QVERIFY(release.has_value());
    QCOMPARE(release->version, QStringLiteral("0.1.6"));
    QCOMPARE(release->assetUrl, QStringLiteral("https://example/win.zip"));
    QCOMPARE(release->assetSize, 100);
    QCOMPARE(release->notes, QStringLiteral("Final notes"));
    // The macOS package for a Mac, and nothing for a platform no release builds for.
    QCOMPARE(UpdateController::newestRelease(releases(), QStringLiteral("0.1.5"), false, QStringLiteral("macos-arm64.zip"))->assetUrl,
             QStringLiteral("https://example/mac.zip"));
    QCOMPARE(UpdateController::newestRelease(releases(), QStringLiteral("0.1.5"), false, QStringLiteral("macos-x64.zip"))->assetUrl,
             QStringLiteral("https://example/mac-intel.zip"));
    QVERIFY(!UpdateController::newestRelease(releases(), QStringLiteral("0.1.5"), false, QStringLiteral("linux.zip")).has_value());
    // Already the newest: nothing to offer. Drafts never count.
    QVERIFY(!UpdateController::newestRelease(releases(), QStringLiteral("0.1.6"), false, QStringLiteral("windows-x64.zip")).has_value());
    QVERIFY(!UpdateController::newestRelease("not json", QStringLiteral("0.1.5"), false, QStringLiteral("windows-x64.zip")).has_value());
}

void UpdateControllerTest::betasAreOfferedOnlyWhenAskedFor()
{
    const auto beta = UpdateController::newestRelease(releases(), QStringLiteral("0.1.5"), true, QStringLiteral("windows-x64.zip"));
    QVERIFY(beta.has_value());
    QCOMPARE(beta->version, QStringLiteral("0.1.7-beta.1"));
    // Someone on a beta of 0.1.6 gets the 0.1.6 release.
    QCOMPARE(UpdateController::newestRelease(releases(), QStringLiteral("0.1.6-beta.3"), false, QStringLiteral("windows-x64.zip"))->version,
             QStringLiteral("0.1.6"));
}

void UpdateControllerTest::aCheckAgainstAReleaseListReportsWhatItFound()
{
    QTemporaryDir directory;
    const auto path = directory.filePath(QStringLiteral("releases.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(releases());
    file.close();

    UpdateController updates;
    updates.setIncludePrereleases(false);
    updates.setReleasesUrl(QUrl::fromLocalFile(path).toString());
    QSignalSpy changed(&updates, &UpdateController::changed);
    updates.checkForUpdates(true);
    QCOMPARE(updates.state(), UpdateController::State::Checking);
    if (UpdateController::assetSuffixForThisBuild().isEmpty()) {
        QTRY_COMPARE(updates.state(), UpdateController::State::UpToDate);
        return;
    }
    QTRY_COMPARE(updates.state(), UpdateController::State::Available);
    QCOMPARE(updates.availableVersion(), QStringLiteral("0.1.6"));
    QVERIFY(updates.bytesTotal() > 0);
    QVERIFY(!updates.dismissed());
    updates.dismiss();
    QVERIFY(updates.dismissed());
}

void UpdateControllerTest::theBuildKnowsItsBetaLabel()
{
    UpdateController updates;
    QCOMPARE(updates.currentVersion(), QStringLiteral("0.1.5-beta.6"));
    // The swap copies the new version over the old, and puts the old back if it cannot.
#ifdef Q_OS_WIN
    QVERIFY(UpdateController::installScript().contains(QStringLiteral("robocopy")));
#else
    const auto script = UpdateController::installScript();
    QVERIFY(script.contains(QStringLiteral("ditto \"$new\" \"$old\"")));
    QVERIFY(script.contains(QStringLiteral("mv \"$backup\" \"$old\"")));
#endif
}

void UpdateControllerTest::aSwapThatCannotCopyAsksForAnAdministratorAndSaysSo()
{
    // A folder only an administrator may change (C:\Program Files) refused the copy every
    // time, and the old version was started again as if the update had worked.
    const auto windows = UpdateController::windowsInstallScript();
    QVERIFY(windows.contains(QStringLiteral("-Verb RunAs")));
    QVERIFY(windows.contains(QStringLiteral("Set-Content -Path $Result")));
    // Loupe itself starts again as the user, from the non-elevated script.
    QVERIFY(windows.contains(QStringLiteral("Start-Process -FilePath (Join-Path $Old 'Loupe.exe')")));
    QVERIFY(!windows.contains(QStringLiteral("-Verb RunAs -FilePath (Join-Path")));
    const auto unix = UpdateController::unixInstallScript();
    QVERIFY(unix.contains(QStringLiteral("> \"$result\"")));
}

void UpdateControllerTest::anUpdateThatCouldNotBeInstalledSaysWhyInsteadOfOfferingItAgain()
{
    if (UpdateController::assetSuffixForThisBuild().isEmpty()) QSKIP("No update packages for this platform");
    // What the install script leaves when it could not put 0.1.6 in place.
    const QString work = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath(QStringLiteral("loupe-update"));
    QDir().mkpath(work);
    const auto resultPath = QDir(work).filePath(QStringLiteral("install-result.txt"));
    {
        QFile result(resultPath);
        QVERIFY(result.open(QIODevice::WriteOnly | QIODevice::Text));
        // As Windows PowerShell writes it: UTF-8 with a byte-order mark.
        result.write("\xEF\xBB\xBF" "0.1.6\nC:\\Program Files\\Loupe\n16\n");
    }
    QTemporaryDir directory;
    const auto listPath = directory.filePath(QStringLiteral("releases.json"));
    {
        QFile list(listPath);
        QVERIFY(list.open(QIODevice::WriteOnly));
        list.write(releases());
    }
    UpdateController updates;
    QVERIFY(!QFile::exists(resultPath)); // read once, at launch
    updates.setIncludePrereleases(false);
    updates.setReleasesUrl(QUrl::fromLocalFile(listPath).toString());
    updates.checkForUpdates(false);
    QTRY_COMPARE(updates.state(), UpdateController::State::Failed);
    QVERIFY2(updates.message().contains(QStringLiteral("Program Files")), qPrintable(updates.message()));
    QCOMPARE(updates.availableVersion(), QStringLiteral("0.1.6"));
    // Once said, the next check offers it again, to try once more.
    updates.checkForUpdates(true);
    QTRY_COMPARE(updates.state(), UpdateController::State::Available);
}

#ifdef Q_OS_WIN
void UpdateControllerTest::theWindowsSwapLeavesAResultWhenTheFilesCannotBeReplaced()
{
    // The real script, against an install whose file is held open so it cannot be replaced,
    // as Program Files refuses a user: the copy fails, a result says so, and the old start runs.
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QDir base(root.path());
    QVERIFY(base.mkpath(QStringLiteral("old")) && base.mkpath(QStringLiteral("new")));
    const auto system = QDir(qEnvironmentVariable("SystemRoot", QStringLiteral("C:/Windows"))).filePath(QStringLiteral("System32"));
    QVERIFY(QFile::copy(QDir(system).filePath(QStringLiteral("whoami.exe")), base.filePath(QStringLiteral("old/Loupe.exe"))));
    QVERIFY(QFile::copy(QDir(system).filePath(QStringLiteral("hostname.exe")), base.filePath(QStringLiteral("new/Loupe.exe"))));
    // Different sizes: robocopy skips a file whose size and time match, and two files
    // written within one clock tick share a time -- the copy then never touches the lock.
    for (const auto& [name, content] : {std::pair{"old/part.dll", QByteArrayLiteral("old")},
                                        std::pair{"new/part.dll", QByteArrayLiteral("the new version")}}) {
        QFile part(base.filePath(QString::fromLatin1(name)));
        QVERIFY(part.open(QIODevice::WriteOnly));
        part.write(content);
    }
    // Shared with nothing: robocopy cannot write it.
    const auto lockedPath = QDir::toNativeSeparators(base.filePath(QStringLiteral("old/part.dll"))).toStdWString();
    const HANDLE locked = CreateFileW(lockedPath.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    QVERIFY(locked != INVALID_HANDLE_VALUE);

    const auto scriptPath = base.filePath(QStringLiteral("install.ps1"));
    {
        QFile script(scriptPath);
        QVERIFY(script.open(QIODevice::WriteOnly));
        script.write(UpdateController::windowsInstallScript().toUtf8());
    }
    const auto resultPath = base.filePath(QStringLiteral("result.txt"));
    QProcess powershell;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("LOUPE_UPDATE_NO_ELEVATION"), QStringLiteral("1"));
    powershell.setProcessEnvironment(environment);
    powershell.start(QStringLiteral("powershell.exe"),
                     {QStringLiteral("-NoProfile"), QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"), QStringLiteral("-File"),
                      QDir::toNativeSeparators(scriptPath), QStringLiteral("0"), QDir::toNativeSeparators(base.filePath(QStringLiteral("old"))),
                      QDir::toNativeSeparators(base.filePath(QStringLiteral("new"))), QDir::toNativeSeparators(base.filePath(QStringLiteral("install.log"))),
                      QDir::toNativeSeparators(resultPath), QStringLiteral("9.9.9")});
    QVERIFY(powershell.waitForFinished(180000));
    CloseHandle(locked);
    QFile result(resultPath);
    QVERIFY2(result.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(QString::fromLocal8Bit(powershell.readAll())));
    const auto lines = QString::fromUtf8(result.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QVERIFY(lines.size() >= 3);
    QCOMPARE(lines.at(0).trimmed().remove(QChar(0xFEFF)), QStringLiteral("9.9.9"));
    QVERIFY(lines.at(2).trimmed().toInt() >= 8);
}

void UpdateControllerTest::theWindowsSwapAsksForAnAdministratorAtOnceWhenTheFolderIsClosedToTheUser()
{
    // Program Files refuses every file to a user. The script used to try the copy as the user
    // first: robocopy spent two seconds of retries on each of the package's ~2500 files, five
    // times over, before asking for an administrator -- hours in a hidden window, so the update
    // looked as if it had not happened. A folder the user cannot write to now goes straight to
    // the administrator copy (here declined, by LOUPE_UPDATE_NO_ELEVATION), in seconds.
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QDir base(root.path());
    QVERIFY(base.mkpath(QStringLiteral("old")) && base.mkpath(QStringLiteral("new")));
    const auto system = QDir(qEnvironmentVariable("SystemRoot", QStringLiteral("C:/Windows"))).filePath(QStringLiteral("System32"));
    QVERIFY(QFile::copy(QDir(system).filePath(QStringLiteral("whoami.exe")), base.filePath(QStringLiteral("old/Loupe.exe"))));
    QVERIFY(QFile::copy(QDir(system).filePath(QStringLiteral("hostname.exe")), base.filePath(QStringLiteral("new/Loupe.exe"))));
    // Enough files that trying each as the user, as before, would outlast the limit below.
    for (int index = 0; index < 40; ++index) {
        QFile part(base.filePath(QStringLiteral("new/part%1.dll").arg(index)));
        QVERIFY(part.open(QIODevice::WriteOnly));
        part.write(QByteArray("new version ") + QByteArray::number(index));
    }
    const auto oldFolder = QDir::toNativeSeparators(base.filePath(QStringLiteral("old")));
    const auto user = qEnvironmentVariable("USERNAME");
    QVERIFY(!user.isEmpty());
    // Closed to this user, as Program Files is: a deny on writing, which holds even for an
    // administrator's account until it is elevated.
    QCOMPARE(QProcess::execute(QStringLiteral("icacls"), {oldFolder, QStringLiteral("/deny"), user + QStringLiteral(":(OI)(CI)(W)")}), 0);
    const auto restore = qScopeGuard([&] { QProcess::execute(QStringLiteral("icacls"), {oldFolder, QStringLiteral("/remove:d"), user}); });

    const auto scriptPath = base.filePath(QStringLiteral("install.ps1"));
    {
        QFile script(scriptPath);
        QVERIFY(script.open(QIODevice::WriteOnly));
        script.write(UpdateController::windowsInstallScript().toUtf8());
    }
    const auto resultPath = base.filePath(QStringLiteral("result.txt"));
    QProcess powershell;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("LOUPE_UPDATE_NO_ELEVATION"), QStringLiteral("1"));
    powershell.setProcessEnvironment(environment);
    QElapsedTimer clock;
    clock.start();
    powershell.start(QStringLiteral("powershell.exe"),
                     {QStringLiteral("-NoProfile"), QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"), QStringLiteral("-File"),
                      QDir::toNativeSeparators(scriptPath), QStringLiteral("0"), oldFolder,
                      QDir::toNativeSeparators(base.filePath(QStringLiteral("new"))), QDir::toNativeSeparators(base.filePath(QStringLiteral("install.log"))),
                      QDir::toNativeSeparators(resultPath), QStringLiteral("9.9.9")});
    QVERIFY(powershell.waitForFinished(180000));
    const auto elapsed = clock.elapsed();
    // Seconds, not the minutes of per-file retries the old order spent first.
    QVERIFY2(elapsed < 30000, qPrintable(QStringLiteral("took %1 ms").arg(elapsed)));
    QFile result(resultPath);
    QVERIFY2(result.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(QString::fromLocal8Bit(powershell.readAll())));
    const auto lines = QString::fromUtf8(result.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QVERIFY(lines.size() >= 3);
    QCOMPARE(lines.at(0).trimmed().remove(QChar(0xFEFF)), QStringLiteral("9.9.9"));
    QVERIFY(lines.at(2).trimmed().toInt() >= 8);
    // Nothing was half-copied into the folder the user could not write.
    QVERIFY(!QFile::exists(base.filePath(QStringLiteral("old/part0.dll"))));
}
#endif

void UpdateControllerTest::whatsNewIsReadForTheReleaseAndShownOncePerRelease()
{
    const auto json = QByteArrayLiteral(R"({"versions":[
        {"version":"0.1.5","items":[{"icon":"tabs","title":"Tabs","body":"Several files."},{"icon":"x","title":"","body":"no title"}]},
        {"version":"0.1.6","items":[{"icon":"explode","title":"Later","body":"Next."}]}]})");
    // A beta shares its release's notes; a title-less item is dropped.
    const auto items = UpdateController::whatsNewFor(json, QStringLiteral("v0.1.5-beta.11"));
    QCOMPARE(items.size(), 1);
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("title")).toString(), QStringLiteral("Tabs"));
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("icon")).toString(), QStringLiteral("tabs"));
    QVERIFY(UpdateController::whatsNewFor(json, QStringLiteral("0.1.7")).isEmpty());
    QVERIFY(UpdateController::whatsNewFor(json, QStringLiteral("main")).isEmpty());
    QCOMPARE(UpdateController::releaseVersionOf(QStringLiteral("v0.1.5-beta.11")), QStringLiteral("0.1.5"));

    // Once per release: first launch of it (fresh, or after an update), not again for its
    // betas or later launches, and never when there is nothing to show.
    QVERIFY(UpdateController::shouldShowWhatsNew({}, QStringLiteral("0.1.5"), true));
    QVERIFY(UpdateController::shouldShowWhatsNew(QStringLiteral("0.1.4"), QStringLiteral("0.1.5"), true));
    QVERIFY(!UpdateController::shouldShowWhatsNew(QStringLiteral("0.1.5"), QStringLiteral("0.1.5"), true));
    QVERIFY(!UpdateController::shouldShowWhatsNew(QStringLiteral("0.1.4"), QStringLiteral("0.1.5"), false));
}

void UpdateControllerTest::whatsNewListsEveryReleaseSinceTheOneUpdatedFrom()
{
    const auto json = QByteArrayLiteral(R"({"versions":[
        {"version":"0.1.3","items":[{"icon":"a","title":"Three","body":"."}]},
        {"version":"0.1.6","items":[{"icon":"a","title":"Six","body":"."}]},
        {"version":"0.1.4","items":[{"icon":"a","title":"Four","body":"."}]},
        {"version":"0.1.5","items":[{"icon":"a","title":"Five","body":"."}]},
        {"version":"0.1.7","items":[{"icon":"a","title":"Seven, not out yet","body":"."}]}]})");
    const auto versions = [](const QVariantList& releases) {
        QStringList result;
        for (const auto& release : releases) {
            const auto map = release.toMap();
            result << map.value(QStringLiteral("version")).toString() + (map.value(QStringLiteral("unseen")).toBool() ? QStringLiteral("*") : QString{});
        }
        return result;
    };
    // Updated from 0.1.4 to a 0.1.6 beta: 0.1.6 and 0.1.5 are new, newest first; nothing later
    // than this build; the earlier ones follow.
    auto releases = UpdateController::whatsNewReleasesFor(json, QStringLiteral("v0.1.6-beta.5"), QStringLiteral("0.1.4"));
    QCOMPARE(versions(releases), (QStringList{QStringLiteral("0.1.6*"), QStringLiteral("0.1.5*"), QStringLiteral("0.1.4"), QStringLiteral("0.1.3")}));
    QVERIFY(releases.front().toMap().value(QStringLiteral("current")).toBool());
    QCOMPARE(releases.front().toMap().value(QStringLiteral("items")).toList().size(), 1);
    // A fresh install: only this release is new, the one before it still listed.
    releases = UpdateController::whatsNewReleasesFor(json, QStringLiteral("0.1.6"), {}, 2);
    QCOMPARE(versions(releases), (QStringList{QStringLiteral("0.1.6*"), QStringLiteral("0.1.5")}));
    // Many releases skipped: every new one stays, past the limit, with the one before them.
    releases = UpdateController::whatsNewReleasesFor(json, QStringLiteral("0.1.6"), QStringLiteral("0.1.2"), 2);
    QCOMPARE(versions(releases), (QStringList{QStringLiteral("0.1.6*"), QStringLiteral("0.1.5*"), QStringLiteral("0.1.4*"), QStringLiteral("0.1.3*")}));
}

void UpdateControllerTest::everyReleaseShipsWhatsNewWithIconsThatExist()
{
    // The release being built must say what is new in it -- a version bump without a
    // whats-new.json entry fails here -- and every icon named must be one ToolIcon draws.
    QFile file(QStringLiteral(LOUPE_SOURCE_DIR "/src/app/resources/whats-new.json"));
    QVERIFY(file.open(QIODevice::ReadOnly));
    const auto items = UpdateController::whatsNewFor(file.readAll(), QStringLiteral(LOUPE_PROJECT_VERSION));
    QVERIFY2(!items.isEmpty(), "add this version's highlights to src/app/resources/whats-new.json");
    QVERIFY2(items.size() <= 7, "keep What's New to a few highlights");
    QFile icons(QStringLiteral(LOUPE_SOURCE_DIR "/src/app/qml/inspect/ToolIcon.qml"));
    QVERIFY(icons.open(QIODevice::ReadOnly));
    const auto glyphs = QString::fromUtf8(icons.readAll());
    for (const auto& item : items) {
        const auto fields = item.toMap();
        const auto icon = fields.value(QStringLiteral("icon")).toString();
        QVERIFY2(glyphs.contains(QStringLiteral("\"%1\": {").arg(icon)), qPrintable(icon));
        QVERIFY(!fields.value(QStringLiteral("body")).toString().isEmpty());
    }
}

void UpdateControllerTest::launchSweepsTheTempFilesOfLoupesThatAreGone()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto touch = [&](const QString& name) {
        QFile file(QDir(directory.path()).filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
    };
    touch(QStringLiteral("loupe-worker-111-0a1b2c3d"));   // a crashed Loupe's importer copy
    touch(QStringLiteral("loupe-111-deadbeef"));          // its connection
    touch(QStringLiteral("loupe-worker-222-0a1b2c3d"));   // another Loupe, still running
    touch(QStringLiteral("loupe-worker-333-0a1b2c3d"));   // this Loupe
    touch(QStringLiteral("loupe-update.txt"));            // not ours to judge
    touch(QStringLiteral("someone-else-111-0a1b2c3d"));
    const auto running = [](const qint64 pid) { return pid == 222; };
    const auto sweep = loupe::app::platform::sweepStaleTempFiles(directory.path(), 333, running);
    QCOMPARE(sweep.removed, 2);
    const auto left = QDir(directory.path()).entryList(QDir::Files);
    QVERIFY(!left.contains(QStringLiteral("loupe-worker-111-0a1b2c3d")));
    QVERIFY(!left.contains(QStringLiteral("loupe-111-deadbeef")));
    QVERIFY(left.contains(QStringLiteral("loupe-worker-222-0a1b2c3d")));
    QVERIFY(left.contains(QStringLiteral("loupe-worker-333-0a1b2c3d")));
    QVERIFY(left.contains(QStringLiteral("loupe-update.txt")));
    QVERIFY(left.contains(QStringLiteral("someone-else-111-0a1b2c3d")));
    // This process is running, by the real check.
    QVERIFY(loupe::app::platform::processIsRunning(QCoreApplication::applicationPid()));
}

QTEST_GUILESS_MAIN(UpdateControllerTest)

#include "test_update_controller.moc"
