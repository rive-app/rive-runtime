#ifndef _RIVE_VIEW_MODEL_INSTANCE_NUMBER_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_NUMBER_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_number_base.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include <stdio.h>
namespace rive
{
#ifdef WITH_RIVE_TOOLS
class ViewModelInstanceNumber;
typedef void (*ViewModelNumberChanged)(ViewModelInstanceNumber* vmi,
                                       float value);
#endif
class ViewModelInstanceNumber : public ViewModelInstanceNumberBase
{
protected:
    void propertyValueChanged() override;

public:
#ifdef WITH_RIVE_TOOLS
    void onChanged(ViewModelNumberChanged callback)
    {
        m_changedCallback = callback;
    }
    ViewModelNumberChanged m_changedCallback = nullptr;
#endif
    void applyValue(DataValueNumber*);
};
} // namespace rive

#endif