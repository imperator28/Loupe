#pragma once

#include <QHash>
#include <QString>
#include <QVector>

// The component list both export workspaces show in their picker.
//
// Shared rather than copied because the rules here are not obvious and were arrived at
// from real files: definitions are skipped, a body row whose name is the STEP default
// ("Solid", "Compound", "Body") is suppressed when its parent already stands for the same
// thing, hierarchy paths are built defensively against cycles, and a scene pick has to be
// walked up to the nearest row the picker actually shows. A second copy would drift from
// this one, and the drift would only show up on a customer's assembly.
namespace loupe::app::models {

struct PickerComponent final {
    QString id;
    QString parentId;
    QString name;
    QString hierarchyPath;
    int kind{};
    int depth{};
    bool exportable{};
    bool visibleInPicker{true};
    bool hasVisibleChildren{};
};

class PickerComponents final {
public:
    // Returns false when the snapshot is not a JSON object, leaving the list empty.
    bool replaceSnapshot(const QString& snapshotJson);
    void clear();

    [[nodiscard]] const QVector<PickerComponent>& components() const noexcept { return components_; }
    [[nodiscard]] bool isEmpty() const noexcept { return components_.isEmpty(); }
    [[nodiscard]] const QString& effectiveUnit() const noexcept { return effectiveUnit_; }
    [[nodiscard]] double sourceToMillimeters() const noexcept { return sourceToMillimeters_; }

    [[nodiscard]] int indexOf(const QString& nodeId) const { return indexById_.value(nodeId, -1); }
    [[nodiscard]] bool contains(const QString& nodeId) const { return indexById_.contains(nodeId); }
    [[nodiscard]] const PickerComponent* find(const QString& nodeId) const;
    // The nearest ancestor the picker actually shows, so a pick on a suppressed body row
    // still selects something.
    [[nodiscard]] QString pickerNodeForSceneNode(const QString& nodeId) const;

private:
    [[nodiscard]] QString hierarchyPathFor(const QString& nodeId, QSet<QString>& resolving) const;

    QVector<PickerComponent> components_;
    QHash<QString, int> indexById_;
    QString effectiveUnit_{QStringLiteral("mm")};
    double sourceToMillimeters_{1.0};
};

// Human-readable node kind, for the picker's secondary label.
[[nodiscard]] QString pickerKindLabel(int kind);

} // namespace loupe::app::models
