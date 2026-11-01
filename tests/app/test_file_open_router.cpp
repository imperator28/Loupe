#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <QGuiApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QLocalSocket>
#include <QTemporaryDir>

#include "app/platform/FileOpenRouter.h"

using loupe::app::platform::FileOpenRouter;

// Stands in for Main.qml's window: records what openStepFile receives.
class FakeWindow final : public QObject
{
    Q_OBJECT

public:
    QList<QUrl> opened;
    Q_INVOKABLE void openStepFile(const QVariant& file) { opened.append(file.toUrl()); }
};

class FileOpenRouterTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void picksStepFilesOutOfTheCommandLine();
    void filesArrivingBeforeTheWindowAreDeliveredOnceItExists();
    void aLaterLaunchHandsItsFilesToTheRunningCopy();

private:
    QTemporaryDir home_;
};

void FileOpenRouterTest::initTestCase()
{
    // The server name is per user (from the home folder). A home of its own keeps this test
    // off the name a Loupe running on the same machine is using.
    QVERIFY(home_.isValid());
    qputenv("HOME", home_.path().toLocal8Bit());
    qputenv("USERPROFILE", home_.path().toLocal8Bit());
}

void FileOpenRouterTest::picksStepFilesOutOfTheCommandLine()
{
    const auto files = FileOpenRouter::stepFilesIn({QStringLiteral("Loupe"), QStringLiteral("--self-check"),
                                                    QStringLiteral("/parts/a.STEP"), QStringLiteral("/parts/b.stp"),
                                                    QStringLiteral("/parts/notes.txt")});
    QCOMPARE(files, (QStringList{QStringLiteral("/parts/a.STEP"), QStringLiteral("/parts/b.stp")}));
    QVERIFY(FileOpenRouter::stepFilesIn({QStringLiteral("Loupe")}).isEmpty());
}

void FileOpenRouterTest::filesArrivingBeforeTheWindowAreDeliveredOnceItExists()
{
    // macOS sends Finder's open request as an event, possibly before the window is loaded.
    FileOpenRouter router;
    router.open({QStringLiteral("/parts/early.step"), QStringLiteral("/parts/readme.md")});
    FakeWindow window;
    router.setWindow(&window);
    QCOMPARE(window.opened, QList<QUrl>{QUrl::fromLocalFile(QStringLiteral("/parts/early.step"))});
    router.open({QStringLiteral("/parts/late.stp")});
    QCOMPARE(window.opened.size(), 2);
}

void FileOpenRouterTest::aLaterLaunchHandsItsFilesToTheRunningCopy()
{
    FileOpenRouter running;
    FakeWindow window;
    running.setWindow(&window);
    running.listenForOtherInstances();

    // A second copy that cannot listen must leave the running copy's name alone.
    FileOpenRouter second;
    second.listenForOtherInstances();

    // What forwardToRunningInstance sends, done asynchronously: the running copy's server
    // shares this test's event loop, so the blocking call would wait on itself.
    QLocalSocket client;
    client.connectToServer(QStringLiteral("loupe-open-%1").arg(QString::fromLatin1(
        QCryptographicHash::hash(QDir::homePath().toUtf8(), QCryptographicHash::Sha1).toHex().left(12))));
    QVERIFY(client.waitForConnected(2'000));
    client.write("/parts/forwarded.step\n");
    client.flush();
    QTRY_COMPARE_WITH_TIMEOUT(window.opened.size(), 1, 3'000);
    QCOMPARE(window.opened.first(), QUrl::fromLocalFile(QStringLiteral("/parts/forwarded.step")));
    QTRY_VERIFY_WITH_TIMEOUT(client.bytesAvailable() > 0 || client.waitForReadyRead(50), 3'000);
    QVERIFY(client.readAll().startsWith("ok"));
}

QTEST_MAIN(FileOpenRouterTest)
#include "test_file_open_router.moc"
