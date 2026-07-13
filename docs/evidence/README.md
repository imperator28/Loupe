# Evidence artifacts

Committed evidence contains only generic case IDs, source hashes, toolchain metadata, and outcomes. Private CAD files, absolute source paths, geometry payloads, and generated exports remain ignored.

`platform/windows.json` is produced by `scripts/verify/windows.ps1`. `platform/macos.json` is created by `scripts/verify/macos.sh` once the Apple Silicon station is available.

Use [phase-1-template.md](phase-1-template.md) for the inspection gate. A passing Windows test suite is necessary but cannot close its macOS or geometry-integration rows.

For a clean Apple Silicon workstation, follow [the macOS kickoff guide](../development/macos-apple-silicon.md). It keeps the verification commands and evidence policy separate from Phase 3 signing/notarization work.
