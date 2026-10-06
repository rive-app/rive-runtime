#ifndef _RIVE_VIEW_MODEL_INSTANCE_TRIGGER_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_TRIGGER_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_trigger_base.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/data_bind/data_values/data_value_integer.hpp"
#include <stdio.h>
namespace rive
{
#ifdef WITH_RIVE_TOOLS
class ViewModelInstanceTrigger;
typedef void (*ViewModelTriggerChanged)(ViewModelInstanceTrigger* vmi,
                                        uint32_t value);
#endif
class ViewModelInstanceTrigger : public ViewModelInstanceTriggerBase
{
protected:
    void propertyValueChanged() override;

public:
    // Its count only goes up, by one per fire, and nothing resets it.
    // Consumers see a fire as a change: transitions through the value's change
    // sequence (see ViewModelInstanceValue::changeSequence), listeners
    // through its dirt.
#ifdef WITH_RIVE_TOOLS
    void onChanged(ViewModelTriggerChanged callback)
    {
        m_changedCallback = callback;
    }
    ViewModelTriggerChanged m_changedCallback = nullptr;
#endif

    void fire(const CallbackData& value) override
    {
        propertyValue(propertyValue() + 1);
    }
    void trigger() { propertyValue(propertyValue() + 1); }
    void applyValue(DataValueInteger*);
};
} // namespace rive

#endif