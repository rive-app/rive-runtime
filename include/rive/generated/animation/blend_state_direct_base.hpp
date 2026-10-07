#ifndef _RIVE_BLEND_STATE_DIRECT_BASE_HPP_
#define _RIVE_BLEND_STATE_DIRECT_BASE_HPP_
#include "rive/animation/blend_state.hpp"
namespace rive
{
class BlendStateDirectBase : public BlendState
{
protected:
    typedef BlendState Super;

public:
    static const uint16_t typeKey = 73;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif