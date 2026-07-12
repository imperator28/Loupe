# Cross-platform development contract

Use CMake presets and the platform bootstrap/verification scripts; do not commit build directories, vcpkg caches, IDE state, or private CAD corpus files. Keep source paths UTF-8, includes case-correct, and stable IDs deterministic across compilers.

Windows is the current evidence platform. The Apple Silicon presets are structural until the M2 Pro is available; after that, every phase gate requires Debug and Release verification on both platforms. Platform-specific file replacement, process launching, and system integration belong behind adapters rather than shared core code.
