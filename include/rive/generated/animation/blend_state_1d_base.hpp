#ifndef _RIVE_BLEND_STATE1_DBASE_HPP_
#define _RIVE_BLEND_STATE1_DBASE_HPP_
#include "rive/animation/blend_state.hpp"
namespace rive
{
class BlendState1DBase : public BlendState
{
protected:
    typedef BlendState Super;

public:
    static const uint16_t typeKey = 527;

    uint16_t coreType() const override { return typeKey; }
};
} // namespace rive

#endif