#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/input/standard_gamepad.hpp"
#include "lualib.h"

namespace rive
{

namespace
{

bool gamepadIsStandardMapping(const GamepadSnapshot& s)
{
    return s.mapping == GamepadMappingKind::standard;
}

void pushStandardButton(lua_State* L,
                        const GamepadSnapshot& s,
                        StandardGamepadButton b)
{
    if (!gamepadIsStandardMapping(s))
    {
        lua_pushboolean(L, 0);
        return;
    }
    lua_pushboolean(L, standardGamepadButtonPressed(s.buttonMask, b) ? 1 : 0);
}

void pushStick(lua_State* L,
               const GamepadSnapshot& s,
               StandardGamepadAxis ax,
               StandardGamepadAxis ay)
{
    if (!gamepadIsStandardMapping(s))
    {
        lua_pushvector2(L, 0.f, 0.f);
        return;
    }
    lua_pushvector2(L,
                    standardGamepadAxisValue(s.axes, ax),
                    standardGamepadAxisValue(s.axes, ay));
}

void pushTriggerAxis(lua_State* L,
                     const GamepadSnapshot& s,
                     StandardGamepadAxis a)
{
    if (!gamepadIsStandardMapping(s))
    {
        lua_pushnumber(L, 0.);
        return;
    }
    lua_pushnumber(L, standardGamepadAxisValue(s.axes, a));
}

const char* standardButtonLabel(StandardGamepadButton b)
{
    switch (b)
    {
        case StandardGamepadButton::south:
            return "south";
        case StandardGamepadButton::east:
            return "east";
        case StandardGamepadButton::west:
            return "west";
        case StandardGamepadButton::north:
            return "north";
        case StandardGamepadButton::leftShoulder:
            return "leftShoulder";
        case StandardGamepadButton::rightShoulder:
            return "rightShoulder";
        case StandardGamepadButton::leftTrigger:
            return "leftTrigger";
        case StandardGamepadButton::rightTrigger:
            return "rightTrigger";
        case StandardGamepadButton::back:
            return "back";
        case StandardGamepadButton::forward:
            return "forward";
        case StandardGamepadButton::leftStick:
            return "leftStick";
        case StandardGamepadButton::rightStick:
            return "rightStick";
        case StandardGamepadButton::dpadUp:
            return "dpadUp";
        case StandardGamepadButton::dpadDown:
            return "dpadDown";
        case StandardGamepadButton::dpadLeft:
            return "dpadLeft";
        case StandardGamepadButton::dpadRight:
            return "dpadRight";
        case StandardGamepadButton::start:
            return "start";
    }
    return "unknown";
}

const char* standardAxisLabel(StandardGamepadAxis a)
{
    switch (a)
    {
        case StandardGamepadAxis::leftX:
            return "leftX";
        case StandardGamepadAxis::leftY:
            return "leftY";
        case StandardGamepadAxis::rightX:
            return "rightX";
        case StandardGamepadAxis::rightY:
            return "rightY";
        case StandardGamepadAxis::leftTrigger:
            return "leftTriggerAxis";
        case StandardGamepadAxis::rightTrigger:
            return "rightTriggerAxis";
    }
    return "unknown";
}

} // namespace

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
        case (int)LuaAtoms::isGamepadConnected:
            return pushBool(inv.kind() ==
                            ListenerInvocationKind::gamepadConnected);
        case (int)LuaAtoms::isGamepadEvent:
            return pushBool(inv.kind() == ListenerInvocationKind::gamepadEvent);
        case (int)LuaAtoms::isGamepadDisconnected:
            return pushBool(inv.kind() ==
                            ListenerInvocationKind::gamepadDisconnected);

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

        case (int)LuaAtoms::asGamepadConnected:
            if (const GamepadConnectedInvocation* g = inv.asGamepadConnected())
            {
                lua_newrive<ScriptedGamepadConnected>(L, g->snapshot);
                return 1;
            }
            lua_pushnil(L);
            return 1;

        case (int)LuaAtoms::asGamepadEvent:
            if (const GamepadEventInvocation* e = inv.asGamepadEvent())
            {
                lua_newrive<ScriptedGamepadEvent>(L, *e);
                return 1;
            }
            lua_pushnil(L);
            return 1;

