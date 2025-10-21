#ifndef _RIVE_VIEW_MODEL_INSTANCE_ENUM_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_ENUM_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_enum_base.hpp"
#include "rive/data_bind/data_values/data_value_integer.hpp"
#include <stdio.h>
namespace rive
{
#ifdef WITH_RIVE_TOOLS
class ViewModelInstanceEnum;
typedef void (*ViewModelEnumChanged)(ViewModelInstanceEnum* vmi,
                                     uint32_t value);
#endif
class ViewModelInstanceEnum : public ViewModelInstanceEnumBase
{
public:
    bool value(std::string name);
    bool value(uint32_t index);
    void applyValue(DataValueInteger*);

protected:
    void propertyValueChanged() override;
#ifdef WITH_RIVE_TOOLS
public:
    void onChanged(ViewModelEnumChanged callback)
    {
        m_changedCallback = callback;
    }
    ViewModelEnumChanged m_changedCallback = nullptr;
#endif
};
} // namespace rive

#endif