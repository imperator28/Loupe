#include <QtTest/QTest>

#include "app/render/PartPalette.h"

#include <algorithm>

using loupe::app::render::PartPaletteStyle;

class PartPaletteTest final : public QObject
{
    Q_OBJECT

private slots:
    void namesMapToStyles();
    void everyStyleStaysLowInContrast();
    void neighbouringPartsAreStillDistinguishable();
    void colorsAreReproducibleAndSeedsChangeThem();
    void shuffleReordersTheSamePartsReproducibly();
};

namespace {

constexpr int kSampleSize = 32;
constexpr PartPaletteStyle kStyles[] = {PartPaletteStyle::CoolShades, PartPaletteStyle::WarmShades, PartPaletteStyle::Muted};

} // namespace

void PartPaletteTest::namesMapToStyles()
{
    QCOMPARE(loupe::app::render::partPaletteStyleFromName(QStringLiteral("shades")), PartPaletteStyle::CoolShades);
    QCOMPARE(loupe::app::render::partPaletteStyleFromName(QStringLiteral("warm")), PartPaletteStyle::WarmShades);
    QCOMPARE(loupe::app::render::partPaletteStyleFromName(QStringLiteral("muted")), PartPaletteStyle::Muted);
    QVERIFY(!loupe::app::render::partPaletteStyleFromName(QStringLiteral("file")));
    QVERIFY(!loupe::app::render::partPaletteStyleFromName(QString{}));
}

void PartPaletteTest::everyStyleStaysLowInContrast()
{
    // The request was colours that tell parts apart without the high contrast that makes a
    // dense assembly hard to read. Pin it: no colour far from mid lightness, and no two
    // colours in a style further apart than a modest OKLab distance. For scale, a vivid
    // categorical palette (pure red against pure blue) is about 0.54 apart.
    for (const auto style : kStyles) {
        double farthest = 0.0;
        for (int first = 0; first < kSampleSize; ++first) {
            const auto color = loupe::app::render::partPaletteColor(style, first, 0);
            QVERIFY(color.isValid());
            const auto lightness = loupe::app::render::oklabLightness(color);
            QVERIFY2(lightness >= 0.58 && lightness <= 0.88, qPrintable(color.name()));
            for (int second = first + 1; second < kSampleSize; ++second) {
                farthest = std::max(farthest, loupe::app::render::oklabDistance(
                    color, loupe::app::render::partPaletteColor(style, second, 0)));
            }
        }
        QVERIFY2(farthest <= 0.27, qPrintable(QString::number(farthest)));
    }
    QVERIFY(loupe::app::render::oklabDistance(QColor(Qt::red), QColor(Qt::blue)) > 0.5);
}

void PartPaletteTest::neighbouringPartsAreStillDistinguishable()
{
    // Consecutive parts in the tree are often adjacent in the model, so they must not
    // collapse onto the same shade.
    for (const auto style : kStyles) {
        for (int position = 0; position + 1 < kSampleSize; ++position) {
            const auto distance = loupe::app::render::oklabDistance(
                loupe::app::render::partPaletteColor(style, position, 0),
                loupe::app::render::partPaletteColor(style, position + 1, 0));
            QVERIFY2(distance >= 0.04, qPrintable(QStringLiteral("position %1: %2").arg(position).arg(distance)));
        }
    }
}

void PartPaletteTest::colorsAreReproducibleAndSeedsChangeThem()
{
    for (const auto style : kStyles) {
        QCOMPARE(loupe::app::render::partPaletteColor(style, 7, 0), loupe::app::render::partPaletteColor(style, 7, 0));
        QCOMPARE(loupe::app::render::partPaletteColor(style, 7, 12345), loupe::app::render::partPaletteColor(style, 7, 12345));
        QVERIFY(loupe::app::render::partPaletteColor(style, 7, 0) != loupe::app::render::partPaletteColor(style, 7, 12345));
    }
}

void PartPaletteTest::shuffleReordersTheSamePartsReproducibly()
{
    QStringList keys;
    for (int index = 0; index < 20; ++index) keys.append(QStringLiteral("definition-%1").arg(index));
    QCOMPARE(loupe::app::render::partPaletteOrder(keys, 0), keys);

    const auto shuffled = loupe::app::render::partPaletteOrder(keys, 99);
    QVERIFY(shuffled != keys);
    QCOMPARE(loupe::app::render::partPaletteOrder(keys, 99), shuffled);
    auto sortedShuffle = shuffled;
    auto sortedKeys = keys;
    sortedShuffle.sort();
    sortedKeys.sort();
    QCOMPARE(sortedShuffle, sortedKeys);
}

QTEST_GUILESS_MAIN(PartPaletteTest)
#include "test_part_palette.moc"
