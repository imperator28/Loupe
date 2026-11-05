#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QUrl>

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
    void whatsNewIsReadForTheReleaseAndShownOncePerRelease();
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
