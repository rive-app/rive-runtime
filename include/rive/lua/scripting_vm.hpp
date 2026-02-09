#ifdef WITH_RIVE_SCRIPTING
#ifndef _RIVE_SCRIPTING_VM_HPP_
#define _RIVE_SCRIPTING_VM_HPP_

#include "rive/refcnt.hpp"
#include "rive/span.hpp"
#include <memory>

struct lua_State;

namespace rive
{
class ModuleDetails;
class ScriptingContext;

class ScriptingVM : public RefCnt<ScriptingVM>
{
public:
    // Creates a ScriptingVM that owns both the lua_State and the context.
    explicit ScriptingVM(std::unique_ptr<ScriptingContext> context);

    ~ScriptingVM();

    ScriptingContext* context() { return m_ownedContext.get(); }
    lua_State* state() const { return m_state; }

    // Replaces the owned context with a new one. The old context is deleted
    // and the new context is owned by this VM. Also updates lua thread data.
    void replaceContext(std::unique_ptr<ScriptingContext> newContext);

    static void init(lua_State* state, ScriptingContext* context);

    // Add a module to be registered later via performRegistration()
    void addModule(ModuleDetails* moduleDetails);

    // Perform registration of all added modules, handling dependencies and
    // retries
    void performRegistration();

    static bool registerModule(lua_State* state,
                               const char* name,
                               Span<uint8_t> bytecode);
    static void unregisterModule(lua_State* state, const char* name);
    bool registerModule(const char* name, Span<uint8_t> bytecode);
    void unregisterModule(const char* name);

    static bool registerScript(lua_State* state,
                               const char* name,
                               Span<uint8_t> bytecode);

    bool registerScript(const char* name, Span<uint8_t> bytecode);

    // Loads bytecode into the VM, creating a sandboxed thread and
    // deserializing the bytecode into an unexecuted closure. Pushes the
    // module thread onto the stack. Returns true on success.
    static bool loadModule(lua_State* state,
                           const char* name,
                           Span<uint8_t> bytecode);

    // Executes a previously loaded module thread (on top of stack from
    // loadModule). Runs lua_resume, validates the result, and for utility
    // modules registers into the require cache. On success, the module
    // result (table/function) replaces the thread on the stack.
    // Returns true on success.
    static bool executeModule(lua_State* state,
                              const char* name,
                              bool isUtility);

    static void dumpStack(lua_State* state);

private:
    lua_State* m_state;
    std::unique_ptr<ScriptingContext> m_ownedContext;
};

} // namespace rive
#endif
#endif
