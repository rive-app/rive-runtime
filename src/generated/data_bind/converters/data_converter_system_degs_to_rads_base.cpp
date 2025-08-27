#include "rive/generated/data_bind/converters/data_converter_system_degs_to_rads_base.hpp"
#include "rive/data_bind/converters/data_converter_system_degs_to_rads.hpp"

using namespace rive;

Core* DataConverterSystemDegsToRadsBase::clone() const
{
    auto cloned = new DataConverterSystemDegsToRads();
    cloned->copy(*this);
    return cloned;
}
