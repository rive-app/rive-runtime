#ifndef _RIVE_LUA_STATE_HPP_
#define _RIVE_LUA_STATE_HPP_

#ifdef WITH_RIVE_SCRIPTING
#include "lua.h"
#endif
#include <vector>

namespace rive
{
class ViewModel;

#ifdef WITH_RIVE_SCRIPTING
void initializeLuaData(lua_State* state, std::vector<ViewModel*>& viewModels);
#endif

} // namespace rive
#endif