#include "app/render/HandleRingGeometry.h"

#include <QByteArray>
#include <QVector3D>

#include <cmath>
#include <numbers>

namespace loupe::app::render {

namespace {

constexpr int kRingSegments = 96;
constexpr int kTubeSegments = 10;

} // namespace

HandleRingGeometry::HandleRingGeometry(QQuick3DObject* parent)
    : QQuick3DGeometry(parent)
{
    setPrimitiveType(PrimitiveType::Triangles);
    setStride(static_cast<int>(6 * sizeof(float)));
    addAttribute(Attribute::PositionSemantic, 0, Attribute::F32Type);
    addAttribute(Attribute::NormalSemantic, static_cast<int>(3 * sizeof(float)), Attribute::F32Type);
    addAttribute(Attribute::IndexSemantic, 0, Attribute::U32Type);
    rebuild();
}

void HandleRingGeometry::setTubeRadius(const double radius)
{
    if (radius <= 0.0 || std::abs(radius - tubeRadius_) < 1e-9) return;
    tubeRadius_ = radius;
    rebuild();
    emit tubeRadiusChanged();
}

void HandleRingGeometry::rebuild()
{
    QByteArray vertices;
    vertices.resize(static_cast<qsizetype>(kRingSegments * kTubeSegments * 6 * sizeof(float)));
    auto* vertex = reinterpret_cast<float*>(vertices.data());
    const auto tube = static_cast<float>(tubeRadius_);
    for (int ring = 0; ring < kRingSegments; ++ring) {
        const auto around = 2.0F * std::numbers::pi_v<float> * static_cast<float>(ring) / kRingSegments;
        const QVector3D outward(std::cos(around), std::sin(around), 0.0F);
        for (int step = 0; step < kTubeSegments; ++step) {
            const auto across = 2.0F * std::numbers::pi_v<float> * static_cast<float>(step) / kTubeSegments;
            const auto normal = outward * std::cos(across) + QVector3D(0.0F, 0.0F, std::sin(across));
            const auto position = outward + normal * tube;
            *vertex++ = position.x(); *vertex++ = position.y(); *vertex++ = position.z();
            *vertex++ = normal.x(); *vertex++ = normal.y(); *vertex++ = normal.z();
        }
    }
    QByteArray indices;
    indices.resize(static_cast<qsizetype>(kRingSegments * kTubeSegments * 6 * sizeof(quint32)));
    auto* index = reinterpret_cast<quint32*>(indices.data());
    const auto at = [](const int ring, const int step) {
        return static_cast<quint32>((ring % kRingSegments) * kTubeSegments + step % kTubeSegments);
    };
    for (int ring = 0; ring < kRingSegments; ++ring) {
        for (int step = 0; step < kTubeSegments; ++step) {
            *index++ = at(ring, step); *index++ = at(ring + 1, step); *index++ = at(ring + 1, step + 1);
            *index++ = at(ring, step); *index++ = at(ring + 1, step + 1); *index++ = at(ring, step + 1);
        }
    }
    setVertexData(vertices);
    setIndexData(indices);
    const auto extent = 1.0F + tube;
    setBounds({-extent, -extent, -tube}, {extent, extent, tube});
    update();
}

} // namespace loupe::app::render
