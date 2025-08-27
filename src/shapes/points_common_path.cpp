#include "rive/artboard.hpp"
#include "rive/shapes/points_common_path.hpp"
#include "rive/shapes/vertex.hpp"
#include "rive/shapes/path_vertex.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/bones/skin.hpp"
#include "rive/span.hpp"
#include "rive/shapes/shape_path_flags.hpp"

using namespace rive;

bool PointsCommonPath::isClockwise() const
{
    return (pathFlags() & (int)ShapePathFlags::isCounterClockwise) == 0;
}
