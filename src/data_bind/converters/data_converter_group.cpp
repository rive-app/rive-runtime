#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_group.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"

using namespace rive;

void DataConverterGroup::addItem(DataConverterGroupItem* item) { m_items.push_back(item); }

DataValue* DataConverterGroup::convert(DataValue* input)
{
    DataValue* value = input;
    for (auto item : m_items)
    {
        value = item->converter()->convert(value);
    }
    return value;
}

DataValue* DataConverterGroup::reverseConvert(DataValue* input)
{
    DataValue* value = input;
    for (auto it = m_items.rbegin(); it != m_items.rend(); ++it)
    {
        value = (*it)->converter()->reverseConvert(value);
    }
    return value;
}