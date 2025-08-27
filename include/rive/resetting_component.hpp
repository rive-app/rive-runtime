#ifndef _RIVE_RESETTING_COMPONENT_HPP_
#define _RIVE_RESETTING_COMPONENT_HPP_

namespace rive
{
class Component;
class ResettingComponent
{
public:
    virtual void reset() = 0;
    static ResettingComponent* from(Component* component);
};
} // namespace rive

#endif
