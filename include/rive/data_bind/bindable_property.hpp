#ifndef _RIVE_BINDABLE_PROPERTY_HPP_
#define _RIVE_BINDABLE_PROPERTY_HPP_
#include "rive/generated/data_bind/bindable_property_base.hpp"
#include "rive/data_bind/data_bind.hpp"
#include <stdio.h>
namespace rive
{
class BindableProperty : public BindablePropertyBase
{
public:
    void dataBind(DataBind* value) { m_dataBind = value; };
    DataBind* dataBind() { return m_dataBind; };

private:
    DataBind* m_dataBind;
};
} // namespace rive

#endif