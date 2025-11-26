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
class ScriptedPathEffect : public ScriptedPathEffectBase,
                           public ScriptedObject,
                           public AdvancingComponent,
                           public StrokeEffect
{
public:
#ifdef WITH_RIVE_SCRIPTING
    bool scriptInit(LuaState* state) override;
#endif
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
    void invalidateEffect() override;
    void updateEffect(const ShapePaintPath* source,
                      ShapePaintType shapePaintType) override;
    ShapePaintPath* effectPath() override;
    StatusCode import(ImportStack& importStack) override;
    Core* clone() const override;
    void markNeedsUpdate() override;
    Component* component() override { return this; }
    ShapePaint* parentPaint() override
    {
        return parent() != nullptr ? parent()->as<ShapePaint>() : nullptr;
    }

private:
    ShapePaintPath m_path;
};
} // namespace rive

#endif