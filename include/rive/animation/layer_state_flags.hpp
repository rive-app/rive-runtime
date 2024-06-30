#ifndef _RIVE_LAYER_STATE_FLAGS_HPP_
#define _RIVE_LAYER_STATE_FLAGS_HPP_

#include <type_traits>

namespace rive
{
enum class LayerStateFlags : unsigned char
{
    None = 0,

    /// Whether the transition is disabled.
    Random = 1 << 0,

    /// Whether the blend should include an instance to reset values on apply
    Reset = 1 << 1,
};

inline constexpr LayerStateFlags operator&(LayerStateFlags lhs, LayerStateFlags rhs)
{
    return static_cast<LayerStateFlags>(
        static_cast<std::underlying_type<LayerStateFlags>::type>(lhs) &
        static_cast<std::underlying_type<LayerStateFlags>::type>(rhs));
}
} // namespace rive
#endif