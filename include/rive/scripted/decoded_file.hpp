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
rcp<File> importDecodedFile(Span<const uint8_t> bytes,
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

} // namespace rive

#endif
