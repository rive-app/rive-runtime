#ifndef _RIVE_VIEW_MODEL_INSTANCE_COLOR_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_COLOR_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_color_base.hpp"
#include <stdio.h>
namespace rive
{
#ifdef WITH_RIVE_TOOLS
class ViewModelInstanceColor;
typedef void (*ViewModelColorChanged)(ViewModelInstanceColor* vmi, int value);
#endif
class ViewModelInstanceColor : public ViewModelInstanceColorBase
{
public:
    void propertyValueChanged() override;
#ifdef WITH_RIVE_TOOLS
public:
    void onChanged(ViewModelColorChanged callback) { m_changedCallback = callback; }
    ViewModelColorChanged m_changedCallback = nullptr;
#endif
};
} // namespace rive

#endif