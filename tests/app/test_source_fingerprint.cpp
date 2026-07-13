#include <QtTest/QTest>

#include "app/cache/SourceFingerprint.h"

#include <QFile>
#include <QTemporaryDir>

class SourceFingerprintTest final : public QObject
{
    Q_OBJECT

private slots:
    void contentChangesInvalidateFingerprint();
};

void SourceFingerprintTest::contentChangesInvalidateFingerprint()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("model.step"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("first"), 5LL);
    file.close();
    const auto first = loupe::app::cache::SourceFingerprint::fromFile(path);

    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write("second"), 6LL);
    file.close();
    const auto second = loupe::app::cache::SourceFingerprint::fromFile(path);

    QVERIFY(first.has_value());
    QVERIFY(second.has_value());
    QVERIFY(first->hash != second->hash);
    QCOMPARE(second->size, 6);
}

QTEST_MAIN(SourceFingerprintTest)

#include "test_source_fingerprint.moc"
