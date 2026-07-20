#pragma once

#include <QSharedPointer>
#include <QVector>
#include <QVector3D>

namespace loupe::app::render {

struct SectionSourceData {
    QVector<float> vertices;
    QVector<quint32> indices;
};

struct SectionBuildRequest {
    QSharedPointer<const SectionSourceData> source;
    QVector3D normal{0.0F, 0.0F, 1.0F};
    double offset{};
    bool flipped{false};
    bool capEnabled{true};
    bool sliceOnly{false};
    bool sliceFill{true};
    bool sliceOutline{true};
};

struct SectionBuildResult {
    QVector<float> vertices;
    QVector<quint32> indices;
    int capTriangleCount{};
};

[[nodiscard]] SectionBuildResult buildSectionOverlay(const SectionBuildRequest& request);

} // namespace loupe::app::render
