#ifndef _RIVE_ARTBOARD_CLUSTERS_HPP_
#define _RIVE_ARTBOARD_CLUSTERS_HPP_

#include <atomic>
#include <unordered_map>
#include <vector>

namespace rive
{
class Artboard;
class Core;
class DataBind;
class File;
class KeyFrame;
class LinearAnimation;
class StateMachine;
struct DeferredArtboardBody;

// A source whose body File::import skipped (FileImportOptions):
// the body until it is decoded on first use (Artboard::ensureDecoded), and
// what the import learned about it while skipping it.
struct ArtboardDeferral
{
  // Null once decoded, or when nothing was deferred. Instances of the
  // artboard, and the file's accessors, can first use it on different
  // threads: readers load it with acquire, and File::decodeArtboardBody
  // publishes the decoded body by storing null with release, under
  // deferredDecodeMutex().
  std::atomic<DeferredArtboardBody*> body{nullptr};
  // The file that decodes it. Set once at import, so it can be read without
  // the lock, unlike the body, which the decoding thread frees.
  File* file = nullptr;
  // What the skim saw in the body, so lookups that only care whether it
  // holds a text style paint or a listener needn't decode it.
  bool hasTextStylePaint = false;
  bool hasStateMachineListener = false;
  // The body failed to decode (a malformed file).
  bool failed = false;
  // Owned. Set when the body failed part way through initialize: an empty
  // artboard with the same record that instance() and the decoding
  // accessors hand out in its place (Artboard::decoded). Set before the
  // body is published as null, so whoever sees that sees this too.
  Artboard* standIn = nullptr;

  ArtboardDeferral() = default;
  // A copy of an artboard is never a copy of its pending body, which only
  // its source decodes and frees.
  ArtboardDeferral(const ArtboardDeferral&) {}
  ArtboardDeferral& operator=(const ArtboardDeferral&) { return *this; }
};

// Everything only a *source* artboard ever fills. An instance shares its
// source's animations and state machines rather than copying them, imports
// nothing (so rejects nothing), and resolves keyframe binds through
// Artboard::animationSource() -- so none of this is ever populated on an
// instance, and instances multiply by nested artboard and by
// ArtboardComponentList row. Hanging the lot off one 8 B Sidecar keeps that
// whole population paying a single null pointer.
//
// Declared here rather than in artboard.hpp so the containers stay off the
// public surface, the way LAIBindingExtras does for LinearAnimationInstance.
struct ArtboardSourceState
{
  // Owned by the source: the importer fills these and the source's
  // destructor deletes them. Instances only ever read them.
  std::vector<LinearAnimation*> animations;
  std::vector<StateMachine*> stateMachines;

  // Objects import rejected, retained purely so the destructor frees them.
  std::vector<Core*> invalidObjects;

  // Whether any of the animations has a bind on a keyframe (they own them,
  // see LinearAnimation::keyFrameBinds). Set as the source initializes, from
  // what each animation knew at import, so a deferred body needn't decode.
  bool hasKeyFrameBinds = false;

  ArtboardDeferral deferral;
};
} // namespace rive

#endif
