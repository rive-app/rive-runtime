#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#include "lualib.h"

namespace rive
{

void rive_lua_push_pointer_arg_for_perform(lua_State* L,
                                           const ListenerInvocation& inv)
{
    if (const PointerInvocation* p = inv.asPointer())
    {
        lua_newrive<ScriptedPointerEvent>(L,
                                          (uint8_t)p->pointerId,
                                          p->position,
                                          p->previousPosition,
                                          static_cast<int>(p->hitEvent),
                                          p->timeStamp);
    }
    else
    {
        lua_newrive<ScriptedPointerEvent>(L,
                                          (uint8_t)0,
                                          Vec2D(0, 0),
                                          Vec2D(0, 0),
                                          -1,
                                          0.f);
    }
}

void rive_lua_push_scripted_invocation(lua_State* L,
                                       const ListenerInvocation& inv)
{
    lua_newrive<ScriptedInvocation>(L, inv);
}

static int scripted_invocation_namecall(lua_State* L)
{
    auto* self = lua_torive<ScriptedInvocation>(L, 1);
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str == nullptr)
    {
        luaL_error(L, "invalid call");
        return 0;
    }

    const ListenerInvocation& inv = self->invocation();

    auto pushBool = [&](bool v) {
        lua_pushboolean(L, v ? 1 : 0);
        return 1;
    };

    switch (atom)
    {
        case (int)LuaAtoms::isPointerEvent:
            return pushBool(inv.kind() == ListenerInvocationKind::pointer);
        case (int)LuaAtoms::isKeyboardEvent:
            return pushBool(inv.kind() == ListenerInvocationKind::keyboard);
        case (int)LuaAtoms::isTextInput:
            return pushBool(inv.kind() == ListenerInvocationKind::textInput);
        case (int)LuaAtoms::isFocus:
            return pushBool(inv.kind() == ListenerInvocationKind::focus);
        case (int)LuaAtoms::isReportedEvent:
            return pushBool(inv.kind() ==
                            ListenerInvocationKind::reportedEvent);
        case (int)LuaAtoms::isViewModelChange:
            return pushBool(inv.kind() ==
                            ListenerInvocationKind::viewModelChange);
        case (int)LuaAtoms::isNone:
            return pushBool(inv.kind() == ListenerInvocationKind::none);
        case (int)LuaAtoms::isGamepad:
            return pushBool(inv.kind() == ListenerInvocationKind::gamepad);

        case (int)LuaAtoms::asPointerEvent:
            if (const PointerInvocation* p = inv.asPointer())
            {
                lua_newrive<ScriptedPointerEvent>(L,
                                                  (uint8_t)p->pointerId,
                                                  p->position,
                                                  p->previousPosition,
                                                  static_cast<int>(p->hitEvent),
                                                  p->timeStamp);
                return 1;
            }
            lua_pushnil(L);
            return 1;

        case (int)LuaAtoms::asKeyboardEvent:
            if (const KeyboardInvocation* k = inv.asKeyboard())
            {
                lua_newrive<ScriptedKeyboardInvocation>(L,
                                                        k->key,
                                                        k->modifiers,
                                                        k->isPressed,
                                                        k->isRepeat);
                return 1;
            }
            lua_pushnil(L);
            return 1;

        case (int)LuaAtoms::asTextInput:
            if (const TextInputInvocation* t = inv.asTextInput())
            {
                lua_newrive<ScriptedTextInputInvocation>(L, t->text);
                return 1;
            }
            lua_pushnil(L);
            return 1;

        case (int)LuaAtoms::asFocus:
            if (const FocusInvocation* f = inv.asFocus())
            {
                lua_newrive<ScriptedFocusInvocation>(L, f->isFocus);
                return 1;
            }
            lua_pushnil(L);
            return 1;

        case (int)LuaAtoms::asReportedEvent:
            if (const ReportedEventInvocation* e = inv.asReportedEvent())
            {
                lua_newrive<ScriptedReportedEventInvocation>(L,
                                                             e->reportedEvent,
                                                             e->delaySeconds);
                return 1;
            }
            lua_pushnil(L);
            return 1;

        case (int)LuaAtoms::asViewModelChange:
            if (inv.asViewModelChange() != nullptr)
            {
                lua_newrive<ScriptedViewModelChangeInvocation>(L);
                return 1;
            }
            lua_pushnil(L);
            return 1;

        case (int)LuaAtoms::asGamepad:
            if (const GamepadInvocation* g = inv.asGamepad())
            {
                lua_newrive<ScriptedGamepadInvocation>(L,
                                                       g->deviceId,
                                                       g->buttonMask,
                                                       g->axis0);
                return 1;
            }
            lua_pushnil(L);
            return 1;

        case (int)LuaAtoms::asNone:
            if (inv.asNone() != nullptr)
            {
                lua_newrive<ScriptedNoneInvocation>(L);
                return 1;
            }
            lua_pushnil(L);
            return 1;
    }

    luaL_error(L,
               "%s is not a valid method of %s",
               luaL_checkstring(L, 1),
               ScriptedInvocation::luaName);
    return 0;
}

