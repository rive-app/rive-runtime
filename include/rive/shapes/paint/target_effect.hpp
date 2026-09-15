#ifndef _RIVE_TARGET_EFFECT_HPP_
#define _RIVE_TARGET_EFFECT_HPP_
#include "rive/generated/shapes/paint/target_effect_base.hpp"
#include "rive/shapes/paint/stroke_effect.hpp"
#ifdef WITH_RIVE_EDITOR
#include "rive/editor/object_arena.hpp"
#endif
#include <stdio.h>
namespace rive
{
class GroupEffect;
class TargetEffectPath : public EffectPath
{
public:
    PathProvider* pathProviderProxy() { return &m_pathProviderProxy; }

private:
    PathProvider m_pathProviderProxy;
};
class TargetEffect : public TargetEffectBase, public StrokeEffect
{
public:
    StatusCode onAddedClean(CoreContext* context) override;

    void updateEffect(PathProvider* pathProvider,
                      const ShapePaintPath* source,
                      const ShapePaint* shapePaint) override;
    ShapePaintPath* effectPath(PathProvider* pathProvider) override;
    EffectsContainer* parentPaint() override;
    void addPathProvider(PathProvider* component) override;
    void invalidateEffect(PathProvider* component) override;

protected:
    virtual EffectPath* createEffectPath() override;
#ifdef WITH_RIVE_EDITOR
    // Body in editor_native/native/src/editor/shapes/paint/
    // target_effect_editor.cpp.
    GroupEffect* groupEffect() const;
    void setGroupEffectForEditor(GroupEffect* g);
#else
    inline GroupEffect* groupEffect() const { return m_groupEffect; }
#endif

private:
    GroupEffect* m_groupEffect = nullptr;
#ifdef WITH_RIVE_EDITOR
    // Slice 6 Phase E dual-storage. See targeted_constraint.hpp.
    CoreHandle m_groupEffectHandle;
#endif
};
} // namespace rive

#endif
