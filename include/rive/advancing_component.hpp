#ifndef _RIVE_ADVANCING_COMPONENT_HPP_
#define _RIVE_ADVANCING_COMPONENT_HPP_

#include "rive/advance_flags.hpp"

namespace rive
{
class Component;
class AdvancingComponent
{
public:
    virtual bool advanceComponent(
        float elapsedSeconds,
        AdvanceFlags flags = AdvanceFlags::Animate |
                             AdvanceFlags::NewFrame) = 0;
    static AdvancingComponent* from(Component* component);
};
} // namespace rive

#endif
