#pragma once

#include <cstdint>

namespace loupe::app::models {

// Remaining time for a long job, from (elapsed, fraction done) reports.
//
// Recomputing elapsed * (1 - p) / p on every one-second tick, as the import panel used to,
// makes the estimate grow whenever progress pauses: with p fixed and elapsed rising, the
// remaining time rises with it, so "16 s" became "17 s" two seconds later. This keeps a
// projected finish time instead, smoothed over reports, and counts down toward it between
// them. The countdown never ticks back up for a small revision: it holds until the time
// catches up. A large revision still shows at once, so a wrong estimate cannot stick.
class RemainingTimeEstimator final {
public:
    void reset() noexcept;

    // Record that `fraction` of the work is done after `elapsedMs`.
    void report(std::int64_t elapsedMs, double fraction) noexcept;

    // Whole seconds to show at `elapsedMs`: -1 while there is too little to go on, 0 once
    // the projected finish has passed with the work still running.
    [[nodiscard]] std::int64_t remainingSeconds(std::int64_t elapsedMs) const noexcept;

private:
    double finishMs_{-1.0};
    // What the countdown last showed. Reading it is what advances the countdown, so it is
    // state of the display rather than of the estimate.
    mutable std::int64_t shownSeconds_{-1};
};

} // namespace loupe::app::models
