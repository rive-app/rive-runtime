#ifndef _RIVE_VIEW_MODEL_INSTANCE_TRIGGER_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_TRIGGER_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_trigger_base.hpp"
#include <stdio.h>
namespace rive
{
#ifdef WITH_RIVE_TOOLS
class ViewModelInstanceTrigger;
typedef void (*ViewModelTriggerChanged)(ViewModelInstanceTrigger* vmi, uint32_t value);
#endif
class ViewModelInstanceTrigger : public ViewModelInstanceTriggerBase
{
protected:
    void propertyValueChanged() override;
#ifdef WITH_RIVE_TOOLS
public:
    void onChanged(ViewModelTriggerChanged callback) { m_changedCallback = callback; }
    ViewModelTriggerChanged m_changedCallback = nullptr;
#endif
};
} // namespace rive

#endif