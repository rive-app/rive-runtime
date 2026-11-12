#ifndef _RIVE_DEFERRED_ANIMATION_BODY_HPP_
#define _RIVE_DEFERRED_ANIMATION_BODY_HPP_

#include "rive/simple_array.hpp"
#include "rive/span.hpp"
#include <cstdint>
#include <functional>
#include <vector>

namespace rive
{
class Artboard;
class BinaryReader;
class File;
class RuntimeHeader;

/// The keyed body of a LinearAnimation (its KeyedObjects, KeyedProperties,
/// KeyFrames and the keyframes' binds) that File::import skipped, held as a
/// copy of its bytes until the animation is first used (see
/// LinearAnimation::ensureDecoded).
struct DeferredAnimationBody
{
  /// The body's records, from its first KeyedObject to its last KeyFrame or
  /// bind.
  SimpleArray<uint8_t> bytes;
  /// The file that decodes it. Its header's table of contents types the
  /// properties this runtime doesn't know. The file outlives its animations.
  File* file;
  /// The source artboard the body's object and interpolator ids resolve in.
  Artboard* artboard;
};

/// What skimAnimationBody found: either a body it can defer, or why the body
/// has to be decoded with the rest of the file instead.
enum class AnimationBodySkim : uint8_t
{
  deferred,
  /// No keyed objects at all, nothing to defer.
  empty,
  /// A bind on something other than a keyframe. This is not expected:
  /// LinearAnimation, KeyedObject and KeyedProperty have no bindable
  /// property, so neither exporter writes one. It's only a guard against
  /// damaged or hand made files, whose binds would land somewhere a later
  /// decode can't put them.
  dataBind,
  /// A type that isn't a keyed object, property, keyframe or bind,
  /// including types this runtime doesn't know.
  unexpectedType,
  /// Records that don't nest as KeyedObject > KeyedProperty > KeyFrame. Not
  /// expected (exporters write children after their parents); only a guard
  /// against damaged files.
  outOfOrder,
  /// A property the import would fail on: a reader error, an unknown key,
  /// or a value out of range.
  readError,
};
constexpr size_t kAnimationBodySkimCount = 6;

struct AnimationBodySkimResult
{
  AnimationBodySkim outcome = AnimationBodySkim::readError;
  /// The body's bytes in the stream, when deferred.
  Span<const uint8_t> bytes;
  /// Whether the body has a KeyedProperty (the import leaves a
  /// KeyedPropertyImporter on its stack after such a body).
  bool hasKeyedProperty = false;
  /// Whether any KeyedProperty keys a callback (events, audio...).
  bool hasKeyedCallbacks = false;
  /// Whether any keyframe has a bind.
  bool hasKeyFrameBinds = false;
  /// The ids of the objects whose layout clip the body keys: the import
  /// marks those layouts as it would decoding the body, before the
  /// artboard's one-time drawable proxy injection.
  std::vector<uint32_t> keyedClipObjectIds;
};

/// Reads past the body that follows a LinearAnimation record without
/// creating any objects. The body ends at the next LinearAnimation,
/// StateMachine or Artboard, or at the end of the stream. When the outcome is
/// deferred, [reader] is left at the end of the body; otherwise it is left
/// where it was, for the body to be decoded as usual.
AnimationBodySkimResult skimAnimationBody(
    BinaryReader& reader,
    const RuntimeHeader& header);

/// Calls [visit] with the object id and property key of each keyed property
/// in [body] that has a keyframe and that decoding the body would keep (see
/// KeyedObject::onAddedDirty): its object resolves in the body's artboard
/// and supports the property, and no keyframe of that object's kept
/// properties names an interpolator that doesn't resolve, which drops the
/// whole keyed object. Reads the bytes where they lie and creates nothing.
/// When given, [visitBind] gets the converter id of each bind on a keyframe
/// of those properties: the binds the decode keeps.
void forEachKeptKeyedProperty(
    const DeferredAnimationBody& body,
    const RuntimeHeader& header,
    const std::function<void(uint32_t objectId, uint32_t propertyKey)>& visit,
    const std::function<void(uint32_t converterId)>& visitBind = nullptr);
} // namespace rive

#endif
