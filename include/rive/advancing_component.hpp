#ifndef _RIVE_ADVANCING_COMPONENT_HPP_
#define _RIVE_ADVANCING_COMPONENT_HPP_

#include "rive/advance_flags.hpp"

namespace rive
{
class Component;
class Core;
class AdvancingComponent
{
public:
    virtual bool advanceComponent(
        float elapsedSeconds,
        AdvanceFlags flags = AdvanceFlags::Animate |
                             AdvanceFlags::NewFrame) = 0;

    // Whether advanceComponent would currently do nothing: `quiet` when it
    // would return false having changed nothing, `busy` when it would do work.
    // `never` is for components that can't say, which keeps the artboard
    // hosting them from ever being skipped (see ArtboardComponentList's quiet
    // rows).
    enum class QuietState
    {
        never,
        busy,
        quiet,
    };
    virtual QuietState quietState() { return QuietState::never; }
    static AdvancingComponent* from(Core* component);
};
} // namespace rive

#endif
