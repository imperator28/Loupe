#include <QtTest/QTest>

#include "app/models/RemainingTimeEstimator.h"

using loupe::app::models::RemainingTimeEstimator;

class RemainingTimeEstimatorTest final : public QObject
{
    Q_OBJECT

private slots:
    void staysQuietUntilThereIsSomethingToGoOn();
    void countsDownWhileProgressPauses();
    void smallUpwardRevisionsDoNotTickTheCountdownBackUp();
    void aBadEstimateStillGetsCorrected();
    void sitsAtZeroOncePastTheProjectedFinish();
    void steadyWorkConvergesOnTheTruth();
};

void RemainingTimeEstimatorTest::staysQuietUntilThereIsSomethingToGoOn()
{
    RemainingTimeEstimator estimator;
    QCOMPARE(estimator.remainingSeconds(0), -1);
    estimator.report(500, 0.30); // too early
    estimator.report(4'000, 0.02); // too little done
    QCOMPARE(estimator.remainingSeconds(4'000), -1);
    estimator.report(4'000, 0.25);
    QCOMPARE(estimator.remainingSeconds(4'000), 12);
    estimator.reset();
    QCOMPARE(estimator.remainingSeconds(4'000), -1);
}

void RemainingTimeEstimatorTest::countsDownWhileProgressPauses()
{
    // The reported bug: with progress stuck, the old elapsed * (1 - p) / p grew every
    // second, so 16 s became 17 s two seconds later.
    RemainingTimeEstimator estimator;
    estimator.report(4'000, 0.20);
    QCOMPARE(estimator.remainingSeconds(4'000), 16);
    QCOMPARE(estimator.remainingSeconds(5'000), 15);
    QCOMPARE(estimator.remainingSeconds(6'000), 14);
}

void RemainingTimeEstimatorTest::smallUpwardRevisionsDoNotTickTheCountdownBackUp()
{
    RemainingTimeEstimator estimator;
    estimator.report(4'000, 0.20); // finish projected at 20 s
    QCOMPARE(estimator.remainingSeconds(6'000), 14);
    estimator.report(6'000, 0.26); // projects 23 s: the estimate rises 0.9 s
    QCOMPARE(estimator.remainingSeconds(6'000), 14);
    // It holds there until the time catches up, then counts down again.
    QCOMPARE(estimator.remainingSeconds(7'000), 14);
    QCOMPARE(estimator.remainingSeconds(8'000), 13);
}

void RemainingTimeEstimatorTest::aBadEstimateStillGetsCorrected()
{
    RemainingTimeEstimator estimator;
    estimator.report(4'000, 0.40); // projects 10 s; the job really takes 40 s
    for (std::int64_t elapsed = 5'000; elapsed <= 20'000; elapsed += 500)
        estimator.report(elapsed, static_cast<double>(elapsed) / 40'000.0);
    const auto remaining = estimator.remainingSeconds(20'000);
    QVERIFY2(remaining >= 17 && remaining <= 21, qPrintable(QString::number(remaining)));
}

void RemainingTimeEstimatorTest::sitsAtZeroOncePastTheProjectedFinish()
{
    RemainingTimeEstimator estimator;
    estimator.report(4'000, 0.50);
    QCOMPARE(estimator.remainingSeconds(7'900), 1);
    QCOMPARE(estimator.remainingSeconds(9'000), 0);
}

void RemainingTimeEstimatorTest::steadyWorkConvergesOnTheTruth()
{
    // Reports every 200 ms from a 30 s job with some per-report noise. Once it starts, the
    // countdown is within a couple of seconds of the truth and never ticks up.
    RemainingTimeEstimator estimator;
    std::int64_t previous = -1;
    for (std::int64_t elapsed = 0; elapsed < 30'000; elapsed += 200) {
        const auto noise = static_cast<double>((elapsed / 200) % 3 - 1) * 0.004;
        estimator.report(elapsed, static_cast<double>(elapsed) / 30'000.0 + noise);
        const auto remaining = estimator.remainingSeconds(elapsed);
        if (remaining < 0) continue;
        const auto truth = static_cast<double>(30'000 - elapsed) / 1'000.0;
        QVERIFY2(std::abs(static_cast<double>(remaining) - truth) <= 2.5,
                 qPrintable(QStringLiteral("%1 s left, estimated %2").arg(truth).arg(remaining)));
        if (previous >= 0) QVERIFY2(remaining <= previous, qPrintable(QStringLiteral("rose to %1 at %2 ms").arg(remaining).arg(elapsed)));
        previous = remaining;
    }
}

QTEST_GUILESS_MAIN(RemainingTimeEstimatorTest)
#include "test_remaining_time_estimator.moc"
