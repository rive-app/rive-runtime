/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_HITINFO_HPP_
#define _RIVE_HITINFO_HPP_

#include "rive/math/aabb.hpp"
#include <vector>

namespace rive
{

class NestedArtboard;

struct HitInfo
{
    IAABB area;                          // input
    std::vector<NestedArtboard*> mounts; // output
};

} // namespace rive
#endif
