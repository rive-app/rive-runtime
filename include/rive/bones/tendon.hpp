#ifndef _RIVE_TENDON_HPP_
#define _RIVE_TENDON_HPP_

#include "rive/generated/bones/tendon_base.hpp"
#include "rive/math/mat2d.hpp"
#ifdef WITH_RIVE_EDITOR
#include "rive/editor/object_arena.hpp"
#endif
#include <stdio.h>

namespace rive
{
class Bone;
class Tendon : public TendonBase
{
private:
    Mat2D m_InverseBind;
    Bone* m_Bone = nullptr;
#ifdef WITH_RIVE_EDITOR
    // Slice 6 Phase E dual-storage. See targeted_constraint.hpp.
    CoreHandle m_BoneHandle;
#endif

public:
#ifdef WITH_RIVE_EDITOR
    // Body in editor_native/native/src/editor/bones/tendon_editor.cpp.
    Bone* bone() const;
    void setBoneForEditor(Bone* b);
#else
    inline Bone* bone() const { return m_Bone; }
#endif
    const Mat2D& inverseBind() const { return m_InverseBind; }
    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode onAddedClean(CoreContext* context) override;

#ifdef WITH_RIVE_EDITOR
    // Re-try bone resolution. `onAddedDirty` resolves `boneId → m_Bone`
    // exactly once; coop hydration can deliver a Tendon before its
    // Bone (bone added in a later batch, or an out-of-order single
    // batch where Pass-1 creation order puts the Tendon ahead of the
    // Bone). No-op if `m_Bone` is already set — editor_native's
    // `finalizeBatch` calls this across every Tendon before
    // `Skin::buildDependencies` runs, so a null `m_Bone` after Pass 3
    // is recoverable instead of crashing.
    void resolveBone(CoreContext* context);
#endif
};
} // namespace rive

#endif