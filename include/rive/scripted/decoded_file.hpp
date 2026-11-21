#ifndef _RIVE_DECODED_FILE_HPP_
#define _RIVE_DECODED_FILE_HPP_

#include "rive/animation/state_machine_instance.hpp"
#include "rive/file.hpp"
#include "rive/refcnt.hpp"
#include "rive/span.hpp"

#include <cstring>
#include <memory>

namespace rive
{
class BindableArtboard;
class ViewModelInstance;

/// context:decodeFile's import, shared by the script backends: embedded
/// assets only (no loader), a VM of its own for any scripts, and only
/// Rive-signed scripts run, whatever this build allows otherwise.
rcp<File> importDecodedFile(
    Span<const uint8_t> bytes,
    Factory* factory,
    ImportResult* result);

/// A decoded file's artboard with an instance of its own view model, which
/// the artboard's binds name; the host's would resolve nothing, or the
/// wrong properties.
struct DecodedBindable
{
  rcp<BindableArtboard> artboard;
  rcp<ViewModelInstance> viewModel;
};

// The lookups below take a name as its first length bytes, which may hold any
// character, NUL included, so a name a script passes is matched whole; a null
// name asks for the file's default artboard.

/// The named artboard, or the file's default one when name is null; an
/// empty artboard when there is no such artboard.
DecodedBindable decodedBindable(
    const File& file,
    const char* name,
    size_t length);

/// One of the script's own file's artboards as a bindable, by name or the
/// file's default artboard when name is null; null when there is no such
/// artboard. It holds no instance (the host makes one when it mounts it, so
/// the lookup runs no script init), pins no file, so a host mounting it
/// treats it as one of its own, and carries no view model, so the host binds
/// it as it binds an authored artboard id: to its stateful child or its data
/// context.
rcp<BindableArtboard> hostBindableArtboard(
    const File& file,
    const char* name,
    size_t length);

/// A decoded file's artboard as a root a script owns, set up as a host sets
/// up one it loads: its default state machine bound to its own view model
/// instance and file's globals, under no parent data context.
struct DecodedRoot
{
  DecodedRoot() = default;
  DecodedRoot(DecodedRoot&&) = default;
  // Member-wise, it frees artboard before the old stateMachine that touches it.
  DecodedRoot& operator=(DecodedRoot&&) = delete;

  std::unique_ptr<ArtboardInstance> artboard;
  /// Null when the artboard has no default state machine; the artboard is
  /// then bound directly.
  std::unique_ptr<StateMachineInstance> stateMachine;
  /// The main view model instance, null when the artboard has no view
  /// model.
  rcp<ViewModelInstance> viewModel;
};

/// The named artboard of file, or its default one when name is null, as a
/// root bound to the file's default instance of its view model; empty when
/// there is no such artboard.
DecodedRoot decodedRoot(const File& file, const char* name, size_t length);

/// The same for a NUL-terminated name, as C++ callers have.
inline DecodedRoot decodedRoot(const File& file, const char* name)
{
  return decodedRoot(file, name, name != nullptr ? std::strlen(name) : 0);
}

/// Another root of like's artboard, bound to main when main is an instance
/// of one of file's view models, else as decodedRoot; empty when like is not
/// from file.
DecodedRoot decodedRootLike(
    const File& file,
    const ArtboardInstance& like,
    rcp<ViewModelInstance> main);

} // namespace rive

#endif
