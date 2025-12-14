
#include "rive/lua/lua_state.hpp"
#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#endif
#include "rive/viewmodel/viewmodel.hpp"
#include <stdio.h>
using namespace rive;

#ifdef WITH_RIVE_SCRIPTING
static int viewmodel_new(lua_State* L)
{
    // Get the viewModel pointer from the upvalue
    ViewModel* viewModel = (ViewModel*)lua_touserdata(L, lua_upvalueindex(1));
    if (viewModel)
    {
        auto instance = viewModel->createInstance();
        lua_newrive<ScriptedViewModel>(L, L, ref_rcp(viewModel), instance);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

void LuaState::initializeData(std::vector<ViewModel*>& viewModels)
{
    if (state)
    {
        // create metatable for global Data
        luaL_newmetatable(state, "Data");

        // push it again as lua_setuserdatametatable pops it
        lua_pushvalue(state, -1);
        lua_setuserdatametatable(state, LUA_T_COUNT + 31);
        lua_pop(state, 1);
        lua_getfield(state, luaRegistryIndex, "Data");
        lua_setglobal(state, "Data");

        // Get the Data metatable back on the stack
        lua_getfield(state, luaRegistryIndex, "Data");

        for (auto& viewModel : viewModels)
        {
            // Create a new table for this viewModel
            lua_createtable(state, 0, 1);

            // Push the viewModel pointer as a light userdata (upvalue)
            lua_pushlightuserdata(state, viewModel);

            // Create a closure with the viewModel as an upvalue
            lua_pushcclosurek(state, viewmodel_new, "new", 1, nullptr);

            // Set the function as "new" in the table
            lua_setfield(state, -2, "new");

            // Set the table as a field in Data metatable using the viewModel
            // name
            lua_setfield(state, -2, viewModel->name().c_str());
        }

        // Pop the Data metatable from the stack
        lua_pop(state, 1);
    }
}

#endif