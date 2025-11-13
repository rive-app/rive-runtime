#ifndef _RIVE_SCRIPTED_DRAWABLE_HPP_
#define _RIVE_SCRIPTED_DRAWABLE_HPP_
#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#endif
#include "rive/generated/scripted/scripted_drawable_base.hpp"
#include "rive/advancing_component.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/scripted/scripted_object.hpp"
#include "rive/custom_property_group.hpp"
#include <stdio.h>
namespace rive
{
class ScriptedDrawable : public ScriptedDrawableBase,
                         public ScriptedObject,
                         public AdvancingComponent
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
};
} // namespace rive

#endif