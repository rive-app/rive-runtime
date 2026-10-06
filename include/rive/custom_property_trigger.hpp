#ifndef _RIVE_CUSTOM_PROPERTY_TRIGGER_HPP_
#define _RIVE_CUSTOM_PROPERTY_TRIGGER_HPP_
#include "rive/generated/custom_property_trigger_base.hpp"
#include <cstdint>
#include <stdio.h>
namespace rive
{
class CustomPropertyTrigger : public CustomPropertyTriggerBase
{
public:
    void fire(const CallbackData& value) override
    {
        propertyValue(propertyValue() + 1);
    }

    // Its count only goes up, by one per fire. Like a view model trigger's
    // change, a fire takes a position in ViewModelInstanceValue's change
    // sequence, which is how a state machine's component condition tells
    // when it fired (StateMachineInstance::changePending).
    uint64_t changeSequence() const { return m_changeSequence; }

protected:
    void propertyValueChanged() override;

private:
    uint64_t m_changeSequence = 0;
};
} // namespace rive

#endif
