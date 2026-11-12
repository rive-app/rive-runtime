#ifndef _RIVE_DEFERRED_ARTBOARD_BODY_HPP_
#define _RIVE_DEFERRED_ARTBOARD_BODY_HPP_

#include "rive/simple_array.hpp"
#include "rive/span.hpp"
#include <cstdint>
#include <vector>

namespace rive
{
class BinaryReader;
class File;
class RuntimeHeader;

/// The body of an Artboard (everything its run holds after its own record:
/// components, interpolators, animations, state machines) that File::import
/// skipped, held as a copy of its bytes until the artboard is first used
/// (see Artboard::ensureDecoded).
struct DeferredArtboardBody
{
  /// The run's records after the Artboard record, up to the next artboard.
  SimpleArray<uint8_t> bytes;
};

/// What skimArtboardBody found: either a body it can defer, or why the
/// artboard has to be decoded with the rest of the file instead.
enum class ArtboardBodySkim : uint8_t
{
  deferred,
  /// Nothing follows the record, nothing to defer.
  empty,
  /// A file level type (converters, assets, view models, enums...) inside
  /// the run. This is not expected: both exporters (the editor's and rml's)
  /// write every file level object before the first artboard. It's only a
  /// guard against damaged or hand made files. File level objects are
  /// numbered by where they sit in the stream, and the import appends them
  /// to the file's lists as it reads them, which a later decode can't do
  /// without shifting what everything after them refers to.
  fileLevel,
  /// A property the import would fail on: a reader error, an unknown key,
  /// or a value out of range.
  readError,
  /// The default artboard, decoded at import (never skimmed).
  defaultArtboard,
};
constexpr size_t kArtboardBodySkimCount = 5;

struct ArtboardBodySkimResult
{
  ArtboardBodySkim outcome = ArtboardBodySkim::readError;
  /// The body's bytes in the stream, when deferred.
  Span<const uint8_t> bytes;
  /// The artboard ids the body's nested artboards and artboard paints
  /// reference: its edges in the file's nesting graph, which the import
  /// needs whole to find cycles.
  std::vector<int> nestedArtboardIds;
  /// Whether the body holds a text style paint, which another artboard's
  /// text may look up as a global style without instancing this one.
  bool hasTextStylePaint = false;
  /// Whether the body holds a state machine listener (a style artboard
  /// needs one to listen to its text runs).
  bool hasStateMachineListener = false;
};

/// Reads past an artboard's body, the records after its Artboard record up to
/// the next Artboard record or the end of the stream, without creating any
/// objects. When the outcome is deferred, [reader] is left at the end of the
/// body; otherwise it is left where it was, for the body to be imported as
/// usual.
///
/// A deferred body decodes on its own (File::decodeArtboardBody), from an
/// empty import stack. A record that takes its parent from the stack (a
/// keyed object its animation, a blend state its bindable property...) and
/// finds none in its own artboard's run is dropped there, where an import
/// would hang it off whatever an earlier artboard's run left. Exporters write
/// parents first; real files without one are an exporter bug (a
/// BlendState1DViewModel, ListenerViewModelChange or BlendAnimationDirect
/// written without its bindable property).
ArtboardBodySkimResult skimArtboardBody(
    BinaryReader& reader,
    const RuntimeHeader& header);
} // namespace rive

#endif
