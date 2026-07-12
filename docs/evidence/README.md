# Evidence artifacts

Committed evidence contains only generic case IDs, source hashes, toolchain metadata, and outcomes. Private CAD files, absolute source paths, geometry payloads, and generated exports remain ignored.

`platform/windows.json` is produced by `scripts/verify/windows.ps1`. `platform/macos.json` is created by `scripts/verify/macos.sh` once the Apple Silicon station is available.
