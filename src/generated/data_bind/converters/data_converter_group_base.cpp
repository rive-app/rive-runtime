#include "rive/generated/data_bind/converters/data_converter_group_base.hpp"
#include "rive/data_bind/converters/data_converter_group.hpp"

using namespace rive;

Core* DataConverterGroupBase::clone() const
{
    auto cloned = new DataConverterGroup();
    cloned->copy(*this);
    return cloned;
}
