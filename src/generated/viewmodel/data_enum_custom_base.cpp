#include "rive/generated/viewmodel/data_enum_custom_base.hpp"
#include "rive/viewmodel/data_enum_custom.hpp"

using namespace rive;

Core* DataEnumCustomBase::clone() const
{
    auto cloned = new DataEnumCustom();
    cloned->copy(*this);
    return cloned;
}
