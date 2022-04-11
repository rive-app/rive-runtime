#ifndef _RIVE_BLEND_STATE_DIRECT_HPP_
#define _RIVE_BLEND_STATE_DIRECT_HPP_
#include "rive/generated/animation/blend_state_direct_base.hpp"
#include <stdio.h>
namespace rive {
    class BlendStateDirect : public BlendStateDirectBase {
    public:
        StateInstance* makeInstance(ArtboardInstance*) const override;
    };
} // namespace rive

#endif