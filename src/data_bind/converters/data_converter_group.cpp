#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_group.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"

using namespace rive;

DataConverterGroup::~DataConverterGroup()
{
    for (auto& item : m_items)
    {
        delete item;
    }
}

void DataConverterGroup::addItem(DataConverterGroupItem* item)
{
    m_items.push_back(item);
}

DataValue* DataConverterGroup::convert(DataValue* input, DataBind* dataBind)
{
    DataValue* value = input;
    for (auto& item : m_items)
    {
        if (item->converter() != nullptr)
        {
            value = item->converter()->convert(value, dataBind);
        }
    }
    return value;
}

DataValue* DataConverterGroup::reverseConvert(DataValue* input,
                                              DataBind* dataBind)
{
    DataValue* value = input;
    for (auto it = m_items.rbegin(); it != m_items.rend(); ++it)
    {
        if ((*it)->converter() != nullptr)
        {
            value = (*it)->converter()->reverseConvert(value, dataBind);
        }
    }
    return value;
}

Core* DataConverterGroup::clone() const
{
    auto cloned = DataConverterGroupBase::clone()->as<DataConverterGroup>();
    for (auto& item : m_items)
    {
        if (item->converter() == nullptr)
        {
            continue;
        }
        auto clonedItem = item->clone()->as<DataConverterGroupItem>();
        cloned->addItem(clonedItem);
    }
    return cloned;
}

void DataConverterGroup::bindFromContext(DataContext* dataContext,
                                         DataBind* dataBind)
{
    for (auto& item : m_items)
    {
        auto converter = item->converter();
        if (converter != nullptr)
        {
            converter->bindFromContext(dataContext, dataBind);
        }
    }
}

void DataConverterGroup::update()
{
    for (auto& item : m_items)
    {
        auto converter = item->converter();
        if (converter != nullptr)
        {
            converter->update();
        }
    }
}

bool DataConverterGroup::advance(float elapsedSeconds)
{
    bool didUpdate = false;
    for (auto& item : m_items)
    {
        auto converter = item->converter();
        if (converter != nullptr)
        {
            if (converter->advance(elapsedSeconds))
            {
                didUpdate = true;
            }
        }
    }
    return didUpdate;
}