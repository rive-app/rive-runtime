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
  /// Collapsed, the host's last advance noted where the change sequence
  /// stands and marked what it hosts quiet, and nothing in it has woken
  /// since: skipping its advance would note nothing the state machines
  /// inside read.
  inactiveMarked = 1 << 2,
};

} // namespace rive

#endif
