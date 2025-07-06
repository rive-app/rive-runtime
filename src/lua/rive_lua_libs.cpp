#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#include "lualib.h"
#include <unordered_map>
#include <string>

using namespace rive;

int luaopen_rive_math(lua_State* L);
int luaopen_rive_renderer_library(lua_State* L);

std::unordered_map<std::string, int16_t> atoms = {
    {"length", (int16_t)LuaAtoms::length},
    {"lengthSquared", (int16_t)LuaAtoms::lengthSquared},
    {"normalized", (int16_t)LuaAtoms::normalized},
    {"distance", (int16_t)LuaAtoms::distance},
    {"distanceSquared", (int16_t)LuaAtoms::distanceSquared},
    {"dot", (int16_t)LuaAtoms::dot},
    {"lerp", (int16_t)LuaAtoms::lerp},
    {"moveTo", (int16_t)LuaAtoms::moveTo},
    {"lineTo", (int16_t)LuaAtoms::lineTo},
    {"quadTo", (int16_t)LuaAtoms::quadTo},
    {"cubicTo", (int16_t)LuaAtoms::cubicTo},
    {"close", (int16_t)LuaAtoms::close},
    {"reset", (int16_t)LuaAtoms::reset},
    {"add", (int16_t)LuaAtoms::add},
    {"invert", (int16_t)LuaAtoms::invert},
    {"isIdentity", (int16_t)LuaAtoms::isIdentity},
    {"width", (int16_t)LuaAtoms::width},
    {"height", (int16_t)LuaAtoms::height},
    {"clamp", (int16_t)LuaAtoms::clamp},
    {"repeat", (int16_t)LuaAtoms::repeat},
    {"mirror", (int16_t)LuaAtoms::mirror},
    {"trilinear", (int16_t)LuaAtoms::trilinear},
    {"nearest", (int16_t)LuaAtoms::nearest},
    {"style", (int16_t)LuaAtoms::style},
    {"join", (int16_t)LuaAtoms::join},
    {"cap", (int16_t)LuaAtoms::cap},
    {"thickness", (int16_t)LuaAtoms::thickness},
    {"blendMode", (int16_t)LuaAtoms::blendMode},
    {"feather", (int16_t)LuaAtoms::feather},
    {"gradient", (int16_t)LuaAtoms::gradient},
    {"color", (int16_t)LuaAtoms::color},
    {"stroke", (int16_t)LuaAtoms::stroke},
    {"fill", (int16_t)LuaAtoms::fill},
    {"miter", (int16_t)LuaAtoms::miter},
    {"round", (int16_t)LuaAtoms::round},
    {"bevel", (int16_t)LuaAtoms::bevel},
    {"butt", (int16_t)LuaAtoms::butt},
    {"square", (int16_t)LuaAtoms::square},
    {"srcOver", (int16_t)LuaAtoms::srcOver},
    {"screen", (int16_t)LuaAtoms::screen},
    {"overlay", (int16_t)LuaAtoms::overlay},
    {"darken", (int16_t)LuaAtoms::darken},
    {"lighten", (int16_t)LuaAtoms::lighten},
    {"colorDodge", (int16_t)LuaAtoms::colorDodge},
    {"colorBurn", (int16_t)LuaAtoms::colorBurn},
    {"hardLight", (int16_t)LuaAtoms::hardLight},
    {"softLight", (int16_t)LuaAtoms::softLight},
    {"difference", (int16_t)LuaAtoms::difference},
    {"exclusion", (int16_t)LuaAtoms::exclusion},
    {"multiply", (int16_t)LuaAtoms::multiply},
    {"hue", (int16_t)LuaAtoms::hue},
    {"saturation", (int16_t)LuaAtoms::saturation},
    {"luminosity", (int16_t)LuaAtoms::luminosity},
    {"copy", (int16_t)LuaAtoms::copy},
    {"drawPath", (int16_t)LuaAtoms::drawPath},
    {"drawImage", (int16_t)LuaAtoms::drawImage},
    {"drawImageMesh", (int16_t)LuaAtoms::drawImageMesh},
    {"clipPath", (int16_t)LuaAtoms::clipPath},
    {"save", (int16_t)LuaAtoms::save},
    {"restore", (int16_t)LuaAtoms::restore},
    {"transform", (int16_t)LuaAtoms::transform},
};

static const luaL_Reg lualibs[] = {
    {"", luaopen_base},
    {LUA_STRLIBNAME, luaopen_string},
    {"math", luaopen_rive_math},
    {"renderer", luaopen_rive_renderer_library},
    {NULL, NULL},
};

namespace rive
{
int luaopen_rive(lua_State* L)
{
    lua_callbacks(L)->useratom = [](const char* s, size_t l) -> int16_t {
        auto itr = atoms.find(s);
        if (itr != atoms.end())
        {
            return itr->second;
        }

        return -1;
    };

    const luaL_Reg* lib = lualibs;
    for (; lib->func; lib++)
    {
        lua_pushcfunction(L, lib->func, NULL);
        lua_pushstring(L, lib->name);
        lua_call(L, 1, 0);
    }
    return 0;
}

static void* l_alloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
    (void)ud;
    (void)osize;
    if (nsize == 0)
    {
        free(ptr);
        return NULL;
    }
    else
    {
        return realloc(ptr, nsize);
    }
}

void ScriptingVM::init(lua_State* state, ScriptingContext* context)
{
    luaopen_rive(state);
    luaL_sandbox(state);
    luaL_sandboxthread(state);
    lua_setthreaddata(state, context);
}

ScriptingVM::ScriptingVM(Factory* factory)
{
    m_state = lua_newstate(l_alloc, nullptr);
    m_context.factory = factory;
    init(m_state, &m_context);
}

ScriptingVM::~ScriptingVM() { lua_close(m_state); }
} // namespace rive
#endif