        case (int)LuaAtoms::asGamepadDisconnected:
            if (const GamepadDisconnectedInvocation* d =
                    inv.asGamepadDisconnected())
            {
                lua_newrive<ScriptedGamepadDisconnected>(L, d->deviceId);
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

static void keyboard_invocation_direct_key(void* udata, void* result)
{
    lua_userdatadirectfield_setnumber(
        result,
        (double)(int)((ScriptedKeyboardInvocation*)udata)->m_key);
}

static void keyboard_invocation_direct_shift(void* udata, void* result)
{
    auto* k = (ScriptedKeyboardInvocation*)udata;
    lua_userdatadirectfield_setboolean(
        result,
        (k->m_modifiers & KeyModifiers::shift) == KeyModifiers::shift ? 1 : 0);
}

static void keyboard_invocation_direct_control(void* udata, void* result)
{
    auto* k = (ScriptedKeyboardInvocation*)udata;
    lua_userdatadirectfield_setboolean(
        result,
        (k->m_modifiers & KeyModifiers::ctrl) == KeyModifiers::ctrl ? 1 : 0);
}

static void keyboard_invocation_direct_alt(void* udata, void* result)
{
    auto* k = (ScriptedKeyboardInvocation*)udata;
    lua_userdatadirectfield_setboolean(
        result,
        (k->m_modifiers & KeyModifiers::alt) == KeyModifiers::alt ? 1 : 0);
}

static void keyboard_invocation_direct_meta(void* udata, void* result)
{
    auto* k = (ScriptedKeyboardInvocation*)udata;
    lua_userdatadirectfield_setboolean(
        result,
        (k->m_modifiers & KeyModifiers::meta) == KeyModifiers::meta ? 1 : 0);
}

static void focus_invocation_direct_isFocus(void* udata, void* result)
{
    lua_userdatadirectfield_setboolean(
        result,
        ((ScriptedFocusInvocation*)udata)->m_isFocus ? 1 : 0);
}

static void reported_event_invocation_direct_delaySeconds(void* udata,
                                                          void* result)
{
    lua_userdatadirectfield_setnumber(
        result,
        ((ScriptedReportedEventInvocation*)udata)->m_delaySeconds);
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

static int gamepad_snapshot_namecall(lua_State* L, const GamepadSnapshot& snap)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str == nullptr)
    {
        luaL_error(L, "invalid call");
        return 0;
    }
    const lua_Integer idx = luaL_checkinteger(L, 2);
    switch (atom)
    {
        case (int)LuaAtoms::buttonPressed:
            if (idx < 1 || idx > 64)
            {
                lua_pushboolean(L, 0);
                return 1;
            }
            lua_pushboolean(L,
                            (snap.buttonMask &
                             (uint64_t(1) << static_cast<unsigned>(idx - 1))) !=
                                    0
                                ? 1
                                : 0);
            return 1;
        case (int)LuaAtoms::buttonValue:
            if (idx < 1 || static_cast<size_t>(idx) > snap.buttonValues.size())
            {
                lua_pushnumber(L, 0.);
                return 1;
            }
            lua_pushnumber(L, snap.buttonValues[static_cast<size_t>(idx) - 1]);
            return 1;
        case (int)LuaAtoms::axis:
            if (idx < 1 || static_cast<size_t>(idx) > snap.axes.size())
            {
                lua_pushnumber(L, 0.);
                return 1;
            }
            lua_pushnumber(L, snap.axes[static_cast<size_t>(idx) - 1]);
            return 1;
    }
    luaL_error(L,
               "%s is not a valid method of gamepad data",
               luaL_checkstring(L, 1));
    return 0;
}

static int gamepad_connected_namecall(lua_State* L)
{
    auto* g = lua_torive<ScriptedGamepadConnected>(L, 1);
    return gamepad_snapshot_namecall(L, g->m_snapshot);
}

static int gamepad_event_namecall(lua_State* L)
{
    auto* g = lua_torive<ScriptedGamepadEvent>(L, 1);
    return gamepad_snapshot_namecall(L, g->m_data.fullState);
}

static int gamepad_snapshot_index(lua_State* L,
                                  int atom,
                                  const GamepadSnapshot& snap);
static int gamepad_invocation_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }
    auto* g = lua_torive<ScriptedGamepadConnected>(L, 1);
    return gamepad_snapshot_index(L, atom, g->m_snapshot);
}

