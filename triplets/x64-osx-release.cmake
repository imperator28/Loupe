# CI/release triplet for Intel macOS: identical to arm64-osx-release except the
# architecture. Dependencies are built release-only and statically, so the
# resulting binaries are self-contained. Local developer builds keep the
# default dual-variant triplets.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES x86_64)

set(VCPKG_BUILD_TYPE release)
