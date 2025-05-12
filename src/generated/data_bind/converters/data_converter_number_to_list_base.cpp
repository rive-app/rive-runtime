#include "rive/generated/data_bind/converters/data_converter_number_to_list_base.hpp"
#include "rive/data_bind/converters/data_converter_number_to_list.hpp"

using namespace rive;

Core* DataConverterNumberToListBase::clone() const
{
    auto cloned = new DataConverterNumberToList();
    cloned->copy(*this);
    return cloned;
}
