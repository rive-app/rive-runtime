#ifdef WITH_RIVE_SCRIPTING
#ifndef _RIVE_SCRIPTING_VM_HPP_
#define _RIVE_SCRIPTING_VM_HPP_

#include "rive/refcnt.hpp"
#include "rive/span.hpp"
#include <memory>
#include <unordered_set>

struct lua_State;

namespace rive
{
class ModuleDetails;
class ScriptingContext;
class ScriptedObject;

class ScriptingVM : public RefCnt<ScriptingVM>
{
public:
    // Creates a ScriptingVM that owns both the lua_State and the context.
    explicit ScriptingVM(std::unique_ptr<ScriptingContext> context);

    ~ScriptingVM();

    // Closes the lua_State synchronously and runs all userdata finalizers.
    // Idempotent. Called from ~ScriptingVM but exposed so call sites that
    // need teardown to be observable (e.g. between editor file swaps) can
    // force it.
    void closeLuaState();

    // ScriptedObject holds a non-owning raw ScriptingVM*. These methods let
    // the VM track which ScriptedObjects point at it so ~ScriptingVM (and
    // closeLuaState) can null those pointers before cascading lua_close
    // finalizers tear down the ScriptedObjects via their owning userdatas.
    // Otherwise those finalizers would dereference an already-freed VM.
    void registerScriptedObject(ScriptedObject* obj);
    void unregisterScriptedObject(ScriptedObject* obj);

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

    // name is the require cache key, chunkname the trace label baked into
    // the bytecode, defaulting to name.
    static bool registerModule(lua_State* state,
                               const char* name,
                               Span<uint8_t> bytecode,
                               const char* chunkname = nullptr);
    static void unregisterModule(lua_State* state, const char* name);
    bool registerModule(const char* name, Span<uint8_t> bytecode);
    void unregisterModule(const char* name);

    static bool registerScript(lua_State* state,
                               const char* name,
                               Span<uint8_t> bytecode,
                               const char* chunkname = nullptr);

    bool registerScript(const char* name, Span<uint8_t> bytecode);

    // Loads bytecode into an unexecuted sandboxed thread pushed onto the
    // stack. name is the chunkname baked into the proto.
    static bool loadModule(lua_State* state,
                           const char* name,
                           Span<uint8_t> bytecode);

    // Executes a thread from loadModule and registers utility modules under
    // name. The result replaces the thread on the stack.
    static bool executeModule(lua_State* state,
                              const char* name,
                              bool isUtility,
                              const char* chunkname = nullptr);

    static void dumpStack(lua_State* state);

private:
    lua_State* m_state;
    std::unique_ptr<ScriptingContext> m_ownedContext;
    std::unordered_set<ScriptedObject*> m_scriptedObjects;
};

} // namespace rive
#endif
#endif
