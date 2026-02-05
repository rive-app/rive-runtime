#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/assets/script_asset.hpp"
#include "lualib.h"
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <queue>
#include <vector>
#include <algorithm>

using namespace rive;

int luaopen_rive_base(lua_State* L);
int luaopen_rive_math(lua_State* L);
int luaopen_rive_renderer_library(lua_State* L);
int luaopen_rive_properties(lua_State* L);
int luaopen_rive_artboards(lua_State* L);
int luaopen_rive_data_values(lua_State* L);
int luaopen_rive_data_context(lua_State* L);
int luaopen_rive_input(lua_State* L);
int luaopen_rive_contex(lua_State* L);
int luaopen_rive_audio(lua_State* L);

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
    {"type", (int16_t)LuaAtoms::type},
    {"reset", (int16_t)LuaAtoms::reset},
    {"add", (int16_t)LuaAtoms::add},
    {"contours", (int16_t)LuaAtoms::contours},
    {"measure", (int16_t)LuaAtoms::measure},
    {"invert", (int16_t)LuaAtoms::invert},
    {"isIdentity", (int16_t)LuaAtoms::isIdentity},
    {"width", (int16_t)LuaAtoms::width},
    {"height", (int16_t)LuaAtoms::height},
    {"clamp", (int16_t)LuaAtoms::clamp},
    {"repeat", (int16_t)LuaAtoms::repeat},
    {"mirror", (int16_t)LuaAtoms::mirror},
    {"bilinear", (int16_t)LuaAtoms::bilinear},
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
    {"red", (int16_t)LuaAtoms::red},
    {"green", (int16_t)LuaAtoms::green},
    {"blue", (int16_t)LuaAtoms::blue},
    {"alpha", (int16_t)LuaAtoms::alpha},
    {"getNumber", (int16_t)LuaAtoms::getNumber},
    {"getTrigger", (int16_t)LuaAtoms::getTrigger},
    {"getString", (int16_t)LuaAtoms::getString},
    {"getBoolean", (int16_t)LuaAtoms::getBoolean},
    {"getColor", (int16_t)LuaAtoms::getColor},
    {"getList", (int16_t)LuaAtoms::getList},
    {"getViewModel", (int16_t)LuaAtoms::getViewModel},
    {"getEnum", (int16_t)LuaAtoms::getEnum},
    {"values", (int16_t)LuaAtoms::values},
    {"addListener", (int16_t)LuaAtoms::addListener},
    {"removeListener", (int16_t)LuaAtoms::removeListener},
    {"fire", (int16_t)LuaAtoms::fire},
    {"push", (int16_t)LuaAtoms::push},
    {"insert", (int16_t)LuaAtoms::insert},
    {"pop", (int16_t)LuaAtoms::pop},
    {"swap", (int16_t)LuaAtoms::swap},
    {"shift", (int16_t)LuaAtoms::shift},
    {"draw", (int16_t)LuaAtoms::draw},
    {"advance", (int16_t)LuaAtoms::advance},
    {"frameOrigin", (int16_t)LuaAtoms::frameOrigin},
    {"data", (int16_t)LuaAtoms::data},
    {"instance", (int16_t)LuaAtoms::instance},
    {"animation", (int16_t)LuaAtoms::animation},
    {"new", (int16_t)LuaAtoms::newAtom},
    {"bounds", (int16_t)LuaAtoms::bounds},
    {"pointerDown", (int16_t)LuaAtoms::pointerDown},
    {"pointerUp", (int16_t)LuaAtoms::pointerUp},
    {"pointerMove", (int16_t)LuaAtoms::pointerMove},
    {"pointerExit", (int16_t)LuaAtoms::pointerExit},
    {"isNumber", (int16_t)LuaAtoms::isNumber},
    {"isString", (int16_t)LuaAtoms::isString},
    {"isBoolean", (int16_t)LuaAtoms::isBoolean},
    {"isColor", (int16_t)LuaAtoms::isColor},
    {"hit", (int16_t)LuaAtoms::hit},
    {"id", (int16_t)LuaAtoms::id},
    {"position", (int16_t)LuaAtoms::position},
    {"rotation", (int16_t)LuaAtoms::rotation},
    {"scale", (int16_t)LuaAtoms::scale},
    {"worldTransform", (int16_t)LuaAtoms::worldTransform},
    {"scaleX", (int16_t)LuaAtoms::scaleX},
    {"scaleY", (int16_t)LuaAtoms::scaleY},
    {"decompose", (int16_t)LuaAtoms::decompose},
    {"children", (int16_t)LuaAtoms::children},
    {"parent", (int16_t)LuaAtoms::parent},
    {"node", (int16_t)LuaAtoms::node},
    {"paint", (int16_t)LuaAtoms::paint},
    {"asPath", (int16_t)LuaAtoms::asPath},
    {"asPaint", (int16_t)LuaAtoms::asPaint},
    {"addToPath", (int16_t)LuaAtoms::addToPath},
    {"positionAndTangent", (int16_t)LuaAtoms::positionAndTangent},
    {"warp", (int16_t)LuaAtoms::warp},
    {"extract", (int16_t)LuaAtoms::extract},
    {"next", (int16_t)LuaAtoms::next},
    {"isClosed", (int16_t)LuaAtoms::isClosed},
    {"markNeedsUpdate", (int16_t)LuaAtoms::markNeedsUpdate},
    {"viewModel", (int16_t)LuaAtoms::viewModel},
    {"rootViewModel", (int16_t)LuaAtoms::rootViewModel},
    {"dataContext", (int16_t)LuaAtoms::dataContext},
    {"image", (int16_t)LuaAtoms::image},
    {"blob", (int16_t)LuaAtoms::blob},
    {"size", (int16_t)LuaAtoms::size},
    {"duration", (int16_t)LuaAtoms::duration},
    {"setTime", (int16_t)LuaAtoms::setTime},
    {"setTimeFrames", (int16_t)LuaAtoms::setTimeFrames},
    {"setTimePercentage", (int16_t)LuaAtoms::setTimePercentage},
    {"audioEngine", (int16_t)LuaAtoms::audioEngine},
    {"audio", (int16_t)LuaAtoms::audio},
    {"play", (int16_t)LuaAtoms::play},
    {"stop", (int16_t)LuaAtoms::stop},
    {"seek", (int16_t)LuaAtoms::seek},
    {"seekFrame", (int16_t)LuaAtoms::seekFrame},
    {"volume", (int16_t)LuaAtoms::volume},
    {"completed", (int16_t)LuaAtoms::completed},
    {"time", (int16_t)LuaAtoms::time},
    {"timeFrame", (int16_t)LuaAtoms::timeFrame},
};

