#pragma once

#include "core/drawing/DrawingProjector.h"

#include <string>

// Serialises a projected drawing for the on-screen preview.
//
// Deliberately not a file format. Arcs and cubics are flattened to polylines here, because
// the preview only has to be looked at, and a QML Canvas drawing straight polylines cannot
// disagree with the writers about arc direction or sweep sign -- one of the two
// highest-severity risks in this feature. The written file always comes from the exact
// primitives, never from this.
//
// The measured extents travel with it: they are the user's primary check against a
// wrong-scale export, so the preview has to state them rather than imply them.
namespace loupe::drawing {

// Compact JSON: bounds in millimetres, one polyline per contour, warning codes, and the
// projection statistics the workspace reports.
// Returns UTF-8 JSON as std::string, not QByteArray: loupe-core links Qt privately and its
// public headers stay Qt-free, so a Qt type here would lock out every consumer that does not
// itself link Qt -- the spike among them.
[[nodiscard]] std::string encodePreview(const ProjectionResult& projected);

} // namespace loupe::drawing
