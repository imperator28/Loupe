# CI/release triplet: identical to the built-in arm64-osx except dependencies
# are built release-only. A release pipeline never links the debug variants,
# which cost more than half of the dependency build time and ~9 GB of the
# ~9.6 GB installed tree. Local developer builds keep the default triplet.
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES arm64)

set(VCPKG_BUILD_TYPE release)
