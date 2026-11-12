#ifndef _RIVE_DEFERRED_VIEWMODEL_INSTANCE_BODY_HPP_
#define _RIVE_DEFERRED_VIEWMODEL_INSTANCE_BODY_HPP_

#include "rive/simple_array.hpp"
#include "rive/span.hpp"
#include <cstdint>

namespace rive
{
class BinaryReader;
class RuntimeHeader;

/// The body of a file level ViewModelInstance (its values, their list items
/// and their binds) that File::import skipped, held as a copy of its bytes
/// until the instance is first handed out (see ViewModelInstance::
/// ensureDecoded).
struct DeferredViewModelInstanceBody
{
  /// The body's records, from the first value to the last.
  SimpleArray<uint8_t> bytes;
  /// How many of the file's assets the import had read when it reached the
  /// instance: an asset value holds those (ViewModelInstanceAsset::import).
  uint32_t assetCount;
};

/// What skimViewModelInstanceBody found: either a body it can defer, or why
/// the body has to be decoded with the rest of the file instead.
enum class ViewModelInstanceBodySkim : uint8_t
{
  deferred,
  /// No values at all, nothing to defer.
  empty,
  /// A type this runtime doesn't know: the import reads it as a null object,
  /// which the instance's importers may take as their own.
  unexpectedType,
  /// A bind on the instance itself rather than on one of its values, or a
  /// list item before any list. Not expected (instances aren't bindable, and
  /// exporters write children after their parents); only a guard against
  /// damaged or hand made files, whose records would attach to whatever an
  /// earlier instance left on the import stack.
  outOfOrder,
  /// A property the import would fail on: a reader error, an unknown key,
  /// or a value out of range.
  readError,
};
constexpr size_t kViewModelInstanceBodySkimCount = 5;

struct ViewModelInstanceBodySkimResult
{
  ViewModelInstanceBodySkim outcome = ViewModelInstanceBodySkim::readError;
  /// The body's bytes in the stream, when deferred.
  Span<const uint8_t> bytes;
  /// How many values (list items included) and binds the body holds.
  uint32_t records = 0;
};

/// Reads past the body that follows a file level ViewModelInstance record
/// without creating any objects. The body is every value, list item, bind
/// and bind path after the record, and ends at the first other record (the
/// next instance, view model, artboard...) or at the end of the stream. When
/// the outcome is deferred, [reader] is left at the end of the body;
/// otherwise it is left where it was, for the body to be decoded as usual.
ViewModelInstanceBodySkimResult skimViewModelInstanceBody(
    BinaryReader& reader,
    const RuntimeHeader& header);
} // namespace rive

#endif
