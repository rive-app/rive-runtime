#ifndef _RIVE_SCRIPTED_DRAWABLE_HPP_
#define _RIVE_SCRIPTED_DRAWABLE_HPP_
#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#endif
#include "rive/generated/scripted/scripted_drawable_base.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/advancing_component.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/listener_group.hpp"
#include "rive/scripted/scripted_object.hpp"
#include "rive/custom_property_group.hpp"
#include <stdio.h>
namespace rive
{
class Component;
class HitComponent;
class HitScriptedDrawable;

class ScriptedDrawable : public ScriptedDrawableBase,
                         public ScriptedObject,
                         public AdvancingComponent,
                         public ListenerGroupProvider
{
public:
#ifdef WITH_RIVE_SCRIPTING
    bool scriptInit(LuaState* state) override;
#endif
    void draw(Renderer* renderer) override;
    void update(ComponentDirt value) override;
    Core* hitTest(HitInfo*, const Mat2D&) override;
    uint32_t assetId() override { return scriptAssetId(); }
    StatusCode onAddedDirty(CoreContext* context) override;
    bool advanceComponent(float elapsedSeconds,
                          AdvanceFlags flags = AdvanceFlags::Animate |
                                               AdvanceFlags::NewFrame) override;
    StatusCode import(ImportStack& importStack) override;
    Core* clone() const override;
    const std::vector<Component*>& containerChildren() const override
    {
        return children();
    }
    bool addScriptedDirt(ComponentDirt value, bool recurse = false) override;
    DataContext* dataContext() override
    {
        if (artboard() != nullptr)
        {
            return artboard()->dataContext();
        }
        return nullptr;
    }
    ScriptType scriptType() override { return ScriptType::drawing; }
    std::vector<ListenerGroupWithTargets*> listenerGroups() override
    {
        return {};
    }
    std::vector<HitComponent*> hitComponents(StateMachineInstance* sm) override;
    bool worldToLocal(Vec2D world, Vec2D* local);
};

class HitScriptedDrawable : public HitComponent
{
private:
    ScriptedDrawable* m_drawable;
    bool handlesEvent(bool canHit, ListenerType hitEvent);
    std::string methodName(bool canHit, ListenerType hitEvent);

public:
    HitScriptedDrawable(ScriptedDrawable* drawable,
                        StateMachineInstance* stateMachineInstance) :
        HitComponent(drawable, stateMachineInstance)
    {
        this->m_drawable = drawable;
    }

    bool hitTest(Vec2D position) const override { return true; }
    void prepareEvent(Vec2D position,
                      ListenerType hitType,
                      int pointerId) override
    {}

    HitResult processEvent(Vec2D position,
                           ListenerType hitType,
                           bool canHit,
                           float timeStamp,
                           int pointerId) override;
};
} // namespace rive

#endif