static int gamepad_snapshot_index(lua_State* L,
                                  int atom,
                                  const GamepadSnapshot& snap)
{
    switch (atom)
    {
        case (int)LuaAtoms::deviceId:
            lua_pushinteger(L, snap.deviceId);
            return 1;
        case (int)LuaAtoms::buttonMask:
            lua_pushnumber(L, static_cast<double>(snap.buttonMask));
            return 1;
        case (int)LuaAtoms::buttons:
        {
            const auto& bv = snap.buttonValues;
            lua_createtable(L, static_cast<int>(bv.size()), 0);
            for (size_t i = 0; i < bv.size(); ++i)
            {
                lua_pushnumber(L, bv[i]);
                lua_rawseti(L, -2, static_cast<int>(i + 1));
            }
            return 1;
        }
        case (int)LuaAtoms::axes:
        {
            const auto& ax = snap.axes;
            lua_createtable(L, static_cast<int>(ax.size()), 0);
            for (size_t i = 0; i < ax.size(); ++i)
            {
                lua_pushnumber(L, ax[i]);
                lua_rawseti(L, -2, static_cast<int>(i + 1));
            }
            return 1;
        }
        case (int)LuaAtoms::gamepadMapping:
            lua_pushinteger(L, static_cast<int>(snap.mapping));
            return 1;
        case (int)LuaAtoms::mapping:
            if (snap.mapping == GamepadMappingKind::standard)
            {
                lua_pushstring(L, "standard");
            }
            else
            {
                lua_pushstring(L, "unknown");
            }
            return 1;
        case (int)LuaAtoms::isStandardMapping:
            lua_pushboolean(L, gamepadIsStandardMapping(snap) ? 1 : 0);
            return 1;
        case (int)LuaAtoms::west:
            pushStandardButton(L, snap, StandardGamepadButton::west);
            return 1;
        case (int)LuaAtoms::south:
            pushStandardButton(L, snap, StandardGamepadButton::south);
            return 1;
        case (int)LuaAtoms::north:
            pushStandardButton(L, snap, StandardGamepadButton::north);
            return 1;
        case (int)LuaAtoms::east:
            pushStandardButton(L, snap, StandardGamepadButton::east);
            return 1;
        case (int)LuaAtoms::leftShoulder:
            pushStandardButton(L, snap, StandardGamepadButton::leftShoulder);
            return 1;
        case (int)LuaAtoms::rightShoulder:
            pushStandardButton(L, snap, StandardGamepadButton::rightShoulder);
            return 1;
        case (int)LuaAtoms::gamepadBack:
            pushStandardButton(L, snap, StandardGamepadButton::back);
            return 1;
        case (int)LuaAtoms::gamepadForward:
            pushStandardButton(L, snap, StandardGamepadButton::forward);
            return 1;
        case (int)LuaAtoms::leftStickButton:
            pushStandardButton(L, snap, StandardGamepadButton::leftStick);
            return 1;
        case (int)LuaAtoms::rightStickButton:
            pushStandardButton(L, snap, StandardGamepadButton::rightStick);
            return 1;
        case (int)LuaAtoms::dpadUp:
            pushStandardButton(L, snap, StandardGamepadButton::dpadUp);
            return 1;
        case (int)LuaAtoms::dpadDown:
            pushStandardButton(L, snap, StandardGamepadButton::dpadDown);
            return 1;
        case (int)LuaAtoms::dpadLeft:
            pushStandardButton(L, snap, StandardGamepadButton::dpadLeft);
            return 1;
        case (int)LuaAtoms::dpadRight:
            pushStandardButton(L, snap, StandardGamepadButton::dpadRight);
            return 1;
        case (int)LuaAtoms::start:
            pushStandardButton(L, snap, StandardGamepadButton::start);
            return 1;
        case (int)LuaAtoms::leftTriggerPressed:
            pushStandardButton(L, snap, StandardGamepadButton::leftTrigger);
            return 1;
        case (int)LuaAtoms::rightTriggerPressed:
            pushStandardButton(L, snap, StandardGamepadButton::rightTrigger);
            return 1;
        case (int)LuaAtoms::leftStick:
            pushStick(L,
                      snap,
                      StandardGamepadAxis::leftX,
                      StandardGamepadAxis::leftY);
            return 1;
        case (int)LuaAtoms::rightStick:
            pushStick(L,
                      snap,
                      StandardGamepadAxis::rightX,
                      StandardGamepadAxis::rightY);
            return 1;
        case (int)LuaAtoms::leftTrigger:
            pushTriggerAxis(L, snap, StandardGamepadAxis::leftTrigger);
            return 1;
        case (int)LuaAtoms::rightTrigger:
            pushTriggerAxis(L, snap, StandardGamepadAxis::rightTrigger);
            return 1;
    }
    luaL_error(L,
               "%s is not a valid field of gamepad state",
               luaL_checkstring(L, 1));
    return 0;
}

