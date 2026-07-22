# CI/release triplet: identical to the built-in x64-windows except
# dependencies are built release-only. See arm64-osx-release.cmake for the
# rationale.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

set(VCPKG_BUILD_TYPE release)
