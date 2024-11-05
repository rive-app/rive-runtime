#ifndef _RIVE_ADVANCING_COMPONENT_HPP_
#define _RIVE_ADVANCING_COMPONENT_HPP_

namespace rive
{
class Component;
class AdvancingComponent
{
public:
    virtual bool advanceComponent(float elapsedSeconds,
                                  bool animate = true,
                                  bool newFrame = true) = 0;
    static AdvancingComponent* from(Component* component);
};
} // namespace rive

#endif
