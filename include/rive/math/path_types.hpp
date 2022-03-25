/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_PATH_TYPES_HPP_
#define _RIVE_PATH_TYPES_HPP_

#include "rive/rive_types.hpp"

namespace rive {

    enum class FillRule {
        nonZero,
        evenOdd,
    };

    enum class PathDirection {
        cw,
        ccw,
        // aliases
        clockwise = cw,
        counterclockwise = ccw,
    };

    enum class PathVerb : uint8_t {
        move,
        line,
        quad,
        conic_unused, // so we match skia's order
        cubic,
        close,
    };

} // namespace rive
#endif
