#include "rive/viewmodel/viewmodel_property_enum.hpp"

using namespace rive;

void ViewModelPropertyEnum::dataEnum(DataEnum* value) { m_DataEnum = value; }

DataEnum* ViewModelPropertyEnum::dataEnum() { return m_DataEnum; }

std::string ViewModelPropertyEnum::value(std::string name)
{
    if (dataEnum() != nullptr)
    {
        return dataEnum()->value(name);
    }
    return "";
}

std::string ViewModelPropertyEnum::value(uint32_t index)
{
    if (dataEnum() != nullptr)
    {
        return dataEnum()->value(index);
    }
    return "";
}

bool ViewModelPropertyEnum::value(std::string name, std::string value)
{
    if (dataEnum() != nullptr)
    {
        return dataEnum()->value(name, value);
    }
    return false;
}

bool ViewModelPropertyEnum::value(uint32_t index, std::string value)
{
    if (dataEnum() != nullptr)
    {
        return dataEnum()->value(index, value);
    }
    return false;
}

int ViewModelPropertyEnum::valueIndex(std::string name)
{
    if (dataEnum() != nullptr)
    {
        return dataEnum()->valueIndex(name);
    }
    return -1;
}

int ViewModelPropertyEnum::valueIndex(uint32_t index)
{
    if (dataEnum() != nullptr)
    {
        return dataEnum()->valueIndex(index);
    }
    return -1;
}
