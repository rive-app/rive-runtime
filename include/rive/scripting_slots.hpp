#pragma once

// Backend-dependent scripting members resolve through these aliases so
// Artboard/File layout depends only on WITH_RIVE_SCRIPTING and every
// define set from the umbrella up stays ABI compatible. Each alias must
// keep the exact layout of its live counterpart; the asserts hold the
// backends to that.

#include <memory>
#include <vector>

#ifdef WITH_RIVE_SCRIPTING_LUAU
#include "rive/lua/scripting_vm.hpp"
namespace rive
{
using ScriptingVMSlot = rcp<ScriptingVM>;
static_assert(sizeof(ScriptingVMSlot) == sizeof(void*) &&
                  alignof(ScriptingVMSlot) == alignof(void*),
              "luau vm slot must stay pointer sized");
} // namespace rive
#else
namespace rive
{
using ScriptingVMSlot = void*;
} // namespace rive
#endif

#ifdef WITH_RIVE_SCRIPTING_WASM
namespace rive
{
class WasmScriptingVM;
using WasmVMsSlot = std::vector<std::unique_ptr<WasmScriptingVM>>;
static_assert(sizeof(WasmVMsSlot) == sizeof(std::vector<void*>) &&
                  alignof(WasmVMsSlot) == alignof(std::vector<void*>),
              "wasm vm list slot must match a pointer vector");
} // namespace rive
#else
namespace rive
{
using WasmVMsSlot = std::vector<void*>;
} // namespace rive
#endif
