#ifdef WITH_RIVE_SCRIPTING
#ifndef _RIVE_SCRIPTING_VM_HPP_
#define _RIVE_SCRIPTING_VM_HPP_

#include "rive/refcnt.hpp"
#include "rive/scripted/script_backend.hpp"
#include "rive/span.hpp"
#include <memory>
#include <unordered_set>

struct lua_State;

namespace rive
{
class ModuleDetails;
class ScriptingContext;
class ScriptedObject;

class ScriptingVM : public RefCnt<ScriptingVM>, public ScriptBackend
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

    ScriptingContext* context() { return m_ownedContext.get(); }
    lua_State* state() const { return m_state; }

    // ScriptBackend, implemented in src/lua/lua_script_backend.cpp.
    bool valid() const override { return m_state != nullptr; }
    void releaseRef(int ref) override;
    int resolveGeneratorRef(int ref) const override;
    int instantiate(int generatorRef,
                    ScriptedObject* object,
                    int* outContextRef,
                    ScriptedContext** outContextPtr) override;
    InitResult callUserInit(ScriptedObject* object,
                            int selfRef,
                            int contextRef) override;
    bool callAdvance(ScriptedObject* object,
                     int selfRef,
                     float elapsedSeconds) override;
    void callUpdate(ScriptedObject* object, int selfRef) override;
    void callTrigger(ScriptedObject* object,
                     int selfRef,
                     const char* name) override;
    bool callNumberMethod(ScriptedObject* object,
                          int selfRef,
                          const char* name,
                          const float* args,
                          size_t argCount,
                          float* outResult) override;
    bool callBooleanMethod(ScriptedObject* object,
                           int selfRef,
                           const char* name) override;
    bool callPathEffectUpdate(ScriptedObject* object,
                              int selfRef,
                              const RawPath& sourcePath,
                              const ShapePaint* shapePaint,
                              RawPath* outPath) override;
    bool callDataConvert(ScriptedObject* object,
                         int selfRef,
                         const char* method,
                         DataValue* input,
                         ScriptDataResult* outResult) override;
    void callListenerPerform(ScriptedObject* object,
                             int selfRef,
                             const ListenerInvocation& invocation) override;
    void callDraw(ScriptedObject* object,
                  int selfRef,
                  Renderer* renderer) override;
    bool callGamepadEvent(ScriptedObject* object,
                          int selfRef,
                          const char* method,
                          const ListenerInvocation& invocation) override;
    bool callPointerEvent(ScriptedObject* object,
                          int selfRef,
                          const char* method,
                          int pointerId,
                          Vec2D localPosition,
                          HitResult* outResult) override;
    bool callKeyboardEvent(ScriptedObject* object,
                           int selfRef,
                           Key key,
                           KeyModifiers modifiers,
                           bool isPressed,
                           bool isRepeat) override;
    bool callTextEvent(ScriptedObject* object,
                       int selfRef,
                       const std::string& text) override;
    void callLayoutResize(ScriptedObject* object,
                          int selfRef,
                          Vec2D size) override;
    bool callLayoutMeasure(ScriptedObject* object,
                           int selfRef,
                           Vec2D* outSize) override;
    void setInputBoolean(int selfRef, const char* name, bool value) override;
    void setInputNumber(int selfRef, const char* name, float value) override;
    void setInputUnsigned(int selfRef,
                          const char* name,
                          uint32_t value) override;
    void setInputString(int selfRef,
                        const char* name,
                        const char* value) override;
    void setInputArtboard(int selfRef,
                          const char* name,
                          ScriptedObject* object,
                          Artboard* artboard) override;
    void setInputViewModel(int selfRef,
                           const char* name,
                           ViewModelInstanceValue* value) override;

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
};

} // namespace rive
#endif
#endif
