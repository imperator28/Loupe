#pragma once

namespace loupe::app::platform {

// Opt-in timeline of a launch, for measuring cold start without a screen recording: set
// LOUPE_STARTUP_TRACE to a file path and each mark is appended to it as
// "<ms since the process was created> <label>". Off (one cached check) otherwise.
// Thread-safe: frames are marked from the render thread.
void startupMark(const char* label);
[[nodiscard]] bool startupTraceEnabled();

} // namespace loupe::app::platform
