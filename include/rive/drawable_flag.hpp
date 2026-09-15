#ifndef _RIVE_DRAWABLE_FLAGS_HPP_
#define _RIVE_DRAWABLE_FLAGS_HPP_

#include "rive/enums.hpp"

namespace rive
{
enum class DrawableFlag : unsigned short
{
    None = 0,

    /// Whether the component should be drawn
    Hidden = 1 << 0,

    /// Editor only
    Locked = 1 << 1,

    /// Editor only
    Disconnected = 1 << 2,

    /// Whether this Component lets hit events pass through to components behind
    /// it
    Opaque = 1 << 3,

    /// Whether the computed world bounds for a shape need to be recalculated
    /// Using Clean instead of dirty so it doesn't need to be initialized to 1
    WorldBoundsClean = 1 << 4,

    /// Whether an ArtboardComponentList participates in the layout above it
    /// even when a transparent container (group/Solo) sits in between. A list
    /// provides a layout node unconditionally, so a group between one and its
    /// layout is how older files ask for free-form items placed by x/y or a
    /// follow-path constraint. Unset (the default) keeps that behaviour.
    /// Bit index matches ComponentFlags.participatesInLayout, which is where
    /// the editor authors it.
    ParticipatesInLayout = 1 << 8,
};
} // namespace rive
#endif
