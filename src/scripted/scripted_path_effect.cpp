#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#endif
#include "rive/component_dirt.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/scripted/scripted_path_effect.hpp"
#include "rive/shapes/paint/stroke.hpp"

using namespace rive;

void ScriptedEffectPath::invalidateEffect() { m_path.rewind(); }

#ifdef WITH_RIVE_SCRIPTING
bool ScriptedPathEffect::scriptInit(ScriptingVM* vm)
{
    ScriptedObject::scriptInit(vm);
    addScriptedDirt(ComponentDirt::Paint, true);
    return true;
}

void ScriptedPathEffect::updateEffect(PathProvider* pathProvider,
                                      const ShapePaintPath* source,
                                      const ShapePaint* shapePaint)
{
    if (!updates())
    {
        return;
    }

    auto effectPathIt = m_effectPaths.find(pathProvider);
    if (effectPathIt != m_effectPaths.end())
    {
        auto trimEffectPath =
            static_cast<ScriptedEffectPath*>(effectPathIt->second);
        auto path = trimEffectPath->path();
        if (path->hasRenderPath())
        {
            // Previous result hasn't been invalidated, it's still good.
            return;
        }

        path->rewind(source->isLocal(), source->fillRule());
        lua_State* L = state();
        if (L == nullptr)
        {
            return;
        }
        // Stack: []
        rive_lua_pushRef(L, m_self);
        // Stack: [self]
        lua_getfield(L, -1, "update");
        // Stack: [self, "update"]
        lua_pushvalue(L, -2);
        // Stack: [self, "update", self]
        lua_newrive<ScriptedPathData>(L, source->rawPath());
        // Stack: [self, "update", self, pathData]
        lua_newrive<ScriptedNode>(L,
                                  nullptr,
                                  shapePaint->parentTransformComponent());
        auto scriptedNode = lua_torive<ScriptedNode>(L, -1);
        scriptedNode->shapePaint(shapePaint);
        // Stack: [self, "update", self, pathData, node]
        if (static_cast<lua_Status>(rive_lua_pcall(L, 3, 1)) != LUA_OK)
        {
            fprintf(stderr, "update function failed\n");
        }
        else
        {
            // Stack: [self, outputPathData]
            auto scriptedPath = (ScriptedPathData*)lua_touserdata(L, -1);
            auto rawPath = path->mutableRawPath();
            rawPath->addPath(scriptedPath->rawPath);
        }
        // Stack: [self, status] or [self, outputPathData]
        rive_lua_pop(L, 2);
    }
}
#else
void ScriptedPathEffect::updateEffect(PathProvider* pathProvider,
                                      const ShapePaintPath* source,
                                      const ShapePaint* shapePaint)
{}
#endif

StatusCode ScriptedPathEffect::onAddedDirty(CoreContext* context)
{
    auto code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    artboard()->addScriptedObject(this);
    return StatusCode::Ok;
}

StatusCode ScriptedPathEffect::onAddedClean(CoreContext* context)
{
    auto effectsContainer = EffectsContainer::from(parent());
    if (!effectsContainer)
    {
        return StatusCode::InvalidObject;
    }
    effectsContainer->addStrokeEffect(this);

    return StatusCode::Ok;
}

bool ScriptedPathEffect::advanceComponent(float elapsedSeconds,
                                          AdvanceFlags flags)
{
    if (elapsedSeconds == 0)
    {
        return false;
    }
    if ((flags & AdvanceFlags::AdvanceNested) == 0)
    {
        elapsedSeconds = 0;
    }
    return scriptAdvance(elapsedSeconds);
}

bool ScriptedPathEffect::addScriptedDirt(ComponentDirt value, bool recurse)
{
    return Component::addDirt(value, recurse);
}

void ScriptedPathEffect::addProperty(CustomProperty* prop)
{
    auto scriptInput = ScriptInput::from(prop);
    if (scriptInput != nullptr)
    {
        scriptInput->scriptedObject(this);
    }
    CustomPropertyContainer::addProperty(prop);
}

StatusCode ScriptedPathEffect::import(ImportStack& importStack)
{
    auto result = registerReferencer(importStack);
    if (result != StatusCode::Ok)
    {
        return result;
    }
    return Super::import(importStack);
}

Core* ScriptedPathEffect::clone() const
{
    ScriptedPathEffect* twin =
        ScriptedPathEffectBase::clone()->as<ScriptedPathEffect>();
    if (m_fileAsset != nullptr)
    {
        twin->setAsset(m_fileAsset);
    }
    return twin;
}

void ScriptedPathEffect::markNeedsUpdate()
{
    addScriptedDirt(ComponentDirt::ScriptUpdate);
}

EffectsContainer* ScriptedPathEffect::parentPaint()
{
    return EffectsContainer::from(parent());
}

EffectPath* ScriptedPathEffect::createEffectPath()
{
    return new ScriptedEffectPath();
}

void ScriptedPathEffect::buildDependencies()
{
    Super::buildDependencies();
    if (parent())
    {
        parent()->addDependent(this);
    }
}

void ScriptedPathEffect::update(ComponentDirt value)
{
    Super::update(value);

    if (hasDirt(value, ComponentDirt::ScriptUpdate))
    {
        invalidateEffectFromLocal();
    }
}