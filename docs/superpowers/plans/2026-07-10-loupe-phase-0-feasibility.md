# Loupe Phase 0 Feasibility and Contracts Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prove STEP assembly traversal, unit interpretation, selected export, read-back validation, and the two-workspace interaction contract against a private representative corpus.

**Architecture:** Build a Qt-independent C++ `loupe-core` around OCCT XDE/XCAF and expose immutable domain values through a headless `loupe-spike` CLI. Build the Inspect/Export experience as a separate browser prototype that consumes deterministic JSON snapshots shaped like the future worker protocol.

**Tech Stack:** C++23, CMake 3.28+, Ninja, vcpkg manifests, OCCT, nlohmann/json, xxHash, Catch2, Node.js, Vite, Playwright.

---

## Required Skill Preflight

- Invoke `qt-cmake-project` before Task 1 and keep its target-first CMake rules active through the native spike.
- Invoke `test-driven-development` before every implementation task.
- Invoke `frontend-design`, `web-design-guidelines`, `ui-ux-pro-max`, and `design-motion-principles` before Task 9.
- Invoke `verification-before-completion` before Task 10 reports the Phase 0 gate.

## File and Module Map

Create these ownership boundaries before feature work:

```text
CMakeLists.txt                         Root project and test registration
CMakePresets.json                     Reproducible Windows/macOS configure/build/test presets
vcpkg.json                            Native dependency manifest
cmake/LoupeWarnings.cmake             Warning policy shared by native targets
src/core/domain/AssemblyTypes.h        Stable UI/worker-facing value types only
src/core/domain/StableId.*             Deterministic content/path identity
src/core/units/UnitPolicy.*            Declared/effective unit evidence and overrides
src/core/import/StepImporter.*         OCCT file read, classification, XCAF traversal
src/core/export/ExportPlan.*           Immutable selected-output specification
src/core/export/StepExporter.*         Temporary-document STEP writer
src/core/export/StlExporter.*          Binary-millimeter STL writer
src/core/validation/OutputValidator.*  Read-back unit/body/bounds/placement checks
src/core/report/EvidenceWriter.*       Snapshot, validation, benchmark JSON/CSV
src/spike/main.cpp                     Headless commands and exit codes
tests/fixtures/FixtureFactory.*        Generated non-private OCCT fixtures
tests/core/*.cpp                       Unit and integration tests
corpus/README.md                       Private-corpus placement and safety rules
corpus/cases.example.json              Case metadata contract without private paths
prototype/                             Separate browser interaction prototype
docs/evidence/phase-0-template.md      Evidence-gate report structure
```

## Task 1: Bootstrap the Native Project and Test Harness

**Files:**
- Create: `CMakeLists.txt`
- Create: `CMakePresets.json`
- Create: `vcpkg.json`
- Create: `cmake/LoupeWarnings.cmake`
- Create: `src/core/CMakeLists.txt`
- Create: `src/spike/CMakeLists.txt`
- Create: `src/spike/main.cpp`
- Create: `tests/CMakeLists.txt`
- Create: `tests/core/test_bootstrap.cpp`

- [ ] **Step 0: Install and verify the Windows native prerequisites**

Run in an elevated PowerShell only after the user approves toolchain installation:

```powershell
winget install --id Microsoft.VisualStudio.2022.BuildTools --exact --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
winget install --id Kitware.CMake --exact
winget install --id Ninja-build.Ninja --exact
git clone https://github.com/microsoft/vcpkg "$env:USERPROFILE\vcpkg"
& "$env:USERPROFILE\vcpkg\bootstrap-vcpkg.bat" -disableMetrics
$env:VCPKG_ROOT = "$env:USERPROFILE\vcpkg"
cmake --version
ninja --version
& "$env:VCPKG_ROOT\vcpkg.exe" version
```

Expected: MSVC C++ tools, CMake 3.28 or newer, Ninja, and vcpkg report versions. Persist `VCPKG_ROOT` in the developer environment; do not commit a user-specific path.

- [ ] **Step 1: Write the failing bootstrap test**

```cpp
// tests/core/test_bootstrap.cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("loupe-core test harness starts", "[bootstrap]") {
    REQUIRE(2 + 2 == 4);
}
```

- [ ] **Step 2: Create the dependency manifest and root CMake project**

