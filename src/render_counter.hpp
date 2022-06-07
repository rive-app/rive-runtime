/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_RENDERER_COUNTER_HPP_
#define _RIVE_RENDERER_COUNTER_HPP_

#include "rive/rive_types.hpp"

namespace rive {

enum RenderCounterType {
    kBuffer,
    kPath,
    kPaint,
    kShader,
    kImage,

    kLastCounterType = kImage,
};

struct RenderCounter {
    int counts[RenderCounterType::kLastCounterType + 1] = {};

    void update(RenderCounterType ct, int delta) {
        assert(delta == 1 || delta == -1);
        counts[ct] += delta;
    }

    void dump(const char label[] = nullptr) const;

    static RenderCounter& globalCounter();
};

} // namespace rive

#endif
