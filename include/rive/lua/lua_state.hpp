#ifndef _RIVE_LUA_STATE_HPP_
#define _RIVE_LUA_STATE_HPP_

#ifdef WITH_RIVE_SCRIPTING
#include "lua.h"
#endif
#include <vector>

namespace rive
{
class ViewModel;
class LuaState
{
public:
#ifdef WITH_RIVE_SCRIPTING

    LuaState(lua_State* state) : state(state) {}
    ~LuaState() { state = nullptr; }
    void initializeData(std::vector<ViewModel*>&);

    lua_State* state;
#endif
};

} // namespace rive
#endif