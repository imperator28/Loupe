#pragma once

namespace loupe::app::platform {

// The first Direct3D device a process creates pays for loading and starting the GPU driver,
// a few hundred milliseconds that Qt Quick spends on its render thread before the window's
// first frame -- and it only starts once the window is shown, after the whole interface has
// been built. Creating a throwaway device on a background thread at launch does that work
// while the interface is built instead; Qt's own device, created next, finds the driver
// ready. Windows only, and only when Qt Quick will use Direct3D 11 on the default adapter.
void startGraphicsWarmup();
// Waits for the warm-up and releases its device; call once Qt Quick has its own.
void finishGraphicsWarmup();

} // namespace loupe::app::platform
