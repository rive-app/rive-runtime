#ifndef _RIVE_DEFERRED_DECODE_LOCK_HPP_
#define _RIVE_DEFERRED_DECODE_LOCK_HPP_

#include <mutex>

namespace rive
{
/// The one lock every first-use decode takes, of every file: an artboard's
/// body, an animation's keyed body, a file level view model instance's values
/// (FileImportOptions::deferDecoding). One lock rather than one per kind:
/// decodes of each kind clone data converters, and a scripted converter's
/// clone adds itself to its asset's referencer list, which isn't
/// synchronized. Recursive, as decoding one part can decode another: an
/// artboard initializing a Joystick decodes the Joystick's animations. Each
/// part is decoded once, so it is seldom held and never for long.
std::recursive_mutex& deferredDecodeMutex();
} // namespace rive

#endif
