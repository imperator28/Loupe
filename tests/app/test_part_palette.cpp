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
    void vividSeparatesNeighboursStrongly();
    void coolAndWarmSpreadInHueAndChromaNotJustLightness();
};

namespace {

constexpr int kSampleSize = 32;
constexpr PartPaletteStyle kStyles[] = {PartPaletteStyle::CoolShades, PartPaletteStyle::WarmShades, PartPaletteStyle::Muted};
constexpr PartPaletteStyle kAllStyles[] = {PartPaletteStyle::CoolShades, PartPaletteStyle::WarmShades, PartPaletteStyle::Muted,
                                           PartPaletteStyle::Vivid};

} // namespace

void PartPaletteTest::namesMapToStyles()
{
    QCOMPARE(loupe::app::render::partPaletteStyleFromName(QStringLiteral("shades")), PartPaletteStyle::CoolShades);
    QCOMPARE(loupe::app::render::partPaletteStyleFromName(QStringLiteral("warm")), PartPaletteStyle::WarmShades);
    QCOMPARE(loupe::app::render::partPaletteStyleFromName(QStringLiteral("muted")), PartPaletteStyle::Muted);
    QCOMPARE(loupe::app::render::partPaletteStyleFromName(QStringLiteral("vivid")), PartPaletteStyle::Vivid);
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
        // Cool and warm reach a little further, in hue, to tell parts apart on a lit model.
        const auto ceiling = style == PartPaletteStyle::Muted ? 0.27 : 0.32;
        QVERIFY2(farthest <= ceiling, qPrintable(QString::number(farthest)));
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
            const auto floor = style == PartPaletteStyle::Muted ? 0.04 : 0.06;
            QVERIFY2(distance >= floor, qPrintable(QStringLiteral("style %1 position %2: %3").arg(static_cast<int>(style)).arg(position).arg(distance)));
        }
    }
}

void PartPaletteTest::colorsAreReproducibleAndSeedsChangeThem()
{
    for (const auto style : kAllStyles) {
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

void PartPaletteTest::vividSeparatesNeighboursStrongly()
{
    // The high-contrast style exists to make adjacent parts unmistakable, so its neighbours
    // must sit well apart -- several times the low-contrast styles' floor -- and its colours
    // must actually be saturated, not quietly gamut-clipped back toward grey.
    double nearest = 1.0;
    for (int position = 0; position + 1 < kSampleSize; ++position) {
        nearest = std::min(nearest, loupe::app::render::oklabDistance(
            loupe::app::render::partPaletteColor(PartPaletteStyle::Vivid, position, 0),
            loupe::app::render::partPaletteColor(PartPaletteStyle::Vivid, position + 1, 0)));
    }
    QVERIFY2(nearest >= 0.15, qPrintable(QString::number(nearest)));
    const auto grey = QColor(QStringLiteral("#9AA2B2"));
    for (int position = 0; position < kSampleSize; ++position)
        QVERIFY(loupe::app::render::oklabDistance(loupe::app::render::partPaletteColor(PartPaletteStyle::Vivid, position, 0), grey) > 0.08);
}

void PartPaletteTest::coolAndWarmSpreadInHueAndChromaNotJustLightness()
{
    // On a lit model, lighting moves lightness across most of its range, so parts that
    // differ only in lightness read as one part in shade. The two families must stay in
    // their temperature yet differ in hue and chroma: every colour clearly tinted, the
    // family spanning a real band of hues, and its first dozen colours (the parts a
    // small assembly has) all clearly apart from one another.
    const auto chromaOf = [](const QColor& color) {
        const auto r = color.redF(), g = color.greenF(), b = color.blueF();
        const auto max = std::max({r, g, b}), min = std::min({r, g, b});
        return max - min;
    };
    for (const auto style : {PartPaletteStyle::CoolShades, PartPaletteStyle::WarmShades}) {
        double lowestHue = 360.0, highestHue = 0.0, closest = 1.0;
        for (int first = 0; first < 12; ++first) {
            const auto color = loupe::app::render::partPaletteColor(style, first, 0);
            QVERIFY2(chromaOf(color) > 0.07, qPrintable(color.name()));
            const auto hue = color.hsvHueF() * 360.0;
            lowestHue = std::min(lowestHue, hue);
            highestHue = std::max(highestHue, hue);
            for (int second = first + 1; second < 12; ++second)
                closest = std::min(closest, loupe::app::render::oklabDistance(color, loupe::app::render::partPaletteColor(style, second, 0)));
        }
        QVERIFY2(highestHue - lowestHue > 40.0, qPrintable(QStringLiteral("%1-%2").arg(lowestHue).arg(highestHue)));
        QVERIFY2(closest >= 0.07, qPrintable(QString::number(closest)));
        // In their temperature: cool colours have more blue than red, warm more red than blue.
        for (int position = 0; position < kSampleSize; ++position) {
            const auto color = loupe::app::render::partPaletteColor(style, position, 0);
            if (style == PartPaletteStyle::CoolShades) QVERIFY2(color.blueF() > color.redF(), qPrintable(color.name()));
            else QVERIFY2(color.redF() > color.blueF(), qPrintable(color.name()));
        }
    }
}

QTEST_GUILESS_MAIN(PartPaletteTest)
#include "test_part_palette.moc"
