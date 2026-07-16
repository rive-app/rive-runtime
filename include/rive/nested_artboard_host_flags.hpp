#ifndef _RIVE_NESTED_ARTBOARD_HOST_FLAGS_HPP_
#define _RIVE_NESTED_ARTBOARD_HOST_FLAGS_HPP_

#include "rive/enums.hpp"
#include <cstdint>

namespace rive
{

/// Runtime-only flags stored in NestedArtboard::m_hostFlags (not serialized).
enum class NestedArtboardHostFlags : uint8_t
{
    none = 0,
    /// Stateful VM binding is scheduled and should run on next advance.
    pendingStatefulBinding = 1 << 0,
    /// artboardId is data-bound (VM artboard property); host needs a focus
    /// scope for runtime-swappable nested artboards.
    artboardDataBound = 1 << 1,
};

} // namespace rive

#endif