static int gamepad_event_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }
    auto* g = lua_torive<ScriptedGamepadEvent>(L, 1);
    const GamepadEventInvocation& d = g->m_data;
    const GamepadInputChange& c = d.change;
    const GamepadSnapshot& snap = d.fullState;
    switch (atom)
    {
        case (int)LuaAtoms::changeKind:
            if (c.kind == GamepadInputChangeKind::button)
            {
                lua_pushstring(L, "button");
            }
            else
            {
                lua_pushstring(L, "axis");
            }
            return 1;
        case (int)LuaAtoms::changeIndex:
            lua_pushinteger(L,
                            static_cast<lua_Integer>(c.index) +
                                1); // 1-based, W3C
            return 1;
        case (int)LuaAtoms::changeValue:
            lua_pushnumber(L, c.value);
            return 1;
        case (int)LuaAtoms::hasStandardButtonIntent:
            lua_pushboolean(L, d.hasStandardButtonIntent ? 1 : 0);
            return 1;
        case (int)LuaAtoms::hasStandardAxisIntent:
            lua_pushboolean(L, d.hasStandardAxisIntent ? 1 : 0);
            return 1;
        case (int)LuaAtoms::intentButton:
            if (d.hasStandardButtonIntent)
            {
                lua_pushstring(L, standardButtonLabel(d.standardButton));
            }
            else
            {
                lua_pushnil(L);
            }
            return 1;
        case (int)LuaAtoms::intentAxis:
            if (d.hasStandardAxisIntent)
            {
                lua_pushstring(L, standardAxisLabel(d.standardAxis));
            }
            else
            {
                lua_pushnil(L);
            }
            return 1;
    }
    return gamepad_snapshot_index(L, atom, snap);
}

static int gamepad_disconnected_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }
    auto* g = lua_torive<ScriptedGamepadDisconnected>(L, 1);
    switch (atom)
    {
        case (int)LuaAtoms::deviceId:
            lua_pushinteger(L, g->m_deviceId);
            return 1;
    }
    luaL_error(L,
               "%s is not a valid field of %s",
               luaL_checkstring(L, 1),
               ScriptedGamepadDisconnected::luaName);
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

    {
        uint8_t tag = ScriptedKeyboardInvocation::luaTag;
        lua_registeruserdatadirectfieldget(L,
                                           tag,
                                           "key",
                                           keyboard_invocation_direct_key);
        lua_registeruserdatadirectfieldget(L,
                                           tag,
                                           "shift",
                                           keyboard_invocation_direct_shift);
        lua_registeruserdatadirectfieldget(L,
                                           tag,
                                           "control",
                                           keyboard_invocation_direct_control);
        lua_registeruserdatadirectfieldget(L,
                                           tag,
                                           "alt",
                                           keyboard_invocation_direct_alt);
        lua_registeruserdatadirectfieldget(L,
                                           tag,
                                           "meta",
                                           keyboard_invocation_direct_meta);
    }

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

    lua_registeruserdatadirectfieldget(L,
                                       ScriptedFocusInvocation::luaTag,
                                       "isFocus",
                                       focus_invocation_direct_isFocus);

    lua_register_rive<ScriptedReportedEventInvocation>(L);
    lua_pushcfunction(L, reported_event_invocation_index, nullptr);
    lua_setfield(L, -2, "__index");
    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_registeruserdatadirectfieldget(
        L,
        ScriptedReportedEventInvocation::luaTag,
        "delaySeconds",
        reported_event_invocation_direct_delaySeconds);

    lua_register_rive<ScriptedViewModelChangeInvocation>(L);
    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_register_rive<ScriptedGamepadConnected>(L);
    lua_pushcfunction(L, gamepad_invocation_index, nullptr);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, gamepad_connected_namecall, nullptr);
    lua_setfield(L, -2, "__namecall");
    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_register_rive<ScriptedGamepadEvent>(L);
    lua_pushcfunction(L, gamepad_event_index, nullptr);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, gamepad_event_namecall, nullptr);
    lua_setfield(L, -2, "__namecall");
    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_register_rive<ScriptedGamepadDisconnected>(L);
    lua_pushcfunction(L, gamepad_disconnected_index, nullptr);
    lua_setfield(L, -2, "__index");
    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_register_rive<ScriptedNoneInvocation>(L);
    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);
}

} // namespace rive

#endif