static int keyboard_invocation_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }
    auto* k = lua_torive<ScriptedKeyboardInvocation>(L, 1);
    switch (atom)
    {
        case (int)LuaAtoms::key:
            lua_pushinteger(L, static_cast<int>(k->m_key));
            return 1;
        case (int)LuaAtoms::shift:
            lua_pushboolean(L,
                            (k->m_modifiers & KeyModifiers::shift) ==
                                KeyModifiers::shift);
            return 1;
        case (int)LuaAtoms::control:
            lua_pushboolean(L,
                            (k->m_modifiers & KeyModifiers::ctrl) ==
                                KeyModifiers::ctrl);
            return 1;
        case (int)LuaAtoms::alt:
            lua_pushboolean(L,
                            (k->m_modifiers & KeyModifiers::alt) ==
                                KeyModifiers::alt);
            return 1;
        case (int)LuaAtoms::meta:
            lua_pushboolean(L,
                            (k->m_modifiers & KeyModifiers::meta) ==
                                KeyModifiers::meta);
            return 1;
        case (int)LuaAtoms::phase:
        {
            if (k->m_isPressed)
            {
                if (k->m_isRepeat)
                {
                    lua_pushstring(L, "repeat");
                }
                else
                {
                    lua_pushstring(L, "down");
                }
            }
            else
            {
                lua_pushstring(L, "up");
            }
        }

            return 1;
    }
    luaL_error(L,
               "%s is not a valid field of %s",
               luaL_checkstring(L, 1),
               ScriptedKeyboardInvocation::luaName);
    return 0;
}

static int text_input_invocation_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }
    auto* t = lua_torive<ScriptedTextInputInvocation>(L, 1);
    switch (atom)
    {
        case (int)LuaAtoms::text:
            lua_pushstring(L, t->text().c_str());
            return 1;
    }
    luaL_error(L,
               "%s is not a valid field of %s",
               luaL_checkstring(L, 1),
               ScriptedTextInputInvocation::luaName);
    return 0;
}

static int focus_invocation_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }
    auto* f = lua_torive<ScriptedFocusInvocation>(L, 1);
    switch (atom)
    {
        case (int)LuaAtoms::isFocus:
            lua_pushboolean(L, f->m_isFocus ? 1 : 0);
            return 1;
    }
    luaL_error(L,
               "%s is not a valid field of %s",
               luaL_checkstring(L, 1),
               ScriptedFocusInvocation::luaName);
    return 0;
}

static int reported_event_invocation_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }
    auto* e = lua_torive<ScriptedReportedEventInvocation>(L, 1);
    switch (atom)
    {
        case (int)LuaAtoms::delaySeconds:
            lua_pushnumber(L, e->m_delaySeconds);
            return 1;
    }
    luaL_error(L,
               "%s is not a valid field of %s",
               luaL_checkstring(L, 1),
               ScriptedReportedEventInvocation::luaName);
    return 0;
}

static int gamepad_invocation_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }
    auto* g = lua_torive<ScriptedGamepadInvocation>(L, 1);
    switch (atom)
    {
        case (int)LuaAtoms::deviceId:
            lua_pushinteger(L, g->m_deviceId);
            return 1;
        case (int)LuaAtoms::buttonMask:
            lua_pushnumber(L, static_cast<double>(g->m_buttonMask));
            return 1;
        case (int)LuaAtoms::axis0:
            lua_pushnumber(L, g->m_axis0);
            return 1;
    }
    luaL_error(L,
               "%s is not a valid field of %s",
               luaL_checkstring(L, 1),
               ScriptedGamepadInvocation::luaName);
    return 0;
}

void rive_lua_register_listener_invocation_types(lua_State* L)
{
    lua_register_rive<ScriptedInvocation>(L);
    lua_pushcfunction(L, scripted_invocation_namecall, nullptr);
    lua_setfield(L, -2, "__namecall");
    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_register_rive<ScriptedKeyboardInvocation>(L);
    lua_pushcfunction(L, keyboard_invocation_index, nullptr);
    lua_setfield(L, -2, "__index");
    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_register_rive<ScriptedTextInputInvocation>(L);
    lua_pushcfunction(L, text_input_invocation_index, nullptr);
    lua_setfield(L, -2, "__index");
    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_register_rive<ScriptedFocusInvocation>(L);
    lua_pushcfunction(L, focus_invocation_index, nullptr);
    lua_setfield(L, -2, "__index");
    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_register_rive<ScriptedReportedEventInvocation>(L);
    lua_pushcfunction(L, reported_event_invocation_index, nullptr);
    lua_setfield(L, -2, "__index");
    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_register_rive<ScriptedViewModelChangeInvocation>(L);
    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_register_rive<ScriptedGamepadInvocation>(L);
    lua_pushcfunction(L, gamepad_invocation_index, nullptr);
    lua_setfield(L, -2, "__index");
    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_register_rive<ScriptedNoneInvocation>(L);
    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);
}

} // namespace rive

#endif
