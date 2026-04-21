#ifndef _RIVE_SEMANTIC_DIRT_HPP_
#define _RIVE_SEMANTIC_DIRT_HPP_

#include "rive/enums.hpp"
#include <cstdint>

namespace rive
{

enum class SemanticDirt : uint8_t
{
    None = 0,
    Structure = 1 << 0,
    Content = 1 << 1,
    Bounds = 1 << 2,
    All = (1 << 0) | (1 << 1) | (1 << 2),
};

} // namespace rive

#endif
