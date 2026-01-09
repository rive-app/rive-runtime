#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#endif
#include "rive/component_dirt.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/scripted/scripted_drawable.hpp"

using namespace rive;

#ifdef WITH_RIVE_SCRIPTING
bool ScriptedDrawable::scriptInit(LuaState* state)
{
    ScriptedObject::scriptInit(state);
    addDirt(ComponentDirt::Paint);
    return true;
}

void ScriptedDrawable::draw(Renderer* renderer)
{
    if (!draws())
    {
        return;
    }
    auto state = m_state->state;

    if (m_needsSaveOperation)
    {
        renderer->save();
    }
    renderer->transform(worldTransform());
    // Stack: []
    auto scriptedRenderer = lua_newrive<ScriptedRenderer>(state, renderer);
    // Stack: [scriptedRenderer]
    rive_lua_pushRef(state, m_self);
    // Stack: [scriptedRenderer, self]
    lua_getfield(state, -1, "draw");
    // Stack: [scriptedRenderer, self, "draw"]
    lua_pushvalue(state, -2);
    // Stack: [scriptedRenderer, self, "draw", self]
    lua_pushvalue(state, -4);
    // Stack: [scriptedRenderer, self, "draw", self, scriptedRenderer]
    if (static_cast<lua_Status>(rive_lua_pcall(state, 2, 0)) != LUA_OK)
    {
        // Stack: [scriptedRenderer, self, status]
        rive_lua_pop(state, 1);
    }
    scriptedRenderer->end();
    // Stack: [scriptedRenderer, self]
    rive_lua_pop(state, 2);
    if (m_needsSaveOperation)
    {
        renderer->restore();
    }
}

std::vector<HitComponent*> ScriptedDrawable::hitComponents(
    StateMachineInstance* sm)
{
    if (!listensToPointerEvents())
    {
        return {};
    }
    auto* hitComponent = new HitScriptedDrawable(this, sm);
    return std::vector<HitComponent*>{hitComponent};
}

HitResult HitScriptedDrawable::processEvent(Vec2D position,
                                            ListenerType hitType,
                                            bool canHit,
                                            float timeStamp,
                                            int pointerId)
{
    HitResult hitResult = HitResult::none;
    auto scriptAsset = m_drawable->scriptAsset();
    auto vm = m_drawable->state();
    if (vm == nullptr || scriptAsset == nullptr ||
        !handlesEvent(canHit, hitType))
    {
        return HitResult::none;
    }
    Vec2D localPos;
    auto hasLocalPos = m_drawable->worldToLocal(position, &localPos);
    if (!hasLocalPos)
    {
        return hitResult;
    }
    auto state = vm->state;
    rive_lua_pushRef(state, m_drawable->self());
    auto mName = methodName(canHit, hitType);
    if (static_cast<lua_Type>(lua_getfield(state, -1, mName.c_str())) !=
        LUA_TFUNCTION)
    {
        fprintf(stderr, "expected %s to be a function\n", mName.c_str());
        rive_lua_pop(state, 1);
    }
    else
    {
        lua_pushvalue(state, -2);
        auto pointerEvent =
            lua_newrive<ScriptedPointerEvent>(state, pointerId, localPos);
        if (static_cast<lua_Status>(rive_lua_pcall(state, 2, 0)) != LUA_OK)
        {
            fprintf(stderr, "%s failed\n", mName.c_str());
            rive_lua_pop(state, 1);
        }
        hitResult = pointerEvent->m_hitResult;
    }
    rive_lua_pop(state, 1);
    return hitResult;
}

bool ScriptedDrawable::willDraw()
{
    return Super::willDraw() && m_state != nullptr && draws();
}

#else
void ScriptedDrawable::draw(Renderer* renderer) {}

std::vector<HitComponent*> ScriptedDrawable::hitComponents(
    StateMachineInstance* sm)
{
    return {};
}

HitResult HitScriptedDrawable::processEvent(Vec2D position,
                                            ListenerType hitType,
                                            bool canHit,
                                            float timeStamp,
                                            int pointerId)
{
    return HitResult::none;
}

bool ScriptedDrawable::willDraw() { return Super::willDraw() && draws(); }

#endif

void ScriptedDrawable::update(ComponentDirt value)
{
    Super::update(value);
    if ((value & ComponentDirt::ScriptUpdate) == ComponentDirt::ScriptUpdate)
    {
        scriptUpdate();
        m_isAdvanceActive = true;
    }
}

Core* ScriptedDrawable::hitTest(HitInfo*, const Mat2D&) { return nullptr; }

StatusCode ScriptedDrawable::onAddedDirty(CoreContext* context)
{
    auto code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    artboard()->addScriptedObject(this);
    return StatusCode::Ok;
}

bool ScriptedDrawable::advanceComponent(float elapsedSeconds,
                                        AdvanceFlags flags)
{
    if (elapsedSeconds == 0)
    {
        return false;
    }
    if (!m_isAdvanceActive)
    {
        return false;
    }
    m_isAdvanceActive = false;
    if ((flags & AdvanceFlags::AdvanceNested) == 0)
    {
        elapsedSeconds = 0;
    }
    auto advanced = scriptAdvance(elapsedSeconds);
    if (advanced)
    {
        m_isAdvanceActive = true;
        addScriptedDirt(ComponentDirt::Paint);
    }
    return advanced;
}

bool ScriptedDrawable::addScriptedDirt(ComponentDirt value, bool recurse)
{
    return Drawable::addDirt(value, recurse);
}

void ScriptedDrawable::addProperty(CustomProperty* prop)
{
    auto scriptInput = ScriptInput::from(prop);
    if (scriptInput != nullptr)
    {
        scriptInput->scriptedObject(this);
    }
    CustomPropertyContainer::addProperty(prop);
}

StatusCode ScriptedDrawable::import(ImportStack& importStack)
{
    auto result = registerReferencer(importStack);
    if (result != StatusCode::Ok)
    {
        return result;
    }
    return Super::import(importStack);
}

Core* ScriptedDrawable::clone() const
{
    ScriptedDrawable* twin =
        ScriptedDrawableBase::clone()->as<ScriptedDrawable>();
    if (m_fileAsset != nullptr)
    {
        twin->setAsset(m_fileAsset);
    }
    return twin;
}

void ScriptedDrawable::markNeedsUpdate()
{
    addScriptedDirt(ComponentDirt::ScriptUpdate);
}

bool ScriptedDrawable::worldToLocal(Vec2D world, Vec2D* local)
{
    Mat2D toMountedArtboard;
    if (!worldTransform().invert(&toMountedArtboard))
    {
        return false;
    }

    *local = toMountedArtboard * world;

    return true;
}

bool HitScriptedDrawable::handlesEvent(bool canHit, ListenerType hitEvent)
{
    if (canHit)
    {
        switch (hitEvent)
        {
            case ListenerType::down:
                return m_drawable->wantsPointerDown();
            case ListenerType::up:
                return m_drawable->wantsPointerUp();
            case ListenerType::dragStart:
                return false;
            case ListenerType::dragEnd:
                return false;
            default:
                return m_drawable->wantsPointerMove();
        }
    }
    return m_drawable->wantsPointerExit();
}

std::string HitScriptedDrawable::methodName(bool canHit, ListenerType hitEvent)
{
    if (canHit)
    {
        switch (hitEvent)
        {
            case ListenerType::down:
                return "pointerDown";
            case ListenerType::up:
                return "pointerUp";
            case ListenerType::dragStart:
            case ListenerType::dragEnd:
                return "";
            default:
                return "pointerMove";
        }
    }
    return "pointerExit";
}