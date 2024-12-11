#ifndef _RIVE_BLEND_STATE1_DVIEW_MODEL_HPP_
#define _RIVE_BLEND_STATE1_DVIEW_MODEL_HPP_
#include "rive/generated/animation/blend_state_1d_viewmodel_base.hpp"
#include "rive/data_bind/bindable_property.hpp"
#include <stdio.h>
namespace rive
{
class BlendState1DViewModel : public BlendState1DViewModelBase
{
public:
    StatusCode import(ImportStack& importStack) override;

    BindableProperty* bindableProperty() const { return m_bindableProperty; };

protected:
    BindableProperty* m_bindableProperty;
};
} // namespace rive

#endif