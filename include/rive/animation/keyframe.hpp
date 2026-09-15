#ifndef _RIVE_KEY_FRAME_HPP_
#define _RIVE_KEY_FRAME_HPP_
#include "rive/generated/animation/keyframe_base.hpp"
namespace rive
{
class KeyFrame : public KeyFrameBase
{
public:
    inline float seconds() const { return m_seconds; }

    void computeSeconds(int fps);

    StatusCode import(ImportStack& importStack) override;

#ifdef WITH_RIVE_EDITOR
    // Edit-time only. Mirrors Dart's `KeyFrame.frameChanged` /
    // `KeyFrame.updateSeconds` (rive_core/lib/animation/keyframe.dart).
    // When the user (or coop) writes a new `frame` value through the
    // generated setter, we resolve the parent chain (KeyedProperty →
    // KeyedObject → LinearAnimation), recompute `m_seconds = frame /
    // animation.fps`, and tell the parent KeyedProperty its order is
    // dirty so the editor list is re-sorted before the next apply.
    //
    // The runtime build *never* compiles this override — under runtime
    // builds the base class's empty `frameChanged()` no-ops, since
    // runtime keyframes are imported once and `m_seconds` is locked in
    // by `computeSeconds(fps)` from `keyed_property_importer.cpp`.
    // This keeps the runtime hot path free of the parent-chain walk.
    void frameChanged() override;
#endif

private:
    float m_seconds;
};
} // namespace rive

#endif