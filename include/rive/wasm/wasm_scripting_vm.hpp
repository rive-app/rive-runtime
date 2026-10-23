#ifndef _RIVE_WASM_SCRIPTING_VM_HPP_
#define _RIVE_WASM_SCRIPTING_VM_HPP_

#ifdef WITH_RIVE_SCRIPTING_WASM

#include "rive/async/threading.hpp"
#include "rive/refcnt.hpp"
#include "rive/scripted/script_backend.hpp"
#include "rive/span.hpp"
#include "rive/viewmodel/write_attribution.hpp"
#include "rive/wasm/wasm_lane_layout_gen.hpp"
#include "rive/wasm/wasm_lane_policy.hpp"
#include "rive/wasm/wasm_signal_wait.hpp"

#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace rive
{
class WasmJobs;
class WasmWorkers;
class WasmProcessors;
class WasmLane;
struct VMPollList;
class SharedRegion;

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
class TransformComponent;
class ViewModel;
class WorkTask;
class WasmScriptingVM;
struct PathEffectPaintWire;
namespace scriptnet
{
struct HttpResponse;
}

/// A debugger's view of a module baked with rasc's line probes. Everything
/// arrives on the module's thread: the probes from inside the module, the
/// call brackets around each host call into it, and a stopped probe may
/// block until the debugger resumes.
class WasmDebugHooks
{
public:
  virtual ~WasmDebugHooks() = default;
  virtual void onEnter(
      WasmScriptingVM& vm,
      uint32_t function,
      uint32_t line) = 0;
  /// True when execution stopped here, so the module re-arms its budget.
  virtual bool onLine(WasmScriptingVM& vm, uint32_t line) = 0;
  virtual void onLeave(WasmScriptingVM& vm) = 0;
  virtual void onCallBegin(WasmScriptingVM& vm) = 0;
  /// trap names the exception of a call that died, null for a normal
  /// return; frames the probes left open are the debugger's to drop.
  virtual void onCallEnd(WasmScriptingVM& vm, const char* trap) = 0;
  /// The VM is going away.
  virtual void onDetach(WasmScriptingVM& vm) = 0;
};

class WasmScriptingVM : public ScriptBackend
{
public:
  /// factory makes the real render objects the module's handles resolve to;
  /// it must outlive the VM. print is installed before instantiation, the
  /// only way to see output from module start: rasc runs every script's top
  /// level there, riveRegister included. A module that fails to start sets
  /// outError, unless the boot hook gave it an error sink, which hears the
  /// start failure line instead.
#ifndef __EMSCRIPTEN__
  static std::unique_ptr<WasmScriptingVM> make(
      Span<const uint8_t> module,
      Factory* factory,
      std::string& outError,
      std::function<void(const char*, size_t)> print = {});
#endif
  ~WasmScriptingVM();

  /// One live host object per module handle: 24-bit slot, 7-bit generation,
  /// type tag checked on resolve. Stale or foreign handles resolve null.
  struct HandleTable
  {
    enum class Tag : uint8_t
    {
      empty,
      path,
      measure,
      paint,
      renderer,
      shader,
      object,
      viewModelInstance,
      instanceValue,
      image,
      font,
      buffer,
      meshInstances,
      canvas,
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
      audioSource,
      audioSound,
      drawable,
      text,
      transitionChild,
      // context:decodeFile's files and their bindable artboards.
      riveFile,
      bindableArtboard,
      // rive_lanes_v1's jobs, workers and audio processors.
      lane,
      count,
    };
    struct Slot
    {
      Tag tag = Tag::empty;
      uint8_t generation = 0;
      void* object = nullptr;
    };
    std::vector<Slot> slots;
    std::vector<uint32_t> freeSlots;
    /// Slots whose generation wrapped, oldest first. Each sits out until
    /// retiredLimit more wrap, so its old handles stay stale that long.
    std::deque<uint32_t> retiredSlots;
    size_t retiredLimit = (size_t)1 << 16;
    /// The most slots a handle's 24 slot bits address.
    size_t slotLimit = 0xffffff;

    uint32_t mint(Tag tag, void* object);
    void* resolve(uint32_t handle, Tag tag) const;
    /// Bumps the generation so outstanding copies of the handle go stale.
    void release(uint32_t handle, Tag tag);
    size_t live() const
    {
      return slots.size() - freeSlots.size() - retiredSlots.size();
    }
  };
  HandleTable& handles() { return m_handles; }
  /// Borrowed node handles, released when the path effect update that
  /// minted them returns.
  std::vector<uint32_t>& scopedNodes() { return m_scopedNodes; }
  Factory* factory() const { return m_factory; }

  /// Brackets one call into the module. Calls nest, so the exit reclaims
  /// only the render passes and canvas frames this call left open. The
  /// outermost budgeted scope arms the timeout watchdog.
  class ScriptCallScope
  {
  public:
    explicit ScriptCallScope(WasmScriptingVM* vm, bool budgeted = true);
    ~ScriptCallScope();

  private:
    WasmScriptingVM* m_vm;
#ifdef WITH_RIVE_TOOLS
    WriteAttributionScope m_writeSource{WriteSource::Kind::wasm, m_vm};
#endif
    uint64_t m_passToken = 0;
    uint64_t m_frameToken = 0;
    bool m_budgeted = false;
    bool m_deadlineArmed = false;
    uint32_t m_outerRegionId = 0;
    SharedRegion* m_outerRegion = nullptr;
  };

  /// The deadline of this VM's current budgeted call, polled by the
  /// watchdog thread; 0 when disarmed.
  struct BudgetSlot;

  /// Brackets a wait the host makes for a main lane call. The watchdog
  /// holds off meanwhile, and time past usable() moves the call's
  /// deadlines on, unless the wait began with none usable.
  class WaitScope
  {
  public:
    explicit WaitScope(WasmScriptingVM* vm);
    ~WaitScope();
    /// The seconds of the call's budget the wait may spend, infinity
    /// when the call has none.
    double usable() const { return m_usable; }

  private:
    WasmScriptingVM* m_vm;
    double m_usable;
    std::chrono::steady_clock::time_point m_start;
    int64_t m_paused = 0;
  };

  /// Tokens start at 1, so 0 takes every open frame.
  uint64_t nextCanvasFrameToken() const { return m_nextCanvasFrameToken; }
  void registerOpenCanvasFrame(uint32_t canvas)
  {
    m_openCanvasFrames.push_back({m_nextCanvasFrameToken++, canvas});
  }
  void unregisterOpenCanvasFrame(uint32_t canvas);
  /// Removes and returns the frames begun at or after `token`.
  std::vector<uint32_t> takeOpenCanvasFramesFrom(uint64_t token);

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
  bool registerBytecode(const std::string& name, Span<const uint8_t> bytecode);

  /// Delivers a watched value change to the module's listener registry.
  void notifyDataValueChanged(uint32_t token);

  /// Hands one drawable to the module's draw visitor. False when the
  /// module has none or it trapped.
  bool notifyDrawVisit(uint32_t drawable);

  /// context:decodeImage plumbing. The module issues the token; the decode
  /// runs as a WorkTask on the global WorkPool, so completion lands through
  /// the same per-advance rive_pollAsyncWork poll as the Luau backend and
  /// resolves the module's promise via its host_image_decoded /
  /// host_image_decode_failed exports. Returns false when this build has
  /// no decoders.
  bool startImageDecode(
      const uint8_t* bytes,
      uint32_t byteCount,
      uint32_t token);
  /// Flags the in-flight decode cancelled so the poll skips its callbacks;
  /// fired by the module promise's onCancel hook.
  void cancelImageDecode(uint32_t token);
  /// WorkTask completion callbacks, delivered from rive_pollAsyncWork.
  void resolveImageDecode(
      uint32_t token,
      uint32_t width,
      uint32_t height,
      const uint8_t* pixels,
      uint32_t byteCount);
  void rejectImageDecode(uint32_t token, const char* message);

  /// fetch() plumbing. The module issues the token and passes a NetWire
  /// request; scriptnet runs the policy, this VM's limits and the host's
  /// transport, and the outcome reaches the module on a later
  /// rive_pollAsyncWork, on the thread that started it, through its
  /// host_fetch_resolved / host_fetch_failed exports. Returns false on
  /// hosts built without fetch() support, where nothing would settle, and
  /// for a token already in flight.
  bool startFetch(Span<const uint8_t> request, uint32_t token);
  /// The script cancelled: aborted at the transport, never delivered.
  void cancelFetch(uint32_t token);
  /// scriptnet listener callbacks: the response, or the NetErrorCode and
  /// its "<code>: <message>" rejection.
  void resolveFetch(uint32_t token, scriptnet::HttpResponse&& response);
  void rejectFetch(uint32_t token, uint32_t code, const std::string& message);
  /// Delivers the decode and fetch outcomes that landed while a module
  /// call was running; a no-op while one still is.
  void deliverHeldOutcomes();
  /// deliverHeldOutcomes() for every VM on this thread holding some.
  static void deliverAllHeldOutcomes();
  /// True while a decode or fetch this VM started has yet to reach the
  /// module, held outcomes included.
  bool hasPendingAsyncWork() const;

  /// The module's jobs section, null when it carries none.
  WasmJobs* jobs() const { return m_jobs.get(); }
  /// The module's workers section, null when it carries none.
  WasmWorkers* workers() const { return m_workers.get(); }
  /// The module's audio section, null when it carries none.
  WasmProcessors* processors() const { return m_processors.get(); }
  /// The module carries a section of any kind of lane.
  bool hasLanes() const
  {
    return m_jobs != nullptr || m_workers != nullptr || m_processors != nullptr;
  }
  /// Hands each lane's console lines, and a worker's or processor's
  /// failure, to its VM, on the thread those VMs run on.
  static void deliverAllLaneOutput();
  /// A module call is live on this thread, where a lane run inline could
  /// share its held region.
  static bool inModuleCallOnThread();
  /// Every lane's, once per poll, and settles the main lane's waits.
  void deliverLaneOutput();
  /// One lane's, where its output must land ahead of what the caller
  /// prints next.
  void deliverLaneOutput(WasmLane* lane);
  /// Why a kind's lane module failed to load, empty when it loaded or the
  /// module carries none.
  const std::string& laneLoadError(LaneKind kind) const
  {
    return m_laneLoadErrors[(int)kind];
  }
  /// Registers the VM for lane output on this thread, the only one its
  /// lanes may be started from.
  void watchLanes();
  /// The kind lanes_start last refused with kStartFailed for its load
  /// error, which lanes_failed answers for that code.
  LaneKind refusedStart = LaneKind::job;
  /// The main lane's awaits, settled at a poll through host_lane_wake with
  /// the token they answer.
  uint32_t watchLane(uint32_t lane);
  uint32_t waitOnMain(uint32_t signal, double after, double timeoutSeconds);
  /// Drops the await token answers, whose promise was cancelled.
  void cancelMainWait(uint32_t token);
  /// Owns the shared objects this VM creates, released with it.
  uint64_t sharedOwner();
  /// The holds a ring or snapshot read carried, kept until the std has
  /// decoded the value or, when decoding threw, until the frame ends.
  void keepReadHolds(Span<const uint32_t> ids);
  void releaseReadHolds();

  // ScriptBackend.
  bool valid() const override;
  bool utf16Strings() const { return m_utf16Strings; }
  bool utf16HostStrings() const { return m_utf16HostStrings; }
  bool hostBudget() const { return m_hostBudget; }
  // An init that read data the host had not bound yet fails so it reruns,
  // as the Luau context's missingRequestedData does.
  void noteMissingRequestedData() { m_missingRequestedData = true; }
  void releaseRef(int ref) override;
  int instantiate(
      int generatorRef,
      ScriptedObject* object,
      int* outContextRef,
      ScriptedContext** outContextPtr) override;
  InitResult callUserInit(ScriptedObject* object, int selfRef, int contextRef)
      override;
  bool callAdvance(ScriptedObject* object, int selfRef, float elapsedSeconds)
      override;
  void callUpdate(ScriptedObject* object, int selfRef) override;
  void callTrigger(ScriptedObject* object, int selfRef, const char* name)
      override;
  bool callNumberMethod(
      ScriptedObject* object,
      int selfRef,
      const char* name,
      const float* args,
      size_t argCount,
      float* outResult) override;
  bool callBooleanMethod(ScriptedObject* object, int selfRef, const char* name)
      override;
  /// The node argument exposes the Luau lane's PaintData snapshot of the
  /// ShapePaint (paint / asPaint()); its live transform surface stays host
  /// side.
  bool callPathEffectUpdate(
      ScriptedObject* object,
      int selfRef,
      const RawPath& sourcePath,
      const ShapePaint* shapePaint,
      RawPath* outPath) override;
  /// The same call with the paint already snapshotted, for hosts whose
  /// paint is not a runtime ShapePaint; shape may be null.
  bool callPathEffectUpdate(
      ScriptedObject* object,
      int selfRef,
      const RawPath& sourcePath,
      const PathEffectPaintWire& paint,
      TransformComponent* shape,
      RawPath* outPath);
  bool callDataConvert(
      ScriptedObject* object,
      int selfRef,
      const char* method,
      DataValue* input,
      ScriptDataResult* outResult) override;
  /// Every invocation kind crosses; the host-entangled pointers (focus
  /// group, reported Event, view model source, semantic group) are dropped
  /// because no script surface reads them.
  void callListenerPerform(
      ScriptedObject* object,
      int selfRef,
      const ListenerInvocation& invocation) override;
  void callDraw(ScriptedObject* object, int selfRef, Renderer* renderer)
      override;
  bool callGamepadEvent(
      ScriptedObject* object,
      int selfRef,
      const char* method,
      const ListenerInvocation& invocation) override;
  bool callPointerEvent(
      ScriptedObject* object,
      int selfRef,
      const char* method,
      int pointerId,
      Vec2D localPosition,
      ListenerType hitType,
      float timeStamp,
      HitResult* outResult) override;
  bool callScrollEvent(
      ScriptedObject* object,
      int selfRef,
      int pointerId,
      Vec2D localPosition,
      const ScrollEvent& event,
      float timeStamp,
      HitResult* outResult) override;
  bool callKeyboardEvent(
      ScriptedObject* object,
      int selfRef,
      Key key,
      KeyModifiers modifiers,
      bool isPressed,
      bool isRepeat) override;
  bool callTextEvent(
      ScriptedObject* object,
      int selfRef,
      const std::string& text) override;
  /// The transition protocol is not ported to the module lane yet; there is
  /// no TransitionChild wire type, so these keep the host defaults.
  bool transitionManagesTo(int selfRef) override;
  void callTransitionChanged(
      ScriptedObject* object,
      int selfRef,
      const TransitionChildRef& from,
      const TransitionChildRef& to,
      int direction) override;
  void callTransitionDraw(
      ScriptedObject* object,
      int selfRef,
      Renderer* renderer,
      const TransitionChildRef& from,
      const TransitionChildRef& to) override;
  void callLayoutResize(ScriptedObject* object, int selfRef, Vec2D size)
      override;
  bool callLayoutMeasure(ScriptedObject* object, int selfRef, Vec2D* outSize)
      override;
  void setInputBoolean(int selfRef, const char* name, bool value) override;
  void setInputNumber(int selfRef, const char* name, float value) override;
  void setInputUnsigned(int selfRef, const char* name, uint32_t value) override;
  void setInputString(int selfRef, const char* name, const char* value)
      override;
  /// self[name] becomes a module Artboard userdata over a host-owned
  /// instance handle, mirroring the Luau lane's ScriptedArtboard.
  void setInputArtboard(
      int selfRef,
      const char* name,
      ScriptedObject* object,
      Artboard* artboard) override;
  void setInputViewModel(
      int selfRef,
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
  /// The wasm twin of ScriptingContext::isPlaying: hosts pause and resume
  /// it with playback, and audio refuses to play while it is off.
  void isPlaying(bool value) { m_isPlaying = value; }
  bool isPlaying() const { return m_isPlaying; }
#endif

  const std::string& lastError() const { return m_lastError; }

  /// The representation the module booted on; it runs there for life.
  enum class ExecutionTier : uint8_t
  {
    interp = 0,
    aotO0 = 1,
    aotO3 = 2,
  };
  ExecutionTier executionTier() const { return m_tier; }
  /// How a host's log names a tier, with why it is the interpreter.
  static std::string tierName(
      ExecutionTier tier,
      const std::string& interpReason);
  /// Adds a line per lane module it loaded, as "jobs module tier: aot -O3".
  void laneTierLines(std::vector<std::string>& lines) const;
#ifndef __EMSCRIPTEN__
  /// Loads bytes once for their holders, from compiled code where an artifact
  /// exists. The lease keeps the WAMR module loaded, null with the error.
  static std::shared_ptr<void> loadSharedModule(
      Span<const uint8_t> bytes,
      ExecutionTier& tier,
      std::string& interpReason,
      std::string& error);
  /// How many modules nothing leases stay loaded for a later load of the
  /// same bytes; a load past it unloads the least recently leased.
  static constexpr size_t kIdleModules = 8;
#endif
  /// Why the module runs on the interpreter, for a host's log; empty when
  /// it runs compiled code.
  const std::string& interpReason() const { return m_interpReason; }

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
  /// What frameBoundary returns once a frame collector first collects, a
  /// notice rather than a warning.
  static constexpr char
      kFrameGcNotice[] = "script frame gc: scavenging per frame";
  /// frameBoundary without its notices, for hosts that print it as
  /// warnings.
  const char* frameBoundaryWarnings();
  /// Adds a boundary report to joined, a line each.
  static void joinFrameReport(std::string& joined, std::string_view report);
  const char* handleLeakWarning();
  const char* heapGrowthWarning();

  /// Current size of the module's linear memory in 64KB wasm pages.
  uint32_t memoryPages() const;

  /// Backend seams: host code reaches module memory and module functions
  /// only through these, so a browser backend can substitute staged
  /// copies and JS dispatch for WAMR's direct access.

  /// Resolves size bytes of module memory to a host pointer, null when
  /// out of range. The pointer and writes through it stay coherent until
  /// the next callModule, when a copying backend flushes staged ranges;
  /// WAMR returns the linear memory address directly.
  virtual void* resolveModulePtr(uint32_t appAddr, uint32_t size);
  /// resolveModulePtr for a range the host overwrites whole, such as a
  /// fresh allocation, so a copying backend need not read it first.
  virtual void* resolveModuleWritePtr(uint32_t appAddr, uint32_t size)
  {
    return resolveModulePtr(appAddr, size);
  }

  /// Writes size bytes into module memory, false when out of range. A
  /// copying backend need not stage a range the host never reads.
  virtual bool copyToModule(uint32_t appAddr, const void* src, uint32_t size);

  /// The zero terminated string at appAddr, under the same contract.
  virtual const char* resolveModuleString(uint32_t appAddr);

  /// Calls a module export with i32 args; returns the first result, or 0
  /// when the export is missing or the call traps.
  uint32_t callModule(const char* name, uint32_t argc, uint32_t* argv);

  /// The honest variant: distinguishes a missing export from a trap from
  /// a real zero result, for callers where the difference is a failure.
  enum class CallOutcome
  {
    ok,
    missing,
    trapped,
  };
  virtual CallOutcome callModuleChecked(
      const char* name,
      uint32_t argc,
      uint32_t* argv,
      uint32_t* result);

  virtual bool hasExport(const char* name);

  /// The module memory's size and declared ceiling in 64KB pages; false
  /// when the module has no memory. A ceiling of 0 means unknown.
  virtual bool memoryLimits(uint32_t* pages, uint32_t* maxPages) const;

  /// Ends module execution like a trap once the current native returns.
  virtual void raiseModuleError(const char* message);

  /// raiseModuleError for a spent execution budget, which lastTrap marks.
  void raiseBudgetExceeded();

  /// raiseModuleError for a message the script threw, which lastTrap marks.
  void raiseThrown(const char* message);

  /// What ended the last call that trapped: the message the script threw,
  /// else the runtime's own trap text. A test takes only a thrown trap for
  /// the error it expected.
  struct Trap
  {
    std::string message;
    bool budget = false;
    bool thrown = false;
  };
  const Trap& lastTrap() const { return m_lastTrap; }

  /// Traps still land in lastTrap but print nothing, for callers that
  /// expect them.
  void setQuietTraps(bool quiet) { m_quietTraps = quiet; }

  /// The trap text without the runtime's "Exception: " prefix: the full
  /// raiseModuleError text when `exception` is its truncation (the
  /// runtime's exception buffer is small), otherwise `exception`.
  const char* fullTrapMessage(const char* exception) const;

  /// Function imports the module declares that no host native resolves.
  /// They trap only when first called, so surface them at load instead.
  const std::vector<std::string>& unresolvedImports() const
  {
    return m_unresolvedImports;
  }

  /// Script execution budget, matching the Luau backend: 0 disables the
  /// metering, which is the host's trust decision for validated content
  /// (armed metering costs about 5 percent on loop heavy scripts). Starts
  /// at the limits' main call budget.
  void setTimeoutMs(int ms);
  int timeoutMs() const { return m_timeoutMs; }
  /// What a lane's run may take while it holds the page's thread, as a call
  /// may: the call budget where it bounds calls, else 0.
  uint32_t pageThreadBudgetMs() const;
  /// The host's limits when the VM was made.
  const ScriptingLimits& limits() const { return m_limits; }

  /// Backs a Tests suite's blob(name); empty when the blob is missing,
  /// which every blob is outside test runs.
  using TestBlobLookup = std::function<Span<const uint8_t>(const std::string&)>;
  void setTestBlobs(TestBlobLookup lookup) { m_testBlobs = std::move(lookup); }
  Span<const uint8_t> testBlob(const std::string& name) const
  {
    return m_testBlobs ? m_testBlobs(name) : Span<const uint8_t>();
  }

  /// Receives script print output; defaults to stdout.
  void onPrint(std::function<void(const char*, size_t)> handler)
  {
    m_print = std::move(handler);
  }

  /// Receives each script error line, such as a trapped call, a failed
  /// lane or a module that failed to start. Without it they go to stderr,
  /// and a lane's failure to the print sink.
  void onError(std::function<void(const std::string&)> handler)
  {
    m_error = std::move(handler);
  }
  void reportError(const std::string& line);
  static constexpr char
      kModuleStartFailed[] = "wasm script module failed to start: ";

  /// A debugger over this module's line probes, or null. The hooks must
  /// outlive the VM or clear themselves first.
  void setDebugHooks(WasmDebugHooks* hooks) { m_debugHooks = hooks; }
  WasmDebugHooks* debugHooks() const { return m_debugHooks; }

  /// Runs for every VM made on the calling thread, after its print sink
  /// is set and before its module starts, with the module as baked: the
  /// place to install debug hooks and a print sink that see module
  /// start, where rasc runs every script's top level.
  using BootHook = std::function<
      void(WasmScriptingVM& vm, Span<const uint8_t> module)>;
  static void setBootHook(BootHook hook);

  /// Modules started after the call must carry a bound this host can
  /// enforce, their own fuel or natively the watchdog, as an editor needs.
  static void requireBounds(bool on);

  /// The module can never run again.
  bool poisoned() const { return m_poisoned; }

protected:
  /// Loads the lane modules the main module carries, and their policy.
  void attachLanes(Span<const uint8_t> module);
  /// Never runs the module again, nor any lane it started.
  void poison();

  // The browser backend subclasses over the seams and boots without the
  // WAMR init, so it shares construction, error, factory, and lua state.
  // Out of line so subclass TUs need no complete member types.
  WasmScriptingVM();
  std::string m_lastError;
  // Full text of the last raiseModuleError; see fullTrapMessage().
  std::string m_moduleErrorDetail;
  Factory* m_factory = nullptr;
  uint32_t m_L = 0;
  uint32_t m_callDepth = 0;
  // Method names the module keeps past the frame, one copy per instance.
  std::unordered_map<std::string, uint32_t> m_guestNames;
  // console.time starts by label, in steady nanoseconds.
  std::unordered_map<std::string, int64_t> m_consoleTimers;
  bool m_advancedOnce = false;
  bool m_missingRequestedData = false;
  /// Module exports __riveUtf16Strings: its string arguments arrive as
  /// UTF-16. Modules baked before that, and the Luau one, pass UTF-8.
  bool m_utf16Strings = false;
  /// Exports modules built before them lack, probed once at boot.
  bool m_rawAllocExport = false;
  bool m_inputSlotExport = false;
  bool m_pathEffectV2 = false;
  bool m_pointerEventV2 = false;
  bool m_layoutResizeV2 = false;
  bool m_transitionManagesToExport = false;
  std::function<void(const char*, size_t)> m_print;
  std::function<void(const std::string&)> m_error;

  /// Reads a flag export. Backends call it off the module before start,
  /// which runs every script's top level and already passes strings.
  void noteModuleExport(const char* name);
  /// What follows instantiation on every backend: the optional export
  /// probes, then the module side boot. False with m_lastError set.
  bool bootModule();
  /// Fills lastTrap from a backend's trap text and logs it unless quiet.
  void recordTrap(const char* where, const char* exception);
  /// Calls an export with args converted to its parameter types, for
  /// exports that take floats. name must be a string literal: a backend
  /// may cache the export by the pointer.
  virtual CallOutcome callF64(
      const char* name,
      const double* args,
      uint32_t argc,
      double* result);
  uint32_t guestString(const char* text, const char* allocator = "malloc");
  void guestFree(uint32_t ptr);
  // A call scoped handle over a transition child, 0 for an empty one.
  uint32_t mintTransitionChild(const TransitionChildRef& ref);
  void releaseTransitionChild(uint32_t handle);
  void releaseScopedNodes(size_t from);
  // A method name in guest memory: made once per instance when the module
  // can keep it past the frame, else copied for this call alone.
  class GuestName
  {
  public:
    GuestName(WasmScriptingVM* vm, const char* name);
    ~GuestName();
    uint32_t ptr = 0;

  private:
    WasmScriptingVM* m_vm;
    bool m_owned = false;
  };
  // The input argument of a set or trigger call: the slot, or the name
  // itself for modules baked before the slot ABI, which owned then holds
  // for the caller to free. False when the instance lacks the input.
  bool inputArg(int selfRef, const char* name, uint32_t& arg, uint32_t& owned);

private:
#ifndef __EMSCRIPTEN__
  bool init(Span<const uint8_t> module);
#endif
  // The module's slot for an input name, resolved once per instance; -1
  // when the instance declares no such input.
  int32_t inputSlot(int selfRef, const char* name);
  bool legacyInputs();
  std::unordered_map<int, std::unordered_map<std::string, int32_t>>
      m_inputSlots;

#ifndef __EMSCRIPTEN__
  struct WamrState;
  std::unique_ptr<WamrState> m_state;
#endif
  ExecutionTier m_tier = ExecutionTier::interp;
  std::string m_interpReason;
  WasmDebugHooks* m_debugHooks = nullptr;
  std::vector<std::string> m_unresolvedImports;
  ScriptingLimits m_limits = ScriptingLimits::get();
  int m_timeoutMs = (int)m_limits.mainCallBudgetMs;
  TestBlobLookup m_testBlobs;
  HandleTable m_handles;
  std::vector<uint32_t> m_scopedNodes;
  /// Canvas frames begun and not ended, as handles with a monotonic token
  /// so a script call's exit reclaims only the frames it began.
  struct OpenCanvasFrame
  {
    uint64_t token;
    uint32_t canvas;
  };
  std::vector<OpenCanvasFrame> m_openCanvasFrames;
  uint64_t m_nextCanvasFrameToken = 1;
  /// Object handles minted for the init scoped context, released once init
  /// completes or the context ref is released.
  std::unordered_map<int, uint32_t> m_contextObjects;
  /// In-flight decodes by module token, for cancellation; completed or
  /// cancelled entries erase themselves.
#ifdef __EMSCRIPTEN__
  /// The browser decodes, so each token keeps its browser request id.
  std::unordered_map<uint32_t, uint32_t> m_pendingDecodes;
#else
  std::unordered_map<uint32_t, rcp<WorkTask>> m_pendingDecodes;
  uint64_t m_decodeOwnerId = 0;
#endif
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
  // Logs the pending trap and clears it so later calls can run.
  void reportTrap(const char* where);
  /// Calls a pointer or scroll export, whose 0 means unhandled and anything
  /// else is 1 + the HitResult the handler reported.
  bool callHitExport(
      const char* name,
      const double* args,
      uint32_t argc,
      HitResult* outResult);
  /// In-flight fetches: module token to scriptnet request id, for
  /// cancellation. The owner id scopes this VM's rate limits and lets
  /// teardown cancel everything it started.
  std::unordered_map<uint32_t, uint32_t> m_pendingFetches;
  uint64_t m_netOwnerId = 0;
  void deliverFetchFailure(
      uint32_t token,
      uint32_t code,
      const std::string& message);
  /// A nested advance polls async work while the module is mid call;
  /// settling its promises there would run script under that call.
  bool inModuleCall() const;
  void holdOutcome(std::function<void()> deliver);
  std::vector<std::function<void()>> m_heldOutcomes;
  struct MainWait
  {
    SignalWait wait;
    // The watched lane's handle, 0 for a signal wait.
    uint32_t lane;
  };
  std::vector<MainWait> m_mainWaits;
  uint32_t m_nextMainWait = 1;
  void settleMainWaits();
  std::unique_ptr<BudgetSlot> m_budgetSlot;
  std::unique_ptr<WasmJobs> m_jobs;
  std::unique_ptr<WasmWorkers> m_workers;
  std::unique_ptr<WasmProcessors> m_processors;
  std::string m_laneLoadErrors[3];
  // The polls that reach its lanes and its held outcomes, null until it
  // has some.
#if RIVE_ONE_THREAD()
  VMPollList* m_laneList = nullptr;
  VMPollList* m_holdingList = nullptr;
#else
  std::shared_ptr<VMPollList> m_laneList;
  std::shared_ptr<VMPollList> m_holdingList;
#endif
  uint64_t m_sharedOwner = 0;
  std::vector<uint32_t> m_readHolds;
  /// Set while a budgeted call holds the slot's deadline, which calls
  /// nested in it share.
  bool m_budgetArmed = false;
  /// Gives the call running on env (a wasm_exec_env_t) one timeout, and
  /// takes it back, poisoning the module when the watchdog fired.
  // Fuel stops metered code first with a trap the module survives; the
  // watchdog, which poisons it, only catches code fuel cannot see.
  int watchdogBudgetMs() const { return m_timeoutMs * (m_fuelBudget ? 2 : 1); }
  void armWatchdog(void* env);
  void disarmWatchdog();
  /// The budget bounds the module's calls and its top level.
  bool boundsCalls() const;
  /// Set while a bounded call holds the deadline this VM's waits read,
  /// which calls nested in it share.
  bool m_deadlineArmed = false;
  std::chrono::steady_clock::time_point m_outerDeadline;
  /// Gives a bounded call's waits their deadline; false when a call it
  /// nests in already did, or the module has no budget or no lanes.
  bool armDeadline();
  void disarmDeadline();
#ifdef __EMSCRIPTEN__
  /// What a web start set aside until it ends, as a call's scope does: the
  /// VM booting when it began and the region the thread held.
  WasmScriptingVM* m_outerBooting = nullptr;
  uint32_t m_outerRegionId = 0;
  SharedRegion* m_outerRegion = nullptr;
#endif
  /// This VM's own boundary work, without the files it decoded.
  const char* collectFrame();
  /// The VM whose script decoded this VM's file, and the VMs of files
  /// this one's scripts decoded, which collect at its boundaries.
  WasmScriptingVM* m_decodedBy = nullptr;
  std::vector<WasmScriptingVM*> m_decodedVMs;
  /// What frameBoundary returns once those VMs report too, one per line.
  std::string m_frameReport;
  /// Module exports __riveUtf16HostStrings: strings the host hands it are
  /// written as UTF-16 too.
  bool m_utf16HostStrings = false;
  /// Module exports __riveHostBudget: the watchdog bounds its calls. The
  /// rest meter themselves or are trusted to run unbounded.
  bool m_hostBudget = false;
  /// Module exports __riveFuelCheck: it traps itself once a call overruns.
  bool m_fuelBudget = false;
  bool m_fuelShift = false;
  /// Module exports __riveFetchBlobBody: a fetched body crosses as a blob.
  bool m_fetchBlobBody = false;
  /// Set once the watchdog interrupts a call or the collector traps; the
  /// module never runs again.
  bool m_poisoned = false;
  bool m_budgetRaised = false;
  bool m_thrownRaised = false;
  bool m_quietTraps = false;
  Trap m_lastTrap;
  bool m_frameMinor = false;
  bool m_frameMinorAnnounced = false;
  /// Module exports __riveFrameRelease: its reap queues the handles it
  /// finds dead and the host releases them right after each collection.
  bool m_frameRelease = false;
  /// Module exports __riveHeapUsed; the leak watch reads bump bytes
  /// instead of page counts.
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
  uint32_t frameMajors();
  uint8_t m_handleCollects = 0;
  uint32_t m_handleCollectMajors = 0;
  bool m_frameMajorsProbe = false;
  RawPath* m_pathEffectOut = nullptr;
  ScriptDataResult* m_convertResultOut = nullptr;
  std::vector<ViewModel*>* m_viewModels = nullptr;
  File* m_file = nullptr;
  int m_embeddedModules = 0;
#ifdef WITH_RIVE_TOOLS
  std::unordered_map<std::string, std::vector<uint8_t>> m_shaderRstbs;
  bool m_isPlaying = false;
#endif

  friend struct WasmScriptingVMNatives;
};

} // namespace rive

#endif
#endif
