#ifndef _RIVE_SCRIPTED_PATH_EFFECT_HPP_
#define _RIVE_SCRIPTED_PATH_EFFECT_HPP_
#include "rive/generated/scripted/scripted_path_effect_base.hpp"
#include "rive/scripted/scripted_object.hpp"
#include "rive/advancing_component.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/paint/stroke_effect.hpp"
#include <stdio.h>
namespace rive
{

class ScriptedEffectPath : public EffectPath
{
public:
    void invalidateEffect() override;
    ShapePaintPath* path() override { return &m_path; }

private:
    ShapePaintPath m_path;
};
class ScriptedPathEffect : public ScriptedPathEffectBase,
                           public ScriptedObject,
                           public AdvancingComponent,
                           public StrokeEffect
{
public:
#ifdef WITH_RIVE_SCRIPTING
    bool scriptInit(LuaState* state) override;
#endif
    void addProperty(CustomProperty* prop) override;
    StatusCode onAddedClean(CoreContext* context) override;
    StatusCode onAddedDirty(CoreContext* context) override;
    uint32_t assetId() override { return scriptAssetId(); }
    bool advanceComponent(float elapsedSeconds,
                          AdvanceFlags flags = AdvanceFlags::Animate |
                                               AdvanceFlags::NewFrame) override;
    bool addScriptedDirt(ComponentDirt value, bool recurse = false) override;
    DataContext* dataContext() override
    {
        if (artboard() != nullptr)
        {
            return artboard()->dataContext();
        }
        return nullptr;
    }
    ScriptProtocol scriptProtocol() override
    {
        return ScriptProtocol::pathEffect;
    }
    void updateEffect(PathProvider* pathProvider,
                      const ShapePaintPath* source,
                      ShapePaintType shapePaintType) override;
    StatusCode import(ImportStack& importStack) override;
    Core* clone() const override;
    void markNeedsUpdate() override;
    Component* component() override { return this; }
    EffectsContainer* parentPaint() override;

protected:
    virtual EffectPath* createEffectPath() override;
};
} // namespace rive

#endif