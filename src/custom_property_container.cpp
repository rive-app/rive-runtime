#include "rive/component.hpp"
#include "rive/custom_property.hpp"
#include "rive/custom_property_container.hpp"

using namespace rive;

void CustomPropertyContainer::syncCustomProperties()
{
    m_customProperties.clear();
    for (auto child : containerChildren())
    {
        if (child->is<CustomProperty>())
        {
            m_customProperties.push_back(child->as<CustomProperty>());
        }
    }
}