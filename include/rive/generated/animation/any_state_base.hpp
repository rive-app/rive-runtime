#ifndef _RIVE_ANY_STATE_BASE_HPP_
#define _RIVE_ANY_STATE_BASE_HPP_
#include "rive/animation/layer_state.hpp"
namespace rive
{
class AnyStateBase : public LayerState
{
protected:
    typedef LayerState Super;

public:
    static const uint16_t typeKey = 62;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif