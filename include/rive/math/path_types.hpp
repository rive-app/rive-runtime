/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_PATH_TYPES_HPP_
#define _RIVE_PATH_TYPES_HPP_

#include "rive/rive_types.hpp"

namespace rive
{

enum class FillRule
{
    nonZero,
    evenOdd,
};

enum class PathDirection
{
    cw,
    ccw,
    // aliases
    clockwise = cw,
    counterclockwise = ccw,
};

enum class PathVerb : uint8_t
{
    // These deliberately match Skia's values
    move = 0,
    line = 1,
    quad = 2,
    // conic
    cubic = 4,
    close = 5,
};

int path_verb_to_point_count(PathVerb);

} // namespace rive
#endif
