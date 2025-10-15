#ifndef _RIVE_CUSTOM_PROPERTY_CONTAINER_HPP_
#define _RIVE_CUSTOM_PROPERTY_CONTAINER_HPP_
#include <stdio.h>
#include <vector>
namespace rive
{
class Component;
class CustomProperty;

class CustomPropertyContainer
{
protected:
    std::vector<CustomProperty*> m_customProperties;
    void syncCustomProperties();

public:
    virtual void addPropertyChild(Component* component) {}
    virtual const std::vector<Component*>& containerChildren() const
    {
        static const std::vector<Component*> emptyVec;
        return emptyVec;
    }
};
} // namespace rive

#endif