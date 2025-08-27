#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#include "lualib.h"
#include <unordered_map>
#include <string>

using namespace rive;

int luaopen_rive_base(lua_State* L);
int luaopen_rive_math(lua_State* L);
int luaopen_rive_renderer_library(lua_State* L);
int luaopen_rive_properties(lua_State* L);
int luaopen_rive_artboards(lua_State* L);

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
    {"value", (int16_t)LuaAtoms::value},
    {"getNumber", (int16_t)LuaAtoms::getNumber},
    {"getTrigger", (int16_t)LuaAtoms::getTrigger},
    {"addListener", (int16_t)LuaAtoms::addListener},
    {"removeListener", (int16_t)LuaAtoms::removeListener},
    {"fire", (int16_t)LuaAtoms::fire},
    {"draw", (int16_t)LuaAtoms::draw},
    {"advance", (int16_t)LuaAtoms::advance},
    {"frameOrigin", (int16_t)LuaAtoms::frameOrigin},
    {"data", (int16_t)LuaAtoms::data},
    {"instance", (int16_t)LuaAtoms::instance},
};

static const luaL_Reg lualibs[] = {
    {"", luaopen_base},
    {LUA_TABLIBNAME, luaopen_table},
    {LUA_MATHLIBNAME, luaopen_math},
    {"rive", luaopen_rive_base},
    {LUA_STRLIBNAME, luaopen_string},
    {"math", luaopen_rive_math},
    {"renderer", luaopen_rive_renderer_library},
    {"properties", luaopen_rive_properties},
    {"artboard", luaopen_rive_artboards},
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
        // delete[] (uint8_t*)ptr;
        return NULL;
    }
    else
    {
        // auto nptr = new uint8_t[nsize];
        // memcpy(nptr, ptr, std::min(nsize, osize));
        // delete[] (uint8_t*)ptr;
        // return nptr;
        return realloc(ptr, nsize);
    }
}

static const char* registeredCacheTableKey = "_MODULES";

static int checkRegisteredModules(lua_State* L, const char* path)
{
    luaL_findtable(L, LUA_REGISTRYINDEX, registeredCacheTableKey, 1);
    lua_getfield(L, -1, path);
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 2);
        return 0;
    }

    lua_remove(L, -2);
    return 1;
}

static int lua_requireinternal(lua_State* L, const char* requirerChunkname)
{
    // Discard extra arguments, we only use path
    lua_settop(L, 1);
    const char* path = luaL_checkstring(L, 1);

    if (checkRegisteredModules(L, path) == 1)
    {
        return 1;
    }

    luaL_error(L, "require could not find a script named %s", path);
    return 0;
}

static int lua_require(lua_State* L)
{
    lua_Debug ar;
    int level = 1;

    do
    {
        if (!lua_getinfo(L, level++, "s", &ar))
        {
            luaL_error(L, "require is not supported in this context");
        }
    } while (ar.what[0] == 'C');

    return lua_requireinternal(L, ar.source);
}

static int luaR_error(lua_State* L)
{
    int level = luaL_optinteger(L, 2, 1);
    lua_settop(L, 1);
    if (lua_isstring(L, 1) && level > 0)
    {
        luaL_where(L, level);
        lua_pushvalue(L, 1);
        lua_concat(L, 2);
    }
    lua_error(L);
}

static int lua_late(lua_State* L)
{
    lua_pushnil(L);
    return 1;
}

void ScriptingVM::init(lua_State* state, ScriptingContext* context)
{
    luaopen_rive(state);
    lua_setthreaddata(state, context);

    lua_pushcclosurek(state, lua_require, "require", 0, nullptr);
    lua_setglobal(state, "require");

    lua_pushcclosurek(state, luaR_error, "error", 0, nullptr);
    lua_setglobal(state, "error");

    lua_pushcclosurek(state, lua_late, "late", 0, nullptr);
    lua_setglobal(state, "late");

    luaL_sandbox(state);
    luaL_sandboxthread(state);
}

