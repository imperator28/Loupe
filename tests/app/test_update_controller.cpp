#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <QFile>
#include <QTemporaryDir>
#include <QUrl>

#include "app/platform/UpdateController.h"

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
                   {"name":"loupe-v0.1.6-macos-arm64.zip","browser_download_url":"https://example/mac.zip","size":200}]},
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

QTEST_GUILESS_MAIN(UpdateControllerTest)

#include "test_update_controller.moc"