```json
// vcpkg.json
{
  "name": "loupe",
  "version-string": "0.0.1",
  "dependencies": [
    "catch2",
    "nlohmann-json",
    "opencascade",
    "xxhash"
  ]
}
```

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.28)
project(Loupe VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LOUPE_BUILD_TESTS "Build Loupe tests" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(LoupeWarnings)

find_package(OpenCASCADE CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(xxHash CONFIG REQUIRED)

add_subdirectory(src/core)
add_subdirectory(src/spike)

if(LOUPE_BUILD_TESTS)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()
```

```cmake
# src/core/CMakeLists.txt
add_library(loupe-core INTERFACE)
target_include_directories(loupe-core INTERFACE "${PROJECT_SOURCE_DIR}/src")
```

```cmake
# src/spike/CMakeLists.txt
add_executable(loupe-spike main.cpp)
target_link_libraries(loupe-spike PRIVATE loupe-core)
loupe_enable_warnings(loupe-spike)
```

```cpp
// src/spike/main.cpp
#include <iostream>

int main() {
    std::cout << "loupe-spike 0.0.1\n";
    return 0;
}
```

```cmake
# tests/CMakeLists.txt
find_package(Catch2 3 CONFIG REQUIRED)
add_executable(loupe-core-tests core/test_bootstrap.cpp)
target_link_libraries(loupe-core-tests PRIVATE loupe-core Catch2::Catch2WithMain)
loupe_enable_warnings(loupe-core-tests)
include(Catch)
catch_discover_tests(loupe-core-tests)
```

```json
// CMakePresets.json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "base",
      "hidden": true,
      "generator": "Ninja",
      "toolchainFile": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
      "cacheVariables": {
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",
        "LOUPE_BUILD_TESTS": "ON"
      }
    },
    {
      "name": "windows-debug",
      "inherits": "base",
      "binaryDir": "${sourceDir}/build/windows-debug",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug" }
    },
    {
      "name": "windows-release",
      "inherits": "base",
      "binaryDir": "${sourceDir}/build/windows-release",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Release" }
    }
  ],
  "buildPresets": [
    { "name": "windows-debug", "configurePreset": "windows-debug" },
    { "name": "windows-release", "configurePreset": "windows-release" }
  ],
  "testPresets": [
    { "name": "windows-debug", "configurePreset": "windows-debug" },
    { "name": "windows-release", "configurePreset": "windows-release" }
  ]
}
```

- [ ] **Step 3: Add strict but cross-platform warning settings**

```cmake
# cmake/LoupeWarnings.cmake
function(loupe_enable_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc)
  else()
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
    )
  endif()
endfunction()
```

- [ ] **Step 4: Configure, build, and run the bootstrap test**

Run:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug --output-on-failure
```

Expected: configure succeeds, `loupe-core-tests` builds, and one test passes.

- [ ] **Step 5: Commit the bootstrap**

```powershell
git add CMakeLists.txt CMakePresets.json vcpkg.json cmake src/core/CMakeLists.txt src/spike tests
git commit -m "build: bootstrap native Loupe spike"
```

## Task 2: Define Stable Assembly Contracts

**Files:**
- Create: `src/core/domain/AssemblyTypes.h`
- Create: `src/core/domain/StableId.h`
- Create: `src/core/domain/StableId.cpp`
- Create: `tests/core/test_stable_id.cpp`
- Modify: `src/core/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing deterministic-ID tests**

```cpp
// tests/core/test_stable_id.cpp
#include <catch2/catch_test_macros.hpp>
#include "core/domain/StableId.h"

using loupe::domain::stableId;

TEST_CASE("stable IDs repeat for identical importer paths", "[domain]") {
    REQUIRE(stableId("sha256:file", "/0/2/5", "occurrence") ==
            stableId("sha256:file", "/0/2/5", "occurrence"));
}

TEST_CASE("stable IDs distinguish node kinds and paths", "[domain]") {
    REQUIRE(stableId("sha256:file", "/0/2/5", "occurrence") !=
            stableId("sha256:file", "/0/2/5", "definition"));
    REQUIRE(stableId("sha256:file", "/0/2/5", "occurrence") !=
            stableId("sha256:file", "/0/2/6", "occurrence"));
}
```

- [ ] **Step 2: Run the tests and verify the missing API failure**

Run:

```powershell
cmake --build --preset windows-debug
ctest --preset windows-debug -R stable --output-on-failure
```

Expected: compilation fails because `core/domain/StableId.h` does not exist.

- [ ] **Step 3: Add focused domain values and deterministic xxHash IDs**

```cpp
// src/core/domain/StableId.h
#pragma once
#include <string>
#include <string_view>

namespace loupe::domain {
std::string stableId(std::string_view sourceHash,
                     std::string_view canonicalPath,
                     std::string_view nodeKind);
}
```

```cpp
// src/core/domain/StableId.cpp
#include "core/domain/StableId.h"
#include <array>
#include <cstdint>
#include <format>
#include <xxhash.h>

