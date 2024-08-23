#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/data_enum.hpp"
#include "rive/viewmodel/data_enum_value.hpp"
#include "rive/viewmodel/viewmodel_property.hpp"
#include "rive/backboard.hpp"
#include "rive/importers/backboard_importer.hpp"

using namespace rive;

void DataEnum::addValue(DataEnumValue* value) { m_Values.push_back(value); }

std::string DataEnum::value(std::string key)
{
    for (auto enumValue : m_Values)
    {
        if (enumValue->key() == key)
        {
            return enumValue->value();
        };
    }
    return "";
}

std::string DataEnum::value(uint32_t index)
{
    if (index < m_Values.size())
    {
        return m_Values[index]->value();
    }
    return "";
}

bool DataEnum::value(std::string key, std::string value)
{
    for (auto enumValue : m_Values)
    {
        if (enumValue->key() == key)
        {
            enumValue->value(value);
            return true;
        };
    }
    return false;
}

bool DataEnum::value(uint32_t index, std::string value)
{
    if (index < m_Values.size())
    {
        m_Values[index]->value(value);
        return true;
    }
    return false;
}

int DataEnum::valueIndex(std::string key)
{
    int index = 0;
    for (auto enumValue : m_Values)
    {
        if (enumValue->key() == key)
        {
            return index;
        };
        index++;
    }
    return -1;
}

int DataEnum::valueIndex(uint32_t index)
{
    if (index < m_Values.size())
    {
        return index;
    }
    return -1;
}