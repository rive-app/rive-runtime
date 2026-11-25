#ifndef _RIVE_BACKBOARD_RUNTIME_FLAGS_HPP_
#define _RIVE_BACKBOARD_RUNTIME_FLAGS_HPP_

#include <cstdint>

namespace rive
{

/// The bits of Backboard::runtimeFlags: how the runtime loads the file.
enum class BackboardRuntimeFlags : uint8_t
{
  none = 0,
  /// A nested artboard that starts collapsed (an inactive Solo option, or
  /// anything under one) is instanced the first time it is shown, instead of
  /// with the artboard it sits in (NestedArtboard::mountDeferred). Once
  /// instanced it stays instanced, so a Solo still keeps the state of the
  /// options it hides. A host bound to a view model artboard property is
  /// instanced as usual: the binding decides what it mounts. On by default
  /// (Backboard::runtimeFlags starts at 1), for files from 7.6 on: older ones
  /// instance every nested artboard up front (File::
  /// defersCollapsedNestedArtboards).
  deferCollapsedNestedArtboards = 1 << 0,
};

} // namespace rive

#endif
