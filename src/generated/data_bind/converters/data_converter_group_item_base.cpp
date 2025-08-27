#include "rive/generated/data_bind/converters/data_converter_group_item_base.hpp"
#include "rive/data_bind/converters/data_converter_group_item.hpp"

using namespace rive;

Core* DataConverterGroupItemBase::clone() const
{
    auto cloned = new DataConverterGroupItem();
    cloned->copy(*this);
    return cloned;
}
