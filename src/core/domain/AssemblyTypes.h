#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace loupe::domain {

enum class InputClass {
    StructuredAssembly,
    FlatMultiSolid,
    SinglePart,
    Partial,
    Invalid,
    ExternalReferences,
};

enum class NodeKind {
    Root,
    Subassembly,
    Occurrence,
    Definition,
    Body,
};

enum class LoadStage {
    Acknowledged,
    Classifying,
    UnitReview,
    TreeReady,
    CoarseView,
    Refined,
    Failed,
    Canceled,
};

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
    // Set only on a Body node synthesized by splitting a multi-solid compound: the
    // 0-based index of this node's solid within a TopAbs_SOLID exploration of the
    // shape at hierarchyPath. Absent for every other node, including a Body whose
    // own shape is a single solid.
    std::optional<int> subSolidIndex;
};

struct AssemblySnapshot {
    std::string sourceHash;
    InputClass classification{};
    LoadStage stage{};
    std::vector<std::string> rootIds;
    std::vector<AssemblyNode> nodes;
    std::vector<Warning> warnings;
};

} // namespace loupe::domain
