#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance_enum.hpp"
#include "rive/viewmodel/viewmodel_property_enum.hpp"
#include "rive/component_dirt.hpp"

using namespace rive;

void ViewModelInstanceEnum::propertyValueChanged() { addDirt(ComponentDirt::Bindings); }

bool ViewModelInstanceEnum::value(std::string name)
{
    auto enumProperty = viewModelProperty()->as<ViewModelPropertyEnum>();
    int index = enumProperty->valueIndex(name);
    if (index != -1)
    {
        propertyValue(index);
        return true;
    }
    return false;
}

bool ViewModelInstanceEnum::value(uint32_t index)
{
    auto enumProperty = viewModelProperty()->as<ViewModelPropertyEnum>();
    if (enumProperty->valueIndex(index) != -1)
    {
        propertyValue(index);
        return true;
    }
    return false;
}