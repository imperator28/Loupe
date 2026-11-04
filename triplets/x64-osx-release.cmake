# CI/release triplet for Intel macOS: identical to arm64-osx-release except the
# architecture. Dependencies are built release-only and statically, so the
# resulting binaries are self-contained. Local developer builds keep the
# default dual-variant triplets.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
# Built on a newer macOS than the oldest Loupe supports: without this the SDK's own
# version becomes the minimum, and the app would not open on macOS 14.
set(VCPKG_OSX_DEPLOYMENT_TARGET 14.0)
set(VCPKG_OSX_ARCHITECTURES x86_64)

set(VCPKG_BUILD_TYPE release)
