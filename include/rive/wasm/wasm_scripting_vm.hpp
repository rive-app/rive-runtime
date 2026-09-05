#ifndef _RIVE_WASM_SCRIPTING_VM_HPP_
#define _RIVE_WASM_SCRIPTING_VM_HPP_

#ifdef WITH_RIVE_SCRIPTING_WASM

#include "rive/refcnt.hpp"
#include "rive/scripted/script_backend.hpp"
#include "rive/span.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace rive
{

/// Executes a .riv's self contained wasm script module (VM + bindings +
/// compiled scripts) under WAMR. One instance per file instance. The module
/// needs no instantiation time wiring beyond emscripten's setjmp/longjmp glue,
/// provided here as native symbols per the luau_wasm EH spike. Implements the
/// ScriptBackend seam over the module's host_obj_* exports, whose bodies
/// mirror the Luau backend.
class Factory;
class File;
class RenderPaint;
class RenderPath;
class ViewModel;
class WorkTask;

class WasmScriptingVM : public ScriptBackend
{
public:
    /// factory makes the real render objects the module's handles resolve to;
    /// it must outlive the VM. print is installed before instantiation, the
    /// only way to see output from module start: rasc runs every script's top
    /// level there, riveRegister included.
    static std::unique_ptr<WasmScriptingVM> make(
        Span<const uint8_t> module,
        Factory* factory,
        std::string& outError,
        std::function<void(const char*, size_t)> print = {});
    ~WasmScriptingVM();

    /// One live host object per module handle: 24-bit slot, 8-bit generation,
    /// type tag checked on resolve. Stale or foreign handles resolve null.
    struct HandleTable
    {
        enum class Tag : uint8_t
        {
            empty,
            path,
            paint,
            renderer,
            shader,
            object,
            viewModelInstance,
            instanceValue,
            image,
            font,
            buffer,
            gpuCanvas,
            gpuPass,
            gpuBuffer,
            gpuTexture,
            gpuSampler,
            gpuTextureView,
            gpuShaderModule,
            gpuBindGroupLayout,
            gpuBindGroup,
            gpuPipeline,
            dataContext,
            artboard,
            animation,
            node,
        };
        struct Slot
        {
            Tag tag = Tag::empty;
            uint8_t generation = 0;
            void* object = nullptr;
        };
        std::vector<Slot> slots;
        std::vector<uint32_t> freeSlots;

        uint32_t mint(Tag tag, void* object);
        void* resolve(uint32_t handle, Tag tag) const;
        /// Bumps the generation so outstanding copies of the handle go stale.
        void release(uint32_t handle, Tag tag);
    };
    HandleTable& handles() { return m_handles; }
    Factory* factory() const { return m_factory; }

    /// The file's view models, backing the module's Data constructors; the
    /// file outlives the VM.
    void viewModels(std::vector<ViewModel*>* value) { m_viewModels = value; }
    std::vector<ViewModel*>* viewModels() const { return m_viewModels; }

    /// The owning file, for id-bound asset property resolution through its
    /// registry; the file outlives the VM.
    void file(File* value) { m_file = value; }
    File* file() const { return m_file; }

    /// Loads and runs a module registered in the embedded registry; returns
    /// false with the error in lastError(). outResultRef, when set, receives
    /// a ref to the module's result (the generator for protocol scripts).
    bool requireModule(const std::string& name, int* outResultRef = nullptr);
    /// Registers loose Luau bytecode under `name` for require, the dynamic
    /// twin of the linker's embedded registry; lets bytecode-only files run
    /// on a stock vm module.
    bool registerBytecode(const std::string& name,
                          Span<const uint8_t> bytecode);

    /// Delivers a watched value change to the module's listener registry.
    void notifyDataValueChanged(uint32_t token);

    /// context:decodeImage plumbing. The module issues the token; the decode
    /// runs as a WorkTask on the global WorkPool, so completion lands through
    /// the same per-advance rive_pollAsyncWork poll as the Luau backend and
    /// resolves the module's promise via its host_image_decoded /
    /// host_image_decode_failed exports. Returns false when this build has
    /// no decoders.
    bool startImageDecode(const uint8_t* bytes,
                          uint32_t byteCount,
                          uint32_t token);
    /// Flags the in-flight decode cancelled so the poll skips its callbacks;
    /// fired by the module promise's onCancel hook.
    void cancelImageDecode(uint32_t token);
    /// WorkTask completion callbacks, delivered from rive_pollAsyncWork.
    void resolveImageDecode(uint32_t token,
                            uint32_t width,
                            uint32_t height,
                            const uint8_t* pixels,
                            uint32_t byteCount);
    void rejectImageDecode(uint32_t token, const char* message);

    /// Advances detached view model instances the module holds handles to,
    /// mirroring the Luau context's tracked instance advance.
    void advanceDetachedViewModels();

    // ScriptBackend.
    bool valid() const override;
    void releaseRef(int ref) override;
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
    /// The node argument exposes the Luau lane's PaintData snapshot of the
    /// ShapePaint (paint / asPaint()); its live transform surface stays host
    /// side.
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
    /// Every invocation kind crosses; the host-entangled pointers (focus
    /// group, reported Event, view model source, semantic group) are dropped
    /// because no script surface reads them.
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
    /// self[name] becomes a module Artboard userdata over a host-owned
    /// instance handle, mirroring the Luau lane's ScriptedArtboard.
    void setInputArtboard(int selfRef,
                          const char* name,
                          ScriptedObject* object,
                          Artboard* artboard) override;
    void setInputViewModel(int selfRef,
                           const char* name,
                           ViewModelInstanceValue* value) override;

    /// Result sinks the module's rive_path_v1.effect_result and
    /// rive_data_v1.convert_result imports write through; non-null only
    /// while the corresponding seam call runs.
    RawPath* pathEffectOut() const { return m_pathEffectOut; }
    ScriptDataResult* convertResultOut() const { return m_convertResultOut; }

    /// Modules baked into the embedded registry, from host_register_embedded.
    int embeddedModuleCount() const { return m_embeddedModules; }

#ifdef WITH_RIVE_TOOLS
    /// Edit-time shader side-band, the wasm twin of ScriptingContext's RSTB
    /// registry: requestWasmVM registers freshly compiled WGSL here and the
    /// module's shader natives consult it before file assets.
    void registerShaderRstb(std::string name, std::vector<uint8_t> bytes);
    const std::vector<uint8_t>* findShaderRstb(const std::string& name) const;
#endif

    const std::string& lastError() const { return m_lastError; }

    /// The representation currently executing; interp is tier 0, artifacts
    /// arrive by transplant.
    enum class ExecutionTier : uint8_t
    {
        interp = 0,
        aotO0 = 1,
        aotO3 = 2,
    };
    ExecutionTier executionTier() const { return m_tier; }

    /// Queue this module on the process tier ladder; arrivals are picked up
    /// by maybeUpgradeTier. laneId identifies the logical script so a newer
    /// module content supersedes in-flight compiles.
    void scheduleTierCompiles(const std::string& laneId);

    /// Frame-boundary poll: when a better artifact than the current tier is
    /// in the ladder cache, swap onto it, carrying all live state. Call with
    /// no wasm frames on the stack. Returns true when a swap happened.
    bool maybeUpgradeTier();

    /// The per-frame collection point: frame collector modules scavenge
    /// here (call once during load after init so init promotion lands in
    /// load time). All rasc modules also get the leak watch: once the heap
    /// grows well past its post-first-advance baseline the call returns a
    /// warning for the host's log channel, re-armed per 8MB of further
    /// growth and carrying a time-to-trap estimate (RIVE_WASM_LEAK_WARN=0
    /// silences it, e.g. for benches).
    ///
    /// Call once per host frame with no wasm frames live; returns null when
    /// there is nothing to report.
    const char* frameBoundary();
    const char* handleLeakWarning();
    const char* heapGrowthWarning();

    /// Current size of the module's linear memory in 64KB wasm pages.
    uint32_t memoryPages() const;

    /// Swap execution onto a compiled artifact of this module, carrying
    /// memory, globals, and tables. No wasm frames may be live. On failure
    /// the current instance keeps running.
    bool applyTierArtifact(Span<const uint8_t> artifactBytes,
                           ExecutionTier tier,
                           std::string& error,
                           bool hwBounds = false);

    /// Backend seams: host code reaches module memory and module functions
    /// only through these, so a browser backend can substitute staged
    /// copies and JS dispatch for WAMR's direct access.

    /// Resolves size bytes of module memory to a host pointer, null when
    /// out of range. The pointer and writes through it stay coherent until
    /// the next callModule, when a copying backend flushes staged ranges;
    /// WAMR returns the linear memory address directly.
    virtual void* resolveModulePtr(uint32_t appAddr, uint32_t size);

    /// Calls a module export with i32 args; returns the first result, or 0
    /// when the export is missing or the call traps.
    virtual uint32_t callModule(const char* name,
                                uint32_t argc,
                                uint32_t* argv);

    /// The honest variant: distinguishes a missing export from a trap from
    /// a real zero result, for callers where the difference is a failure.
    enum class CallOutcome
    {
        ok,
        missing,
        trapped,
    };
    CallOutcome callModuleChecked(const char* name,
                                  uint32_t argc,
                                  uint32_t* argv,
                                  uint32_t* result);

    /// Ends module execution like a trap once the current native returns.
    virtual void raiseModuleError(const char* message);

    /// Function imports the module declares that no host native resolves.
    /// They trap only when first called, so surface them at load instead.
    const std::vector<std::string>& unresolvedImports() const
    {
        return m_unresolvedImports;
    }

    /// Script execution budget, matching the Luau backend: 0 disables the
    /// metering, which is the host's trust decision for validated content
    /// (armed metering costs about 5 percent on loop heavy scripts).
    void setTimeoutMs(int ms);
    int timeoutMs() const { return m_timeoutMs; }

    /// Budget for VMs created after the call; the host's trust decision for
    /// files whose modules it baked itself (e.g. dangerouslyFast content).
    static void defaultTimeoutMs(int ms) { sm_defaultTimeoutMs = ms; }

    /// Receives script print output; defaults to stdout.
    void onPrint(std::function<void(const char*, size_t)> handler)
    {
        m_print = std::move(handler);
    }

protected:
    // The browser backend subclasses over the seams and boots without the
    // WAMR init, so it shares construction, error, factory, and lua state.
    // Out of line so subclass TUs need no complete member types.
    WasmScriptingVM();
    std::string m_lastError;
    Factory* m_factory = nullptr;
    uint32_t m_L = 0;

private:
    bool init(Span<const uint8_t> module);
    uint32_t guestString(const char* text);
    void guestFree(uint32_t ptr);
    // The module's slot for an input name, resolved once per instance; -1
    // when the instance declares no such input.
    int32_t inputSlot(int selfRef, const char* name);
    // The input argument of a set or trigger call: the slot, or the name
    // itself for modules baked before the slot ABI, which owned then holds
    // for the caller to free. False when the instance lacks the input.
    bool inputArg(int selfRef,
                  const char* name,
                  uint32_t& arg,
                  uint32_t& owned);
    bool legacyInputs() const;
    std::unordered_map<int, std::unordered_map<std::string, int32_t>>
        m_inputSlots;

    struct WamrState;
    std::unique_ptr<WamrState> m_state;
    std::vector<uint8_t> m_moduleBytes;
    // Stable view of the module bytes once the shared cache owns them.
    Span<const uint8_t> m_scheduleBytes;
    uint64_t m_moduleKey = 0;
    ExecutionTier m_tier = ExecutionTier::interp;
    // A debug boot (RIVE_WASM_AOT_SYNC=o0) pins the tier: no background
    // compile is scheduled and no upgrade runs, so the module keeps
    // running the -O0 code a debugger and stable codegen want.
    bool m_tierPinned = false;
    std::function<void(const char*, size_t)> m_print;
    std::vector<std::string> m_unresolvedImports;
    static int sm_defaultTimeoutMs;
    int m_timeoutMs = sm_defaultTimeoutMs;
    HandleTable m_handles;
    /// Object handles minted for the init scoped context, released once init
    /// completes or the context ref is released.
    std::unordered_map<int, uint32_t> m_contextObjects;
    /// In-flight decodes by module token, for cancellation; the pool owns
    /// execution, completed or cancelled entries erase themselves.
    std::unordered_map<uint32_t, rcp<WorkTask>> m_pendingDecodes;
    uint64_t m_decodeOwnerId = 0;
    // Delivery is synchronous, so the pixels borrow the decoder's buffer.
    struct DecodeResult
    {
        bool ok = false;
        uint32_t token = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        Span<const uint8_t> pixels;
        std::string error;
    };
    void deliverDecodeResult(const DecodeResult& result);
    bool m_advancedOnce = false;
    bool m_frameMinor = false;
    bool m_frameMinorAnnounced = false;
    /// Module exports __riveHeapUsed; the leak watch reads bump bytes
    /// instead of page counts, which pregrown aot memory freezes.
    bool m_heapUsedProbe = false;
    /// Collected runtimes warn only on a second consecutive growth window.
    bool m_leakArmedCollected = false;
    bool m_leakWatch = false;
    std::string m_leakWarning;
    uint32_t m_leakBaselinePages = 0;
    uint32_t m_leakFirstBaselinePages = 0;
    uint32_t m_leakFrames = 0;
    uint32_t m_leakTotalFrames = 0;
    uint32_t m_leakWarningCount = 0;
    bool m_leakTrapContextPrinted = false;
    bool m_collectedRuntime = false;
    bool m_handleWatch = false;
    uint32_t m_handleBaselineLive = 0;
    uint32_t m_handleFrames = 0;
    RawPath* m_pathEffectOut = nullptr;
    ScriptDataResult* m_convertResultOut = nullptr;
    std::vector<ViewModel*>* m_viewModels = nullptr;
    File* m_file = nullptr;
    int m_embeddedModules = 0;
#ifdef WITH_RIVE_TOOLS
    std::unordered_map<std::string, std::vector<uint8_t>> m_shaderRstbs;
#endif

    friend struct WasmScriptingVMNatives;
};

} // namespace rive

#endif
#endif