static const luaL_Reg lualibs[] = {
    {"", luaopen_base},
    {LUA_TABLIBNAME, luaopen_table},
    {LUA_MATHLIBNAME, luaopen_math},
    {"rive", luaopen_rive_base},
    {LUA_OSLIBNAME, luaopen_os},
    {LUA_STRLIBNAME, luaopen_string},
    {LUA_UTF8LIBNAME, luaopen_utf8},
    {LUA_BUFFERLIBNAME, luaopen_buffer},
    {LUA_BITLIBNAME, luaopen_bit32},
    {"math", luaopen_rive_math},
    {"renderer", luaopen_rive_renderer_library},
    {"properties", luaopen_rive_properties},
    {"artboard", luaopen_rive_artboards},
    {"dataValue", luaopen_rive_data_values},
    {"input", luaopen_rive_input},
    {"context", luaopen_rive_contex},
    {"dataContext", luaopen_rive_data_context},
    {"audio", luaopen_rive_audio},
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

int rive_luaErrorHandler(lua_State* L)
{
    ScriptingContext* context =
        static_cast<ScriptingContext*>(lua_getthreaddata(L));
    context->printError(L);

    // Optionally, you can push a new value onto the stack to be returned by
    // lua_pcall For example, push a specific error code or a more detailed
    // message
    const char* error = lua_tostring(L, -1);
    lua_pushstring(L, error);
    return 1; // Number of return values
    // return 0;
}

int rive_lua_pcall(lua_State* state, int nargs, int nresults)
{
    ScriptingContext* context =
        static_cast<ScriptingContext*>(lua_getthreaddata(state));

    return context->pCall(state, nargs, nresults);
}

int rive_lua_pushRef(lua_State* state, int ref)
{
    return lua_rawgeti(state, luaRegistryIndex, ref);
}

void rive_lua_pop(lua_State* state, int count)
{
    lua_settop(state, -count - 1);
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

    // Record missing dependency if we're registering a module
    if (requirerChunkname)
    {
        ScriptingContext* context =
            static_cast<ScriptingContext*>(lua_getthreaddata(L));
        if (context)
        {
            context->recordMissingDependency(requirerChunkname, path);
        }
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

ScriptingVM::ScriptingVM(ScriptingContext* context) :
    m_context(context), m_ownsState(true)
{
    m_state = lua_newstate(l_alloc, nullptr);
    init(m_state, m_context);
}

ScriptingVM::ScriptingVM(ScriptingContext* context, lua_State* existingState) :
    m_state(existingState), m_context(context), m_ownsState(false)
{}

ScriptingVM::~ScriptingVM()
{
    if (m_ownsState)
    {
        lua_close(m_state);
    }
}

// Loads bytecode into a sandboxed thread without executing it.
// On success, pushes the module thread (with loaded closure) onto L's stack.
// Returns true on success.
bool ScriptingVM::loadModule(lua_State* L,
                             const char* name,
                             Span<uint8_t> bytecode)
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

    if (status != 0)
    {
        // luau_load failed — error string is on ML stack
        lua_xmove(ML, L, 1);
        ScriptingContext* context =
            static_cast<ScriptingContext*>(lua_getthreaddata(L));
        context->printError(L);
        lua_pop(L, 2); // pop error + thread
        return false;
    }

    // Thread with loaded closure is on top of L's stack.
    return true;
}

// Executes a previously loaded module thread (on top of L's stack from
// loadModule). On success, replaces the thread with the module result.
// If isUtility, also registers the result in the require cache.
// Returns true on success.
bool ScriptingVM::executeModule(lua_State* L, const char* name, bool isUtility)
{
    // The module thread should be on top of the stack.
    lua_State* ML = lua_tothread(L, -1);
    if (ML == nullptr)
    {
        return false;
    }

    int status = lua_resume(ML, L, 0);
    if (status == 0)
    {
        if (lua_gettop(ML) == 0)
        {
            lua_pushfstring(ML, "%s:1: module must return a value", name);
        }
        else if (!lua_istable(ML, -1) && !lua_isfunction(ML, -1))
        {
            lua_pushfstring(ML,
                            "%s:1: module must return a table or function",
                            name);
        }
    }
    else if (status == LUA_YIELD)
    {
        lua_pushfstring(ML, "%s:1: module can not yield", name);
    }
    else if (!lua_isstring(ML, -1))
    {
        lua_pushfstring(ML, "%s:1: unknown error while running module", name);
    }

    // add ML result to L stack
    lua_xmove(ML, L, 1);
    // An error occurred if the top of the stack is a string.
    if (lua_isstring(L, -1))
    {
        ScriptingContext* context =
            static_cast<ScriptingContext*>(lua_getthreaddata(L));
        context->printError(L);
        lua_pop(L, 2); // pop error + thread
        return false;
    }
    // remove ML thread from L stack
    lua_remove(L, -2);
    // added one value to L stack: module result

    if (isUtility)
    {
        // Register into the require cache directly.
        luaL_findtable(L, LUA_REGISTRYINDEX, registeredCacheTableKey, 1);
        lua_pushstring(L, name);
        lua_pushvalue(L, -3); // copy module result (below cache table + name)
        lua_settable(L, -3);  // cache[name] = result
        lua_pop(L, 1);        // pop cache table
    }

    return true;
}

static void dump_stack(lua_State* state)
{
    int i;
    int top = lua_gettop(state);
    for (i = 1; i <= top; i++)
    { /* repeat for each level */
        int t = lua_type(state, i);
        switch (t)
        {

            case LUA_TSTRING: /* strings */
                fprintf(stderr,
                        "  (%i)[STRING] %s\n",
                        i,
                        lua_tostring(state, i));
                break;

            case LUA_TBOOLEAN: /* booleans */
                fprintf(stderr,
                        "  (%i)[BOOLEAN] %s\n",
                        i,
                        lua_toboolean(state, i) ? "true" : "false");
                break;

            case LUA_TNUMBER: /* numbers */
                fprintf(stderr,
                        "  (%i)[NUMBER] %g\n",
                        i,
                        lua_tonumber(state, i));
                break;

            default: /* other values */
                fprintf(stderr, "  (%i)[%s]\n", i, lua_typename(state, t));
                break;
        }
    }
    fprintf(stderr, "\n"); /* end the listing */
}

void ScriptingVM::dumpStack(lua_State* state) { dump_stack(state); }

void ScriptingContext::addModule(ModuleDetails* moduleDetails)
{
    m_modulesToRegister.push_back(moduleDetails);
    m_moduleLookup[moduleDetails->moduleName()] = moduleDetails;
}

bool ScriptingContext::tryRegisterModule(lua_State* state,
                                         ModuleDetails* moduleDetails)
{
    if (!moduleDetails->verified())
    {
        return false;
    }
    const std::string& name = moduleDetails->moduleName();
    bool registerSuccess = false;
    int functionRef = 0;
    if (moduleDetails->isProtocolScript())
    {
        if (ScriptingVM::registerScript(state,
                                        name.c_str(),
                                        moduleDetails->moduleBytecode()))
        {
            // registerScript leaves the function on the stack
            if (static_cast<lua_Type>(lua_type(state, -1)) == LUA_TFUNCTION)
            {
                functionRef = lua_ref(state, -1);
            }
            lua_pop(state, 1);
            registerSuccess = true;
        }
    }
    else
    {
        if (ScriptingVM::registerModule(state,
                                        name.c_str(),
                                        moduleDetails->moduleBytecode()))
        {
            registerSuccess = true;
        }
    }
    if (registerSuccess)
    {
        moduleDetails->registrationComplete(functionRef);
        onModuleRegistered(moduleDetails);
        return true;
    }

    return false;
}

void ScriptingContext::performRegistration(lua_State* state)
{
    // Loop over all of the modules once. We need do a tryRegister
    // pass on each module in order to determine if it has any
    // required dependencies
    for (ModuleDetails* moduleDetails : m_modulesToRegister)
    {
        // Skip if already registered
        if (checkRegisteredModules(state,
                                   moduleDetails->moduleName().c_str()) == 1)
        {
            continue;
        }
        tryRegisterModule(state, moduleDetails);
    }

    // If any modules had dependencies, resolve their registration order
    // and try registering again
    if (!m_pendingModules.empty())
    {
        std::vector<ModuleDetails*> pendingModules;
        for (auto module : m_pendingModules)
        {
            pendingModules.push_back(module);
        }
        std::vector<ModuleDetails*> sortedModules;
        std::unordered_set<ModuleDetails*> visitedModules;

        ModuleDetails* module = pendingModules.back();
        pendingModules.pop_back();
        sortNextModule(module,
                       &pendingModules,
                       &sortedModules,
                       &visitedModules);

        // Register modules in sorted order
        for (ModuleDetails* moduleDetails : sortedModules)
        {
            tryRegisterModule(state, moduleDetails);
        }
    }

    m_modulesToRegister.clear();
    m_pendingModules.clear();
}

void ScriptingContext::sortNextModule(
    ModuleDetails* module,
    std::vector<ModuleDetails*>* pendingModules,
    std::vector<ModuleDetails*>* sortedModules,
    std::unordered_set<ModuleDetails*>* visitedModules)
{
    // If already visited, skip
    if (visitedModules->find(module) != visitedModules->end())
    {
        return;
    }

    auto dependencies = module->missingDependencies();
    for (const auto& dependencyName : dependencies)
    {
        auto lookupIt = m_moduleLookup.find(dependencyName);
        if (lookupIt != m_moduleLookup.end())
        {
            ModuleDetails* dependencyModule = lookupIt->second;
            // Recursively process the dependency
            sortNextModule(dependencyModule,
                           pendingModules,
                           sortedModules,
                           visitedModules);
        }
    }

    if (std::find(sortedModules->begin(), sortedModules->end(), module) ==
        sortedModules->end())
    {
        sortedModules->push_back(module);
    }
    visitedModules->insert(module);
    if (!pendingModules->empty())
    {
        ModuleDetails* nextModule = pendingModules->back();
        pendingModules->pop_back();
        sortNextModule(nextModule,
                       pendingModules,
                       sortedModules,
                       visitedModules);
    }
}

void ScriptingContext::recordMissingDependency(
    const std::string& requiringModule,
    const std::string& missingModule)
{
    if (!requiringModule.empty())
    {
        ModuleDetails* moduleDetails = m_moduleLookup[requiringModule];
        if (moduleDetails != nullptr)
        {
            moduleDetails->addMissingDependency(missingModule);
            m_pendingModules.insert(moduleDetails);
        }
    }
}

void ScriptingContext::onModuleRegistered(ModuleDetails* moduleDetails)
{
    for (ModuleDetails* module : m_modulesToRegister)
    {
        if (!module->missingDependencies().empty())
        {
            moduleDetails->clearMissingDependency(moduleDetails->moduleName());
        }
    }
    auto it = m_pendingModules.find(moduleDetails);
    if (it != m_pendingModules.end())
    {
        m_pendingModules.erase(it);
    }
}

#ifdef WITH_RIVE_TOOLS
void ScriptingContext::setGeneratorRef(uint32_t assetId, int ref)
{
    m_assetGeneratorRefs[assetId] = ref;
}

int ScriptingContext::getGeneratorRef(uint32_t assetId) const
{
    auto it = m_assetGeneratorRefs.find(assetId);
    return it != m_assetGeneratorRefs.end() ? it->second : 0;
}

void ScriptingContext::clearGeneratorRefs() { m_assetGeneratorRefs.clear(); }

bool ScriptingContext::hasGeneratorRef(uint32_t assetId) const
{
    return m_assetGeneratorRefs.find(assetId) != m_assetGeneratorRefs.end();
}
#endif

bool ScriptingVM::registerScript(lua_State* state,
                                 const char* name,
                                 Span<uint8_t> bytecode)
{
    // Check if already registered
    if (checkRegisteredModules(state, name) == 1)
    {
        return true;
    }

    if (!loadModule(state, name, bytecode))
    {
        return false;
    }

    if (!executeModule(state, name, false))
    {
        return false;
    }

    return true;
}

bool ScriptingVM::registerModule(lua_State* state,
                                 const char* name,
                                 Span<uint8_t> bytecode)
{
    // Check if already registered
    if (checkRegisteredModules(state, name) == 1)
    {
        return true;
    }

    if (!loadModule(state, name, bytecode))
    {
        return false;
    }

    if (!executeModule(state, name, true))
    {
        return false;
    }

    // executeModule with isUtility=true registers into the require cache
    // and leaves the module result on the stack. Pop it.
    lua_pop(state, 1);
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

int CPPRuntimeScriptingContext::pCall(lua_State* state, int nargs, int nresults)
{
    // calculate stack position for message handler
    int hpos = lua_gettop(state) - nargs;
    lua_pushcfunction(state, rive_luaErrorHandler, "riveErrorHandler");
    lua_insert(state, hpos);

    startTimedExecution(state);
    int ret = lua_pcall(state, nargs, nresults, hpos);
    endTimedExecution(state);
    lua_remove(state, hpos);
    return ret;
}

} // namespace rive
#endif
