#pragma once

#include <QQuick3DGeometry>

namespace loupe::app::render {

// A torus in the XY plane, around the Z axis: the ring an exploded-view rotation handle
// draws (#17). Radius 1; the handle scales it to a fixed size on screen.
class HandleRingGeometry : public QQuick3DGeometry {
    Q_OBJECT
    // The tube's radius as a fraction of the ring's.
    Q_PROPERTY(double tubeRadius READ tubeRadius WRITE setTubeRadius NOTIFY tubeRadiusChanged)

public:
    explicit HandleRingGeometry(QQuick3DObject* parent = nullptr);

    [[nodiscard]] double tubeRadius() const noexcept { return tubeRadius_; }
    void setTubeRadius(double radius);

signals:
    void tubeRadiusChanged();

private:
    void rebuild();

    double tubeRadius_{0.025};
};

} // namespace loupe::app::render
