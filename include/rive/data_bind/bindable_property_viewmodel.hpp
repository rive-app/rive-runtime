#ifndef _RIVE_BINDABLE_PROPERTY_VIEW_MODEL_HPP_
#define _RIVE_BINDABLE_PROPERTY_VIEW_MODEL_HPP_
#include "rive/generated/data_bind/bindable_property_viewmodel_base.hpp"
#include <stdio.h>
namespace rive
{
class ViewModelInstance;
class BindablePropertyViewModel : public BindablePropertyViewModelBase
{
public:
    constexpr static uint32_t defaultValue = -1;

    void viewModelInstanceValue(ViewModelInstance* value)
    {
        m_viewModelInstance = value;
    }

    ViewModelInstance* viewModelInstanceValue() const
    {
        return m_viewModelInstance;
    }

    // Backward-compatible aliases.
    void viewModelInstance(ViewModelInstance* value)
    {
        viewModelInstanceValue(value);
    }

    ViewModelInstance* viewModelInstance() const
    {
        return viewModelInstanceValue();
    }

private:
    ViewModelInstance* m_viewModelInstance = nullptr;
};
} // namespace rive

#endif