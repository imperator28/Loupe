#pragma once

// The 2D output formats, in one place.
//
// This started as two identical declarations, one in DrawingPlan.h and one in
// DrawingValidator.h. They never clashed while each header was used alone, and broke the
// moment a translation unit needed both -- which the worker does, since it plans, writes
// and then validates. Kept as its own header so both can stay standalone.
namespace loupe::drawing {

enum class DrawingFormat { Dxf, Svg, Pdf };

} // namespace loupe::drawing
