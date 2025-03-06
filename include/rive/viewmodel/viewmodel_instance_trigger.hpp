#ifndef _RIVE_VIEW_MODEL_INSTANCE_TRIGGER_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_TRIGGER_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_trigger_base.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include <stdio.h>
namespace rive
{
#ifdef WITH_RIVE_TOOLS
class ViewModelInstanceTrigger;
typedef void (*ViewModelTriggerChanged)(ViewModelInstanceTrigger* vmi,
                                        uint32_t value);
#endif
class ViewModelInstanceTrigger : public ViewModelInstanceTriggerBase,
                                 public Triggerable
{
protected:
    void propertyValueChanged() override;

public:
    void advanced() override;
#ifdef WITH_RIVE_TOOLS
    void onChanged(ViewModelTriggerChanged callback)
    {
        m_changedCallback = callback;
    }
    ViewModelTriggerChanged m_changedCallback = nullptr;
#endif

    void trigger() { propertyValue(propertyValue() + 1); }
};
} // namespace rive

#endif