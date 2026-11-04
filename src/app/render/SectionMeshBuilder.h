#pragma once

#include <QSharedPointer>
#include <QVector>
#include <QVector3D>

namespace loupe::app::render {

struct SectionSourceData {
    QVector<float> vertices;
    QVector<quint32> indices;
    // The vertices' bounding box, so a plane that misses the part entirely is seen
    // without looking at a single triangle.
    QVector3D minimum;
    QVector3D maximum;
};

// Whether the plane passes through the box at all: when it does not, the section adds
// nothing for that part (no cap, no slice), and nothing has to be built.
[[nodiscard]] bool sectionPlaneCrossesBox(const QVector3D& minimum, const QVector3D& maximum,
                                          const QVector3D& normal, double offset);

struct SectionBuildRequest {
    QSharedPointer<const SectionSourceData> source;
    QVector3D normal{0.0F, 0.0F, 1.0F};
    double offset{};
    bool flipped{false};
    bool capEnabled{true};
    bool sliceOnly{false};
    bool sliceFill{true};
    bool sliceOutline{true};
    float outlineWidth{};
};

struct SectionBuildResult {
    QVector<float> vertices;
    QVector<quint32> indices;
    int capTriangleCount{};
    int fillIndexCount{};
};

[[nodiscard]] SectionBuildResult buildSectionOverlay(const SectionBuildRequest& request);

} // namespace loupe::app::render
