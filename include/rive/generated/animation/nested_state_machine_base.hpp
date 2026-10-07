#ifndef _RIVE_NESTED_STATE_MACHINE_BASE_HPP_
#define _RIVE_NESTED_STATE_MACHINE_BASE_HPP_
#include "rive/nested_animation.hpp"
namespace rive
{
class NestedStateMachineBase : public NestedAnimation
{
protected:
    typedef NestedAnimation Super;

public:
    static const uint16_t typeKey = 95;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif