#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#endif
#include "rive/component_dirt.hpp"
#include "rive/layout_component.hpp"
#include "rive/scripted/scripted_layout.hpp"

using namespace rive;

#ifdef WITH_RIVE_SCRIPTING
bool ScriptedLayout::scriptInit(lua_State* state)
{
    ScriptedDrawable::scriptInit(state);
    if (parent() != nullptr && parent()->is<LayoutComponent>())
    {
        parent()->as<LayoutComponent>()->markLayoutNodeDirty(true);
    }
    return true;
}

void ScriptedLayout::callScriptedResize(Vec2D size)
{
    if (m_state == nullptr)
    {
        return;
    }
    rive_lua_pushRef(m_state, m_self);
    if (static_cast<lua_Type>(lua_getfield(m_state, -1, "resize")) !=
        LUA_TFUNCTION)
    {
        fprintf(stderr, "expected resize to be a function\n");
        rive_lua_pop(m_state, 1);
    }
    else
    {
        lua_pushvalue(m_state, -2);
        lua_pushvec2d(m_state, size);
        if (static_cast<lua_Status>(rive_lua_pcall(m_state, 2, 0)) != LUA_OK)
        {
            fprintf(stderr, "resize failed\n");
            rive_lua_pop(m_state, 1);
        }
    }
    rive_lua_pop(m_state, 1);
}

Vec2D ScriptedLayout::measureLayout(float width,
                                    LayoutMeasureMode widthMode,
                                    float height,
                                    LayoutMeasureMode heightMode)
{
    if (!measures() || m_state == nullptr)
    {
        return Vec2D(0, 0);
    }
    auto measuredWidth = std::numeric_limits<float>::max();
    auto measuredHeight = std::numeric_limits<float>::max();
    rive_lua_pushRef(m_state, m_self);
    if (static_cast<lua_Type>(lua_getfield(m_state, -1, "measure")) !=
        LUA_TFUNCTION)
    {
        fprintf(stderr, "expected measure to be a function\n");
        rive_lua_pop(m_state, 1);
    }
    else
    {
        lua_pushvalue(m_state, -2);
        if (static_cast<lua_Status>(rive_lua_pcall(m_state, 1, 1)) != LUA_OK)
        {
            fprintf(stderr, "measure failed\n");
        }
        else
        {
            if (static_cast<lua_Type>(lua_type(m_state, -1)) != LUA_TVECTOR)
            {
                fprintf(stderr, "expected measure to return a Vec2D\n");
            }
            else
            {
                auto size = lua_tovec2d(m_state, -1);
                measuredWidth = size->x;
                measuredHeight = size->y;
            }
        }
        rive_lua_pop(m_state, 1);
    }
    rive_lua_pop(m_state, 1);

    return Vec2D(std::min((widthMode == LayoutMeasureMode::undefined
                               ? std::numeric_limits<float>::max()
                               : width),
                          measuredWidth),
                 std::min((heightMode == LayoutMeasureMode::undefined
                               ? std::numeric_limits<float>::max()
                               : height),
                          measuredHeight));
}

void ScriptedLayout::controlSize(Vec2D size,
                                 LayoutScaleType widthScaleType,
                                 LayoutScaleType heightScaleType,
                                 LayoutDirection direction)
{
    m_size = size;
    callScriptedResize(size);
}
#else
Vec2D ScriptedLayout::measureLayout(float width,
                                    LayoutMeasureMode widthMode,
                                    float height,
                                    LayoutMeasureMode heightMode)
{
    return Vec2D((widthMode == LayoutMeasureMode::undefined
                      ? std::numeric_limits<float>::max()
                      : width),
                 (heightMode == LayoutMeasureMode::undefined
                      ? std::numeric_limits<float>::max()
                      : height));
}

void ScriptedLayout::controlSize(Vec2D size,
                                 LayoutScaleType widthScaleType,
                                 LayoutScaleType heightScaleType,
                                 LayoutDirection direction)
{
    m_size = size;
}
#endif

void ScriptedLayout::addProperty(CustomProperty* prop)
{
    auto scriptInput = ScriptInput::from(prop);
    if (scriptInput != nullptr)
    {
        scriptInput->scriptedObject(this);
    }
    CustomPropertyContainer::addProperty(prop);
}

Core* ScriptedLayout::clone() const
{
    ScriptedLayout* twin = ScriptedLayoutBase::clone()->as<ScriptedLayout>();
    if (m_fileAsset != nullptr)
    {
        twin->setAsset(m_fileAsset);
    }
    return twin;
}