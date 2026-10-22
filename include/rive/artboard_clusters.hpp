#ifndef _RIVE_ARTBOARD_CLUSTERS_HPP_
#define _RIVE_ARTBOARD_CLUSTERS_HPP_

#include <unordered_map>
#include <vector>

namespace rive
{
class Core;
class DataBind;
class KeyFrame;
class LinearAnimation;
class StateMachine;

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

  // Index of the source's data binds that target keyframes, keyed by the
  // (shared) keyframe. Keyframe binds live in dataBinds() but are never
  // applied directly -- keyframes aren't Components -- so a playing
  // LinearAnimationInstance clones them onto per-instance holders and
  // resolves them through Artboard::keyFrameSourceBind. Built once, lazily;
  // `built` is what distinguishes "looked and found none" from "not looked
  // yet", since the map shares this allocation with the lists above.
  std::unordered_map<const KeyFrame*, DataBind*> keyFrameBinds;
  bool keyFrameBindsBuilt = false;
};
} // namespace rive

#endif
