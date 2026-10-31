#include "app/models/RemainingTimeEstimator.h"

#include <algorithm>
#include <cmath>

namespace loupe::app::models {
namespace {

// Below these an extrapolation says more about start-up costs than about the job.
constexpr double kMinimumFraction = 0.05;
constexpr std::int64_t kMinimumElapsedMs = 1'500;
// How far one report moves the projected finish toward its own projection.
constexpr double kRevisionWeight = 0.3;
// A rise smaller than this holds the countdown instead of showing.
constexpr std::int64_t kRiseThresholdSeconds = 3;
constexpr double kRiseThresholdShare = 0.25;

} // namespace

void RemainingTimeEstimator::reset() noexcept
{
    finishMs_ = -1.0;
    shownSeconds_ = -1;
}

void RemainingTimeEstimator::report(const std::int64_t elapsedMs, const double fraction) noexcept
{
    if (!std::isfinite(fraction) || fraction < kMinimumFraction || fraction >= 1.0 || elapsedMs < kMinimumElapsedMs) return;
    const auto projectedFinish = static_cast<double>(elapsedMs) / fraction;
    finishMs_ = finishMs_ < 0.0 ? projectedFinish : finishMs_ + kRevisionWeight * (projectedFinish - finishMs_);
}

std::int64_t RemainingTimeEstimator::remainingSeconds(const std::int64_t elapsedMs) const noexcept
{
    if (finishMs_ < 0.0) return -1;
    const auto remainingMs = finishMs_ - static_cast<double>(elapsedMs);
    const auto estimate = remainingMs <= 0.0 ? std::int64_t{0} : static_cast<std::int64_t>(std::ceil(remainingMs / 1'000.0));
    const auto riseThreshold = std::max(kRiseThresholdSeconds,
                                        static_cast<std::int64_t>(std::ceil(kRiseThresholdShare * static_cast<double>(shownSeconds_))));
    if (shownSeconds_ < 0 || estimate <= shownSeconds_ || estimate - shownSeconds_ >= riseThreshold) shownSeconds_ = estimate;
    return shownSeconds_;
}

} // namespace loupe::app::models
