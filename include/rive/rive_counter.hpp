/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_COUNTER_HPP_
#define _RIVE_COUNTER_HPP_

#include "rive/rive_types.hpp"

namespace rive
{

struct Counter
{
    enum Type
    {
        kFile,
        kArtboardInstance,
        kLinearAnimationInstance,
        kStateMachineInstance,

        kBuffer,
        kPath,
        kPaint,
        kShader,
        kImage,

        kLastType = kImage,
    };

    static constexpr int kNumTypes = Type::kLastType + 1;
    static int counts[kNumTypes];

    static void update(Type ct, int delta)
    {
        assert(delta == 1 || delta == -1);
        counts[ct] += delta;
        assert(counts[ct] >= 0);
    }
};

} // namespace rive

#endif