namespace loupe::domain {
std::string stableId(const std::string_view sourceHash,
                     const std::string_view canonicalPath,
                     const std::string_view nodeKind) {
    const std::string material = std::format("{}\n{}\n{}", sourceHash, canonicalPath, nodeKind);
    const XXH128_hash_t hash = XXH3_128bits(material.data(), material.size());
    return std::format("{:016x}{:016x}", hash.high64, hash.low64);
}
}
```

```cpp
// src/core/domain/AssemblyTypes.h
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace loupe::domain {
enum class InputClass { StructuredAssembly, FlatMultiSolid, SinglePart, Partial, Invalid, ExternalReferences };
enum class NodeKind { Root, Subassembly, Occurrence, Definition, Body };
enum class LoadStage { Acknowledged, Classifying, UnitReview, TreeReady, CoarseView, Refined, Failed, Canceled };

struct Transform {
    std::array<double, 16> columnMajor{};
};

struct Warning {
    std::string code;
    std::string message;
    std::optional<std::string> nodeId;
};

struct AssemblyNode {
    std::string id;
    NodeKind kind{};
    std::string name;
    std::string hierarchyPath;
    std::optional<std::string> parentId;
    std::optional<std::string> definitionId;
    Transform placement{};
    std::vector<std::string> bodyIds;
    std::vector<Warning> warnings;
};

struct AssemblySnapshot {
    std::string sourceHash;
    InputClass classification{};
    LoadStage stage{};
    std::vector<std::string> rootIds;
    std::vector<AssemblyNode> nodes;
    std::vector<Warning> warnings;
};
}
```

Replace `src/core/CMakeLists.txt` with:

```cmake
add_library(loupe-core STATIC domain/StableId.cpp)
target_include_directories(loupe-core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(loupe-core PUBLIC nlohmann_json::nlohmann_json xxHash::xxhash)
loupe_enable_warnings(loupe-core)
```

- [ ] **Step 4: Register and run the domain tests**

Add `core/test_stable_id.cpp` to `loupe-core-tests`, build, then run:

```powershell
ctest --preset windows-debug -R "stable IDs" --output-on-failure
```

Expected: both deterministic-ID tests pass.

- [ ] **Step 5: Commit the domain contract**

```powershell
git add src/core/domain src/core/CMakeLists.txt tests
git commit -m "feat: define stable assembly contracts"
```

## Task 3: Implement Declared and Effective Unit Policy

**Files:**
- Create: `src/core/units/UnitPolicy.h`
- Create: `src/core/units/UnitPolicy.cpp`
- Create: `tests/core/test_unit_policy.cpp`
- Modify: `src/core/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing unit-policy tests**

```cpp
// tests/core/test_unit_policy.cpp
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "core/units/UnitPolicy.h"

using namespace loupe::units;

TEST_CASE("inch metadata normalizes to millimeters", "[units]") {
    const UnitEvidence evidence{{LengthUnit::Inch}, LengthUnit::Inch, 4.0, false};
    const UnitDecision result = decide(evidence, std::nullopt);
    REQUIRE(result.effectiveUnit == LengthUnit::Inch);
    REQUIRE(result.sourceToMillimeters == Catch::Approx(25.4));
    REQUIRE(result.confidence == UnitConfidence::Confirmed);
}

TEST_CASE("tiny declared millimeter assembly is suspicious", "[units]") {
    const UnitEvidence evidence{{LengthUnit::Millimeter}, LengthUnit::Millimeter, 4.0, false};
    REQUIRE(decide(evidence, std::nullopt).confidence == UnitConfidence::Suspicious);
}

TEST_CASE("manual inch override is explicit and traceable", "[units]") {
    const UnitEvidence evidence{{LengthUnit::Millimeter}, LengthUnit::Millimeter, 4.0, false};
    const UnitOverride override{LengthUnit::Inch, 1.0, "user reviewed alternate extents"};
    const UnitDecision result = decide(evidence, override);
    REQUIRE(result.confidence == UnitConfidence::UserOverride);
    REQUIRE(result.sourceToMillimeters == Catch::Approx(25.4));
    REQUIRE(result.reason == "user reviewed alternate extents");
}

TEST_CASE("missing or mixed units require resolution", "[units]") {
    REQUIRE(decide(UnitEvidence{{}, LengthUnit::Unknown, 120.0, false}, std::nullopt).blocksExport());
    REQUIRE(decide(UnitEvidence{{LengthUnit::Inch, LengthUnit::Millimeter}, LengthUnit::Mixed, 120.0, true}, std::nullopt).blocksExport());
}
```

- [ ] **Step 2: Run the tests and verify they fail**

Run:

```powershell
cmake --build --preset windows-debug
ctest --preset windows-debug -R unit --output-on-failure
```

Expected: compilation fails because `UnitPolicy.h` is missing.

- [ ] **Step 3: Implement the non-destructive unit decision model**

```cpp
// src/core/units/UnitPolicy.h
#pragma once
#include <optional>
#include <string>
#include <vector>

namespace loupe::units {
enum class LengthUnit { Millimeter, Inch, Unknown, Mixed };
enum class UnitConfidence { Confirmed, Suspicious, MissingOrMixed, UserOverride };

struct UnitEvidence {
    std::vector<LengthUnit> declaredRepresentationUnits;
    LengthUnit xcafUnit{LengthUnit::Unknown};
    double normalizedLongestExtentMm{};
    bool internallyInconsistent{};
};

struct UnitOverride {
    LengthUnit interpretAs{LengthUnit::Unknown};
    double customFactor{1.0};
    std::string reason;
};

struct UnitDecision {
    LengthUnit effectiveUnit{LengthUnit::Unknown};
    UnitConfidence confidence{UnitConfidence::MissingOrMixed};
    double sourceToMillimeters{1.0};
    std::string reason;
    [[nodiscard]] bool blocksExport() const noexcept {
        return confidence == UnitConfidence::MissingOrMixed || confidence == UnitConfidence::Suspicious;
    }
};

UnitDecision decide(const UnitEvidence& evidence, const std::optional<UnitOverride>& overrideValue);
double millimetersPerUnit(LengthUnit unit);
}
```

```cpp
// src/core/units/UnitPolicy.cpp
#include "core/units/UnitPolicy.h"
#include <algorithm>
#include <stdexcept>

namespace loupe::units {
double millimetersPerUnit(const LengthUnit unit) {
    switch (unit) {
    case LengthUnit::Millimeter: return 1.0;
    case LengthUnit::Inch: return 25.4;
    case LengthUnit::Unknown:
    case LengthUnit::Mixed: throw std::invalid_argument("unit has no single scale");
    }
    throw std::invalid_argument("unhandled unit");
}

UnitDecision decide(const UnitEvidence& evidence, const std::optional<UnitOverride>& overrideValue) {
    if (overrideValue.has_value()) {
        return {overrideValue->interpretAs, UnitConfidence::UserOverride,
                millimetersPerUnit(overrideValue->interpretAs) * overrideValue->customFactor,
                overrideValue->reason};
    }
    if (evidence.xcafUnit == LengthUnit::Unknown || evidence.xcafUnit == LengthUnit::Mixed ||
        evidence.declaredRepresentationUnits.empty() || evidence.internallyInconsistent) {
        return {evidence.xcafUnit, UnitConfidence::MissingOrMixed, 1.0,
                "missing, mixed, or inconsistent STEP unit evidence"};
    }
    const bool implausibleExtent = evidence.normalizedLongestExtentMm < 10.0 ||
                                   evidence.normalizedLongestExtentMm > 100000.0;
    return {evidence.xcafUnit,
            implausibleExtent ? UnitConfidence::Suspicious : UnitConfidence::Confirmed,
            millimetersPerUnit(evidence.xcafUnit),
            implausibleExtent ? "normalized extent crossed conservative unit-review threshold"
                              : "coherent STEP and XCAF unit evidence"};
}
}
```

- [ ] **Step 4: Run the unit tests**

Run:

```powershell
ctest --preset windows-debug -R units --output-on-failure
```

Expected: all four unit-policy cases pass.

- [ ] **Step 5: Commit the unit contract**

```powershell
git add src/core/units src/core/CMakeLists.txt tests
git commit -m "feat: add explicit STEP unit policy"
```

## Task 4: Import and Classify STEP/XCAF Assemblies

**Files:**
- Create: `src/core/import/StepImporter.h`
- Create: `src/core/import/StepImporter.cpp`
- Create: `tests/fixtures/FixtureFactory.h`
- Create: `tests/fixtures/FixtureFactory.cpp`
- Create: `tests/core/test_step_import.cpp`
- Modify: `src/core/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write a generated structured-assembly fixture and failing import test**

```cpp
// tests/core/test_step_import.cpp
#include <catch2/catch_test_macros.hpp>
#include "core/import/StepImporter.h"
#include "fixtures/FixtureFactory.h"

TEST_CASE("STEP importer preserves definition and repeated occurrences", "[step][import]") {
    const auto file = loupe::tests::writeRepeatedBoxAssembly("repeated-box.step", loupe::tests::FixtureUnit::Millimeter);
    loupe::import::StepImporter importer;
    const auto result = importer.read(file);

    REQUIRE(result.snapshot.classification == loupe::domain::InputClass::StructuredAssembly);
    REQUIRE(result.definitionCount == 1);
    REQUIRE(result.occurrenceCount == 2);
    REQUIRE(result.unitEvidence.xcafUnit == loupe::units::LengthUnit::Millimeter);
}
```

- [ ] **Step 2: Run the focused test and verify it fails**

Run:

```powershell
cmake --build --preset windows-debug
ctest --preset windows-debug -R "STEP importer" --output-on-failure
```

Expected: compilation fails because `StepImporter` and `FixtureFactory` are missing.

- [ ] **Step 3: Define the importer result and OCCT ownership boundary**

```cpp
// src/core/import/StepImporter.h
#pragma once
#include "core/domain/AssemblyTypes.h"
#include "core/units/UnitPolicy.h"
#include <filesystem>
#include <memory>

namespace loupe::import {
struct NativeDocument;

struct ImportResult {
    domain::AssemblySnapshot snapshot;
    units::UnitEvidence unitEvidence;
    std::size_t definitionCount{};
    std::size_t occurrenceCount{};
    std::shared_ptr<const NativeDocument> native;
};

class StepImporter {
public:
    ImportResult read(const std::filesystem::path& file) const;
};
}
```

Implement `StepImporter.cpp` with this exact sequence:

```cpp
STEPCAFControl_Reader reader;
reader.SetNameMode(true);
reader.SetColorMode(true);
const IFSelect_ReturnStatus status = reader.ReadFile(file.string().c_str());
// Capture STEPControl_Reader::FileUnits before transfer.
// Transfer into a TDocStd_Document created through XCAFApp_Application.
// Read XCAFDoc_DocumentTool::ShapeTool and traverse free shapes recursively.
// Create one Definition node per referred label and one Occurrence per component label.
// Use label-entry paths plus source hash to produce stable IDs.
// Classify structured/flat/single/partial/external-reference outcomes explicitly.
```

The implementation must keep `TDocStd_Document`, labels, and `TopoDS_Shape` values inside `NativeDocument`; only `AssemblySnapshot` crosses the boundary.

- [ ] **Step 4: Add generated mm, inch, flat, and single-part cases**

Use OCCT primitives and XCAF labels in `FixtureFactory.cpp` to generate:

```cpp
writeRepeatedBoxAssembly(path, FixtureUnit::Millimeter);
writeRepeatedBoxAssembly(path, FixtureUnit::Inch);
writeFlatTwoSolidStep(path);
writeSingleCylinderStep(path);
```

Run:

```powershell
ctest --preset windows-debug -R "STEP importer|classification|inch metadata" --output-on-failure
```

Expected: structured, flat, single-part, and inch cases all pass with explicit classifications and unit evidence.

- [ ] **Step 5: Commit the importer**

```powershell
git add src/core/import src/core/CMakeLists.txt tests
git commit -m "feat: import and classify STEP assemblies"
```

## Task 5: Build Immutable Selection and Export Plans

**Files:**
- Create: `src/core/export/ExportPlan.h`
- Create: `src/core/export/ExportPlan.cpp`
- Create: `tests/core/test_export_plan.cpp`
- Modify: `src/core/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing occurrence/definition plan tests**

```cpp
// tests/core/test_export_plan.cpp
#include <catch2/catch_test_macros.hpp>
#include "core/export/ExportPlan.h"

TEST_CASE("viewer highlight never changes export selections", "[export-plan]") {
    loupe::exporting::SelectionDraft draft;
    draft.highlight("occ-2");
    REQUIRE(draft.checked().empty());
    draft.setChecked("def-1", loupe::exporting::SelectionKind::Definition, true);
    draft.highlight("occ-3");
    REQUIRE(draft.checked().size() == 1);
}

TEST_CASE("unresolved unit decision blocks a reviewed plan", "[export-plan]") {
    loupe::exporting::PlanRequest request;
    request.unitDecision.confidence = loupe::units::UnitConfidence::Suspicious;
    REQUIRE_THROWS_AS(loupe::exporting::buildPlan(request), loupe::exporting::PlanError);
}
```

- [ ] **Step 2: Run the tests and verify they fail**

Run `ctest --preset windows-debug -R export-plan --output-on-failure`.

Expected: compilation fails because the plan API is missing.

- [ ] **Step 3: Add explicit plan types**

```cpp
// src/core/export/ExportPlan.h
#pragma once
#include "core/units/UnitPolicy.h"
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace loupe::exporting {
enum class SelectionKind { Occurrence, Definition, Body, Subassembly };
enum class Format { Step, Stl };
enum class Coordinates { Local, Assembly };
enum class Grouping { SeparateFiles, PreserveAssembly, FlattenMultiBody };
enum class StepOutputUnit { Millimeter, Inch };

struct CheckedSelection { std::string nodeId; SelectionKind kind{}; };

class SelectionDraft {
public:
    void highlight(std::string nodeId) { highlighted_ = std::move(nodeId); }
    void setChecked(std::string nodeId, SelectionKind kind, bool checked);
    [[nodiscard]] const std::vector<CheckedSelection>& checked() const noexcept { return checked_; }
private:
    std::optional<std::string> highlighted_;
    std::vector<CheckedSelection> checked_;
};

struct OutputRow {
    std::string sourceNodeId;
    SelectionKind selectionKind{};
    std::filesystem::path outputPath;
    Format format{};
    Coordinates coordinates{};
    Grouping grouping{};
    StepOutputUnit stepUnit{StepOutputUnit::Millimeter};
    double stlScale{1.0};
};

struct PlanRequest {
    std::vector<CheckedSelection> selections;
    units::UnitDecision unitDecision;
    std::filesystem::path destination;
    Format format{Format::Step};
    Coordinates coordinates{Coordinates::Local};
    Grouping grouping{Grouping::SeparateFiles};
};

struct ExportPlan { std::vector<OutputRow> rows; std::string fingerprint; };
class PlanError final : public std::runtime_error { using std::runtime_error::runtime_error; };
ExportPlan buildPlan(const PlanRequest& request);
}
```

- [ ] **Step 4: Implement validation, deterministic names, and fingerprinting**

`buildPlan` must reject empty selections, unresolved units, blank destinations, duplicate final paths, occurrence-in-local ambiguity, and incompatible grouping. It must sort rows by hierarchy path before hashing so the same reviewed choices yield the same fingerprint.

Run:

```powershell
ctest --preset windows-debug -R export-plan --output-on-failure
```

Expected: highlight/check separation, unit blocking, collision, and fingerprint tests pass.

- [ ] **Step 5: Commit the plan model**

```powershell
git add src/core/export src/core/CMakeLists.txt tests
git commit -m "feat: add immutable export plans"
```

## Task 6: Export Selected STEP and Binary STL

**Files:**
- Create: `src/core/export/StepExporter.h`
- Create: `src/core/export/StepExporter.cpp`
- Create: `src/core/export/StlExporter.h`
- Create: `src/core/export/StlExporter.cpp`
- Create: `tests/core/test_selected_export.cpp`
- Modify: `src/core/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing round-trip selection tests**

```cpp
TEST_CASE("definition STEP export writes one local definition", "[step][export]") {
    const auto imported = importRepeatedFixture();
    const auto plan = planDefinitionExport(imported, "box-definition", StepOutputUnit::Millimeter);
    const auto result = StepExporter{}.write(imported.native, plan.rows.front());
    REQUIRE(result.written);
    REQUIRE(result.outputCount == 1);
}

TEST_CASE("occurrence STL export applies placement and writes binary millimeters", "[stl][export]") {
    const auto imported = importRepeatedFixture();
    const auto row = occurrenceStlRow("box-occurrence-2", Coordinates::Assembly, 1.0);
    const auto result = StlExporter{}.write(imported.native, row);
    REQUIRE(result.written);
    REQUIRE(result.binary);
    REQUIRE(result.outputUnit == "mm");
}
```

- [ ] **Step 2: Run the focused tests and verify missing exporter failures**

Run `ctest --preset windows-debug -R "definition STEP|occurrence STL" --output-on-failure`.

Expected: compilation fails because exporters are missing.

- [ ] **Step 3: Implement temporary-document STEP export**

`StepExporter` must:

```cpp
// Resolve the selected XCAF label from the stable-ID index.
// Create a new temporary TDocStd_Document.
// Copy only selected definitions/occurrences and required attributes.
// Apply local or assembly coordinate policy exactly once.
// Set output XCAF length unit to mm or inch.
// Transfer the temporary label/document with STEPCAFControl_Writer.
// Write to <final>.partial and rename only after successful writer status.
```

Do not remove or relabel nodes in the imported document.

- [ ] **Step 4: Implement binary STL export and mirrored winding checks**

`StlExporter` must obtain the selected placed shape, normalize it to millimeters, apply the explicit uniform scale, triangulate at the requested profile, correct mirrored triangle winding, write binary STL to a partial path, and atomically finalize it.

Run:

```powershell
ctest --preset windows-debug -R "selected export|mirrored|binary millimeters" --output-on-failure
```

Expected: STEP and STL files are produced, only selected geometry is present, and mirrored STL normals are outward.

- [ ] **Step 5: Commit exporters**

```powershell
git add src/core/export src/core/CMakeLists.txt tests
git commit -m "feat: export selected STEP and STL geometry"
```

## Task 7: Add Mandatory Read-Back Validation and Evidence Reports

**Files:**
- Create: `src/core/validation/OutputValidator.h`
- Create: `src/core/validation/OutputValidator.cpp`
- Create: `src/core/report/EvidenceWriter.h`
- Create: `src/core/report/EvidenceWriter.cpp`
- Create: `tests/core/test_output_validation.cpp`
- Modify: `src/core/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing validation tests**

```cpp
TEST_CASE("STEP validation detects wrong output unit", "[validation]") {
    const auto output = writeFixtureStepInInches();
    const auto expected = expectedValidation(OutputUnit::Millimeter);
    const auto result = OutputValidator{}.validate(output, expected);
    REQUIRE_FALSE(result.passed);
    REQUIRE(result.errors.front().code == "unit_mismatch");
}

TEST_CASE("validation reports each batch row independently", "[validation]") {
    const auto results = validateBatch({validRow(), missingRow()});
    REQUIRE(results.at(0).passed);
    REQUIRE_FALSE(results.at(1).passed);
}
```

- [ ] **Step 2: Run the tests and verify they fail**

Run `ctest --preset windows-debug -R validation --output-on-failure`.

Expected: compilation fails because validation types are missing.

- [ ] **Step 3: Implement exact validation records**

```cpp
struct ExpectedOutput {
    std::filesystem::path path;
    OutputUnit unit;
    std::size_t bodyCount;
    Bounds boundsMm;
    Vec3 centroidMm;
    double relativeTolerance{1e-5};
};

struct ValidationIssue { std::string code; std::string message; };
struct ValidationResult {
    std::filesystem::path path;
    bool reopened{};
    bool shapeValid{};
    bool passed{};
    std::size_t actualBodyCount{};
    Bounds actualBoundsMm{};
    Vec3 actualCentroidMm{};
    OutputUnit actualUnit{};
    std::vector<ValidationIssue> warnings;
    std::vector<ValidationIssue> errors;
};
```

STEP validation must reopen through OCCT and run `BRepCheck_Analyzer`. STL validation must parse the binary header/count, recompute bounds, and verify triangle orientation for mirrored fixtures.

- [ ] **Step 4: Serialize snapshots, results, and benchmark rows**

`EvidenceWriter` writes:

```text
evidence/<case-id>/assembly-snapshot.json
evidence/<case-id>/validation.json
evidence/<case-id>/benchmark.csv
evidence/<case-id>/failure.json
```

Every JSON file includes `schemaVersion`, `sourceHash`, `importerVersion`, `effectiveUnitDecision`, and `generatedAtUtc`.

Run `ctest --preset windows-debug -R "validation|evidence" --output-on-failure`.

Expected: unit, body-count, bounds, centroid, missing-file, and mirrored-mesh tests pass; reports parse as JSON.

- [ ] **Step 5: Commit validation and reporting**

```powershell
git add src/core/validation src/core/report src/core/CMakeLists.txt tests
git commit -m "feat: validate outputs and write evidence"
```

## Task 8: Build the Corpus Contract and Headless Spike CLI

**Files:**
- Create: `corpus/README.md`
- Create: `corpus/cases.example.json`
- Create: `src/spike/main.cpp`
- Create: `tests/core/test_spike_cli.cpp`
- Modify: `src/spike/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing CLI contract tests**

```cpp
TEST_CASE("spike exits 2 for unresolved units", "[cli]") {
    const auto result = runSpike({"inspect", missingUnitFixture().string(), "--json"});
    REQUIRE(result.exitCode == 2);
    REQUIRE(result.json.at("status") == "unit_review_required");
}

TEST_CASE("spike corpus command reports every case", "[cli]") {
    const auto result = runSpike({"corpus", fixtureCasesJson().string(), "--evidence", tempEvidenceDir().string()});
    REQUIRE(result.exitCode == 0);
    REQUIRE(result.json.at("caseCount") == 4);
}
```

- [ ] **Step 2: Add the private-corpus metadata contract**

```json
// corpus/cases.example.json
{
  "schemaVersion": 1,
  "cases": [
    {
      "id": "structured-mm-example",
      "file": "private/structured-mm.step",
      "expectedClass": "structured_assembly",
      "expectedDeclaredUnits": ["mm"],
      "expectedOutcome": "supported",
      "tags": ["ap242", "repeated-definitions"]
    }
  ]
}
```

`corpus/README.md` must state that `private/` is ignored, local only, user-owned, and never copied into reports. Reports contain hashes and metadata, not geometry.

- [ ] **Step 3: Implement exact CLI commands and exit codes**

```text
loupe-spike inspect <file> [--interpret-as mm|in] [--factor N] --json
loupe-spike export <file> --selection <stable-id> --kind occurrence|definition \
  --format step|stl --coordinates local|assembly --output-unit mm|in --out <dir>
loupe-spike corpus <cases.json> --evidence <dir>
loupe-spike benchmark <cases.json> --csv <file>
```

Exit codes:

```text
0 success
1 invalid arguments
2 review required
3 import failure
4 export failure
5 validation failure
6 corpus contract failure
```

- [ ] **Step 4: Run generated fixtures through all CLI paths**

Run:

```powershell
ctest --preset windows-debug -R cli --output-on-failure
build/windows-debug/src/spike/loupe-spike.exe corpus build/windows-debug/generated-corpus/cases.json --evidence build/windows-debug/evidence
```

Expected: every generated case receives an evidence directory and summary; no source geometry is copied.

- [ ] **Step 5: Commit the corpus harness**

```powershell
git add corpus src/spike tests src/spike/CMakeLists.txt
git commit -m "feat: add corpus and spike command harness"
```

## Task 9: Build the Separate Inspect/Export Browser Prototype

**Files:**
- Create: `prototype/package.json`
- Create: `prototype/index.html`
- Create: `prototype/src/app.js`
- Create: `prototype/src/styles.css`
- Create: `prototype/src/fixture/assembly.json`
- Create: `prototype/tests/workspaces.spec.js`
- Create: `prototype/playwright.config.js`

- [ ] **Step 1: Write failing interaction tests**

```javascript
// prototype/tests/workspaces.spec.js
import { test, expect } from '@playwright/test';

test('Inspect is default and exposes common inspection tools', async ({ page }) => {
  await page.goto('/');
  await expect(page.getByRole('tab', { name: 'Inspect' })).toHaveAttribute('aria-selected', 'true');
  for (const tool of ['Fit', 'Isolate', 'Section', 'Measure', 'Ghost', 'Capture']) {
    await expect(page.getByRole('button', { name: tool })).toBeVisible();
  }
});

test('highlight previews a component without checking it', async ({ page }) => {
  await page.goto('/');
  await page.getByRole('tab', { name: 'Export' }).click();
  await page.getByRole('option', { name: /Front cover/ }).click();
  await expect(page.getByTestId('master-highlight')).toHaveText('Front cover');
  await expect(page.getByTestId('isolated-preview')).toHaveText('Front cover');
  await expect(page.getByRole('checkbox', { name: /Export Front cover/ })).not.toBeChecked();
});

test('unit warning blocks review until acknowledged', async ({ page }) => {
  await page.goto('/?fixture=suspicious-units');
  await page.getByRole('tab', { name: 'Export' }).click();
  await expect(page.getByRole('button', { name: 'Review export plan' })).toBeDisabled();
  await page.getByRole('button', { name: 'Review source units' }).click();
  await page.getByLabel('Interpret source values as').selectOption('in');
  await page.getByRole('button', { name: 'Apply inch interpretation' }).click();
  await expect(page.getByRole('button', { name: 'Review export plan' })).toBeEnabled();
});
```

- [ ] **Step 2: Run Playwright and verify the prototype is absent**

Run:

```powershell
Set-Location prototype
npm install
npx playwright install chromium
npm test
```

Expected: tests fail because the Vite app and accessible controls do not exist.

- [ ] **Step 3: Implement the fixture-driven state model**

```javascript
// prototype/src/app.js
export const state = {
  workspace: 'inspect',
  highlightedId: null,
  checkedIds: new Set(),
  unitDecision: { confidence: 'suspicious', effectiveUnit: 'mm', acknowledged: false }
};

export function highlight(id) {
  state.highlightedId = id;
  renderMasterHighlight(id);
  renderIsolatedPreview(id);
}

export function setExportChecked(id, checked) {
  checked ? state.checkedIds.add(id) : state.checkedIds.delete(id);
  renderExportCount();
}
```

Build semantic tabs, listbox/options, checkboxes, two labeled viewer regions, a stable floating Inspect toolbar, source-unit review dialog, and reduced-motion CSS. The minimum DOM contract is:

```html
<div role="tablist" aria-label="Workspace">
  <button role="tab" aria-selected="true">Inspect</button>
  <button role="tab" aria-selected="false">Export</button>
</div>
<main data-workspace="inspect">
  <section aria-label="Assembly viewer"></section>
  <nav aria-label="Inspection tools">
    <button>Fit</button><button>Isolate</button><button>Section</button>
    <button>Measure</button><button>Ghost</button><button>Capture</button>
  </nav>
</main>
<main data-workspace="export" hidden>
  <div role="listbox" aria-label="Components"></div>
  <section aria-label="Assembly context" data-testid="master-highlight"></section>
  <section aria-label="Isolated component" data-testid="isolated-preview"></section>
  <button disabled>Review export plan</button>
</main>
```

- [ ] **Step 4: Run interaction and accessibility-focused tests**

Run:

```powershell
npm test
```

Expected: workspace, highlight/check separation, dual preview, unit gate, keyboard focus, and reduced-motion tests pass.

- [ ] **Step 5: Commit the prototype**

```powershell
git add prototype
git commit -m "feat: prototype Inspect and Export workspaces"
```

## Task 10: Produce and Review the Phase 0 Evidence Gate

**Files:**
- Create: `docs/evidence/phase-0-template.md`
- Create: `docs/evidence/README.md`
- Create: `tools/baseline/time_creo_tasks.ps1`
- Modify: `docs/superpowers/specs/2026-07-10-loupe-phase-0-and-roadmap-design.md`

- [ ] **Step 1: Create the evidence template with pass/fail rows**

```markdown
# Phase 0 Evidence Report

## Environment
- Loupe commit: populated by `git rev-parse HEAD`
- OCCT version: populated by `loupe-spike --dependencies`
- Compiler: populated by `loupe-spike --build-info`
- Windows reference machine: populated by `Get-ComputerInfo | ConvertTo-Json`

## Corpus Summary
| Case | Class | Units | Traversal | Selected STEP | Selected STL | Read-back | Result |
|---|---|---|---|---|---|---|---|

## Performance
| Case | Acknowledge | Tree | Coarse view | Inspect ready | Peak memory |
|---|---:|---:|---:|---:|---:|

## Named Failures
| Case | Failure code | User-visible behavior | Reproduction command |
|---|---|---|---|

## Gate Decision
- Structured traversal: PASS or FAIL from corpus summary
- Occurrence/definition grouping: PASS or FAIL from corpus summary
- Unit evidence and correction: PASS or FAIL from unit cases
- Selected export: PASS or FAIL from output rows
- Read-back validation: PASS or FAIL from validation rows
- Windows benchmark: PASS or FAIL from benchmark targets
- macOS gate status: Open pending real M-series hardware
```

- [ ] **Step 2: Run the complete native and prototype verification suite**

Run:

```powershell
cmake --build --preset windows-debug
ctest --preset windows-debug --output-on-failure
cmake --build --preset windows-release
ctest --preset windows-release --output-on-failure
Push-Location prototype
npm ci
npm test
Pop-Location
```

Expected: all generated-fixture and prototype tests pass in Debug and Release.

- [ ] **Step 3: Run the private corpus without committing source paths or geometry**

Run:

```powershell
build/windows-release/src/spike/loupe-spike.exe corpus corpus/private/cases.json --evidence evidence/private-run
build/windows-release/src/spike/loupe-spike.exe benchmark corpus/private/cases.json --csv evidence/private-run/benchmark.csv
```

Expected: every case receives an explicit supported, warning, review-required, or failed result; no unlisted case disappears.

Before the corpus run, time the same three representative part/subassembly preparation tasks in Creo with:

```powershell
# tools/baseline/time_creo_tasks.ps1
param(
  [Parameter(Mandatory)][string]$SourceHash,
  [Parameter(Mandatory)][string]$OutputCsv
)
$tasks = @(
  'export selected occurrence',
  'export selected subassembly',
  'correct units and export STL'
)
'task_id,operator,source_hash,task_description,elapsed_seconds,errors,assistance' | Set-Content -LiteralPath $OutputCsv
for ($i = 0; $i -lt $tasks.Count; $i++) {
  Read-Host "Press Enter to start: $($tasks[$i])"
  $watch = [System.Diagnostics.Stopwatch]::StartNew()
  Read-Host 'Press Enter immediately after the final output is written'
  $watch.Stop()
  $errors = Read-Host 'Number of errors or restarts'
  $assistance = Read-Host 'Was assistance required? true/false'
  $seconds = [math]::Round($watch.Elapsed.TotalSeconds, 2)
  "$($i + 1),interim-owner,$SourceHash,$($tasks[$i]),$seconds,$errors,$assistance" | Add-Content -LiteralPath $OutputCsv
}
```

Run:

```powershell
$sourceHash = (build/windows-release/src/spike/loupe-spike.exe inspect corpus/private/baseline-source.step --json | ConvertFrom-Json).sourceHash
tools/baseline/time_creo_tasks.ps1 -SourceHash $sourceHash -OutputCsv evidence/phase-0-creo-baseline.csv
```

Expected: three measured rows with no private paths; the source hash is captured from the CLI at execution time.

- [ ] **Step 4: Review the gate against the design spec**

Confirm all of these are true before Phase 1:

```text
Every corpus file has a named result.
Supported structured files preserve definitions and occurrences.
Unit evidence and manual overrides reproduce deterministically.
Selected STEP/STL outputs reopen and validate.
Mirrored fixtures retain correct mesh winding.
The browser prototype validates the approved two-workspace model.
Windows evidence is recorded; macOS remains explicitly open.
```

- [ ] **Step 5: Commit the evidence report without private geometry**

```powershell
git add docs/evidence docs/superpowers/specs/2026-07-10-loupe-phase-0-and-roadmap-design.md
git commit -m "docs: record Phase 0 feasibility evidence"
```

## Phase 0 Completion Gate

Do not begin Phase 1 until:

- All generated tests pass.
- Every private corpus case has a retained result.
- Supported cases pass traversal, unit interpretation, selected export, and read-back checks.
- Failures have stable codes and reproduction commands.
- The Inspect/Export prototype is approved against real fixture content.
- The Phase 0 evidence report is reviewed and committed.
