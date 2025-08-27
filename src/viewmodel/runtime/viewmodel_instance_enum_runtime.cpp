
#include "rive/viewmodel/runtime/viewmodel_instance_enum_runtime.hpp"
#include "rive/viewmodel/viewmodel_property_enum.hpp"

// Default namespace for Rive Cpp code
using namespace rive;

std::vector<DataEnumValue*> ViewModelInstanceEnumRuntime::dataValues() const
{
    auto enumProperty = m_viewModelInstanceValue->viewModelProperty()
                            ->as<ViewModelPropertyEnum>();
    auto dataEnum = enumProperty->dataEnum();
    return dataEnum->values();
}

std::string ViewModelInstanceEnumRuntime::value() const
{
    auto values = dataValues();
    uint32_t index =
        m_viewModelInstanceValue->as<ViewModelInstanceEnum>()->propertyValue();
    if (index < values.size())
    {
        return values[index]->key();
    }
    return "";
}

void ViewModelInstanceEnumRuntime::value(std::string val)
{
    m_viewModelInstanceValue->as<ViewModelInstanceEnum>()->value(val);
}

uint32_t ViewModelInstanceEnumRuntime::valueIndex() const
{
    auto values = dataValues();
    uint32_t index =
        m_viewModelInstanceValue->as<ViewModelInstanceEnum>()->propertyValue();
    if (index < values.size())
    {
        return index;
    }
    return 0;
}

void ViewModelInstanceEnumRuntime::valueIndex(uint32_t index)
{
    auto values = dataValues();
    if (index < values.size())
    {
        m_viewModelInstanceValue->as<ViewModelInstanceEnum>()->value(
            values[index]->key());
    }
}

std::vector<std::string> ViewModelInstanceEnumRuntime::values() const
{
    std::vector<std::string> stringValues;
    for (auto value : dataValues())
    {
        stringValues.push_back(value->key());
    }
    return stringValues;
}

std::string ViewModelInstanceEnumRuntime::enumType() const
{
    auto enumProperty = m_viewModelInstanceValue->viewModelProperty()
                            ->as<ViewModelPropertyEnum>();
    assert(enumProperty);
    auto dataEnum = enumProperty->dataEnum();
    assert(dataEnum);
    return dataEnum->enumName();
}