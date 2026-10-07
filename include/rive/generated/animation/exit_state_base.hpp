#ifndef _RIVE_EXIT_STATE_BASE_HPP_
#define _RIVE_EXIT_STATE_BASE_HPP_
#include "rive/animation/layer_state.hpp"
namespace rive
{
class ExitStateBase : public LayerState
{
protected:
    typedef LayerState Super;

public:
    static const uint16_t typeKey = 64;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif