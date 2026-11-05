#include <QtTest/QTest>

#include <QCoreApplication>
#include <QSettings>
#include <QStandardPaths>

#include "app/models/SavedViews.h"

class SavedViewsTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void explodeLayoutsAreKeptPerFileAndCanBeSavedOver();
};

void SavedViewsTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("LoupeTests"));
    QCoreApplication::setApplicationName(QStringLiteral("saved-views-tests"));
}

void SavedViewsTest::explodeLayoutsAreKeptPerFileAndCanBeSavedOver()
{
    const auto document = QStringLiteral("/tmp/loupe-saved-views-test/part.step");
    QSettings().remove(loupe::app::models::SavedViews::storageKeyFor(document));

    loupe::app::models::SavedViews views;
    views.setDocument(document);
    QCOMPARE(views.nextName(QStringLiteral("layout")), QStringLiteral("Layout 1"));
    const QVariantMap first{{QStringLiteral("transforms"), QVariantMap{{QStringLiteral("a"), QVariantList{1, 0, 0, 5, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}}}}};
    const auto id = views.add(QStringLiteral("layout"), {}, QStringLiteral("1 part moved"), first);
    views.add(QStringLiteral("camera"), {}, QStringLiteral("Front"), {{QStringLiteral("x"), 1}});
    QCOMPARE(views.explodeLayouts().size(), 1);
    QCOMPARE(views.cameraViews().size(), 1);
    QCOMPARE(views.explodeLayouts().front().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Layout 1"));
    QCOMPARE(views.nextName(QStringLiteral("layout")), QStringLiteral("Layout 2"));

    const QVariantMap second{{QStringLiteral("transforms"), QVariantMap{{QStringLiteral("a"), QVariantList{1, 0, 0, 9, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}}}}};
    QVERIFY(views.replace(id, QStringLiteral("moved further"), second));
    QVERIFY(!views.replace(QStringLiteral("missing"), {}, second));

    // Through the settings and back, as the next time the file is opened.
    loupe::app::models::SavedViews reopened;
    reopened.setDocument(document);
    QCOMPARE(reopened.explodeLayouts().size(), 1);
    const auto entry = reopened.explodeLayouts().front().toMap();
    QCOMPARE(entry.value(QStringLiteral("summary")).toString(), QStringLiteral("moved further"));
    const auto stored = reopened.data(id).value(QStringLiteral("transforms")).toMap().value(QStringLiteral("a")).toList();
    QCOMPARE(stored.value(3).toDouble(), 9.0);
    QSettings().remove(loupe::app::models::SavedViews::storageKeyFor(document));
}

QTEST_GUILESS_MAIN(SavedViewsTest)
#include "test_saved_views.moc"