ScriptingVM::ScriptingVM(ScriptingContext* context) : m_context(context)
{
    m_state = lua_newstate(l_alloc, nullptr);
    init(m_state, m_context);
}

ScriptingVM::~ScriptingVM() { lua_close(m_state); }

static int register_module(lua_State* L)
{
    // This is only called internally where we ensure we're pushing the right
    // elements on the stack so we can assert these at debug time only.
    assert(lua_gettop(L) == 2 &&
           "expected 2 arguments: require script name and desired result");
    assert(luaL_checkstring(L, 1) &&
           "first argument must be the name of the script");

    luaL_findtable(L, LUA_REGISTRYINDEX, registeredCacheTableKey, 1);
    // (1) path, (2) result, (3) cache table

    lua_insert(L, 1);
    // (1) cache table, (2) path, (3) result

    lua_settable(L, 1);
    // (1) cache table

    lua_pop(L, 1);

    return 0;
}

static bool push_module(lua_State* L, const char* name, Span<uint8_t> bytecode)
{
    if (bytecode.empty())
    {
        return false;
    }
    // module needs to run in a new thread, isolated from the rest
    // note: we create ML on main thread so that it doesn't inherit environment
    // of L
    lua_State* GL = lua_mainthread(L);
    lua_State* ML = lua_newthread(GL);
    lua_xmove(GL, L, 1);
    // new thread needs to have the globals sandboxed
    luaL_sandboxthread(ML);
    lua_setthreaddata(ML, lua_getthreaddata(L));

    int status =
        luau_load(ML, name, (const char*)bytecode.data(), bytecode.size(), 0);

    if (status == 0)
    {
        int status = lua_resume(ML, L, 0);
        if (status == 0)
        {
            if (lua_gettop(ML) == 0)
            {
                lua_pushstring(ML, "module must return a value");
            }
            else if (!lua_istable(ML, -1) && !lua_isfunction(ML, -1))
            {
                lua_pushstring(ML, "module must return a table or function");
            }
        }
        else if (status == LUA_YIELD)
        {
            lua_pushstring(ML, "module can not yield");
        }
        else if (!lua_isstring(ML, -1))
        {
            lua_pushstring(ML, "unknown error while running module");
        }
    }

    // add ML result to L stack
    lua_xmove(ML, L, 1);
    if (lua_isstring(L, -1))
    {
        fprintf(stderr, "Failed to load module %s\n", name);
        lua_pop(L, 1);
        lua_remove(L, -2);
        return false;
    }
    // remove ML thread from L stack
    lua_remove(L, -2);
    // added one value to L stack: module result
    return true;
}

bool ScriptingVM::registerScript(lua_State* state,
                                 const char* name,
                                 Span<uint8_t> bytecode)
{
    if (!push_module(state, name, bytecode))
    {
        return false;
    }

    return true;
}

bool ScriptingVM::registerModule(lua_State* state,
                                 const char* name,
                                 Span<uint8_t> bytecode)
{
    lua_pushcfunction(state, register_module, nullptr);
    lua_pushstring(state, name);
    if (!push_module(state, name, bytecode))
    {
        return false;
    }

    lua_call(state, 2, 0);
    return true;
}

void ScriptingVM::unregisterModule(lua_State* state, const char* name)
{
    luaL_findtable(state, LUA_REGISTRYINDEX, registeredCacheTableKey, 1);
    lua_pushstring(state, name);
    lua_pushnil(state);
    lua_settable(state, -3);
    lua_pop(state, 1);
}

void ScriptingVM::unregisterModule(const char* name)
{
    return unregisterModule(m_state, name);
}

bool ScriptingVM::registerModule(const char* name, Span<uint8_t> bytecode)
{
    return registerModule(m_state, name, bytecode);
}

bool ScriptingVM::registerScript(const char* name, Span<uint8_t> bytecode)
{
    return registerScript(m_state, name, bytecode);
}

} // namespace rive
#endif
