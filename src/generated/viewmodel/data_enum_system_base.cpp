#include "rive/generated/viewmodel/data_enum_system_base.hpp"
#include "rive/viewmodel/data_enum_system.hpp"

using namespace rive;

Core* DataEnumSystemBase::clone() const
{
    auto cloned = new DataEnumSystem();
    cloned->copy(*this);
    return cloned;
}
