#ifndef _RIVE_VIEW_MODEL_INSTANCE_BOOLEAN_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_BOOLEAN_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_boolean_base.hpp"
#include <stdio.h>
namespace rive
{
#ifdef WITH_RIVE_TOOLS
class ViewModelInstanceBoolean;
typedef void (*ViewModelBooleanChanged)(ViewModelInstanceBoolean* vmi, bool value);
#endif
class ViewModelInstanceBoolean : public ViewModelInstanceBooleanBase
{
protected:
    void propertyValueChanged() override;
#ifdef WITH_RIVE_TOOLS
public:
    void onChanged(ViewModelBooleanChanged callback) { m_changedCallback = callback; }
    ViewModelBooleanChanged m_changedCallback = nullptr;
#endif
};
} // namespace rive

#endif