#include "rive/generated/viewmodel/data_enum_value_base.hpp"
#include "rive/viewmodel/data_enum_value.hpp"

using namespace rive;

Core* DataEnumValueBase::clone() const
{
    auto cloned = new DataEnumValue();
    cloned->copy(*this);
    return cloned;
}
