#pragma once

#include <QQuick3DGeometry>
#include <QVariantList>

namespace loupe::app::render {

// The exploded view's explode lines as scene geometry (#41): thin dashed lines from where each
// part seats to where it is, drawn in the 3D view so parts in front hide them, the viewport
// clips them, and a capture holds them. Dashes are cut here, in scene units, as GL_LINES
// segments the edge material draws.
class ExplodeLineGeometry : public QQuick3DGeometry {
    Q_OBJECT
    Q_PROPERTY(int segmentCount READ segmentCount NOTIFY linesChanged)

public:
    explicit ExplodeLineGeometry(QQuick3DObject* parent = nullptr);

    // ExplodeController::explodeLines(): [{segments: [{from, to, centerline}]}]. `dash` is a
    // dash's length in scene units and `gap` the space after it; a centerline's long dash is
    // twice a dash, with a short dot between.
    Q_INVOKABLE void setLines(const QVariantList& lines, double dash, double gap);
    Q_INVOKABLE void clearLines();
    // The drawn pieces (dashes and dots), for tests.
    [[nodiscard]] int segmentCount() const noexcept { return segmentCount_; }

signals:
    void linesChanged();

private:
    int segmentCount_{0};
};

} // namespace loupe::app::render
