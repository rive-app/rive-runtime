#ifndef _RIVE_VIEW_MODEL_INSTANCE_STRING_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_STRING_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_string_base.hpp"
#include <stdio.h>
namespace rive
{
#ifdef WITH_RIVE_TOOLS
class ViewModelInstanceString;
typedef void (*ViewModelStringChanged)(ViewModelInstanceString* vmi, const char* value);
#endif
class ViewModelInstanceString : public ViewModelInstanceStringBase
{
public:
    void propertyValueChanged() override;
#ifdef WITH_RIVE_TOOLS
public:
    void onChanged(ViewModelStringChanged callback) { m_changedCallback = callback; }
    ViewModelStringChanged m_changedCallback = nullptr;
#endif
};
} // namespace rive

#endif