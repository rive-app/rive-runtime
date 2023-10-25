#ifndef _RIVE_NESTED_TRIGGER_HPP_
#define _RIVE_NESTED_TRIGGER_HPP_
#include "rive/generated/animation/nested_trigger_base.hpp"
#include <stdio.h>
namespace rive
{
class NestedTrigger : public NestedTriggerBase
{
public:
    void applyValue() override;
    void fire(const CallbackData& value) override;
};
} // namespace rive

#endif