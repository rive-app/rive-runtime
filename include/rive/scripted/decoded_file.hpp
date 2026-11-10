#ifndef _RIVE_DECODED_FILE_HPP_
#define _RIVE_DECODED_FILE_HPP_

#include "rive/file.hpp"
#include "rive/refcnt.hpp"
#include "rive/span.hpp"

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

/// The named artboard, or the file's default one when name is null; an
/// empty artboard when there is no such artboard.
DecodedBindable decodedBindable(const File& file, const char* name);

/// One of the script's own file's artboards as a bindable, by name or the
/// file's default artboard when name is null; null when there is no such
/// artboard. It holds no instance (the host makes one when it mounts it, so
/// the lookup runs no script init), pins no file, so a host mounting it
/// treats it as one of its own, and carries no view model, so the host binds
/// it as it binds an authored artboard id: to its stateful child or its data
/// context.
rcp<BindableArtboard> hostBindableArtboard(const File& file, const char* name);

} // namespace rive

#endif
