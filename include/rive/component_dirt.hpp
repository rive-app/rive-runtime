#ifndef _RIVE_DIRTY_FLAGS_HPP_
#define _RIVE_DIRTY_FLAGS_HPP_
#include <type_traits>

namespace rive
{
enum class ComponentDirt : unsigned short
{
    None = 0,

    Dependents = 1 << 0,

    /// General flag for components are dirty (if this is up, the update
    /// cycle runs). It gets automatically applied with any other dirt.
    Components = 1 << 1,

    /// Draw order needs to be re-computed.
    DrawOrder = 1 << 2,

    /// Path is dirty and needs to be rebuilt.
    Path = 1 << 3,

    /// Skin needs to recompute bone transformations.
    Skin = 1 << 3,

    /// Vertices have changed, re-order cached lists.
    Vertices = 1 << 4,

    /// Used by any component that needs to recompute their local transform.
    /// Usually components that have their transform dirty will also have
    /// their worldTransform dirty.
    Transform = 1 << 5,

    /// Used by any component that needs to update its world transform.
    WorldTransform = 1 << 6,

    /// Marked when the stored render opacity needs to be updated.
    RenderOpacity = 1 << 7,

    /// Dirt used to mark some stored paint needs to be rebuilt or that we
    /// just want to trigger an update cycle so painting occurs.
    Paint = 1 << 8,

    /// Used by the gradients track when the stops need to be re-ordered.
    Stops = 1 << 9,

    /// Blend modes need to be updated
    // TODO: do we need this?
    // BlendMode = 1 << 9,

    // Everything is dirty.
    Filthy = 0xFFFF
};

inline constexpr ComponentDirt operator&(ComponentDirt lhs, ComponentDirt rhs)
{
    return static_cast<ComponentDirt>(static_cast<std::underlying_type<ComponentDirt>::type>(lhs) &
                                      static_cast<std::underlying_type<ComponentDirt>::type>(rhs));
}

inline constexpr ComponentDirt operator^(ComponentDirt lhs, ComponentDirt rhs)
{
    return static_cast<ComponentDirt>(static_cast<std::underlying_type<ComponentDirt>::type>(lhs) ^
                                      static_cast<std::underlying_type<ComponentDirt>::type>(rhs));
}

inline constexpr ComponentDirt operator|(ComponentDirt lhs, ComponentDirt rhs)
{
    return static_cast<ComponentDirt>(static_cast<std::underlying_type<ComponentDirt>::type>(lhs) |
                                      static_cast<std::underlying_type<ComponentDirt>::type>(rhs));
}

inline constexpr ComponentDirt operator~(ComponentDirt rhs)
{
    return static_cast<ComponentDirt>(~static_cast<std::underlying_type<ComponentDirt>::type>(rhs));
}

inline ComponentDirt& operator|=(ComponentDirt& lhs, ComponentDirt rhs)
{
    lhs = static_cast<ComponentDirt>(static_cast<std::underlying_type<ComponentDirt>::type>(lhs) |
                                     static_cast<std::underlying_type<ComponentDirt>::type>(rhs));

    return lhs;
}

inline ComponentDirt& operator&=(ComponentDirt& lhs, ComponentDirt rhs)
{
    lhs = static_cast<ComponentDirt>(static_cast<std::underlying_type<ComponentDirt>::type>(lhs) &
                                     static_cast<std::underlying_type<ComponentDirt>::type>(rhs));

    return lhs;
}

inline ComponentDirt& operator^=(ComponentDirt& lhs, ComponentDirt rhs)
{
    lhs = static_cast<ComponentDirt>(static_cast<std::underlying_type<ComponentDirt>::type>(lhs) ^
                                     static_cast<std::underlying_type<ComponentDirt>::type>(rhs));

    return lhs;
}
} // namespace rive
#endif