#ifdef WITH_RIVE_SCRIPTING_WASM

#include "rive/wasm/aot_artifact.hpp"
#include "rive/wasm/module_tier_ladder.hpp"
#include "rive/wasm/wamr_state_transplant.hpp"
#include "rive/wasm/wasm_scripting_vm.hpp"
#include <unordered_map>
#ifdef WITH_RIVE_TEXT
#include "rive/text/raw_text.hpp"
#include "rive/text/cursor.hpp"
#include "rive/text/font_hb.hpp"
#endif
#include "rive/wasm/prelinked_aot.hpp"
#if WASM_ENABLE_PRELINKED_AOT != 0
// AOT_MAGIC_NUMBER / AOT_CURRENT_VERSION for container validation; same
// internal-header precedent as wamr_state_transplant.cpp.
#include "aot_runtime.h"
#endif

#include "rive/animation/linear_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/listener_invocation.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/input/focus_manager.hpp"
#include "rive/artboard.hpp"
#include "rive/assets/blob_asset.hpp"
#include "rive/custom_property_boolean.hpp"
#include "rive/custom_property_color.hpp"
#include "rive/assets/manifest_asset.hpp"
#include "rive/custom_property.hpp"
#include "rive/custom_property_number.hpp"
#include "rive/custom_property_string.hpp"
#include "rive/drawable.hpp"
#ifdef WITH_RIVE_AUDIO
#include "rive/assets/audio_asset.hpp"
#include "rive/audio/audio_engine.hpp"
#include "rive/audio/audio_sound.hpp"
#include "rive/audio/audio_source.hpp"
#endif
#include "rive/bones/root_bone.hpp"
#include "rive/constraints/constraint.hpp"
#include "rive/container_component.hpp"
#include "rive/math/transform_components.hpp"
#include "rive/node.hpp"
#include "rive/shapes/path.hpp"
#include "rive/transform_component.hpp"
#include "rive/viewmodel/viewmodel_instance_symbol_list_index.hpp"
#include "rive/wasm/artboard_wire.hpp"
#include "rive/assets/font_asset.hpp"
#include "rive/assets/image_asset.hpp"
#include "rive/assets/shader_asset.hpp"
#include "rive/async/work_pool.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/data_bind/data_values/data_value_boolean.hpp"
#include "rive/data_bind/data_values/data_value_color.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include "rive/factory.hpp"
#include "rive/file.hpp"
#include "rive/shapes/paint/feather.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/paint/image_sampler.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/paint/solid_color.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/math/contour_measure.hpp"
#include "rive/math/path_measure.hpp"
#include "rive/renderer.hpp"
#include "rive/scripted/decoded_file.hpp"
#include "rive/scripted/scripted_object.hpp"
#include "rive/scripted/transition_child_ref.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_string.hpp"
#include "rive/viewmodel/data_enum.hpp"
#include "rive/viewmodel/data_enum_value.hpp"
#include "rive/viewmodel/viewmodel_instance_color.hpp"
#include "rive/viewmodel/viewmodel_instance_enum.hpp"
#include "rive/viewmodel/viewmodel_property_enum.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance_asset_blob.hpp"
#include "rive/viewmodel/viewmodel_instance_asset_font.hpp"
#include "rive/viewmodel/viewmodel_instance_asset_image.hpp"
#include "rive/wasm/data_convert_wire.hpp"
#include "rive/wasm/gamepad_wire.hpp"
#include "rive/wasm/listener_wire.hpp"
#include "rive/wasm/path_effect_wire.hpp"
#include "rive/wasm/net_wire.hpp"
#include "rive/bindable_artboard.hpp"
#include "rive/viewmodel/viewmodel_instance_artboard.hpp"
#ifdef WITH_RIVE_SCRIPTNET
#include "rive/scriptnet/net.hpp"
#endif

#ifdef RIVE_DECODERS
#include "rive/decoders/bitmap_decoder.hpp"

#include <chrono>
#include <sys/stat.h>
#endif

#ifdef RIVE_CANVAS
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/cmd/deferred_canvas_host.hpp"
#include "rive/renderer/rive_render_image.hpp"
#endif
#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
#include "rive/renderer/ore/ore_context.hpp"
#include "rive/renderer/ore/ore_deferred_bind_groups.hpp"
#include "rive/renderer/ore/ore_script_guards.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/cmd/ore_deferred_render_pass.hpp"
#include "rive/renderer/cmd/deferred_render_resource.hpp"

#include <algorithm>
#include <vector>
#include <atomic>
#include <unordered_map>
#include <condition_variable>
#include <thread>
#include <mutex>
#include <cmath>
#include <cassert>
#endif

#include "wasm_export.h"

#include <chrono>
#include <cstddef>
#include <string.h>
#include <time.h>

using namespace rive;

namespace
{

constexpr char kLongjmpTag[] = "rive-longjmp";

WasmScriptingVM* vmFromEnv(wasm_exec_env_t env)
{
    return static_cast<WasmScriptingVM*>(wasm_runtime_get_user_data(env));
}

// Module start runs inside wasm_runtime_instantiate, before an exec env
// exists to carry the vm, so natives called from there resolve none. rasc
// runs every script's top level there, and its output is the one thing a
// host still needs; the sink for that window is parked here.
thread_local const std::function<void(const char*, size_t)>* s_bootPrint =
    nullptr;
// The VM whose module is starting, for the natives its top level reaches.
thread_local WasmScriptingVM* s_booting = nullptr;
thread_local WasmScriptingVM::BootHook s_bootHook;

} // namespace

struct rive::WasmScriptingVMNatives
{
    static void print(WasmScriptingVM* vm, const char* data, size_t size);
    static WasmScriptingVM* adoptBooting(wasm_exec_env_t env);
};

// Loaded modules are immutable and shared: fast-interp translation costs
// ~11ms per load while instantiation is microseconds, and every file
// instance of the same content reloads identical bytes. Entries live for
// the process; the byte buffer must outlive the module (wasm_runtime_load
// keeps referencing it).
struct SharedWasmModule
{
    // The buffer WAMR loads from and references for the module's lifetime;
    // the loader null-terminates import/export names in place, so these
    // bytes are unfit to recompile.
    std::vector<uint8_t> bytes;
    // Pristine pre-load copy handed to wamrc for the AOT lane.
    std::vector<uint8_t> pristineBytes;
    wasm_module_t module = nullptr;
    // Artifact-backed entries hand every later VM their real tier; without
    // this a cache hit reports interp while running compiled code.
    WasmScriptingVM::ExecutionTier tier =
        WasmScriptingVM::ExecutionTier::interp;
};

static std::unordered_map<uint64_t, SharedWasmModule>& sharedModuleCache()
{
    static std::unordered_map<uint64_t, SharedWasmModule> cache;
    return cache;
}

struct WasmScriptingVM::WamrState
{
    // Backing for an artifact loaded by a tier swap; wasm_runtime_load keeps
    // referencing it.
    std::vector<uint8_t> artifactBytes;
    wasm_module_t module = nullptr;
    bool ownsModule = true;
    wasm_module_inst_t instance = nullptr;
    wasm_exec_env_t execEnv = nullptr;
    uint32_t callDepth = 0;
    std::unordered_map<std::string, uint32_t> guestNames;

    // Export names reach callModule as literals, so the pointer is the key;
    // the owned copy lets debug builds catch a caller that reuses a buffer.
    struct ExportSlot
    {
        const char* key = nullptr;
        std::string name;
        wasm_function_inst_t function = nullptr;
    };
    ExportSlot exportCache[64];

    wasm_function_inst_t lookupExport(const char* name)
    {
        ExportSlot& slot = exportCache[(reinterpret_cast<uintptr_t>(name) *
                                        0x9E3779B97F4A7C15ull) >>
                                       58];
        if (slot.key != name)
        {
            wasm_function_inst_t function =
                wasm_runtime_lookup_function(instance, name);
            if (function == nullptr)
            {
                return nullptr;
            }
            slot = {name, name, function};
        }
        assert(slot.name == name);
        return slot.function;
    }

    ~WamrState()
    {
        if (execEnv != nullptr)
        {
            wasm_runtime_destroy_exec_env(execEnv);
        }
        if (instance != nullptr)
        {
            wasm_runtime_deinstantiate(instance);
        }
        if (module != nullptr && ownsModule)
        {
            wasm_runtime_unload(module);
        }
    }
};

#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
namespace
{
ore::Context* gpuOreContext(WasmScriptingVM* vm);
}
#endif
#ifdef RIVE_CANVAS
namespace
{
void canvasEndFrameImpl(WasmScriptingVM* vm, uint32_t canvas);
}
#endif

uint32_t WasmScriptingVM::callModule(const char* name,
                                     uint32_t argc,
                                     uint32_t* argv)
{
    uint32_t result = 0;
    callModuleChecked(name, argc, argv, &result);
    return result;
}

struct WasmScriptingVM::BudgetSlot
{
    std::atomic<int64_t> deadline{0};
    // Set when the watchdog interrupted the last budgeted call.
    std::atomic<bool> fired{false};
    wasm_exec_env_t env = nullptr;
};

namespace
{
// One thread bounds every VM's calls, so modules need no fuel counters: it
// polls each VM's deadline and, on an overrun, sets the call's terminate
// flag so the interpreter or AOT code traps at its next loop back-edge.
// Calls only store and swap an atomic, so the script thread never waits.
constexpr int64_t kFiring = -1;

int64_t steadyNanos()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

class ScriptWatchdog
{
public:
    // Leaked, with a detached thread, so no static destructor joins it or
    // frees the mutex a VM in another static still uses.
    static ScriptWatchdog& instance()
    {
        static ScriptWatchdog* watchdog = new ScriptWatchdog();
        return *watchdog;
    }

    void add(WasmScriptingVM::BudgetSlot* slot)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_started)
        {
            m_started = true;
            std::thread([this] { run(); }).detach();
        }
        m_slots.push_back(slot);
    }

    void remove(WasmScriptingVM::BudgetSlot* slot)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_slots.erase(std::remove(m_slots.begin(), m_slots.end(), slot),
                      m_slots.end());
    }

    // Calls arm without the lock; only an arm that finds the thread idle
    // takes it, to end a wait that has no deadline.
    void armed()
    {
        if (m_idle.load())
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_wake.notify_one();
        }
    }

private:
    void run()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (true)
        {
            // Idle is published before the scan, so an arm either shows up
            // in it or sees idle and ends the wait.
            m_idle.store(true);
            int64_t now = steadyNanos();
            int64_t earliest = 0;
            for (WasmScriptingVM::BudgetSlot* slot : m_slots)
            {
                int64_t deadline = slot->deadline.load();
                if (deadline > 0 && now >= deadline &&
                    slot->deadline.compare_exchange_strong(deadline, kFiring))
                {
                    slot->fired.store(true);
                    wasm_runtime_rive_interrupt(slot->env);
                    slot->deadline.store(0);
                }
                else if (deadline > 0 && (earliest == 0 || deadline < earliest))
                {
                    earliest = deadline;
                }
            }
            if (earliest == 0)
            {
                m_wake.wait(lock);
                continue;
            }
            m_idle.store(false);
            m_wake.wait_until(lock,
                              std::chrono::steady_clock::time_point(
                                  std::chrono::duration_cast<
                                      std::chrono::steady_clock::duration>(
                                      std::chrono::nanoseconds(earliest))));
        }
    }

    std::mutex m_mutex;
    std::condition_variable m_wake;
    std::vector<WasmScriptingVM::BudgetSlot*> m_slots;
    std::atomic<bool> m_idle{false};
    bool m_started = false;
};
} // namespace

WasmScriptingVM::ScriptCallScope::ScriptCallScope(WasmScriptingVM* vm,
                                                  bool budgeted) :
    m_vm(vm)
{
    if (vm->m_debugHooks != nullptr)
    {
        vm->m_debugHooks->onCallBegin(*vm);
    }
    // The outermost scope sweeps what any nested call left open.
    if (vm->m_state->callDepth++ != 0)
    {
        return;
    }
    // Paths that consume their own traps never reach reportTrap to clear it.
    vm->m_budgetRaised = false;
    vm->m_thrownRaised = false;
    // A debugger pauses mid call, so its sessions run unbudgeted.
    if (budgeted && vm->m_hostBudget && vm->m_timeoutMs > 0 &&
        vm->m_debugHooks == nullptr)
    {
        if (vm->m_budgetSlot == nullptr)
        {
            vm->m_budgetSlot = std::make_unique<BudgetSlot>();
            ScriptWatchdog::instance().add(vm->m_budgetSlot.get());
        }
        BudgetSlot* slot = vm->m_budgetSlot.get();
        slot->env = vm->m_state->execEnv;
        slot->fired.store(false);
        slot->deadline.store(steadyNanos() +
                             (int64_t)vm->m_timeoutMs * 1000000);
        ScriptWatchdog::instance().armed();
        m_budgeted = true;
    }
#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
    ore::Context* oreContext = gpuOreContext(vm);
    m_passToken = oreContext != nullptr ? oreContext->nextRenderPassToken() : 0;
#endif
#ifdef RIVE_CANVAS
    m_frameToken = vm->m_nextCanvasFrameToken;
#endif
}

WasmScriptingVM::ScriptCallScope::~ScriptCallScope()
{
    if (m_budgeted)
    {
        // Swapping out a zero means the watchdog claimed the deadline; wait
        // out its firing so the interrupt never lands on a later call.
        BudgetSlot* slot = m_vm->m_budgetSlot.get();
        int64_t deadline = slot->deadline.load();
        while (deadline == kFiring ||
               !slot->deadline.compare_exchange_weak(deadline, 0))
        {
            deadline = slot->deadline.load();
        }
        if (slot->fired.load())
        {
            fprintf(stderr,
                    "script error: execution exceeded %d millisecond timeout\n",
                    m_vm->m_timeoutMs);
            // The interrupt can land inside the module's allocator or
            // collector, so its heap is no longer trusted.
            m_vm->m_poisoned = true;
            fprintf(stderr,
                    "script stopped: its module may have been interrupted "
                    "mid-update\n");
        }
        wasm_runtime_rive_clear_interrupt(slot->env);
    }
    if (--m_vm->m_state->callDepth == 0)
    {
#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
        ore::Context* oreContext = gpuOreContext(m_vm);
        if (oreContext != nullptr &&
            oreContext->finishOpenRenderPassesFrom(m_passToken) != 0)
        {
            fprintf(stderr,
                    "GPU render pass left open at script return. Call finish() "
                    "on render passes before returning.\n");
        }
#endif
#ifdef RIVE_CANVAS
        std::vector<uint32_t> openFrames =
            m_vm->takeOpenCanvasFramesFrom(m_frameToken);
        for (uint32_t canvas : openFrames)
        {
            canvasEndFrameImpl(m_vm, canvas);
        }
        if (!openFrames.empty())
        {
            fprintf(stderr,
                    "Canvas frame left open at script return. Call endFrame() "
                    "before returning.\n");
        }
#endif
    }
    if (m_vm->m_debugHooks != nullptr)
    {
        // Still set here when the call trapped; the caller clears it.
        const char* trap =
            m_vm->m_state != nullptr && m_vm->m_state->instance != nullptr
                ? wasm_runtime_get_exception(m_vm->m_state->instance)
                : nullptr;
        m_vm->m_debugHooks->onCallEnd(*m_vm, trap);
    }
}

WasmScriptingVM::CallOutcome WasmScriptingVM::callModuleChecked(
    const char* name,
    uint32_t argc,
    uint32_t* argv,
    uint32_t* result)
{
    if (m_poisoned)
    {
        return CallOutcome::trapped;
    }
    wasm_function_inst_t f = m_state->lookupExport(name);
    if (f == nullptr)
    {
        // Callers probing optional exports read the outcome; a plain
        // callModule folds this to zero, so probe before relying on it.
        return CallOutcome::missing;
    }
    uint32_t buf[8] = {0};
    for (uint32_t i = 0; i < argc; i++)
    {
        buf[i] = argv[i];
    }
    bool ok;
    {
        // Runtime services, the collector among them, must never be cut off.
        ScriptCallScope callScope(this, strncmp(name, "__rive", 6) != 0);
        ok = m_state->callDepth > 1
                 ? wasm_runtime_call_wasm_nested(m_state->execEnv, f, argc, buf)
                 : wasm_runtime_call_wasm(m_state->execEnv, f, argc, buf);
    }
    if (!ok)
    {
        reportTrap(name);
        return CallOutcome::trapped;
    }
    *result = buf[0];
    return CallOutcome::ok;
}

void WasmScriptingVM::reportTrap(const char* where)
{
    wasm_module_inst_t inst = m_state->instance;
    // A silent fold hides real traps; name them so a script that dies
    // mid-call is diagnosable instead of a mystery no-op.
    const char* exception = fullTrapMessage(wasm_runtime_get_exception(inst));
    // AOT code traps as unreachable; both tiers name the timeout.
    m_lastTrap.budget = m_budgetRaised ||
                        (m_budgetSlot != nullptr && m_budgetSlot->fired.load());
    m_lastTrap.thrown = m_thrownRaised && !m_lastTrap.budget;
    m_budgetRaised = false;
    m_thrownRaised = false;
    if (m_lastTrap.budget)
    {
        exception = "execution exceeded timeout";
    }
    m_lastTrap.message = exception != nullptr ? exception : "";
    if (exception == nullptr)
    {
        return;
    }
    if (!m_quietTraps)
    {
        fprintf(stderr, "wasm call trapped in %s: %s\n", where, exception);
#if WASM_ENABLE_DUMP_CALL_STACK != 0
        wasm_runtime_dump_call_stack(m_state->execEnv);
#endif
    }
    if (m_leakWarningCount > 0 && !m_leakTrapContextPrinted && !m_quietTraps)
    {
        // A bare trap after leak warnings is almost always the memory
        // ceiling; say so once for hosts that dropped the warning strings.
        m_leakTrapContextPrinted = true;
        fprintf(stderr,
                "wasm call trapped after %u script heap leak warnings; the "
                "module likely hit its wasmMaxPages ceiling\n",
                m_leakWarningCount);
    }
    // A pending exception makes WAMR refuse the next call, the collector's
    // included.
    wasm_runtime_clear_exception(inst);
}

void WasmScriptingVM::unregisterOpenCanvasFrame(uint32_t canvas)
{
    for (size_t i = 0; i < m_openCanvasFrames.size(); i++)
    {
        if (m_openCanvasFrames[i].canvas == canvas)
        {
            m_openCanvasFrames.erase(m_openCanvasFrames.begin() + i);
            return;
        }
    }
}

std::vector<uint32_t> WasmScriptingVM::takeOpenCanvasFramesFrom(uint64_t token)
{
    std::vector<uint32_t> taken;
    size_t keep = 0;
    for (size_t i = 0; i < m_openCanvasFrames.size(); i++)
    {
        if (m_openCanvasFrames[i].token >= token)
        {
            taken.push_back(m_openCanvasFrames[i].canvas);
        }
        else
        {
            m_openCanvasFrames[keep++] = m_openCanvasFrames[i];
        }
    }
    m_openCanvasFrames.resize(keep);
    return taken;
}

void* WasmScriptingVM::resolveModulePtr(uint32_t appAddr, uint32_t size)
{
    wasm_module_inst_t inst = m_state->instance;
    if (!wasm_runtime_validate_app_addr(inst, appAddr, size))
    {
        return nullptr;
    }
    return wasm_runtime_addr_app_to_native(inst, appAddr);
}

void WasmScriptingVM::raiseBudgetExceeded()
{
    m_budgetRaised = true;
    raiseModuleError("execution exceeded timeout");
}

void WasmScriptingVM::raiseThrown(const char* message)
{
    m_thrownRaised = true;
    raiseModuleError(message);
}

void WasmScriptingVM::raiseModuleError(const char* message)
{
    // The runtime's exception buffer truncates long messages (shader
    // compiler output); keep the full text for trap reporting.
    m_moduleErrorDetail = message;
    wasm_runtime_set_exception(m_state->instance, message);
}

const char* WasmScriptingVM::fullTrapMessage(const char* exception) const
{
    if (exception == nullptr)
    {
        return nullptr;
    }
    const char* text = exception;
    constexpr char kPrefix[] = "Exception: ";
    if (strncmp(text, kPrefix, sizeof(kPrefix) - 1) == 0)
    {
        text += sizeof(kPrefix) - 1;
    }
    // Substitute only when the exception is a truncation of the detail, so
    // an unrelated later trap keeps its own message. An empty remainder
    // matches every prefix and must not adopt stale detail.
    if (*text != '\0' && !m_moduleErrorDetail.empty() &&
        m_moduleErrorDetail.compare(0, strlen(text), text) == 0)
    {
        return m_moduleErrorDetail.c_str();
    }
    return text;
}

WasmScriptingVM* WasmScriptingVMNatives::adoptBooting(wasm_exec_env_t env)
{
    // Instantiate assigns the same instance on success and null on failure.
    if (s_booting != nullptr)
    {
        s_booting->m_state->instance = wasm_runtime_get_module_inst(env);
    }
    return s_booting;
}

void WasmScriptingVMNatives::print(WasmScriptingVM* vm,
                                   const char* data,
                                   size_t size)
{
    const std::function<void(const char*, size_t)>* sink =
        vm != nullptr ? &vm->m_print : s_bootPrint;
    if (sink != nullptr && *sink)
    {
        (*sink)(data, size);
    }
    else
    {
        fwrite(data, 1, size, stdout);
    }
}

namespace
{

// A string argument as the impl cores read it, UTF-8. A module that hands
// over its own UTF-16 has it transcoded here, on the stack for anything the
// size of a name.
class WasmStringArg
{
public:
    WasmStringArg(WasmScriptingVM* vm, const char* bytes, uint32_t byteCount) :
        m_data(bytes), m_size(byteCount)
    {
        // Module start calls in before the vm rides on the exec env.
        WasmScriptingVM* owner = vm != nullptr ? vm : s_booting;
        if (owner == nullptr || !owner->utf16Strings() || bytes == nullptr)
        {
            return;
        }
        auto in = reinterpret_cast<const uint8_t*>(bytes);
        uint32_t units = byteCount / 2;
        // A unit is three bytes at most, a surrogate pair four for its two.
        char* out = m_inline;
        if ((size_t)units * 3 > sizeof(m_inline))
        {
            m_heap.resize((size_t)units * 3);
            out = &m_heap[0];
        }
        m_data = out;
        auto unitAt = [in](uint32_t i) {
            return (uint32_t)in[i * 2] | (uint32_t)in[i * 2 + 1] << 8;
        };
        for (uint32_t i = 0; i < units; i++)
        {
            uint32_t code = unitAt(i);
            if (code >= 0xD800 && code < 0xDC00 && i + 1 < units)
            {
                uint32_t low = unitAt(i + 1);
                if (low >= 0xDC00 && low < 0xE000)
                {
                    code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                    i++;
                }
            }
            // A lone surrogate encodes as it stands, as the module's own
            // encoder did.
            if (code < 0x80)
            {
                *out++ = (char)code;
            }
            else if (code < 0x800)
            {
                *out++ = (char)(0xC0 | code >> 6);
                *out++ = (char)(0x80 | (code & 0x3F));
            }
            else if (code < 0x10000)
            {
                *out++ = (char)(0xE0 | code >> 12);
                *out++ = (char)(0x80 | (code >> 6 & 0x3F));
                *out++ = (char)(0x80 | (code & 0x3F));
            }
            else
            {
                *out++ = (char)(0xF0 | code >> 18);
                *out++ = (char)(0x80 | (code >> 12 & 0x3F));
                *out++ = (char)(0x80 | (code >> 6 & 0x3F));
                *out++ = (char)(0x80 | (code & 0x3F));
            }
        }
        m_size = (uint32_t)(out - m_data);
    }

    const char* data() const { return m_data; }
    uint32_t size() const { return m_size; }

private:
    const char* m_data;
    uint32_t m_size;
    char m_inline[192];
    std::string m_heap;
};

// Transcodes UTF-8 the way the module's own decoder would, writing only the
// units that fit; returns the full UTF-16 byte length.
size_t encodeUtf16(const char* data, size_t size, uint8_t* out, size_t capacity)
{
    auto in = reinterpret_cast<const uint8_t*>(data);
    size_t written = 0;
    auto unit = [&](uint32_t value) {
        if (written + 2 <= capacity)
        {
            out[written] = (uint8_t)value;
            out[written + 1] = (uint8_t)(value >> 8);
        }
        written += 2;
    };
    size_t i = 0;
    while (i < size)
    {
        uint32_t code = in[i];
        if (code < 0x80)
        {
            unit(code);
            i++;
            continue;
        }
        size_t tail = (code & 0xE0) == 0xC0 ? 1 : (code & 0xF0) == 0xE0 ? 2 : 3;
        if (i + tail >= size)
        {
            break;
        }
        code &= 0x3F >> tail;
        for (size_t k = 1; k <= tail; k++)
        {
            code = code << 6 | (in[i + k] & 0x3F);
        }
        i += tail + 1;
        if (code < 0x10000)
        {
            unit(code);
        }
        else
        {
            code -= 0x10000;
            unit(0xD800 | code >> 10);
            unit(0xDC00 | (code & 0x3FF));
        }
    }
    return written;
}

bool moduleReadsUtf16(WasmScriptingVM* vm)
{
    WasmScriptingVM* owner = vm != nullptr ? vm : s_booting;
    return owner != nullptr && owner->utf16HostStrings();
}

// A host string in the encoding the module reads.
std::string moduleStringBytes(WasmScriptingVM* vm,
                              const char* data,
                              size_t size)
{
    if (!moduleReadsUtf16(vm))
    {
        return std::string(data, size);
    }
    std::string out(encodeUtf16(data, size, nullptr, 0), '\0');
    encodeUtf16(data, size, reinterpret_cast<uint8_t*>(&out[0]), out.size());
    return out;
}

// The retrying length contract in the module's encoding: copies what fits,
// returns the full byte length.
uint32_t fillModuleString(WasmScriptingVM* vm,
                          const std::string& value,
                          char* buffer,
                          uint32_t capacity)
{
    if (!moduleReadsUtf16(vm))
    {
        memcpy(buffer, value.data(), std::min<size_t>(value.size(), capacity));
        return (uint32_t)value.size();
    }
    return (uint32_t)encodeUtf16(value.data(),
                                 value.size(),
                                 reinterpret_cast<uint8_t*>(buffer),
                                 capacity);
}

// Module start runs inside wasm_runtime_instantiate, before the exec env
// carries the vm, so a native that traps there adopts the booting one.
WasmScriptingVM* bootVmFromEnv(wasm_exec_env_t env)
{
    WasmScriptingVM* vm = vmFromEnv(env);
    return vm != nullptr ? vm : WasmScriptingVMNatives::adoptBooting(env);
}

// Prototypes, descriptor PODs, and registration tables for the rive_*_v1
// namespaces come from the binding IDL (src/wasm/idl/bindings.py); the
// declarations pin each implementation below to the contract signature.
#include "wasm_natives_gen.hpp"

// --- emscripten setjmp/longjmp glue -----------------------------------------

void throwLongjmpNative(wasm_exec_env_t env)
{
    wasm_runtime_set_exception(wasm_runtime_get_module_inst(env), kLongjmpTag);
}

void invokeViiNative(wasm_exec_env_t env,
                     uint32_t index,
                     uint32_t a1,
                     uint32_t a2)
{
    WasmScriptingVM* vm = vmFromEnv(env);
    wasm_module_inst_t inst = wasm_runtime_get_module_inst(env);
    uint32_t sp = vm->callModule("emscripten_stack_get_current", 0, nullptr);
    uint32_t argv[2] = {a1, a2};
    if (wasm_runtime_call_indirect(env, index, 2, argv))
    {
        return;
    }
    const char* exception = wasm_runtime_get_exception(inst);
    if (exception != nullptr && strstr(exception, kLongjmpTag) != nullptr)
    {
        wasm_runtime_clear_exception(inst);
        uint32_t restoreArgs[1] = {sp};
        vm->callModule("_emscripten_stack_restore", 1, restoreArgs);
        uint32_t threwArgs[2] = {1, 0};
        vm->callModule("setThrew", 2, threwArgs);
        return;
    }
    // Genuine trap: leave the exception set so it propagates outward.
}

// --- remaining emscripten env imports ---------------------------------------

void abortJs(wasm_exec_env_t env)
{
    wasm_runtime_set_exception(wasm_runtime_get_module_inst(env),
                               "script module abort");
}

void memcpyJs(wasm_exec_env_t env, uint32_t dst, uint32_t src, uint32_t n)
{
    WasmScriptingVM* vm = vmFromEnv(env);
    void* dstPtr = vm->resolveModulePtr(dst, n);
    void* srcPtr = vm->resolveModulePtr(src, n);
    if (dstPtr == nullptr || srcPtr == nullptr)
    {
        wasm_runtime_set_exception(wasm_runtime_get_module_inst(env),
                                   "script module memcpy out of bounds");
        return;
    }
    memmove(dstPtr, srcPtr, n);
}

double getNow(wasm_exec_env_t env)
{
    // Same dev hook as dateNow: clock() feeds Luau's default RNG seed. A
    // pinned clock also parks the module's execution budget.
    if (getenv("RIVE_WASM_FIXED_DATE") != nullptr)
    {
        return 0;
    }
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double, std::milli>(now).count();
}

double dateNow(wasm_exec_env_t env)
{
    // Dev hook: a pinned date (epoch ms) keeps wasm lanes reproducible when
    // harnesses A/B the same module over wall-clock-seeded content.
    static const char* fixed = getenv("RIVE_WASM_FIXED_DATE");
    if (fixed != nullptr)
    {
        return atof(fixed);
    }
    auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration<double, std::milli>(now).count();
}

// sbrk's growth request: enlarge linear memory to cover `size` bytes.
// The module's declared maximum still caps the growth.
uint32_t resizeHeap(wasm_exec_env_t env, uint32_t size)
{
    wasm_module_inst_t inst = wasm_runtime_get_module_inst(env);
    wasm_memory_inst_t memory = wasm_runtime_get_memory(inst, 0);
    if (memory == nullptr)
    {
        return 0;
    }
    const uint64_t kPageSize = 65536;
    uint64_t wantPages = ((uint64_t)size + kPageSize - 1) / kPageSize;
    uint64_t curPages = wasm_memory_get_cur_page_count(memory);
    if (wantPages <= curPages)
    {
        return 1;
    }
    return wasm_runtime_enlarge_memory(inst, wantPages - curPages) ? 1 : 0;
}

static bool localTime(double epochSeconds, time_t& at, struct tm& local)
{
    at = (time_t)epochSeconds;
#ifdef _WIN32
    return localtime_s(&local, &at) == 0;
#else
    return localtime_r(&at, &local) != nullptr;
#endif
}

// Seconds the local rendering sits ahead of UTC.
static int32_t utcOffsetOf(struct tm local, time_t at)
{
#ifdef _WIN32
    return (int32_t)(_mkgmtime(&local) - at);
#else
    return (int32_t)(timegm(&local) - at);
#endif
}

// --- emscripten time imports: the Luau VM blob's os.date and os.time walk
// through these, so they serve the host's real calendar. Without WASM_BIGINT
// the module passes time_t as two i32 halves. struct tm is musl's wasm32
// layout: nine ints, the gmtoff long, then tm_zone, which the module's own
// libc fills.

static time_t moduleTime(uint32_t low, uint32_t high)
{
    return (time_t)(((uint64_t)high << 32) | low);
}

static void writeTm(wasm_exec_env_t env,
                    uint32_t tmPtr,
                    const struct tm& time,
                    int32_t gmtoff)
{
    int32_t* fields = (int32_t*)vmFromEnv(env)->resolveModulePtr(tmPtr, 44);
    if (fields == nullptr)
    {
        return;
    }
    fields[0] = time.tm_sec;
    fields[1] = time.tm_min;
    fields[2] = time.tm_hour;
    fields[3] = time.tm_mday;
    fields[4] = time.tm_mon;
    fields[5] = time.tm_year;
    fields[6] = time.tm_wday;
    fields[7] = time.tm_yday;
    fields[8] = time.tm_isdst;
    fields[9] = gmtoff;
}

void gmtimeJs(wasm_exec_env_t env, uint32_t low, uint32_t high, uint32_t tmPtr)
{
    time_t at = moduleTime(low, high);
    struct tm utc;
#ifdef _WIN32
    if (gmtime_s(&utc, &at) != 0)
#else
    if (gmtime_r(&at, &utc) == nullptr)
#endif
    {
        return;
    }
    writeTm(env, tmPtr, utc, 0);
}

void localtimeJs(wasm_exec_env_t env,
                 uint32_t low,
                 uint32_t high,
                 uint32_t tmPtr)
{
    time_t at;
    struct tm local;
    if (!localTime((double)moduleTime(low, high), at, local))
    {
        return;
    }
    writeTm(env, tmPtr, local, utcOffsetOf(local, at));
}

// The module's tzset: seconds west of UTC, whether the zone observes
// daylight saving, and the two zone names into its 17 byte buffers.
void tzsetJs(wasm_exec_env_t env,
             uint32_t timezonePtr,
             uint32_t daylightPtr,
             uint32_t stdNamePtr,
             uint32_t dstNamePtr)
{
    WasmScriptingVM* vm = vmFromEnv(env);
#ifdef _WIN32
    _tzset();
    long west = 0;
    int observesDst = 0;
    _get_timezone(&west);
    _get_daylight(&observesDst);
    const char* names[2] = {_tzname[0], _tzname[1]};
#elif defined(RIVE_NX)
    // nnSdk's libc has no tz globals, so read the zone off January and July
    // of this year, standard time being whichever sits further west.
    long west = 0;
    int observesDst = 0;
    const char* names[2] = {"", ""};
    time_t at;
    struct tm probe[2];
    if (localTime((double)time(nullptr), at, probe[0]))
    {
        int year = probe[0].tm_year;
        int32_t offset[2];
        bool ok = true;
        for (int i = 0; i < 2 && ok; i++)
        {
            struct tm start{};
            start.tm_year = year;
            start.tm_mon = i * 6;
            start.tm_mday = 1;
            ok = localTime((double)timegm(&start), at, probe[i]);
            offset[i] = utcOffsetOf(probe[i], at);
        }
        if (ok)
        {
            int std = offset[1] < offset[0] ? 1 : 0;
            west = -offset[std];
            observesDst = offset[0] != offset[1];
            names[0] = probe[std].tm_zone;
            names[1] = probe[observesDst ? 1 - std : std].tm_zone;
        }
    }
#else
    tzset();
    long west = timezone;
    int observesDst = daylight;
    const char* names[2] = {tzname[0], tzname[1]};
#endif
    if (int32_t* out = (int32_t*)vm->resolveModulePtr(timezonePtr, 4))
    {
        *out = (int32_t)west;
    }
    if (int32_t* out = (int32_t*)vm->resolveModulePtr(daylightPtr, 4))
    {
        *out = observesDst;
    }
    uint32_t namePtrs[2] = {stdNamePtr, dstNamePtr};
    for (int i = 0; i < 2; i++)
    {
        if (char* out = (char*)vm->resolveModulePtr(namePtrs[i], 17))
        {
            strncpy(out, names[i] != nullptr ? names[i] : "", 16);
            out[16] = '\0';
        }
    }
}

uint32_t strftimeNative(wasm_exec_env_t env,
                        uint32_t out,
                        uint32_t capacity,
                        uint32_t format,
                        uint32_t tmPtr)
{
    WasmScriptingVM* vm = vmFromEnv(env);
    wasm_module_inst_t inst = wasm_runtime_get_module_inst(env);
    char* outPtr = (char*)vm->resolveModulePtr(out, capacity);
    const int32_t* fields = (const int32_t*)vm->resolveModulePtr(tmPtr, 44);
    if (outPtr == nullptr || fields == nullptr ||
        !wasm_runtime_validate_app_str_addr(inst, format))
    {
        return 0;
    }
    struct tm time = {};
    time.tm_sec = fields[0];
    time.tm_min = fields[1];
    time.tm_hour = fields[2];
    time.tm_mday = fields[3];
    time.tm_mon = fields[4];
    time.tm_year = fields[5];
    time.tm_wday = fields[6];
    time.tm_yday = fields[7];
    time.tm_isdst = fields[8];
#ifndef _WIN32
    time.tm_gmtoff = fields[9];
    // The zone name lives in module memory, so it must be translated before
    // the host's %Z reads it.
    uint32_t zone = (uint32_t)fields[10];
    if (zone != 0 && wasm_runtime_validate_app_str_addr(inst, zone))
    {
        time.tm_zone = (char*)wasm_runtime_addr_app_to_native(inst, zone);
    }
#endif
    return (uint32_t)strftime(
        outPtr,
        capacity,
        (const char*)wasm_runtime_addr_app_to_native(inst, format),
        &time);
}

// Older libc++ routes stream formatting through the locale variant.
uint32_t strftimeLNative(wasm_exec_env_t env,
                         uint32_t out,
                         uint32_t capacity,
                         uint32_t format,
                         uint32_t tmPtr,
                         uint32_t locale)
{
    return strftimeNative(env, out, capacity, format, tmPtr);
}

// A module assert would otherwise trap with no message.
void assertFail(wasm_exec_env_t env,
                uint32_t message,
                uint32_t file,
                uint32_t line,
                uint32_t function)
{
    WasmScriptingVM* vm = vmFromEnv(env);
    auto text = [vm](uint32_t ptr) {
        const char* str =
            ptr != 0 ? (const char*)vm->resolveModulePtr(ptr, 1) : nullptr;
        return str != nullptr ? str : "?";
    };
    fprintf(stderr,
            "module assertion failed: %s (%s:%u, %s)\n",
            text(message),
            text(file),
            line,
            text(function));
    wasm_runtime_set_exception(wasm_runtime_get_module_inst(env),
                               "module assertion failed");
}

// The blob's print goes through fd_write; intercept it so script output
// reaches the host's sink instead of the process stdout. The signature must
// match the libc-wasi builtin's "(i*i*)i": wamrc resolves WASI imports
// against the builtin table at compile time and inlines the app-to-native
// conversion for the '*' params, so an int-only registration receives
// native pointers under AOT while the interpreter honors the raw ints.
uint32_t fdWrite(wasm_exec_env_t env,
                 uint32_t fd,
                 uint32_t* iovs,
                 uint32_t iovcnt,
                 uint32_t* nwritten)
{
    WasmScriptingVM* vm = vmFromEnv(env);
    wasm_module_inst_t inst = wasm_runtime_get_module_inst(env);
    if (!wasm_runtime_validate_native_addr(inst, iovs, (uint64_t)iovcnt * 8) ||
        !wasm_runtime_validate_native_addr(inst, nwritten, 4))
    {
        return 28; // EINVAL
    }
    uint32_t total = 0;
    for (uint32_t i = 0; i < iovcnt; i++)
    {
        uint32_t* io = iovs + i * 2;
        uint32_t ptr = io[0], len = io[1];
        const char* data =
            len != 0 ? (const char*)vm->resolveModulePtr(ptr, len) : nullptr;
        if (data != nullptr)
        {
            // Module stderr carries script errors; keep it out of the print
            // sink so a capturing harness cannot swallow them.
            if (fd == 2)
            {
                fwrite(data, 1, len, stderr);
            }
            else
            {
                WasmScriptingVMNatives::print(vm, data, len);
            }
            total += len;
        }
    }
    *nwritten = total;
    return 0;
}

// vm_host externalizes a handful of rive C++ methods as env imports that
// the runtime path never reaches (the live GPU path goes through the
// rive_gpu_v1 natives, not these direct calls). The fast interpreter
// tolerates an unresolved import, trapping only on an actual call, but
// wamrc bakes a null call target into the AOT that faults the moment a
// static initializer takes its address. Registering inert stubs gives the
// AOT valid pointers. Signatures mirror vm_host's import types.
uint32_t deadEnvImport_i_i(wasm_exec_env_t, uint32_t) { return 0; }
void deadCoreBytesDeserialize(wasm_exec_env_t, uint32_t, uint32_t) {}
void deadCoreStringDeserialize(wasm_exec_env_t, uint32_t, uint32_t) {}
void deadBeginFrame(wasm_exec_env_t, uint32_t, uint32_t) {}
void deadFlush(wasm_exec_env_t, uint32_t, uint32_t) {}
void deadAddFileAsset(wasm_exec_env_t, uint32_t, uint32_t) {}
uint32_t deadEnvImport_ii_i(wasm_exec_env_t, uint32_t, uint32_t) { return 0; }
void deadEnvImport_iiii(wasm_exec_env_t, uint32_t, uint32_t, uint32_t, uint32_t)
{}

// __cxa_throw never returns; a no-op stub falls through to the module's
// `unreachable` and corrupts the shadow stack under AOT. Trap instead so
// the call unwinds like the interpreter's unlinked-import path, and name
// the thrown type from its Itanium type_info (wasm32: __type_name at +4).
// Registered with libc-builtin's "(**i)": wamrc knows that table and
// inlines the pointer conversion, so an int-only registration would
// receive native pointers under AOT.
void cxaThrowNative(wasm_exec_env_t env,
                    void* thrown,
                    void* tinfo,
                    uint32_t dest)
{
    (void)thrown;
    (void)dest;
    wasm_module_inst_t inst = wasm_runtime_get_module_inst(env);
    const char* typeName = "?";
    if (tinfo != nullptr && wasm_runtime_validate_native_addr(inst, tinfo, 8))
    {
        uint32_t nameApp = ((uint32_t*)tinfo)[1];
        if (wasm_runtime_validate_app_str_addr(inst, nameApp))
        {
            typeName =
                (const char*)wasm_runtime_addr_app_to_native(inst, nameApp);
        }
    }
    fprintf(stderr, "[wasm] C++ exception thrown: %s\n", typeName);
    wasm_runtime_set_exception(inst, "c++ exception");
}

// Math.random's seed; the fixed date env that pins script time pins it too.
double envSeed(wasm_exec_env_t)
{
    if (getenv("RIVE_WASM_FIXED_DATE") != nullptr)
    {
        return 1.0;
    }
    return (double)std::chrono::steady_clock::now().time_since_epoch().count();
}

NativeSymbol kEnvNatives[] = {
    {"invoke_vii", (void*)invokeViiNative, "(iii)", nullptr},
    {"seed", (void*)envSeed, "()F", nullptr},
    {"_ZN4rive12CoreUintType11deserializeERNS_12BinaryReaderE",
     (void*)deadEnvImport_i_i,
     "(i)i",
     nullptr},
    {"_ZN4rive13CoreBytesType11deserializeERNS_12BinaryReaderE",
     (void*)deadCoreBytesDeserialize,
     "(ii)",
     nullptr},
    {"_ZN4rive14CoreStringType11deserializeERNS_12BinaryReaderE",
     (void*)deadCoreStringDeserialize,
     "(ii)",
     nullptr},
    {"_ZN4rive3gpu13RenderContext24makeDeferredRenderCanvasEjj",
     (void*)deadEnvImport_iiii,
     "(iiii)",
     nullptr},
    {"_ZN4rive3gpu13RenderContext16makeRenderCanvasEjj",
     (void*)deadEnvImport_iiii,
     "(iiii)",
     nullptr},
    {"_ZN4rive3gpu13RenderContext10beginFrameERKNS1_15FrameDescriptorE",
     (void*)deadBeginFrame,
     "(ii)",
     nullptr},
    {"_ZN4rive12RiveRendererC1EPNS_3gpu13RenderContextE",
     (void*)deadEnvImport_ii_i,
     "(ii)i",
     nullptr},
    {"_ZN4rive3gpu13RenderContext5flushERKNS1_14FlushResourcesE",
     (void*)deadFlush,
     "(ii)",
     nullptr},
    {"_ZN4rive17BackboardImporter12addFileAssetENS_3rcpINS_9FileAssetEEE",
     (void*)deadAddFileAsset,
     "(ii)",
     nullptr},
    {"__cxa_throw", (void*)cxaThrowNative, "(**i)", nullptr},
    {"_emscripten_throw_longjmp", (void*)throwLongjmpNative, "()", nullptr},
    {"_abort_js", (void*)abortJs, "()", nullptr},
    {"_emscripten_memcpy_js", (void*)memcpyJs, "(iii)", nullptr},
    {"emscripten_get_now", (void*)getNow, "()F", nullptr},
    {"emscripten_date_now", (void*)dateNow, "()F", nullptr},
    {"emscripten_resize_heap", (void*)resizeHeap, "(i)i", nullptr},
    {"strftime", (void*)strftimeNative, "(iiii)i", nullptr},
    {"strftime_l", (void*)strftimeLNative, "(iiiii)i", nullptr},
    {"_tzset_js", (void*)tzsetJs, "(iiii)", nullptr},
    {"_localtime_js", (void*)localtimeJs, "(iii)", nullptr},
    {"_gmtime_js", (void*)gmtimeJs, "(iii)", nullptr},
    {"__assert_fail", (void*)assertFail, "(iiii)", nullptr},
};

// Module streams are not seekable. The blob imports the emscripten-legalized
// form, offset i64 split across two i32s.
uint32_t fdSeek(wasm_exec_env_t env,
                uint32_t fd,
                uint32_t offsetLow,
                uint32_t offsetHigh,
                uint32_t whence,
                uint32_t newOffsetPtr)
{
    return 70; // WASI ESPIPE
}

// The blob's libc asks for the environment at start and closes its streams
// at exit; there is no environment and the streams are the log.
uint32_t environSizesGet(wasm_exec_env_t env, uint32_t* count, uint32_t* size)
{
    wasm_module_inst_t inst = wasm_runtime_get_module_inst(env);
    if (!wasm_runtime_validate_native_addr(inst, count, 4) ||
        !wasm_runtime_validate_native_addr(inst, size, 4))
    {
        return 21; // WASI EFAULT
    }
    *count = 0;
    *size = 0;
    return 0;
}

uint32_t environGet(wasm_exec_env_t env, uint32_t* environ, char* buffer)
{
    return 0;
}

uint32_t fdClose(wasm_exec_env_t env, uint32_t fd) { return 0; }

NativeSymbol kWasiNatives[] = {
    {"fd_write", (void*)fdWrite, "(i*i*)i", nullptr},
    {"fd_seek", (void*)fdSeek, "(iiiii)i", nullptr},
    {"environ_sizes_get", (void*)environSizesGet, "(**)i", nullptr},
    {"environ_get", (void*)environGet, "(**)i", nullptr},
    {"fd_close", (void*)fdClose, "(i)i", nullptr},
};

// --- rive_path/paint/renderer_v1: handle-backed render objects --------------

// The module's ModuleRenderPath mirror; geometry arrives through update and
// rebuilds the real render path.
// Size then fill: copies when it fits, always answers the full count.
template <typename T>
uint32_t fillOut(Span<const T> words, T* out, uint32_t outCount)
{
    if (out != nullptr && words.size() <= outCount)
    {
        memcpy(out, words.data(), words.size() * sizeof(T));
    }
    return (uint32_t)words.size();
}

template <typename T>
uint32_t fillOut(const std::vector<T>& words, T* out, uint32_t outCount)
{
    return fillOut(Span<const T>(words.data(), words.size()), out, outCount);
}

// A RawPath's points as the flat float stream the modules read.
Span<const float> pathFloats(const RawPath& path)
{
    return Span<const float>((const float*)path.points().data(),
                             path.points().size() * 2);
}

Span<const uint8_t> pathVerbBytes(const RawPath& path)
{
    return Span<const uint8_t>((const uint8_t*)path.verbs().data(),
                               path.verbs().size());
}

// The geometry stays host side so paths composed from other paths never
// cross back; the render path rebuilds lazily after a host side append.
struct HostPath
{
    RawPath raw;
    FillRule fillRule = FillRule::clockwise;
    rcp<RenderPath> path;
    bool renderDirty = false;
};

HostPath* resolvePath(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return nullptr;
    }
    return static_cast<HostPath*>(
        vm->handles().resolve(handle, WasmScriptingVM::HandleTable::Tag::path));
}

RenderPath* hostRenderPath(WasmScriptingVM* vm, HostPath* hostPath)
{
    if (hostPath == nullptr)
    {
        return nullptr;
    }
    if (hostPath->renderDirty)
    {
        hostPath->path =
            vm->factory()->makeRenderPath(hostPath->raw, hostPath->fillRule);
        hostPath->renderDirty = false;
    }
    return hostPath->path.get();
}

struct HostPaint
{
    rcp<RenderPaint> paint;
};

uint32_t pathNewImpl(WasmScriptingVM* vm)
{
    if (vm == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::path,
                              new HostPath());
}

void pathUpdateImpl(WasmScriptingVM* vm,
                    uint32_t handle,
                    const uint8_t* verbs,
                    uint32_t verbCount,
                    const float* points,
                    uint32_t floatCount,
                    uint32_t fillRule)
{
    if (vm == nullptr)
    {
        return;
    }
    auto hostPath = static_cast<HostPath*>(
        vm->handles().resolve(handle, WasmScriptingVM::HandleTable::Tag::path));
    if (hostPath == nullptr)
    {
        return;
    }
    hostPath->raw =
        RawPath(Span<const PathVerb>((const PathVerb*)verbs, verbCount),
                Span<const Vec2D>((const Vec2D*)points, floatCount / 2));
    hostPath->fillRule = (FillRule)fillRule;
    hostPath->path =
        vm->factory()->makeRenderPath(hostPath->raw, hostPath->fillRule);
    hostPath->renderDirty = false;
}

void pathAddImpl(WasmScriptingVM* vm,
                 uint32_t handle,
                 uint32_t otherHandle,
                 float xx,
                 float xy,
                 float yx,
                 float yy,
                 float tx,
                 float ty)
{
    auto hostPath = resolvePath(vm, handle);
    auto other = resolvePath(vm, otherHandle);
    if (hostPath == nullptr || other == nullptr)
    {
        return;
    }
    Mat2D transform(xx, xy, yx, yy, tx, ty);
    hostPath->raw.addPath(other->raw, &transform);
    hostPath->renderDirty = true;
}

uint32_t pathVerbsImpl(WasmScriptingVM* vm,
                       uint32_t handle,
                       uint8_t* out,
                       uint32_t outCount)
{
    auto hostPath = resolvePath(vm, handle);
    if (hostPath == nullptr)
    {
        return 0;
    }
    return fillOut(pathVerbBytes(hostPath->raw), out, outCount);
}

uint32_t pathPointsImpl(WasmScriptingVM* vm,
                        uint32_t handle,
                        float* out,
                        uint32_t outCount)
{
    auto hostPath = resolvePath(vm, handle);
    if (hostPath == nullptr)
    {
        return 0;
    }
    return fillOut(pathFloats(hostPath->raw), out, outCount);
}

void pathEffectResultImpl(WasmScriptingVM* vm,
                          const uint8_t* verbs,
                          uint32_t verbCount,
                          const float* points,
                          uint32_t floatCount)
{
    if (vm == nullptr || vm->pathEffectOut() == nullptr)
    {
        return;
    }
    RawPath rawPath(Span<const PathVerb>((const PathVerb*)verbs, verbCount),
                    Span<const Vec2D>((const Vec2D*)points, floatCount / 2));
    vm->pathEffectOut()->addPath(rawPath);
}

// One handle kind for the three measure shapes: a whole path measure, a
// contour iterator (measures nothing itself) and a contour it produced.
// The geometry is copied so the module's path can change underneath.
struct HostMeasure
{
    std::unique_ptr<RawPath> source;
    std::unique_ptr<PathMeasure> path;
    std::unique_ptr<ContourMeasureIter> iter;
    rcp<ContourMeasure> contour;
    RawPath segment;

    float length() const
    {
        return path ? path->length() : contour ? contour->length() : 0.0f;
    }

    bool isClosed() const
    {
        return path ? path->isClosed() : contour ? contour->isClosed() : false;
    }

    ContourMeasure::PosTan posTan(float distance) const
    {
        if (path)
        {
            auto at = path->atDistance(distance);
            return {at.pos, at.tan};
        }
        if (contour)
        {
            return contour->getPosTan(distance);
        }
        return {};
    }

    void getSegment(float start, float end, RawPath* dst, bool startWithMove)
    {
        if (path)
        {
            path->getSegment(start, end, dst, startWithMove);
        }
        else if (contour)
        {
            contour->getSegment(start, end, dst, startWithMove);
        }
    }
};

HostMeasure* resolveMeasure(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return nullptr;
    }
    return static_cast<HostMeasure*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::measure));
}

std::unique_ptr<RawPath> copyGeometry(const uint8_t* verbs,
                                      uint32_t verbCount,
                                      const float* points,
                                      uint32_t floatCount)
{
    return std::make_unique<RawPath>(
        Span<const PathVerb>((const PathVerb*)verbs, verbCount),
        Span<const Vec2D>((const Vec2D*)points, floatCount / 2));
}

uint32_t measurePathNewImpl(WasmScriptingVM* vm,
                            const uint8_t* verbs,
                            uint32_t verbCount,
                            const float* points,
                            uint32_t floatCount)
{
    if (vm == nullptr)
    {
        return 0;
    }
    auto measure = new HostMeasure();
    measure->source = copyGeometry(verbs, verbCount, points, floatCount);
    measure->path = std::make_unique<PathMeasure>(measure->source.get());
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::measure,
                              measure);
}

uint32_t measureContoursNewImpl(WasmScriptingVM* vm,
                                const uint8_t* verbs,
                                uint32_t verbCount,
                                const float* points,
                                uint32_t floatCount)
{
    if (vm == nullptr)
    {
        return 0;
    }
    auto measure = new HostMeasure();
    measure->source = copyGeometry(verbs, verbCount, points, floatCount);
    measure->iter = std::make_unique<ContourMeasureIter>(measure->source.get());
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::measure,
                              measure);
}

uint32_t measureContourNextImpl(WasmScriptingVM* vm, uint32_t handle)
{
    HostMeasure* iter = resolveMeasure(vm, handle);
    if (iter == nullptr || !iter->iter)
    {
        return 0;
    }
    rcp<ContourMeasure> next = iter->iter->next();
    if (next == nullptr)
    {
        return 0;
    }
    auto measure = new HostMeasure();
    measure->contour = std::move(next);
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::measure,
                              measure);
}

float measureLengthImpl(WasmScriptingVM* vm, uint32_t handle)
{
    HostMeasure* measure = resolveMeasure(vm, handle);
    return measure != nullptr ? measure->length() : 0.0f;
}

uint32_t measureIsClosedImpl(WasmScriptingVM* vm, uint32_t handle)
{
    HostMeasure* measure = resolveMeasure(vm, handle);
    return measure != nullptr && measure->isClosed() ? 1 : 0;
}

void measurePosTanImpl(WasmScriptingVM* vm,
                       uint32_t handle,
                       float distance,
                       float* out,
                       uint32_t outCount)
{
    HostMeasure* measure = resolveMeasure(vm, handle);
    if (measure == nullptr || out == nullptr || outCount < 4)
    {
        return;
    }
    auto posTan = measure->posTan(distance);
    out[0] = posTan.pos.x;
    out[1] = posTan.pos.y;
    out[2] = posTan.tan.x;
    out[3] = posTan.tan.y;
}

void measureWarpImpl(WasmScriptingVM* vm,
                     uint32_t handle,
                     float x,
                     float y,
                     float* out,
                     uint32_t outCount)
{
    HostMeasure* measure = resolveMeasure(vm, handle);
    if (measure == nullptr || out == nullptr || outCount < 2)
    {
        return;
    }
    auto posTan = measure->posTan(x);
    out[0] = posTan.pos.x - posTan.tan.y * y;
    out[1] = posTan.pos.y + posTan.tan.x * y;
}

uint32_t measureExtractImpl(WasmScriptingVM* vm,
                            uint32_t handle,
                            float startDistance,
                            float endDistance,
                            uint32_t startWithMove)
{
    HostMeasure* measure = resolveMeasure(vm, handle);
    if (measure == nullptr)
    {
        return 0;
    }
    measure->segment.rewind();
    measure->getSegment(startDistance,
                        endDistance,
                        &measure->segment,
                        startWithMove != 0);
    return (uint32_t)measure->segment.verbs().size();
}

uint32_t measureExtractReadImpl(WasmScriptingVM* vm,
                                uint32_t handle,
                                uint8_t* verbs,
                                uint32_t verbCount,
                                float* points,
                                uint32_t floatCount)
{
    HostMeasure* measure = resolveMeasure(vm, handle);
    if (measure == nullptr)
    {
        return 0;
    }
    const RawPath& segment = measure->segment;
    uint32_t floats = (uint32_t)segment.points().size() * 2;
    if (verbCount < segment.verbs().size() || floatCount < floats)
    {
        return 0;
    }
    memcpy(verbs, segment.verbs().data(), segment.verbs().size());
    memcpy(points, segment.points().data(), floats * sizeof(float));
    return floats;
}

void measureReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return;
    }
    delete resolveMeasure(vm, handle);
    vm->handles().release(handle, WasmScriptingVM::HandleTable::Tag::measure);
}

void pathReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return;
    }
    delete static_cast<HostPath*>(
        vm->handles().resolve(handle, WasmScriptingVM::HandleTable::Tag::path));
    vm->handles().release(handle, WasmScriptingVM::HandleTable::Tag::path);
}

uint32_t paintNewImpl(WasmScriptingVM* vm)
{
    if (vm == nullptr)
    {
        return 0;
    }
    auto hostPaint = new HostPaint{vm->factory()->makeRenderPaint()};
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::paint,
                              hostPaint);
}

void paintReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return;
    }
    delete static_cast<HostPaint*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::paint));
    vm->handles().release(handle, WasmScriptingVM::HandleTable::Tag::paint);
}

RenderPaint* resolvePaint(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return nullptr;
    }
    auto hostPaint = static_cast<HostPaint*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::paint));
    return hostPaint != nullptr ? hostPaint->paint.get() : nullptr;
}

void paintStyleImpl(WasmScriptingVM* vm, uint32_t handle, uint32_t value)
{
    if (auto paint = resolvePaint(vm, handle))
    {
        paint->style((RenderPaintStyle)value);
    }
}
void paintColorImpl(WasmScriptingVM* vm, uint32_t handle, uint32_t value)
{
    if (auto paint = resolvePaint(vm, handle))
    {
        paint->color(value);
    }
}
void paintThicknessImpl(WasmScriptingVM* vm, uint32_t handle, float value)
{
    if (auto paint = resolvePaint(vm, handle))
    {
        paint->thickness(value);
    }
}
void paintJoinImpl(WasmScriptingVM* vm, uint32_t handle, uint32_t value)
{
    if (auto paint = resolvePaint(vm, handle))
    {
        paint->join((StrokeJoin)value);
    }
}
void paintCapImpl(WasmScriptingVM* vm, uint32_t handle, uint32_t value)
{
    if (auto paint = resolvePaint(vm, handle))
    {
        paint->cap((StrokeCap)value);
    }
}
void paintBlendModeImpl(WasmScriptingVM* vm, uint32_t handle, uint32_t value)
{
    if (auto paint = resolvePaint(vm, handle))
    {
        paint->blendMode((BlendMode)value);
    }
}
void paintFeatherImpl(WasmScriptingVM* vm, uint32_t handle, float value)
{
    if (auto paint = resolvePaint(vm, handle))
    {
        paint->feather(value);
    }
}

struct HostShader
{
    rcp<RenderShader> shader;
};

uint32_t shaderLinearImpl(WasmScriptingVM* vm,
                          float sx,
                          float sy,
                          float ex,
                          float ey,
                          uint32_t colorsPtr,
                          uint32_t stopsPtr,
                          uint32_t count)
{
    if (vm == nullptr)
    {
        return 0;
    }
    auto colors = (const ColorInt*)vm->resolveModulePtr(colorsPtr, count * 4);
    auto stops = (const float*)vm->resolveModulePtr(stopsPtr, count * 4);
    if (colors == nullptr || stops == nullptr)
    {
        return 0;
    }
    auto shader =
        vm->factory()->makeLinearGradient(sx, sy, ex, ey, colors, stops, count);
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::shader,
                              new HostShader{std::move(shader)});
}

uint32_t shaderRadialImpl(WasmScriptingVM* vm,
                          float cx,
                          float cy,
                          float radius,
                          uint32_t colorsPtr,
                          uint32_t stopsPtr,
                          uint32_t count)
{
    if (vm == nullptr)
    {
        return 0;
    }
    auto colors = (const ColorInt*)vm->resolveModulePtr(colorsPtr, count * 4);
    auto stops = (const float*)vm->resolveModulePtr(stopsPtr, count * 4);
    if (colors == nullptr || stops == nullptr)
    {
        return 0;
    }
    auto shader =
        vm->factory()->makeRadialGradient(cx, cy, radius, colors, stops, count);
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::shader,
                              new HostShader{std::move(shader)});
}

void shaderReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return;
    }
    delete static_cast<HostShader*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::shader));
    vm->handles().release(handle, WasmScriptingVM::HandleTable::Tag::shader);
}

void paintShaderImpl(WasmScriptingVM* vm,
                     uint32_t paintHandle,
                     uint32_t shaderHandle)
{
    auto paint = resolvePaint(vm, paintHandle);
    if (vm == nullptr || paint == nullptr)
    {
        return;
    }
    auto hostShader = static_cast<HostShader*>(
        vm->handles().resolve(shaderHandle,
                              WasmScriptingVM::HandleTable::Tag::shader));
    paint->shader(hostShader != nullptr ? hostShader->shader : nullptr);
}

// The object's file asset of type T by name; accept skips matches that
// carry nothing, the way the Luau lookups keep scanning past them.
template <typename T>
T* findFileAsset(WasmScriptingVM* vm,
                 uint32_t objectHandle,
                 const char* name,
                 uint32_t length,
                 bool (*accept)(T*) = nullptr)
{
    if (vm == nullptr)
    {
        return nullptr;
    }
    auto object = static_cast<ScriptedObject*>(
        vm->handles().resolve(objectHandle,
                              WasmScriptingVM::HandleTable::Tag::object));
    if (object == nullptr || object->scriptAsset() == nullptr ||
        object->scriptAsset()->file() == nullptr)
    {
        return nullptr;
    }
    std::string key(name, length);
    for (const auto& asset : object->scriptAsset()->file()->assets())
    {
        if (!asset->is<T>() || asset->name() != key)
        {
            continue;
        }
        T* match = asset->template as<T>();
        if (accept == nullptr || accept(match))
        {
            return match;
        }
    }
    return nullptr;
}

struct HostImage
{
    rcp<RenderImage> image;
#ifdef RIVE_CANVAS
    // A canvas image outlives its canvas only through this.
    rcp<gpu::RenderCanvas> sourceCanvas;
#endif
};

HostImage* resolveImage(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return nullptr;
    }
    return static_cast<HostImage*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::image));
}

uint32_t imageFromAssetImpl(WasmScriptingVM* vm,
                            uint32_t objectHandle,
                            const char* name,
                            uint32_t length)
{
    auto asset = findFileAsset<ImageAsset>(vm, objectHandle, name, length);
    if (asset == nullptr || asset->renderImage() == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::image,
                              new HostImage{ref_rcp(asset->renderImage())});
}

uint32_t imageWidthImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto host = resolveImage(vm, handle);
    return host != nullptr ? (uint32_t)host->image->width() : 0;
}

uint32_t imageHeightImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto host = resolveImage(vm, handle);
    return host != nullptr ? (uint32_t)host->image->height() : 0;
}

void imageReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return;
    }
    delete static_cast<HostImage*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::image));
    vm->handles().release(handle, WasmScriptingVM::HandleTable::Tag::image);
}

uint32_t imageDecodeImpl(WasmScriptingVM* vm,
                         const uint8_t* bytes,
                         uint32_t byteCount,
                         uint32_t token)
{
    if (vm == nullptr || bytes == nullptr || byteCount == 0)
    {
        return 0;
    }
    return vm->startImageDecode(bytes, byteCount, token) ? 1 : 0;
}

void imageDecodeCancelImpl(WasmScriptingVM* vm, uint32_t token)
{
    if (vm != nullptr)
    {
        vm->cancelImageDecode(token);
    }
}

#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
struct HostGpuCanvas
{
    rcp<gpu::RenderCanvas> canvas;
    rcp<ore::TextureView> colorView;
};

struct HostGpuPass
{
    std::unique_ptr<ore::RenderPass> pass;
    // setBindGroup calls made before the first setPipeline.
    ore::DeferredBindGroups deferredBindGroups;
    bool pipelineSet = false;
};

struct HostGpuBuffer
{
    rcp<ore::Buffer> buffer;
};

struct HostGpuTexture
{
    rcp<ore::Texture> texture;
};

struct HostGpuSampler
{
    rcp<ore::Sampler> sampler;
};

struct HostGpuTextureView
{
    rcp<ore::TextureView> view;
};

struct HostGpuShaderModule
{
    rcp<ore::ShaderModule> shaderModule;
};

struct HostGpuBindGroupLayout
{
    rcp<ore::BindGroupLayout> layout;
};

struct HostGpuBindGroup
{
    rcp<ore::BindGroup> bindGroup;
};

struct HostGpuPipeline
{
    rcp<ore::Pipeline> pipeline;
};

ore::Context* gpuOreContext(WasmScriptingVM* vm)
{
    if (vm == nullptr || vm->factory() == nullptr)
    {
        return nullptr;
    }
    if (auto* recording = vm->factory()->ore())
    {
        return recording;
    }
    Factory* renderContext = vm->factory()->renderContext();
    return renderContext != nullptr ? renderContext->ore() : nullptr;
}

// The Lua bindings raise lastError as a script error; the module lane traps
// with the same text so the author reads the cause instead of a 0 handle.
uint32_t gpuRejected(WasmScriptingVM* vm,
                     ore::Context* context,
                     const char* what)
{
    std::string message = std::string(what) + ": " +
                          (context->lastError().empty() ? "creation failed"
                                                        : context->lastError());
    vm->raiseModuleError(message.c_str());
    return 0;
}

uint32_t gpuCanvasNewImpl(WasmScriptingVM* vm, uint32_t width, uint32_t height)
{
    if (vm == nullptr || vm->factory() == nullptr)
    {
        return 0;
    }
    // Like the Luau lane, a zero size gives a canvas without backing that
    // resize allocates once the layout size is known.
    if (width == 0 || height == 0)
    {
        return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::gpuCanvas,
                                  new HostGpuCanvas{});
    }
    auto* renderContext =
        static_cast<gpu::RenderContext*>(vm->factory()->renderContext());
    ore::Context* oreContext = gpuOreContext(vm);
    if (renderContext == nullptr || oreContext == nullptr)
    {
        return 0;
    }
    auto* deferredHost = vm->factory()->deferredCanvasHost();
    auto canvas = deferredHost != nullptr
                      ? deferredHost->makeContentCanvas(width, height)
                      : renderContext->makeRenderCanvas(width, height);
    if (canvas == nullptr)
    {
        return 0;
    }
    auto colorView = oreContext->wrapCanvasTexture(canvas.get());
    if (colorView == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(
        WasmScriptingVM::HandleTable::Tag::gpuCanvas,
        new HostGpuCanvas{std::move(canvas), std::move(colorView)});
}

void gpuCanvasReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return;
    }
    delete static_cast<HostGpuCanvas*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::gpuCanvas));
    vm->handles().release(handle, WasmScriptingVM::HandleTable::Tag::gpuCanvas);
}

uint32_t gpuCanvasColorViewImpl(WasmScriptingVM* vm,
                                uint32_t canvasHandle,
                                uint32_t* props,
                                uint32_t propCount)
{
    ore::Context* oreContext = gpuOreContext(vm);
    if (vm == nullptr || oreContext == nullptr || propCount < 4)
    {
        return 0;
    }
    auto host = static_cast<HostGpuCanvas*>(
        vm->handles().resolve(canvasHandle,
                              WasmScriptingVM::HandleTable::Tag::gpuCanvas));
    if (host == nullptr)
    {
        return 0;
    }
    props[2] = (uint32_t)oreContext->canvasTargetFormat();
    props[3] = 1;
    if (host->canvas == nullptr)
    {
        props[0] = 0;
        props[1] = 0;
        return 0;
    }
    props[0] = host->canvas->width();
    props[1] = host->canvas->height();
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::gpuTextureView,
                              new HostGpuTextureView{host->colorView});
}

uint32_t gpuCanvasImageImpl(WasmScriptingVM* vm, uint32_t canvasHandle)
{
    if (vm == nullptr)
    {
        return 0;
    }
    auto host = static_cast<HostGpuCanvas*>(
        vm->handles().resolve(canvasHandle,
                              WasmScriptingVM::HandleTable::Tag::gpuCanvas));
    if (host == nullptr || host->canvas == nullptr ||
        host->canvas->renderImage() == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::image,
                              new HostImage{ref_rcp(static_cast<RenderImage*>(
                                                host->canvas->renderImage())),
                                            host->canvas});
}

uint32_t gpuCanvasResizeImpl(WasmScriptingVM* vm,
                             uint32_t canvasHandle,
                             uint32_t width,
                             uint32_t height,
                             uint32_t* props,
                             uint32_t propCount)
{
    ore::Context* oreContext = gpuOreContext(vm);
    if (vm == nullptr || vm->factory() == nullptr || oreContext == nullptr ||
        width == 0 || height == 0 || propCount < 4)
    {
        return 0;
    }
    auto host = static_cast<HostGpuCanvas*>(
        vm->handles().resolve(canvasHandle,
                              WasmScriptingVM::HandleTable::Tag::gpuCanvas));
    if (host == nullptr)
    {
        return 0;
    }
    auto* renderContext =
        static_cast<gpu::RenderContext*>(vm->factory()->renderContext());
    if (renderContext == nullptr)
    {
        return 0;
    }
    auto* deferredHost = vm->factory()->deferredCanvasHost();
    auto canvas = deferredHost != nullptr
                      ? deferredHost->makeContentCanvas(width, height)
                      : renderContext->makeRenderCanvas(width, height);
    if (canvas == nullptr)
    {
        return 0;
    }
    auto colorView = oreContext->wrapCanvasTexture(canvas.get());
    if (colorView == nullptr)
    {
        return 0;
    }
    host->canvas = std::move(canvas);
    host->colorView = std::move(colorView);
    props[0] = host->canvas->width();
    props[1] = host->canvas->height();
    props[2] = (uint32_t)oreContext->canvasTargetFormat();
    props[3] = 1;
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::gpuTextureView,
                              new HostGpuTextureView{host->colorView});
}

uint32_t gpuTargetViewImpl(WasmScriptingVM* vm,
                           uint32_t current,
                           uint32_t* props,
                           uint32_t propCount)
{
    ore::Context* oreContext = gpuOreContext(vm);
    // Props stay unwritten where no host could ever expose a target.
    if (vm == nullptr || oreContext == nullptr || !oreContext->isRecording() ||
        propCount < 4)
    {
        return 0;
    }
    rcp<ore::TextureView> view = oreContext->targetView();
    if (view == nullptr)
    {
        props[0] = 0;
        props[1] = 0;
        props[2] = (uint32_t)ore::TextureFormat::rgba8unorm;
        props[3] = 1;
        return 0;
    }
    props[0] = view->width();
    props[1] = view->height();
    props[2] = (uint32_t)view->texture()->format();
    props[3] = view->texture()->sampleCount();
    auto host = static_cast<HostGpuTextureView*>(vm->handles().resolve(
        current,
        WasmScriptingVM::HandleTable::Tag::gpuTextureView));
    if (host != nullptr && host->view == view)
    {
        return current;
    }
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::gpuTextureView,
                              new HostGpuTextureView{std::move(view)});
}

uint32_t gpuFeaturesImpl(WasmScriptingVM* vm, uint32_t* out, uint32_t outCount)
{
    ore::Context* oreContext = gpuOreContext(vm);
    if (vm == nullptr || oreContext == nullptr || outCount < 19)
    {
        return 0;
    }
    // With ReplayCaps a recording context answers from shipped data; an
    // unknown snapshot must raise module-side instead of guessing.
    if (!oreContext->featuresKnown())
    {
        return ~0u;
    }
    const ore::Features& f = oreContext->features();
    out[0] = f.bc ? 1 : 0;
    out[1] = f.etc2 ? 1 : 0;
    out[2] = f.astc ? 1 : 0;
    out[3] = (uint32_t)f.maxTextureSize2D;
    out[4] = (uint32_t)f.maxTextureSizeCube;
    out[5] = (uint32_t)f.maxTextureSize3D;
    out[6] = f.anisotropicFiltering ? 1 : 0;
    out[7] = f.texture3D ? 1 : 0;
    out[8] = f.textureArrays ? 1 : 0;
    out[9] = f.colorBufferFloat ? 1 : 0;
    out[10] = f.colorBufferHalfFloat ? 1 : 0;
    out[11] = f.perTargetBlend ? 1 : 0;
    out[12] = f.perTargetWriteMask ? 1 : 0;
    out[13] = f.drawBaseInstance ? 1 : 0;
    out[14] = f.depthBiasClamp ? 1 : 0;
    out[15] = (uint32_t)f.maxColorAttachments;
    out[16] = (uint32_t)f.maxUniformBufferSize;
    out[17] = (uint32_t)f.maxSamplers;
    out[18] = (uint32_t)f.maxSamples;
    // Guests built before the alignment field ask for 19.
    if (outCount < 20)
    {
        return 19;
    }
    out[19] = f.minUniformBufferOffsetAlignment;
    return 20;
}

uint32_t gpuPassBeginImpl(WasmScriptingVM* vm,
                          const rive_gpu_pass_desc_v1* podDesc,
                          uint32_t descByteCount,
                          const uint8_t* blob,
                          uint32_t blobCount)
{
    ore::Context* oreContext = gpuOreContext(vm);
    // The label is optional, so a descriptor from before it still passes.
    if (vm == nullptr || oreContext == nullptr ||
        descByteCount < offsetof(rive_gpu_pass_desc_v1, labelSize) ||
        podDesc->colorCount > 4)
    {
        return 0;
    }
    const uint32_t labelSize =
        descByteCount >= sizeof(*podDesc) ? podDesc->labelSize : 0;
    const uint64_t colorBytes = uint64_t(podDesc->colorCount) *
                                sizeof(rive_gpu_pass_color_attachment_v1);
    if (colorBytes + labelSize != blobCount)
    {
        return 0;
    }
    auto colors =
        reinterpret_cast<const rive_gpu_pass_color_attachment_v1*>(blob);
    std::string label;
    if (labelSize != 0)
    {
        label.assign(reinterpret_cast<const char*>(blob + colorBytes),
                     labelSize);
    }
    auto resolveView = [vm](uint32_t handle) -> ore::TextureView* {
        auto host = static_cast<HostGpuTextureView*>(vm->handles().resolve(
            handle,
            WasmScriptingVM::HandleTable::Tag::gpuTextureView));
        return host != nullptr ? host->view.get() : nullptr;
    };
    ore::RenderPassDesc desc;
    desc.colorCount = podDesc->colorCount;
    for (uint32_t i = 0; i < podDesc->colorCount; i++)
    {
        auto& out = desc.colorAttachments[i];
        out.view = resolveView(colors[i].view);
        if (out.view == nullptr)
        {
            return 0;
        }
        out.resolveTarget = colors[i].resolveTarget != 0
                                ? resolveView(colors[i].resolveTarget)
                                : nullptr;
        out.loadOp = (ore::LoadOp)colors[i].loadOp;
        out.storeOp = (ore::StoreOp)colors[i].storeOp;
        out.clearColor = {colors[i].clearR,
                          colors[i].clearG,
                          colors[i].clearB,
                          colors[i].clearA};
    }
    if (podDesc->depthView != 0)
    {
        desc.depthStencil.view = resolveView(podDesc->depthView);
        if (desc.depthStencil.view == nullptr)
        {
            return 0;
        }
    }
    desc.depthStencil.depthLoadOp = (ore::LoadOp)podDesc->depthLoadOp;
    desc.depthStencil.depthStoreOp = (ore::StoreOp)podDesc->depthStoreOp;
    desc.depthStencil.depthClearValue = podDesc->depthClearValue;
    desc.depthStencil.stencilLoadOp = (ore::LoadOp)podDesc->stencilLoadOp;
    desc.depthStencil.stencilStoreOp = (ore::StoreOp)podDesc->stencilStoreOp;
    desc.depthStencil.stencilClearValue = podDesc->stencilClearValue;
    desc.label = label.empty() ? nullptr : label.c_str();
    oreContext->clearLastError();
    auto pass = ore::cmd::beginRecordedRenderPass(*oreContext, desc);
    if (pass == nullptr)
    {
        // A pass refused without an error drops, like one with no target.
        return oreContext->lastError().empty()
                   ? 0
                   : gpuRejected(vm, oreContext, "beginRenderPass");
    }
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::gpuPass,
                              new HostGpuPass{std::move(pass)});
}

uint32_t gpuImageViewImpl(WasmScriptingVM* vm,
                          uint32_t imageHandle,
                          uint32_t width,
                          uint32_t height)
{
    ore::Context* oreContext = gpuOreContext(vm);
    if (vm == nullptr || oreContext == nullptr)
    {
        return 0;
    }
    auto host = static_cast<HostImage*>(
        vm->handles().resolve(imageHandle,
                              WasmScriptingVM::HandleTable::Tag::image));
    if (host == nullptr)
    {
        return 0;
    }
    // Same dispatch as Image:view() in the Luau binding.
    rcp<ore::TextureView> view;
    if (!oreContext->isRecording())
    {
        if (host->sourceCanvas != nullptr)
        {
            view = oreContext->wrapCanvasSampleView(host->sourceCanvas.get());
        }
        else if (auto* riveImage =
                     lite_rtti_cast<RiveRenderImage*>(host->image.get());
                 riveImage != nullptr && riveImage->getTexture() != nullptr)
        {
            view = oreContext->wrapRiveTexture(riveImage->getTexture(),
                                               host->image->width(),
                                               host->image->height());
        }
    }
    else if (host->sourceCanvas != nullptr)
    {
        view = oreContext->recordWrapCanvasImage(host->sourceCanvas.get(),
                                                 host->image->width(),
                                                 host->image->height());
    }
    else if (auto* deferredImage =
                 lite_rtti_cast<cmd::DeferredRenderImage*>(host->image.get()))
    {
        view = oreContext->recordWrapImageView(deferredImage->id(),
                                               host->image->width(),
                                               host->image->height());
    }
    else
    {
        // A foreign image the recorder was handed rather than created.
        view = oreContext->recordWrapForeignImageView(host->image.get(),
                                                      host->image->width(),
                                                      host->image->height());
    }
    if (view == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::gpuTextureView,
                              new HostGpuTextureView{std::move(view)});
}

HostGpuPass* resolveHostPass(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return nullptr;
    }
    return static_cast<HostGpuPass*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::gpuPass));
}

ore::RenderPass* resolvePass(WasmScriptingVM* vm, uint32_t handle)
{
    auto host = resolveHostPass(vm, handle);
    return host != nullptr ? host->pass.get() : nullptr;
}

// The draw guards the Luau binding raises, with the same text so both lanes
// read the same cause. Returns false after raising.
bool gpuPassDrawAllowed(WasmScriptingVM* vm,
                        HostGpuPass* host,
                        const char* what,
                        int32_t baseVertex,
                        uint32_t firstInstance)
{
    if (!host->pipelineSet)
    {
        vm->raiseModuleError(ore::kGuardSetPipelineBeforeDraw);
        return false;
    }
    ore::Context* oreContext = gpuOreContext(vm);
    bool featuresKnown = oreContext != nullptr && oreContext->featuresKnown();
    if (featuresKnown && !oreContext->features().drawBaseInstance)
    {
        char message[160];
        if (baseVertex != 0)
        {
            snprintf(message,
                     sizeof(message),
                     ore::kGuardBaseVertexFormat,
                     what,
                     baseVertex);
            vm->raiseModuleError(message);
            return false;
        }
        if (firstInstance > 0)
        {
            snprintf(message,
                     sizeof(message),
                     ore::kGuardFirstInstanceFormat,
                     what,
                     firstInstance);
            vm->raiseModuleError(message);
            return false;
        }
    }
    return true;
}

void gpuPassSetPipelineImpl(WasmScriptingVM* vm,
                            uint32_t passHandle,
                            uint32_t pipelineHandle)
{
    auto* host = resolveHostPass(vm, passHandle);
    auto pipeline = static_cast<HostGpuPipeline*>(
        vm->handles().resolve(pipelineHandle,
                              WasmScriptingVM::HandleTable::Tag::gpuPipeline));
    if (host == nullptr || pipeline == nullptr)
    {
        return;
    }
    // An attachment compat failure no-ops the backend and later draws crash
    // in the driver, so it surfaces here the way the Luau binding raises it.
    ore::Context* oreContext = gpuOreContext(vm);
    if (oreContext != nullptr)
    {
        oreContext->clearLastError();
    }
    host->pass->setPipeline(pipeline->pipeline.get());
    host->deferredBindGroups.flush(host->pass.get());
    if (oreContext != nullptr && !oreContext->lastError().empty())
    {
        gpuRejected(vm, oreContext, "setPipeline");
        return;
    }
    host->pipelineSet = true;
}

void gpuPassSetVertexBufferImpl(WasmScriptingVM* vm,
                                uint32_t passHandle,
                                uint32_t slot,
                                uint32_t bufferHandle,
                                uint32_t offset)
{
    auto* pass = resolvePass(vm, passHandle);
    auto buffer = static_cast<HostGpuBuffer*>(
        vm->handles().resolve(bufferHandle,
                              WasmScriptingVM::HandleTable::Tag::gpuBuffer));
    if (pass == nullptr || buffer == nullptr)
    {
        return;
    }
    if (slot >= ore::kMaxVertexBufferSlots)
    {
        char message[96];
        snprintf(message,
                 sizeof(message),
                 ore::kGuardVertexSlotRangeFormat,
                 ore::kMaxVertexBufferSlots - 1,
                 slot);
        vm->raiseModuleError(message);
        return;
    }
    pass->setVertexBuffer(slot, buffer->buffer.get(), offset);
}

void gpuPassSetIndexBufferImpl(WasmScriptingVM* vm,
                               uint32_t passHandle,
                               uint32_t bufferHandle,
                               uint32_t indexFormat,
                               uint32_t offset)
{
    auto* pass = resolvePass(vm, passHandle);
    auto buffer = static_cast<HostGpuBuffer*>(
        vm->handles().resolve(bufferHandle,
                              WasmScriptingVM::HandleTable::Tag::gpuBuffer));
    if (pass == nullptr || buffer == nullptr)
    {
        return;
    }
    pass->setIndexBuffer(buffer->buffer.get(),
                         (ore::IndexFormat)indexFormat,
                         offset);
}

void gpuPassSetBindGroupImpl(WasmScriptingVM* vm,
                             uint32_t passHandle,
                             uint32_t groupIndex,
                             uint32_t bindGroupHandle,
                             const uint32_t* dynamicOffsets,
                             uint32_t dynamicOffsetByteCount)
{
    auto* host = resolveHostPass(vm, passHandle);
    auto bindGroup = static_cast<HostGpuBindGroup*>(
        vm->handles().resolve(bindGroupHandle,
                              WasmScriptingVM::HandleTable::Tag::gpuBindGroup));
    if (host == nullptr || host->pass == nullptr || bindGroup == nullptr ||
        dynamicOffsetByteCount % sizeof(uint32_t) != 0)
    {
        return;
    }
    uint32_t count = dynamicOffsetByteCount / (uint32_t)sizeof(uint32_t);
    // The checks the Luau binding makes, so a misuse raises on the call
    // instead of reaching a backend assert or a silent misbind.
    ore::Context* oreContext = gpuOreContext(vm);
    if (std::string err; !ore::validateSetBindGroup(
            groupIndex,
            bindGroup->bindGroup.get(),
            dynamicOffsets,
            count,
            ore::scriptDynamicOffsetAlignment(oreContext),
            &err))
    {
        vm->raiseModuleError(err.c_str());
        return;
    }
    if (!host->pipelineSet)
    {
        host->deferredBindGroups.defer(groupIndex,
                                       bindGroup->bindGroup.get(),
                                       dynamicOffsets,
                                       count);
        return;
    }
    if (oreContext != nullptr)
    {
        oreContext->clearLastError();
    }
    host->pass->setBindGroup(groupIndex,
                             bindGroup->bindGroup.get(),
                             count != 0 ? dynamicOffsets : nullptr,
                             count);
    if (oreContext != nullptr && !oreContext->lastError().empty())
    {
        gpuRejected(vm, oreContext, "setBindGroup");
    }
}

void gpuPassSetViewportImpl(WasmScriptingVM* vm,
                            uint32_t passHandle,
                            float x,
                            float y,
                            float width,
                            float height,
                            float minDepth,
                            float maxDepth)
{
    if (auto* pass = resolvePass(vm, passHandle))
    {
        pass->setViewport(x, y, width, height, minDepth, maxDepth);
    }
}

void gpuPassSetScissorImpl(WasmScriptingVM* vm,
                           uint32_t passHandle,
                           uint32_t x,
                           uint32_t y,
                           uint32_t width,
                           uint32_t height)
{
    if (auto* pass = resolvePass(vm, passHandle))
    {
        pass->setScissorRect(x, y, width, height);
    }
}

void gpuPassSetStencilReferenceImpl(WasmScriptingVM* vm,
                                    uint32_t passHandle,
                                    uint32_t ref)
{
    if (auto* pass = resolvePass(vm, passHandle))
    {
        pass->setStencilReference(ref);
    }
}

void gpuPassSetBlendColorImpl(WasmScriptingVM* vm,
                              uint32_t passHandle,
                              float r,
                              float g,
                              float b,
                              float a)
{
    if (auto* pass = resolvePass(vm, passHandle))
    {
        pass->setBlendColor(r, g, b, a);
    }
}

void gpuPassDrawImpl(WasmScriptingVM* vm,
                     uint32_t passHandle,
                     uint32_t vertexCount,
                     uint32_t instanceCount,
                     uint32_t firstVertex,
                     uint32_t firstInstance)
{
    auto* host = resolveHostPass(vm, passHandle);
    if (host == nullptr ||
        !gpuPassDrawAllowed(vm, host, "draw", 0, firstInstance))
    {
        return;
    }
    host->pass->draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void gpuPassDrawIndexedImpl(WasmScriptingVM* vm,
                            uint32_t passHandle,
                            uint32_t indexCount,
                            uint32_t instanceCount,
                            uint32_t firstIndex,
                            int32_t baseVertex,
                            uint32_t firstInstance)
{
    auto* host = resolveHostPass(vm, passHandle);
    if (host == nullptr ||
        !gpuPassDrawAllowed(vm, host, "drawIndexed", baseVertex, firstInstance))
    {
        return;
    }
    host->pass->drawIndexed(indexCount,
                            instanceCount,
                            firstIndex,
                            baseVertex,
                            firstInstance);
}

void gpuPassFinishImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return;
    }
    auto host = static_cast<HostGpuPass*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::gpuPass));
    if (host == nullptr || host->pass == nullptr)
    {
        return;
    }
    if (!host->pass->isFinished())
    {
        host->pass->finish();
    }
}

void gpuPassReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return;
    }
    delete static_cast<HostGpuPass*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::gpuPass));
    vm->handles().release(handle, WasmScriptingVM::HandleTable::Tag::gpuPass);
}

uint32_t gpuBufferNewImpl(WasmScriptingVM* vm,
                          uint32_t usage,
                          uint32_t sizeInBytes,
                          uint32_t immutable,
                          const uint8_t* data,
                          uint32_t dataCount)
{
    ore::Context* oreContext = gpuOreContext(vm);
    if (oreContext == nullptr || sizeInBytes == 0 ||
        (dataCount != 0 && dataCount != sizeInBytes))
    {
        return 0;
    }
    ore::BufferDesc desc;
    desc.usage = (ore::BufferUsage)usage;
    desc.size = sizeInBytes;
    desc.immutable = immutable != 0;
    desc.data = dataCount != 0 ? data : nullptr;
    oreContext->clearLastError();
    auto buffer = oreContext->makeBuffer(desc);
    if (buffer == nullptr)
    {
        return gpuRejected(vm, oreContext, "GPUBuffer");
    }
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::gpuBuffer,
                              new HostGpuBuffer{std::move(buffer)});
}

void gpuBufferUpdateImpl(WasmScriptingVM* vm,
                         uint32_t handle,
                         uint32_t dstOffset,
                         const uint8_t* data,
                         uint32_t dataCount)
{
    if (vm == nullptr)
    {
        return;
    }
    auto host = static_cast<HostGpuBuffer*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::gpuBuffer));
    if (host == nullptr || dataCount == 0 ||
        uint64_t(dstOffset) + dataCount > host->buffer->size())
    {
        return;
    }
    host->buffer->update(data, dataCount, dstOffset);
}

void gpuBufferReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return;
    }
    delete static_cast<HostGpuBuffer*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::gpuBuffer));
    vm->handles().release(handle, WasmScriptingVM::HandleTable::Tag::gpuBuffer);
}

uint32_t gpuTextureNewImpl(WasmScriptingVM* vm,
                           const rive_gpu_texture_desc_v1* podDesc,
                           uint32_t descByteCount)
{
    ore::Context* oreContext = gpuOreContext(vm);
    if (oreContext == nullptr || descByteCount < sizeof(*podDesc))
    {
        return 0;
    }
    ore::TextureDesc desc;
    desc.width = podDesc->width;
    desc.height = podDesc->height;
    desc.depthOrArrayLayers = podDesc->depthOrArrayLayers;
    desc.format = (ore::TextureFormat)podDesc->format;
    desc.type = (ore::TextureType)podDesc->textureType;
    desc.renderTarget = podDesc->renderTarget != 0;
    desc.numMipmaps = podDesc->numMipmaps;
    desc.sampleCount = podDesc->sampleCount;
    oreContext->clearLastError();
    auto texture = oreContext->makeTexture(desc);
    if (texture == nullptr)
    {
        return gpuRejected(vm, oreContext, "GPUTexture");
    }
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::gpuTexture,
                              new HostGpuTexture{std::move(texture)});
}

void gpuTextureUploadImpl(WasmScriptingVM* vm,
                          uint32_t handle,
                          const rive_gpu_texture_upload_v1* region,
                          uint32_t regionByteCount,
                          const uint8_t* data,
                          uint32_t dataCount)
{
    if (vm == nullptr || regionByteCount < sizeof(*region) || dataCount == 0)
    {
        return;
    }
    auto host = static_cast<HostGpuTexture*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::gpuTexture));
    if (host == nullptr)
    {
        return;
    }
    ore::TextureDataDesc upload;
    upload.data = data;
    upload.dataSize = dataCount;
    upload.bytesPerRow = region->bytesPerRow;
    upload.rowsPerImage = region->rowsPerImage;
    upload.mipLevel = region->mipLevel;
    upload.layer = region->layer;
    upload.x = region->x;
    upload.y = region->y;
    upload.z = region->z;
    upload.width = region->width;
    upload.height = region->height;
    upload.depth = region->depth;
    std::string error;
    if (!host->texture->upload(upload, &error))
    {
        vm->raiseModuleError(error.c_str());
    }
}

void gpuTextureReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return;
    }
    delete static_cast<HostGpuTexture*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::gpuTexture));
    vm->handles().release(handle,
                          WasmScriptingVM::HandleTable::Tag::gpuTexture);
}

uint32_t gpuSamplerNewImpl(WasmScriptingVM* vm,
                           const rive_gpu_sampler_desc_v1* podDesc,
                           uint32_t descByteCount)
{
    ore::Context* oreContext = gpuOreContext(vm);
    if (oreContext == nullptr || descByteCount < sizeof(*podDesc))
    {
        return 0;
    }
    ore::SamplerDesc desc;
    desc.minFilter = (ore::Filter)podDesc->minFilter;
    desc.magFilter = (ore::Filter)podDesc->magFilter;
    desc.mipmapFilter = (ore::Filter)podDesc->mipmapFilter;
    desc.wrapU = (ore::WrapMode)podDesc->wrapU;
    desc.wrapV = (ore::WrapMode)podDesc->wrapV;
    desc.wrapW = (ore::WrapMode)podDesc->wrapW;
    // The guest sends ~0 for "no comparison sampler".
    desc.compare = podDesc->compare == 0xFFFFFFFFu
                       ? ore::CompareFunction::none
                       : (ore::CompareFunction)podDesc->compare;
    desc.minLod = podDesc->minLod;
    desc.maxLod = podDesc->maxLod;
    desc.maxAnisotropy = podDesc->maxAnisotropy;
    oreContext->clearLastError();
    auto sampler = oreContext->makeSampler(desc);
    if (sampler == nullptr)
    {
        return gpuRejected(vm, oreContext, "GPUSampler");
    }
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::gpuSampler,
                              new HostGpuSampler{std::move(sampler)});
}

void gpuSamplerReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return;
    }
    delete static_cast<HostGpuSampler*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::gpuSampler));
    vm->handles().release(handle,
                          WasmScriptingVM::HandleTable::Tag::gpuSampler);
}

uint32_t gpuTextureViewNewImpl(WasmScriptingVM* vm,
                               uint32_t textureHandle,
                               const rive_gpu_texture_view_desc_v1* podDesc,
                               uint32_t descByteCount)
{
    ore::Context* oreContext = gpuOreContext(vm);
    if (oreContext == nullptr || descByteCount < sizeof(*podDesc))
    {
        return 0;
    }
    auto texture = static_cast<HostGpuTexture*>(
        vm->handles().resolve(textureHandle,
                              WasmScriptingVM::HandleTable::Tag::gpuTexture));
    if (texture == nullptr)
    {
        return 0;
    }
    ore::TextureViewDesc desc;
    desc.texture = texture->texture.get();
    desc.dimension = (ore::TextureViewDimension)podDesc->dimension;
    desc.aspect = (ore::TextureAspect)podDesc->aspect;
    desc.baseMipLevel = podDesc->baseMipLevel;
    desc.mipCount = podDesc->mipCount;
    desc.baseLayer = podDesc->baseLayer;
    desc.layerCount = podDesc->layerCount;
    oreContext->clearLastError();
    auto view = oreContext->makeTextureView(desc);
    if (view == nullptr)
    {
        return gpuRejected(vm, oreContext, "GPUTextureView");
    }
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::gpuTextureView,
                              new HostGpuTextureView{std::move(view)});
}

void gpuTextureViewReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return;
    }
    delete static_cast<HostGpuTextureView*>(vm->handles().resolve(
        handle,
        WasmScriptingVM::HandleTable::Tag::gpuTextureView));
    vm->handles().release(handle,
                          WasmScriptingVM::HandleTable::Tag::gpuTextureView);
}

uint32_t gpuShaderTargetImpl(WasmScriptingVM* vm)
{
    ore::Context* oreContext = gpuOreContext(vm);
    return oreContext != nullptr ? (uint32_t)oreContext->shaderTarget()
                                 : (uint32_t)ore::ShaderTarget::wgsl;
}

uint32_t gpuShaderAssetBytesImpl(WasmScriptingVM* vm,
                                 uint32_t objectHandle,
                                 const char* name,
                                 uint32_t nameLength,
                                 uint8_t* out,
                                 uint32_t outCount)
{
    if (vm == nullptr)
    {
        return 0;
    }
#ifdef WITH_RIVE_TOOLS
    // Editor path first, matching the Luau lane: RSTBs compiled during
    // requestWasmVM shadow whatever a file asset carries.
    {
        const std::vector<uint8_t>* rstb =
            vm->findShaderRstb(std::string(name, nameLength));
        if (rstb != nullptr)
        {
            if (rstb->size() <= outCount)
            {
                memcpy(out, rstb->data(), rstb->size());
            }
            return (uint32_t)rstb->size();
        }
    }
#endif
    auto asset = findFileAsset<ShaderAsset>(vm, objectHandle, name, nameLength);
    if (asset == nullptr)
    {
        return 0;
    }
    auto rstb = asset->rstb();
    if (rstb.size() <= outCount)
    {
        memcpy(out, rstb.data(), rstb.size());
    }
    return (uint32_t)rstb.size();
}

uint32_t gpuShaderAssetIdImpl(WasmScriptingVM* vm,
                              uint32_t objectHandle,
                              const char* name,
                              uint32_t nameLength)
{
    auto asset = findFileAsset<ShaderAsset>(vm, objectHandle, name, nameLength);
    return asset != nullptr ? asset->assetId() : 0;
}

uint32_t gpuShaderModuleNewImpl(WasmScriptingVM* vm,
                                const rive_gpu_shader_module_desc_v1* podDesc,
                                uint32_t descByteCount,
                                const uint8_t* blob,
                                uint32_t blobCount)
{
    ore::Context* oreContext = gpuOreContext(vm);
    if (oreContext == nullptr || descByteCount < sizeof(*podDesc))
    {
        return 0;
    }
    uint64_t total = uint64_t(podDesc->codeSize) + podDesc->hlslSourceSize +
                     podDesc->hlslEntryPointSize + podDesc->bindingMapSize +
                     podDesc->glFixupSize;
    if (total > blobCount)
    {
        return 0;
    }
    const uint8_t* cursor = blob;
    auto slice = [&cursor](uint32_t size) {
        const uint8_t* begin = cursor;
        cursor += size;
        return size != 0 ? begin : nullptr;
    };
    ore::ShaderModuleDesc desc;
    desc.code = slice(podDesc->codeSize);
    desc.codeSize = podDesc->codeSize;
    desc.language = (ore::ShaderLanguage)podDesc->language;
    desc.stage = (ore::ShaderStage)podDesc->stage;
    // hlslSource and entry point are null-terminated strings host side.
    std::string hlslSource;
    std::string hlslEntryPoint;
    if (const uint8_t* bytes = slice(podDesc->hlslSourceSize))
    {
        hlslSource.assign((const char*)bytes, podDesc->hlslSourceSize);
        desc.hlslSource = hlslSource.c_str();
        desc.hlslSourceSize = podDesc->hlslSourceSize;
    }
    if (const uint8_t* bytes = slice(podDesc->hlslEntryPointSize))
    {
        hlslEntryPoint.assign((const char*)bytes, podDesc->hlslEntryPointSize);
        desc.hlslEntryPoint = hlslEntryPoint.c_str();
    }
    desc.bindingMapBytes = slice(podDesc->bindingMapSize);
    desc.bindingMapSize = podDesc->bindingMapSize;
    desc.glFixupBytes = slice(podDesc->glFixupSize);
    desc.glFixupSize = podDesc->glFixupSize;
    desc.shaderAssetId = podDesc->shaderAssetId;
    // Texture-sampler pairs ride the desc so deferred replay rebuilds them;
    // resolved host side from the asset like the Luau lane, not the wire.
    std::vector<uint8_t> pairBytes;
    if (podDesc->shaderAssetId != 0 && vm->file() != nullptr)
    {
        for (const auto& asset : vm->file()->assets())
        {
            if (asset->is<ShaderAsset>() &&
                asset->assetId() == podDesc->shaderAssetId)
            {
                auto pairs = asset->as<ShaderAsset>()->textureSamplerPairs();
                pairBytes.reserve(pairs.size() * 4);
                for (size_t i = 0; i < pairs.size(); i++)
                {
                    pairBytes.push_back(pairs[i].texGroup);
                    pairBytes.push_back(pairs[i].texBinding);
                    pairBytes.push_back(pairs[i].sampGroup);
                    pairBytes.push_back(pairs[i].sampBinding);
                }
                break;
            }
        }
    }
    desc.texSamplerPairBytes = pairBytes.empty() ? nullptr : pairBytes.data();
    desc.texSamplerPairSize = (uint32_t)pairBytes.size();
    oreContext->clearLastError();
    auto shaderModule = oreContext->makeShaderModule(desc);
    if (shaderModule == nullptr)
    {
        return gpuRejected(vm, oreContext, "Shader");
    }
    return vm->handles().mint(
        WasmScriptingVM::HandleTable::Tag::gpuShaderModule,
        new HostGpuShaderModule{std::move(shaderModule)});
}

void gpuShaderModuleReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return;
    }
    delete static_cast<HostGpuShaderModule*>(vm->handles().resolve(
        handle,
        WasmScriptingVM::HandleTable::Tag::gpuShaderModule));
    vm->handles().release(handle,
                          WasmScriptingVM::HandleTable::Tag::gpuShaderModule);
}

uint32_t gpuBindGroupLayoutNewImpl(
    WasmScriptingVM* vm,
    uint32_t groupIndex,
    const rive_gpu_bind_group_layout_entry_v1* entries,
    uint32_t entryByteCount)
{
    ore::Context* oreContext = gpuOreContext(vm);
    if (oreContext == nullptr || entryByteCount % sizeof(*entries) != 0)
    {
        return 0;
    }
    uint32_t count = entryByteCount / (uint32_t)sizeof(*entries);
    std::vector<ore::BindGroupLayoutEntry> resolved(count);
    for (uint32_t i = 0; i < count; i++)
    {
        auto& out = resolved[i];
        const auto& in = entries[i];
        out.binding = in.binding;
        out.kind = (ore::BindingKind)in.kind;
        out.visibility.mask = (uint8_t)in.visibility;
        out.hasDynamicOffset = in.hasDynamicOffset != 0;
        out.textureViewDim = (ore::TextureViewDimension)in.textureViewDim;
        out.textureSampleType =
            (ore::BindGroupLayoutEntry::SampleType)in.textureSampleType;
        out.textureMultisampled = in.textureMultisampled != 0;
        out.minBindingSize = in.minBindingSize;
        out.nativeSlotVS = in.nativeSlotVS;
        out.nativeSlotFS = in.nativeSlotFS;
        out.nativeSlotCS = in.nativeSlotCS;
        out.samplerNonFiltering = in.samplerNonFiltering != 0;
    }
    ore::BindGroupLayoutDesc desc;
    desc.groupIndex = groupIndex;
    desc.entries = resolved.data();
    desc.entryCount = count;
    oreContext->clearLastError();
    auto layout = oreContext->makeBindGroupLayout(desc);
    if (layout == nullptr)
    {
        return gpuRejected(vm, oreContext, "GPUBindGroupLayout");
    }
    return vm->handles().mint(
        WasmScriptingVM::HandleTable::Tag::gpuBindGroupLayout,
        new HostGpuBindGroupLayout{std::move(layout)});
}

void gpuBindGroupLayoutReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return;
    }
    delete static_cast<HostGpuBindGroupLayout*>(vm->handles().resolve(
        handle,
        WasmScriptingVM::HandleTable::Tag::gpuBindGroupLayout));
    vm->handles().release(
        handle,
        WasmScriptingVM::HandleTable::Tag::gpuBindGroupLayout);
}

uint32_t gpuBindGroupLayoutFromShaderImpl(WasmScriptingVM* vm,
                                          uint32_t shaderModule,
                                          uint32_t groupIndex,
                                          const uint32_t* dynamicUBOs,
                                          uint32_t dynamicUBOCount)
{
    ore::Context* oreContext = gpuOreContext(vm);
    if (oreContext == nullptr)
    {
        return 0;
    }
    auto host = static_cast<HostGpuShaderModule*>(vm->handles().resolve(
        shaderModule,
        WasmScriptingVM::HandleTable::Tag::gpuShaderModule));
    if (host == nullptr)
    {
        return 0;
    }
    auto layout = ore::makeBindGroupLayoutFromShader(*oreContext,
                                                     host->shaderModule.get(),
                                                     groupIndex,
                                                     dynamicUBOs,
                                                     dynamicUBOCount);
    if (layout == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(
        WasmScriptingVM::HandleTable::Tag::gpuBindGroupLayout,
        new HostGpuBindGroupLayout{std::move(layout)});
}

uint32_t gpuBindGroupLayoutFromShadersImpl(WasmScriptingVM* vm,
                                           uint32_t vertexModule,
                                           uint32_t fragmentModule,
                                           uint32_t groupIndex,
                                           const uint32_t* dynamicUBOs,
                                           uint32_t dynamicUBOCount)
{
    ore::Context* oreContext = gpuOreContext(vm);
    if (oreContext == nullptr)
    {
        return 0;
    }
    auto resolveModule = [vm](uint32_t handle) -> ore::ShaderModule* {
        auto host = static_cast<HostGpuShaderModule*>(vm->handles().resolve(
            handle,
            WasmScriptingVM::HandleTable::Tag::gpuShaderModule));
        return host != nullptr ? host->shaderModule.get() : nullptr;
    };
    ore::ShaderModule* vertex = resolveModule(vertexModule);
    ore::ShaderModule* fragment = resolveModule(fragmentModule);
    if (vertex == nullptr || (fragmentModule != 0 && fragment == nullptr))
    {
        return 0;
    }
    auto layout = ore::makeBindGroupLayoutFromBindingMap(
        *oreContext,
        ore::bindingMapForStages(vertex, fragment),
        groupIndex,
        dynamicUBOs,
        dynamicUBOCount,
        vertex,
        fragment);
    if (layout == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(
        WasmScriptingVM::HandleTable::Tag::gpuBindGroupLayout,
        new HostGpuBindGroupLayout{std::move(layout)});
}

uint32_t gpuBindGroupNewImpl(WasmScriptingVM* vm,
                             uint32_t layoutHandle,
                             const rive_gpu_bind_group_ubo_v1* ubos,
                             uint32_t uboByteCount,
                             const rive_gpu_bind_group_texture_v1* textures,
                             uint32_t textureByteCount,
                             const rive_gpu_bind_group_sampler_v1* samplers,
                             uint32_t samplerByteCount)
{
    ore::Context* oreContext = gpuOreContext(vm);
    if (oreContext == nullptr || uboByteCount % sizeof(*ubos) != 0 ||
        textureByteCount % sizeof(*textures) != 0 ||
        samplerByteCount % sizeof(*samplers) != 0)
    {
        return 0;
    }
    auto layout = static_cast<HostGpuBindGroupLayout*>(vm->handles().resolve(
        layoutHandle,
        WasmScriptingVM::HandleTable::Tag::gpuBindGroupLayout));
    if (layout == nullptr)
    {
        return 0;
    }
    ore::BindGroupDesc desc;
    desc.layout = layout->layout.get();

    uint32_t uboCount = uboByteCount / (uint32_t)sizeof(*ubos);
    std::vector<ore::BindGroupDesc::UBOEntry> uboEntries(uboCount);
    for (uint32_t i = 0; i < uboCount; i++)
    {
        auto buffer = static_cast<HostGpuBuffer*>(vm->handles().resolve(
            ubos[i].buffer,
            WasmScriptingVM::HandleTable::Tag::gpuBuffer));
        if (buffer == nullptr)
        {
            return 0;
        }
        uboEntries[i].slot = ubos[i].slot;
        uboEntries[i].buffer = buffer->buffer.get();
        uboEntries[i].offset = ubos[i].offset;
        uboEntries[i].size = ubos[i].size;
    }
    desc.ubos = uboEntries.data();
    desc.uboCount = uboCount;

    uint32_t texCount = textureByteCount / (uint32_t)sizeof(*textures);
    std::vector<ore::BindGroupDesc::TexEntry> texEntries(texCount);
    for (uint32_t i = 0; i < texCount; i++)
    {
        auto view = static_cast<HostGpuTextureView*>(vm->handles().resolve(
            textures[i].view,
            WasmScriptingVM::HandleTable::Tag::gpuTextureView));
        if (view == nullptr)
        {
            return 0;
        }
        texEntries[i].slot = textures[i].slot;
        texEntries[i].view = view->view.get();
    }
    desc.textures = texEntries.data();
    desc.textureCount = texCount;

    uint32_t sampCount = samplerByteCount / (uint32_t)sizeof(*samplers);
    std::vector<ore::BindGroupDesc::SampEntry> sampEntries(sampCount);
    for (uint32_t i = 0; i < sampCount; i++)
    {
        auto sampler = static_cast<HostGpuSampler*>(vm->handles().resolve(
            samplers[i].sampler,
            WasmScriptingVM::HandleTable::Tag::gpuSampler));
        if (sampler == nullptr)
        {
            return 0;
        }
        sampEntries[i].slot = samplers[i].slot;
        sampEntries[i].sampler = sampler->sampler.get();
    }
    desc.samplers = sampEntries.data();
    desc.samplerCount = sampCount;

    oreContext->clearLastError();
    auto bindGroup = oreContext->makeBindGroup(desc);
    if (bindGroup == nullptr)
    {
        return gpuRejected(vm, oreContext, "GPUBindGroup");
    }
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::gpuBindGroup,
                              new HostGpuBindGroup{std::move(bindGroup)});
}

void gpuBindGroupReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return;
    }
    delete static_cast<HostGpuBindGroup*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::gpuBindGroup));
    vm->handles().release(handle,
                          WasmScriptingVM::HandleTable::Tag::gpuBindGroup);
}

uint32_t gpuPipelineNewImpl(WasmScriptingVM* vm,
                            const rive_gpu_pipeline_desc_v1* podDesc,
                            uint32_t descByteCount,
                            const uint8_t* blob,
                            uint32_t blobCount)
{
    ore::Context* oreContext = gpuOreContext(vm);
    if (oreContext == nullptr || descByteCount < sizeof(*podDesc))
    {
        return 0;
    }
    uint64_t total =
        uint64_t(podDesc->vertexEntrySize) + podDesc->fragmentEntrySize +
        uint64_t(podDesc->colorCount) * sizeof(rive_gpu_color_target_v1) +
        uint64_t(podDesc->vertexBufferCount) *
            sizeof(rive_gpu_vertex_buffer_layout_v1) +
        uint64_t(podDesc->attributeCount) *
            sizeof(rive_gpu_vertex_attribute_v1) +
        uint64_t(podDesc->bindGroupLayoutCount) * sizeof(uint32_t);
    if (total > blobCount || podDesc->colorCount > 4 ||
        podDesc->bindGroupLayoutCount > ore::kMaxBindGroups)
    {
        return 0;
    }
    const uint8_t* cursor = blob;
    auto slice = [&cursor](uint32_t size) {
        const uint8_t* begin = cursor;
        cursor += size;
        return size != 0 ? begin : nullptr;
    };

    ore::PipelineDesc desc;
    auto vertexModule = static_cast<HostGpuShaderModule*>(vm->handles().resolve(
        podDesc->vertexModule,
        WasmScriptingVM::HandleTable::Tag::gpuShaderModule));
    auto fragmentModule =
        static_cast<HostGpuShaderModule*>(vm->handles().resolve(
            podDesc->fragmentModule,
            WasmScriptingVM::HandleTable::Tag::gpuShaderModule));
    desc.vertexModule =
        vertexModule != nullptr ? vertexModule->shaderModule.get() : nullptr;
    desc.fragmentModule = fragmentModule != nullptr
                              ? fragmentModule->shaderModule.get()
                              : nullptr;

    std::string vertexEntry;
    std::string fragmentEntry;
    if (const uint8_t* bytes = slice(podDesc->vertexEntrySize))
    {
        vertexEntry.assign((const char*)bytes, podDesc->vertexEntrySize);
        desc.vertexEntryPoint = vertexEntry.c_str();
    }
    if (const uint8_t* bytes = slice(podDesc->fragmentEntrySize))
    {
        fragmentEntry.assign((const char*)bytes, podDesc->fragmentEntrySize);
        desc.fragmentEntryPoint = fragmentEntry.c_str();
    }

    auto colorTargets = (const rive_gpu_color_target_v1*)slice(
        podDesc->colorCount * (uint32_t)sizeof(rive_gpu_color_target_v1));
    desc.colorCount = podDesc->colorCount;
    for (uint32_t i = 0; i < podDesc->colorCount; i++)
    {
        auto& out = desc.colorTargets[i];
        out.format = (ore::TextureFormat)colorTargets[i].format;
        out.blendEnabled = colorTargets[i].blendEnabled != 0;
        out.blend.srcColor = (ore::BlendFactor)colorTargets[i].srcColor;
        out.blend.dstColor = (ore::BlendFactor)colorTargets[i].dstColor;
        out.blend.colorOp = (ore::BlendOp)colorTargets[i].colorOp;
        out.blend.srcAlpha = (ore::BlendFactor)colorTargets[i].srcAlpha;
        out.blend.dstAlpha = (ore::BlendFactor)colorTargets[i].dstAlpha;
        out.blend.alphaOp = (ore::BlendOp)colorTargets[i].alphaOp;
        out.writeMask = (ore::ColorWriteMask)colorTargets[i].writeMask;
    }

    auto bufferPods = (const rive_gpu_vertex_buffer_layout_v1*)slice(
        podDesc->vertexBufferCount *
        (uint32_t)sizeof(rive_gpu_vertex_buffer_layout_v1));
    auto attributePods = (const rive_gpu_vertex_attribute_v1*)slice(
        podDesc->attributeCount *
        (uint32_t)sizeof(rive_gpu_vertex_attribute_v1));
    std::vector<ore::VertexAttribute> attributes(podDesc->attributeCount);
    for (uint32_t i = 0; i < podDesc->attributeCount; i++)
    {
        attributes[i].format = (ore::VertexFormat)attributePods[i].format;
        attributes[i].offset = attributePods[i].offset;
        attributes[i].shaderSlot = attributePods[i].shaderSlot;
    }
    std::vector<ore::VertexBufferLayout> buffers(podDesc->vertexBufferCount);
    uint32_t attributeCursor = 0;
    for (uint32_t i = 0; i < podDesc->vertexBufferCount; i++)
    {
        uint32_t count = bufferPods[i].attributeCount;
        if (uint64_t(attributeCursor) + count > podDesc->attributeCount)
        {
            return 0;
        }
        buffers[i].stride = bufferPods[i].stride;
        buffers[i].stepMode = (ore::VertexStepMode)bufferPods[i].stepMode;
        buffers[i].attributes = attributes.data() + attributeCursor;
        buffers[i].attributeCount = count;
        attributeCursor += count;
    }
    desc.vertexBuffers = buffers.data();
    desc.vertexBufferCount = podDesc->vertexBufferCount;

    auto layoutHandles = (const uint32_t*)slice(podDesc->bindGroupLayoutCount *
                                                (uint32_t)sizeof(uint32_t));
    ore::BindGroupLayout* layouts[ore::kMaxBindGroups] = {};
    for (uint32_t i = 0; i < podDesc->bindGroupLayoutCount; i++)
    {
        if (layoutHandles[i] == 0)
        {
            continue;
        }
        auto layout =
            static_cast<HostGpuBindGroupLayout*>(vm->handles().resolve(
                layoutHandles[i],
                WasmScriptingVM::HandleTable::Tag::gpuBindGroupLayout));
        if (layout == nullptr)
        {
            return 0;
        }
        layouts[i] = layout->layout.get();
    }
    desc.bindGroupLayouts = layouts;
    desc.bindGroupLayoutCount = podDesc->bindGroupLayoutCount;

    desc.topology = (ore::PrimitiveTopology)podDesc->topology;
    desc.indexFormat = (ore::IndexFormat)podDesc->indexFormat;
    desc.cullMode = (ore::CullMode)podDesc->cullMode;
    desc.winding = (ore::FaceWinding)podDesc->winding;
    desc.depthStencil.format = (ore::TextureFormat)podDesc->depthFormat;
    desc.depthStencil.depthCompare =
        (ore::CompareFunction)podDesc->depthCompare;
    desc.depthStencil.depthWriteEnabled = podDesc->depthWriteEnabled != 0;
    desc.depthStencil.depthBias = (int32_t)podDesc->depthBias;
    desc.depthStencil.depthBiasSlopeScale = podDesc->depthBiasSlopeScale;
    desc.depthStencil.depthBiasClamp = podDesc->depthBiasClamp;
    desc.stencilFront.compare =
        (ore::CompareFunction)podDesc->stencilFrontCompare;
    desc.stencilFront.failOp = (ore::StencilOp)podDesc->stencilFrontFailOp;
    desc.stencilFront.depthFailOp =
        (ore::StencilOp)podDesc->stencilFrontDepthFailOp;
    desc.stencilFront.passOp = (ore::StencilOp)podDesc->stencilFrontPassOp;
    desc.stencilBack.compare =
        (ore::CompareFunction)podDesc->stencilBackCompare;
    desc.stencilBack.failOp = (ore::StencilOp)podDesc->stencilBackFailOp;
    desc.stencilBack.depthFailOp =
        (ore::StencilOp)podDesc->stencilBackDepthFailOp;
    desc.stencilBack.passOp = (ore::StencilOp)podDesc->stencilBackPassOp;
    desc.stencilReadMask = (uint8_t)podDesc->stencilReadMask;
    desc.stencilWriteMask = (uint8_t)podDesc->stencilWriteMask;
    desc.sampleCount = podDesc->sampleCount;

    oreContext->clearLastError();
    auto pipeline = oreContext->makePipeline(desc);
    if (pipeline == nullptr)
    {
        return gpuRejected(vm, oreContext, "GPUPipeline");
    }
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::gpuPipeline,
                              new HostGpuPipeline{std::move(pipeline)});
}

void gpuPipelineReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return;
    }
    delete static_cast<HostGpuPipeline*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::gpuPipeline));
    vm->handles().release(handle,
                          WasmScriptingVM::HandleTable::Tag::gpuPipeline);
}
#else
// Builds without the canvas renderer keep the namespace linkable; scripts
// see the same nil/no-op surface the Luau backend presents there.
uint32_t gpuCanvasNewImpl(WasmScriptingVM*, uint32_t, uint32_t) { return 0; }
void gpuCanvasReleaseImpl(WasmScriptingVM*, uint32_t) {}
uint32_t gpuCanvasColorViewImpl(WasmScriptingVM*, uint32_t, uint32_t*, uint32_t)
{
    return 0;
}
uint32_t gpuCanvasImageImpl(WasmScriptingVM*, uint32_t) { return 0; }
uint32_t gpuCanvasResizeImpl(WasmScriptingVM*,
                             uint32_t,
                             uint32_t,
                             uint32_t,
                             uint32_t*,
                             uint32_t)
{
    return 0;
}
uint32_t gpuTargetViewImpl(WasmScriptingVM*, uint32_t, uint32_t*, uint32_t)
{
    return 0;
}
uint32_t gpuFeaturesImpl(WasmScriptingVM*, uint32_t*, uint32_t) { return 0; }
uint32_t gpuPassBeginImpl(WasmScriptingVM*,
                          const rive_gpu_pass_desc_v1*,
                          uint32_t,
                          const uint8_t*,
                          uint32_t)
{
    return 0;
}
void gpuPassSetPipelineImpl(WasmScriptingVM*, uint32_t, uint32_t) {}
void gpuPassSetVertexBufferImpl(WasmScriptingVM*,
                                uint32_t,
                                uint32_t,
                                uint32_t,
                                uint32_t)
{}
void gpuPassSetIndexBufferImpl(WasmScriptingVM*,
                               uint32_t,
                               uint32_t,
                               uint32_t,
                               uint32_t)
{}
void gpuPassSetBindGroupImpl(WasmScriptingVM*,
                             uint32_t,
                             uint32_t,
                             uint32_t,
                             const uint32_t*,
                             uint32_t)
{}
void gpuPassSetViewportImpl(WasmScriptingVM*,
                            uint32_t,
                            float,
                            float,
                            float,
                            float,
                            float,
                            float)
{}
void gpuPassSetScissorImpl(WasmScriptingVM*,
                           uint32_t,
                           uint32_t,
                           uint32_t,
                           uint32_t,
                           uint32_t)
{}
void gpuPassSetStencilReferenceImpl(WasmScriptingVM*, uint32_t, uint32_t) {}
void gpuPassSetBlendColorImpl(WasmScriptingVM*,
                              uint32_t,
                              float,
                              float,
                              float,
                              float)
{}
void gpuPassDrawImpl(WasmScriptingVM*,
                     uint32_t,
                     uint32_t,
                     uint32_t,
                     uint32_t,
                     uint32_t)
{}
void gpuPassDrawIndexedImpl(WasmScriptingVM*,
                            uint32_t,
                            uint32_t,
                            uint32_t,
                            uint32_t,
                            int32_t,
                            uint32_t)
{}
void gpuPassFinishImpl(WasmScriptingVM*, uint32_t) {}
void gpuPassReleaseImpl(WasmScriptingVM*, uint32_t) {}
uint32_t gpuImageViewImpl(WasmScriptingVM*, uint32_t, uint32_t, uint32_t)
{
    return 0;
}
uint32_t gpuBufferNewImpl(WasmScriptingVM*,
                          uint32_t,
                          uint32_t,
                          uint32_t,
                          const uint8_t*,
                          uint32_t)
{
    return 0;
}
void gpuBufferUpdateImpl(WasmScriptingVM*,
                         uint32_t,
                         uint32_t,
                         const uint8_t*,
                         uint32_t)
{}
void gpuBufferReleaseImpl(WasmScriptingVM*, uint32_t) {}
uint32_t gpuTextureNewImpl(WasmScriptingVM*,
                           const rive_gpu_texture_desc_v1*,
                           uint32_t)
{
    return 0;
}
void gpuTextureUploadImpl(WasmScriptingVM*,
                          uint32_t,
                          const rive_gpu_texture_upload_v1*,
                          uint32_t,
                          const uint8_t*,
                          uint32_t)
{}
void gpuTextureReleaseImpl(WasmScriptingVM*, uint32_t) {}
uint32_t gpuSamplerNewImpl(WasmScriptingVM*,
                           const rive_gpu_sampler_desc_v1*,
                           uint32_t)
{
    return 0;
}
void gpuSamplerReleaseImpl(WasmScriptingVM*, uint32_t) {}
uint32_t gpuTextureViewNewImpl(WasmScriptingVM*,
                               uint32_t,
                               const rive_gpu_texture_view_desc_v1*,
                               uint32_t)
{
    return 0;
}
void gpuTextureViewReleaseImpl(WasmScriptingVM*, uint32_t) {}
uint32_t gpuShaderTargetImpl(WasmScriptingVM*) { return 0; }
uint32_t gpuShaderAssetBytesImpl(WasmScriptingVM*,
                                 uint32_t,
                                 const char*,
                                 uint32_t,
                                 uint8_t*,
                                 uint32_t)
{
    return 0;
}
uint32_t gpuShaderAssetIdImpl(WasmScriptingVM*, uint32_t, const char*, uint32_t)
{
    return 0;
}
uint32_t gpuShaderModuleNewImpl(WasmScriptingVM*,
                                const rive_gpu_shader_module_desc_v1*,
                                uint32_t,
                                const uint8_t*,
                                uint32_t)
{
    return 0;
}
void gpuShaderModuleReleaseImpl(WasmScriptingVM*, uint32_t) {}
uint32_t gpuBindGroupLayoutNewImpl(WasmScriptingVM*,
                                   uint32_t,
                                   const rive_gpu_bind_group_layout_entry_v1*,
                                   uint32_t)
{
    return 0;
}
void gpuBindGroupLayoutReleaseImpl(WasmScriptingVM*, uint32_t) {}
uint32_t gpuBindGroupLayoutFromShaderImpl(WasmScriptingVM*,
                                          uint32_t,
                                          uint32_t,
                                          const uint32_t*,
                                          uint32_t)
{
    return 0;
}
uint32_t gpuBindGroupLayoutFromShadersImpl(WasmScriptingVM*,
                                           uint32_t,
                                           uint32_t,
                                           uint32_t,
                                           const uint32_t*,
                                           uint32_t)
{
    return 0;
}
uint32_t gpuBindGroupNewImpl(WasmScriptingVM*,
                             uint32_t,
                             const rive_gpu_bind_group_ubo_v1*,
                             uint32_t,
                             const rive_gpu_bind_group_texture_v1*,
                             uint32_t,
                             const rive_gpu_bind_group_sampler_v1*,
                             uint32_t)
{
    return 0;
}
void gpuBindGroupReleaseImpl(WasmScriptingVM*, uint32_t) {}
uint32_t gpuPipelineNewImpl(WasmScriptingVM*,
                            const rive_gpu_pipeline_desc_v1*,
                            uint32_t,
                            const uint8_t*,
                            uint32_t)
{
    return 0;
}
void gpuPipelineReleaseImpl(WasmScriptingVM*, uint32_t) {}
#endif

#ifdef RIVE_CANVAS
// Frames go through the deferred canvas host, the only path that can open
// inside the screen frame already open around the draw.
struct HostCanvas
{
    rcp<gpu::RenderCanvas> canvas;
    // A size requested before the device bound, allocated on first use.
    uint32_t pendingWidth = 0;
    uint32_t pendingHeight = 0;
    cmd::DeferredCanvasHost* frameHost = nullptr;
    uint32_t rendererHandle = 0;
};

HostCanvas* resolveCanvas(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return nullptr;
    }
    return static_cast<HostCanvas*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::canvas));
}

// Allocates a pending size once a device is there to allocate against.
// False only when a device is present and refuses.
bool canvasSatisfyPending(WasmScriptingVM* vm, HostCanvas* host)
{
    if (host->pendingWidth == 0 || host->pendingHeight == 0)
    {
        return true;
    }
    auto* renderContext =
        static_cast<gpu::RenderContext*>(vm->factory()->renderContext());
    if (renderContext == nullptr)
    {
        return true;
    }
    // A refused size leaves the previous backing in place.
    auto* deferredHost = vm->factory()->deferredCanvasHost();
    rcp<gpu::RenderCanvas> canvas =
        deferredHost != nullptr
            ? deferredHost->makeContentCanvas(host->pendingWidth,
                                              host->pendingHeight)
            : renderContext->makeRenderCanvas(host->pendingWidth,
                                              host->pendingHeight);
    host->pendingWidth = 0;
    host->pendingHeight = 0;
    if (canvas == nullptr)
    {
        return false;
    }
    host->canvas = std::move(canvas);
    return true;
}

uint32_t canvasNewImpl(WasmScriptingVM* vm, uint32_t width, uint32_t height)
{
    if (vm == nullptr || vm->factory() == nullptr)
    {
        return 0;
    }
    auto* host = new HostCanvas();
    if (width != 0 && height != 0)
    {
        host->pendingWidth = width;
        host->pendingHeight = height;
        // A size only waits when a recording host will bind a device later.
        if (!canvasSatisfyPending(vm, host) ||
            (host->canvas == nullptr &&
             vm->factory()->deferredCanvasHost() == nullptr))
        {
            delete host;
            return 0;
        }
    }
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::canvas, host);
}

void canvasReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto* host = resolveCanvas(vm, handle);
    if (host == nullptr)
    {
        return;
    }
    canvasEndFrameImpl(vm, handle);
    delete host;
    vm->handles().release(handle, WasmScriptingVM::HandleTable::Tag::canvas);
}

uint32_t canvasWidthImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto* host = resolveCanvas(vm, handle);
    if (host == nullptr)
    {
        return 0;
    }
    canvasSatisfyPending(vm, host);
    return host->canvas != nullptr ? host->canvas->width() : host->pendingWidth;
}

uint32_t canvasHeightImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto* host = resolveCanvas(vm, handle);
    if (host == nullptr)
    {
        return 0;
    }
    canvasSatisfyPending(vm, host);
    return host->canvas != nullptr ? host->canvas->height()
                                   : host->pendingHeight;
}

uint32_t canvasResizeImpl(WasmScriptingVM* vm,
                          uint32_t handle,
                          uint32_t width,
                          uint32_t height)
{
    auto* host = resolveCanvas(vm, handle);
    if (host == nullptr || host->frameHost != nullptr)
    {
        return 0;
    }
    if (width == 0 || height == 0)
    {
        host->canvas = nullptr;
        host->pendingWidth = 0;
        host->pendingHeight = 0;
        return 1;
    }
    // An unchanged size would churn a new texture per frame.
    if (host->canvas != nullptr && host->canvas->width() == width &&
        host->canvas->height() == height)
    {
        return 1;
    }
    host->pendingWidth = width;
    host->pendingHeight = height;
    return canvasSatisfyPending(vm, host) ? 1 : 0;
}

uint32_t canvasImageImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto* host = resolveCanvas(vm, handle);
    if (host == nullptr)
    {
        return 0;
    }
    canvasSatisfyPending(vm, host);
    if (host->canvas == nullptr || host->canvas->renderImage() == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::image,
                              new HostImage{ref_rcp(static_cast<RenderImage*>(
                                                host->canvas->renderImage())),
                                            host->canvas});
}

uint32_t canvasBeginFrameImpl(WasmScriptingVM* vm,
                              uint32_t handle,
                              uint32_t clearColor)
{
    auto* host = resolveCanvas(vm, handle);
    if (host == nullptr || host->frameHost != nullptr)
    {
        return 0;
    }
    canvasSatisfyPending(vm, host);
    cmd::DeferredCanvasHost* frameHost = vm->factory()->deferredCanvasHost();
    if (host->canvas == nullptr || frameHost == nullptr)
    {
        return 0;
    }
    Renderer* renderer =
        frameHost->beginCanvasContent(host->canvas.get(), clearColor);
    if (renderer == nullptr)
    {
        return 0;
    }
    host->frameHost = frameHost;
    host->rendererHandle =
        vm->handles().mint(WasmScriptingVM::HandleTable::Tag::renderer,
                           renderer);
    vm->registerOpenCanvasFrame(handle);
    return host->rendererHandle;
}

void canvasEndFrameImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto* host = resolveCanvas(vm, handle);
    if (host == nullptr || host->frameHost == nullptr)
    {
        return;
    }
    vm->unregisterOpenCanvasFrame(handle);
    // Releasing bumps the generation so a stashed renderer goes stale.
    vm->handles().release(host->rendererHandle,
                          WasmScriptingVM::HandleTable::Tag::renderer);
    host->rendererHandle = 0;
    host->frameHost->endCanvasContent(host->canvas.get());
    host->frameHost = nullptr;
}
#else
uint32_t canvasNewImpl(WasmScriptingVM*, uint32_t, uint32_t) { return 0; }
void canvasReleaseImpl(WasmScriptingVM*, uint32_t) {}
uint32_t canvasWidthImpl(WasmScriptingVM*, uint32_t) { return 0; }
uint32_t canvasHeightImpl(WasmScriptingVM*, uint32_t) { return 0; }
uint32_t canvasResizeImpl(WasmScriptingVM*, uint32_t, uint32_t, uint32_t)
{
    return 0;
}
uint32_t canvasImageImpl(WasmScriptingVM*, uint32_t) { return 0; }
uint32_t canvasBeginFrameImpl(WasmScriptingVM*, uint32_t, uint32_t)
{
    return 0;
}
void canvasEndFrameImpl(WasmScriptingVM*, uint32_t) {}
#endif

struct HostBuffer
{
    rcp<RenderBuffer> buffer;
};

HostBuffer* resolveBuffer(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return nullptr;
    }
    return static_cast<HostBuffer*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::buffer));
}

uint32_t bufferNewImpl(WasmScriptingVM* vm,
                       uint32_t bufferType,
                       uint32_t flags,
                       uint32_t sizeInBytes)
{
    if (vm == nullptr)
    {
        return 0;
    }
    auto buffer = vm->factory()->makeRenderBuffer((RenderBufferType)bufferType,
                                                  (RenderBufferFlags)flags,
                                                  sizeInBytes);
    if (buffer == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::buffer,
                              new HostBuffer{std::move(buffer)});
}

void bufferUpdateImpl(WasmScriptingVM* vm,
                      uint32_t handle,
                      const uint8_t* bytes,
                      uint32_t byteCount)
{
    auto host = resolveBuffer(vm, handle);
    if (host == nullptr || byteCount != host->buffer->sizeInBytes())
    {
        return;
    }
    void* data = host->buffer->map();
    if (data != nullptr)
    {
        memcpy(data, bytes, byteCount);
        host->buffer->unmap();
    }
}

void bufferReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return;
    }
    delete static_cast<HostBuffer*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::buffer));
    vm->handles().release(handle, WasmScriptingVM::HandleTable::Tag::buffer);
}

struct HostMeshInstances
{
    rcp<ImageMeshInstances> instances;
};

HostMeshInstances* resolveMeshInstances(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return nullptr;
    }
    return static_cast<HostMeshInstances*>(vm->handles().resolve(
        handle,
        WasmScriptingVM::HandleTable::Tag::meshInstances));
}

uint32_t meshInstancesNewImpl(WasmScriptingVM* vm, uint32_t count)
{
    if (vm == nullptr)
    {
        return 0;
    }
    auto instances = vm->factory()->makeImageMeshInstances(count);
    if (instances == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::meshInstances,
                              new HostMeshInstances{std::move(instances)});
}

void meshInstancesResizeImpl(WasmScriptingVM* vm,
                             uint32_t handle,
                             uint32_t count)
{
    auto host = resolveMeshInstances(vm, handle);
    if (host == nullptr)
    {
        return;
    }
    host->instances->edit(count);
    host->instances->endEdit();
}

void meshInstancesUpdateImpl(WasmScriptingVM* vm,
                             uint32_t handle,
                             uint32_t first,
                             const uint8_t* bytes,
                             uint32_t byteCount)
{
    auto host = resolveMeshInstances(vm, handle);
    if (host == nullptr || byteCount % sizeof(ImageMeshInstanceData) != 0)
    {
        return;
    }
    size_t count = byteCount / sizeof(ImageMeshInstanceData);
    Span<ImageMeshInstanceData> data = host->instances->edit();
    if (first <= data.size() && count <= data.size() - first)
    {
        memcpy(data.data() + first, bytes, byteCount);
    }
    host->instances->endEdit();
}

void meshInstancesReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return;
    }
    delete static_cast<HostMeshInstances*>(vm->handles().resolve(
        handle,
        WasmScriptingVM::HandleTable::Tag::meshInstances));
    vm->handles().release(handle,
                          WasmScriptingVM::HandleTable::Tag::meshInstances);
}

Renderer* resolveRenderer(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return nullptr;
    }
    return static_cast<Renderer*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::renderer));
}

uint32_t testBlobImpl(WasmScriptingVM* vm,
                      const char* name,
                      uint32_t nameLength,
                      uint8_t* out,
                      uint32_t outCount)
{
    if (vm == nullptr)
    {
        return 0;
    }
    return fillOut(vm->testBlob(std::string(name, nameLength)), out, outCount);
}

// A transition child handle points at the caller's TransitionChildRef and
// is released when the call returns.
const ScriptBackend::TransitionChildRef* resolveTransitionChild(
    WasmScriptingVM* vm,
    uint32_t handle)
{
    if (vm == nullptr)
    {
        return nullptr;
    }
    return static_cast<const ScriptBackend::TransitionChildRef*>(
        vm->handles().resolve(
            handle,
            WasmScriptingVM::HandleTable::Tag::transitionChild));
}

void transitionChildDrawImpl(WasmScriptingVM* vm,
                             uint32_t child,
                             uint32_t rendererHandle)
{
    auto ref = resolveTransitionChild(vm, child);
    Renderer* renderer = resolveRenderer(vm, rendererHandle);
    if (ref != nullptr && renderer != nullptr)
    {
        ref->draw(renderer);
    }
}

float transitionChildWidthImpl(WasmScriptingVM* vm, uint32_t child)
{
    auto ref = resolveTransitionChild(vm, child);
    return ref != nullptr ? ref->width() : 0.0f;
}

float transitionChildHeightImpl(WasmScriptingVM* vm, uint32_t child)
{
    auto ref = resolveTransitionChild(vm, child);
    return ref != nullptr ? ref->height() : 0.0f;
}

void rendererSaveImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (auto renderer = resolveRenderer(vm, handle))
    {
        renderer->save();
        auto& saves = vm->visitSaves();
        saves.open += renderer == saves.renderer ? 1 : 0;
    }
}
void rendererRestoreImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (auto renderer = resolveRenderer(vm, handle))
    {
        renderer->restore();
        auto& saves = vm->visitSaves();
        saves.open -= renderer == saves.renderer ? 1 : 0;
    }
}
void rendererTransformImpl(WasmScriptingVM* vm,
                           uint32_t handle,
                           float xx,
                           float xy,
                           float yx,
                           float yy,
                           float tx,
                           float ty)
{
    if (auto renderer = resolveRenderer(vm, handle))
    {
        renderer->transform(Mat2D(xx, xy, yx, yy, tx, ty));
    }
}
void rendererDrawPathImpl(WasmScriptingVM* vm,
                          uint32_t rendererHandle,
                          uint32_t pathHandle,
                          uint32_t paintHandle)
{
    auto renderer = resolveRenderer(vm, rendererHandle);
    auto paint = resolvePaint(vm, paintHandle);
    if (vm == nullptr || renderer == nullptr || paint == nullptr)
    {
        return;
    }
    RenderPath* path = hostRenderPath(vm, resolvePath(vm, pathHandle));
    if (path == nullptr)
    {
        return;
    }
    renderer->drawPath(path, paint);
}
void rendererDrawImageImpl(WasmScriptingVM* vm,
                           uint32_t rendererHandle,
                           uint32_t imageHandle,
                           uint32_t samplerKey,
                           uint32_t blend,
                           float opacity)
{
    auto renderer = resolveRenderer(vm, rendererHandle);
    auto host = resolveImage(vm, imageHandle);
    if (renderer == nullptr || host == nullptr)
    {
        return;
    }
    renderer->drawImage(host->image.get(),
                        ImageSampler::SamplerFromKey((uint8_t)samplerKey),
                        (BlendMode)blend,
                        opacity);
}

void rendererDrawImageMeshImpl(WasmScriptingVM* vm,
                               uint32_t rendererHandle,
                               uint32_t imageHandle,
                               uint32_t samplerKey,
                               uint32_t vertexHandle,
                               uint32_t uvHandle,
                               uint32_t indexHandle,
                               uint32_t blend,
                               float opacity)
{
    auto renderer = resolveRenderer(vm, rendererHandle);
    auto image = resolveImage(vm, imageHandle);
    auto vertex = resolveBuffer(vm, vertexHandle);
    auto uv = resolveBuffer(vm, uvHandle);
    auto index = resolveBuffer(vm, indexHandle);
    if (renderer == nullptr || image == nullptr || vertex == nullptr ||
        uv == nullptr || index == nullptr)
    {
        return;
    }
    renderer->drawImageMesh(
        image->image.get(),
        ImageSampler::SamplerFromKey((uint8_t)samplerKey),
        vertex->buffer,
        uv->buffer,
        index->buffer,
        (uint32_t)(vertex->buffer->sizeInBytes() / sizeof(Vec2D)),
        (uint32_t)(index->buffer->sizeInBytes() / sizeof(uint16_t)),
        (BlendMode)blend,
        opacity);
}

void rendererDrawImageMeshInstancedImpl(WasmScriptingVM* vm,
                                        uint32_t rendererHandle,
                                        uint32_t imageHandle,
                                        uint32_t samplerKey,
                                        uint32_t vertexHandle,
                                        uint32_t uvHandle,
                                        uint32_t indexHandle,
                                        uint32_t instancesHandle)
{
    auto renderer = resolveRenderer(vm, rendererHandle);
    auto image = resolveImage(vm, imageHandle);
    auto vertex = resolveBuffer(vm, vertexHandle);
    auto uv = resolveBuffer(vm, uvHandle);
    auto index = resolveBuffer(vm, indexHandle);
    auto instances = resolveMeshInstances(vm, instancesHandle);
    if (renderer == nullptr || image == nullptr || vertex == nullptr ||
        uv == nullptr || index == nullptr || instances == nullptr)
    {
        return;
    }
    renderer->drawImageMeshInstanced(
        image->image.get(),
        ImageSampler::SamplerFromKey((uint8_t)samplerKey),
        vertex->buffer,
        uv->buffer,
        index->buffer,
        (uint32_t)(vertex->buffer->sizeInBytes() / sizeof(Vec2D)),
        (uint32_t)(index->buffer->sizeInBytes() / sizeof(uint16_t)),
        instances->instances);
}

void rendererClipPathImpl(WasmScriptingVM* vm,
                          uint32_t rendererHandle,
                          uint32_t pathHandle)
{
    auto renderer = resolveRenderer(vm, rendererHandle);
    if (vm == nullptr || renderer == nullptr)
    {
        return;
    }
    RenderPath* path = hostRenderPath(vm, resolvePath(vm, pathHandle));
    if (path == nullptr)
    {
        return;
    }
    renderer->clipPath(path);
}

void rendererModulateOpacityImpl(WasmScriptingVM* vm,
                                 uint32_t rendererHandle,
                                 float opacity)
{
    auto renderer = resolveRenderer(vm, rendererHandle);
    if (renderer != nullptr)
    {
        renderer->modulateOpacity(opacity);
    }
}

void rendererModulateColorImpl(WasmScriptingVM* vm,
                               uint32_t rendererHandle,
                               uint32_t color,
                               uint32_t replace)
{
    auto renderer = resolveRenderer(vm, rendererHandle);
    if (renderer != nullptr)
    {
        renderer->modulateColor(color, replace != 0);
    }
}

void rtLogImpl(WasmScriptingVM* vm,
               int32_t level,
               const char* message,
               uint32_t length)
{
    WasmScriptingVMNatives::print(vm, message, length);
    WasmScriptingVMNatives::print(vm, "\n", 1);
}

void rtBudgetExceededImpl(WasmScriptingVM* vm, uint32_t ms)
{
    fprintf(stderr,
            "script error: execution exceeded %u millisecond timeout\n",
            ms);
    // Terminates like a trap when the native returns, so the caller's
    // failed-op handling engages.
    vm->raiseBudgetExceeded();
}

void rtErrorImpl(WasmScriptingVM* vm, const char* message, uint32_t length)
{
    vm->raiseThrown(std::string(message, length).c_str());
}

// Unfused multiply-add keeps these bit identical to the module's own f32
// math, on every tier and platform.
#pragma clang fp contract(off)
void mat4MultiplyImpl(WasmScriptingVM*,
                      uint8_t* outBytes,
                      uint32_t outByteCount,
                      const uint8_t* aBytes,
                      uint32_t aByteCount,
                      const uint8_t* bBytes,
                      uint32_t bByteCount)
{
    if (outByteCount < 64 || aByteCount < 64 || bByteCount < 64)
    {
        return;
    }
    float a[16];
    float b[16];
    memcpy(a, aBytes, sizeof(a));
    memcpy(b, bBytes, sizeof(b));
    float result[16];
    for (int col = 0; col < 4; col++)
    {
        for (int row = 0; row < 4; row++)
        {
            result[col * 4 + row] =
                a[row] * b[col * 4] + a[4 + row] * b[col * 4 + 1] +
                a[8 + row] * b[col * 4 + 2] + a[12 + row] * b[col * 4 + 3];
        }
    }
    memcpy(outBytes, result, sizeof(result));
}

uint32_t mat4InvertImpl(WasmScriptingVM*,
                        uint8_t* outBytes,
                        uint32_t outByteCount,
                        const uint8_t* srcBytes,
                        uint32_t srcByteCount)
{
    if (outByteCount < 64 || srcByteCount < 64)
    {
        return 0;
    }
    float m[16];
    memcpy(m, srcBytes, sizeof(m));
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] +
             m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] +
             m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] +
             m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] +
             m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] +
              m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] +
              m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] +
             m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] +
             m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] +
             m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] -
             m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] +
              m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] +
             m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] -
             m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] +
              m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] -
              m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] -
             m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] +
             m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] -
              m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] +
              m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (det == 0)
    {
        return 0;
    }
    float detInv = 1.0f / det;
    for (int i = 0; i < 16; i++)
    {
        inv[i] *= detInv;
    }
    memcpy(outBytes, inv, sizeof(inv));
    return 1;
}
#pragma clang fp contract(on)

uint32_t rtUtcOffsetImpl(WasmScriptingVM* vm, double epochSeconds)
{
    time_t at;
    struct tm local;
    return localTime(epochSeconds, at, local) ? (uint32_t)utcOffsetOf(local, at)
                                              : 0;
}

uint32_t rtIsDstImpl(WasmScriptingVM* vm, double epochSeconds)
{
    time_t at;
    struct tm local;
    return localTime(epochSeconds, at, local) && local.tm_isdst > 0 ? 1 : 0;
}

uint32_t rtZoneNameImpl(WasmScriptingVM* vm,
                        double epochSeconds,
                        char* buffer,
                        uint32_t capacity)
{
    time_t at;
    struct tm local;
    if (!localTime(epochSeconds, at, local))
    {
        return 0;
    }
    char name[64];
    size_t length = strftime(name, sizeof(name), "%Z", &local);
    return fillModuleString(vm, std::string(name, length), buffer, capacity);
}

// Module start has no exec env to carry the vm; the probes its top level
// hits belong to the VM being booted.
static WasmDebugHooks* debugHooksFor(WasmScriptingVM*& vm)
{
    if (vm == nullptr)
    {
        vm = s_booting;
    }
    return vm != nullptr ? vm->debugHooks() : nullptr;
}

void rtDebugEnterImpl(WasmScriptingVM* vm, uint32_t function, uint32_t line)
{
    if (WasmDebugHooks* hooks = debugHooksFor(vm))
    {
        hooks->onEnter(*vm, function, line);
    }
}

uint32_t rtDebugLineImpl(WasmScriptingVM* vm, uint32_t line)
{
    WasmDebugHooks* hooks = debugHooksFor(vm);
    return hooks != nullptr && hooks->onLine(*vm, line) ? 1 : 0;
}

void rtDebugLeaveImpl(WasmScriptingVM* vm)
{
    if (WasmDebugHooks* hooks = debugHooksFor(vm))
    {
        hooks->onLeave(*vm);
    }
}

void rtMarkNeedsUpdateImpl(WasmScriptingVM* vm, uint32_t objectHandle)
{
    if (vm == nullptr)
    {
        return;
    }
    auto object = static_cast<ScriptedObject*>(
        vm->handles().resolve(objectHandle,
                              WasmScriptingVM::HandleTable::Tag::object));
    if (object != nullptr)
    {
        object->markNeedsUpdate();
    }
}

// --- rive_data_v1: view model instances and their values, pinned per handle

struct HostViewModelInstance
{
    rcp<ViewModelInstance> instance;
};

// Forwards core value change notifications into the module's listener
// registry by token.
struct HostValueDelegate : public ViewModelInstanceValueDelegate
{
    WasmScriptingVM* vm = nullptr;
    uint32_t token = 0;
    void valueChanged() override { vm->notifyDataValueChanged(token); }
};

struct HostInstanceValue
{
    rcp<ViewModelInstanceValue> value;
    HostValueDelegate* delegate = nullptr;
    // What string_get last handed the module, for string_changed.
    std::string sent;
    bool hasSent = false;

    ~HostInstanceValue()
    {
        if (delegate != nullptr)
        {
            value->removeDelegate(delegate);
            delete delegate;
        }
    }
};

void dataConvertResultImpl(WasmScriptingVM* vm,
                           uint32_t kind,
                           float number,
                           uint32_t booleanValue,
                           uint32_t color,
                           const char* value,
                           uint32_t length)
{
    ScriptBackend::ScriptDataResult* out =
        vm != nullptr ? vm->convertResultOut() : nullptr;
    if (out == nullptr)
    {
        return;
    }
    switch (kind)
    {
        case DataConvertWire::kindNumber:
            out->kind = ScriptBackend::ScriptDataResult::Kind::number;
            out->number = number;
            break;
        case DataConvertWire::kindString:
            out->kind = ScriptBackend::ScriptDataResult::Kind::string;
            out->string.assign(value, length);
            break;
        case DataConvertWire::kindBoolean:
            out->kind = ScriptBackend::ScriptDataResult::Kind::boolean;
            out->boolean = booleanValue != 0;
            break;
        case DataConvertWire::kindColor:
            out->kind = ScriptBackend::ScriptDataResult::Kind::color;
            out->color = (int)color;
            break;
        default:
            break;
    }
}

uint32_t dataViewModelImpl(WasmScriptingVM* vm, uint32_t objectHandle)
{
    if (vm == nullptr)
    {
        return 0;
    }
    auto object = static_cast<ScriptedObject*>(
        vm->handles().resolve(objectHandle,
                              WasmScriptingVM::HandleTable::Tag::object));
    auto instance = object != nullptr && object->dataContext() != nullptr
                        ? object->dataContext()->mainViewModelInstance()
                        : nullptr;
    if (instance == nullptr)
    {
        vm->noteMissingRequestedData();
        return 0;
    }
    return vm->handles().mint(
        WasmScriptingVM::HandleTable::Tag::viewModelInstance,
        new HostViewModelInstance{std::move(instance)});
}

void dataVmiReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return;
    }
    delete static_cast<HostViewModelInstance*>(vm->handles().resolve(
        handle,
        WasmScriptingVM::HandleTable::Tag::viewModelInstance));
    vm->handles().release(handle,
                          WasmScriptingVM::HandleTable::Tag::viewModelInstance);
}

template <typename T>
uint32_t mintInstanceValue(WasmScriptingVM* vm,
                           uint32_t vmiHandle,
                           const char* name,
                           uint32_t length)
{
    if (vm == nullptr)
    {
        return 0;
    }
    auto host = static_cast<HostViewModelInstance*>(vm->handles().resolve(
        vmiHandle,
        WasmScriptingVM::HandleTable::Tag::viewModelInstance));
    if (host == nullptr)
    {
        return 0;
    }
    auto value = host->instance->propertyValue(std::string(name, length));
    if (value == nullptr || !value->template is<T>())
    {
        return 0;
    }
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::instanceValue,
                              new HostInstanceValue{ref_rcp(value)});
}

ViewModel* findViewModel(WasmScriptingVM* vm, const char* name, uint32_t length)
{
    if (vm == nullptr || vm->viewModels() == nullptr)
    {
        return nullptr;
    }
    std::string key(name, length);
    for (ViewModel* viewModel : *vm->viewModels())
    {
        if (viewModel != nullptr && viewModel->name() == key)
        {
            return viewModel;
        }
    }
    return nullptr;
}

uint32_t dataHasViewModelImpl(WasmScriptingVM* vm,
                              const char* name,
                              uint32_t nameLength)
{
    return findViewModel(vm, name, nameLength) != nullptr ? 1 : 0;
}

uint32_t dataNewViewModelImpl(WasmScriptingVM* vm,
                              const char* name,
                              uint32_t nameLength,
                              const char* templateName,
                              uint32_t templateLength)
{
    ViewModel* viewModel = findViewModel(vm, name, nameLength);
    if (vm == nullptr || viewModel == nullptr)
    {
        return 0;
    }
    auto instance = templateLength != 0
                        ? viewModel->createFromInstance(
                              std::string(templateName, templateLength))
                        : viewModel->createInstance();
    if (instance == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(
        WasmScriptingVM::HandleTable::Tag::viewModelInstance,
        new HostViewModelInstance{std::move(instance)});
}

uint32_t dataVmiNumberImpl(WasmScriptingVM* vm,
                           uint32_t vmiHandle,
                           const char* name,
                           uint32_t length)
{
    return mintInstanceValue<ViewModelInstanceNumber>(vm,
                                                      vmiHandle,
                                                      name,
                                                      length);
}

uint32_t dataVmiBooleanImpl(WasmScriptingVM* vm,
                            uint32_t vmiHandle,
                            const char* name,
                            uint32_t length)
{
    return mintInstanceValue<ViewModelInstanceBoolean>(vm,
                                                       vmiHandle,
                                                       name,
                                                       length);
}

uint32_t dataVmiStringImpl(WasmScriptingVM* vm,
                           uint32_t vmiHandle,
                           const char* name,
                           uint32_t length)
{
    return mintInstanceValue<ViewModelInstanceString>(vm,
                                                      vmiHandle,
                                                      name,
                                                      length);
}

uint32_t dataVmiTriggerImpl(WasmScriptingVM* vm,
                            uint32_t vmiHandle,
                            const char* name,
                            uint32_t length)
{
    return mintInstanceValue<ViewModelInstanceTrigger>(vm,
                                                       vmiHandle,
                                                       name,
                                                       length);
}

uint32_t dataVmiColorImpl(WasmScriptingVM* vm,
                          uint32_t vmiHandle,
                          const char* name,
                          uint32_t length)
{
    return mintInstanceValue<ViewModelInstanceColor>(vm,
                                                     vmiHandle,
                                                     name,
                                                     length);
}

uint32_t dataVmiViewModelImpl(WasmScriptingVM* vm,
                              uint32_t vmiHandle,
                              const char* name,
                              uint32_t length)
{
    return mintInstanceValue<ViewModelInstanceViewModel>(vm,
                                                         vmiHandle,
                                                         name,
                                                         length);
}

void dataPropReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return;
    }
    delete static_cast<HostInstanceValue*>(vm->handles().resolve(
        handle,
        WasmScriptingVM::HandleTable::Tag::instanceValue));
    vm->handles().release(handle,
                          WasmScriptingVM::HandleTable::Tag::instanceValue);
}

template <typename T>
T* resolveInstanceValue(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return nullptr;
    }
    auto host = static_cast<HostInstanceValue*>(vm->handles().resolve(
        handle,
        WasmScriptingVM::HandleTable::Tag::instanceValue));
    if (host == nullptr || !host->value->template is<T>())
    {
        return nullptr;
    }
    return host->value->template as<T>();
}

float dataNumberGetImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto number = resolveInstanceValue<ViewModelInstanceNumber>(vm, handle);
    return number != nullptr ? number->propertyValue() : 0.0f;
}

void dataNumberSetImpl(WasmScriptingVM* vm, uint32_t handle, float value)
{
    auto number = resolveInstanceValue<ViewModelInstanceNumber>(vm, handle);
    if (number != nullptr)
    {
        number->propertyValue(value);
    }
}

uint32_t dataBooleanGetImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto boolean = resolveInstanceValue<ViewModelInstanceBoolean>(vm, handle);
    return boolean != nullptr && boolean->propertyValue() ? 1 : 0;
}

void dataBooleanSetImpl(WasmScriptingVM* vm, uint32_t handle, uint32_t value)
{
    auto boolean = resolveInstanceValue<ViewModelInstanceBoolean>(vm, handle);
    if (boolean != nullptr)
    {
        boolean->propertyValue(value != 0);
    }
}

uint32_t dataStringGetImpl(WasmScriptingVM* vm,
                           uint32_t handle,
                           char* buffer,
                           uint32_t capacity)
{
    auto string = resolveInstanceValue<ViewModelInstanceString>(vm, handle);
    if (string == nullptr)
    {
        return 0;
    }
    uint32_t length =
        fillModuleString(vm, string->propertyValue(), buffer, capacity);
    if (length <= capacity)
    {
        auto host = static_cast<HostInstanceValue*>(vm->handles().resolve(
            handle,
            WasmScriptingVM::HandleTable::Tag::instanceValue));
        host->sent = string->propertyValue();
        host->hasSent = true;
    }
    return length;
}

uint32_t dataStringChangedImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto string = resolveInstanceValue<ViewModelInstanceString>(vm, handle);
    if (string == nullptr)
    {
        return 0;
    }
    auto host = static_cast<HostInstanceValue*>(vm->handles().resolve(
        handle,
        WasmScriptingVM::HandleTable::Tag::instanceValue));
    if (host->hasSent && host->sent == string->propertyValue())
    {
        return ~0u;
    }
    return (uint32_t)fillModuleString(vm, string->propertyValue(), nullptr, 0);
}

void dataStringSetImpl(WasmScriptingVM* vm,
                       uint32_t handle,
                       const char* value,
                       uint32_t length)
{
    auto string = resolveInstanceValue<ViewModelInstanceString>(vm, handle);
    if (string != nullptr)
    {
        string->propertyValue(std::string(value, length));
    }
}

void dataTriggerFireImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto trigger = resolveInstanceValue<ViewModelInstanceTrigger>(vm, handle);
    if (trigger != nullptr)
    {
        trigger->trigger();
    }
}

uint32_t dataColorGetImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto color = resolveInstanceValue<ViewModelInstanceColor>(vm, handle);
    return color != nullptr ? (uint32_t)color->propertyValue() : 0;
}

void dataColorSetImpl(WasmScriptingVM* vm, uint32_t handle, uint32_t value)
{
    auto color = resolveInstanceValue<ViewModelInstanceColor>(vm, handle);
    if (color != nullptr)
    {
        color->propertyValue((int)value);
    }
}

uint32_t dataVmiEnumImpl(WasmScriptingVM* vm,
                         uint32_t vmiHandle,
                         const char* name,
                         uint32_t length)
{
    return mintInstanceValue<ViewModelInstanceEnum>(vm,
                                                    vmiHandle,
                                                    name,
                                                    length);
}

uint32_t dataEnumGetImpl(WasmScriptingVM* vm,
                         uint32_t handle,
                         char* buffer,
                         uint32_t capacity)
{
    auto value = resolveInstanceValue<ViewModelInstanceEnum>(vm, handle);
    if (value == nullptr || value->viewModelProperty() == nullptr ||
        !value->viewModelProperty()->is<ViewModelPropertyEnum>())
    {
        return 0;
    }
    auto dataEnum =
        value->viewModelProperty()->as<ViewModelPropertyEnum>()->dataEnum();
    if (dataEnum == nullptr)
    {
        return 0;
    }
    auto values = dataEnum->values();
    uint32_t index = value->propertyValue();
    if (index >= values.size())
    {
        return 0;
    }
    return fillModuleString(vm, values[index]->key(), buffer, capacity);
}

void dataEnumSetImpl(WasmScriptingVM* vm,
                     uint32_t handle,
                     const char* value,
                     uint32_t length)
{
    auto instanceValue =
        resolveInstanceValue<ViewModelInstanceEnum>(vm, handle);
    if (instanceValue != nullptr)
    {
        instanceValue->value(std::string(value, length));
    }
}

uint32_t dataVmiListImpl(WasmScriptingVM* vm,
                         uint32_t vmiHandle,
                         const char* name,
                         uint32_t length)
{
    return mintInstanceValue<ViewModelInstanceList>(vm,
                                                    vmiHandle,
                                                    name,
                                                    length);
}

uint32_t dataListLengthImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto list = resolveInstanceValue<ViewModelInstanceList>(vm, handle);
    return list != nullptr ? (uint32_t)list->listItems().size() : 0;
}

void dataListPushImpl(WasmScriptingVM* vm, uint32_t handle, uint32_t vmiHandle)
{
    auto list = resolveInstanceValue<ViewModelInstanceList>(vm, handle);
    if (vm == nullptr || list == nullptr)
    {
        return;
    }
    auto host = static_cast<HostViewModelInstance*>(vm->handles().resolve(
        vmiHandle,
        WasmScriptingVM::HandleTable::Tag::viewModelInstance));
    if (host == nullptr)
    {
        return;
    }
    auto item = make_rcp<ViewModelInstanceListItem>();
    item->viewModelInstance(host->instance);
    list->addItem(std::move(item));
}

uint32_t mintRemovedItem(WasmScriptingVM* vm,
                         rcp<ViewModelInstanceListItem> item)
{
    if (vm == nullptr || item == nullptr ||
        item->viewModelInstance() == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(
        WasmScriptingVM::HandleTable::Tag::viewModelInstance,
        new HostViewModelInstance{item->viewModelInstance()});
}

uint32_t dataListPopImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto list = resolveInstanceValue<ViewModelInstanceList>(vm, handle);
    return list != nullptr ? mintRemovedItem(vm, list->pop()) : 0;
}

uint32_t dataListShiftImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto list = resolveInstanceValue<ViewModelInstanceList>(vm, handle);
    return list != nullptr ? mintRemovedItem(vm, list->shift()) : 0;
}

void dataListClearImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto list = resolveInstanceValue<ViewModelInstanceList>(vm, handle);
    if (list != nullptr)
    {
        list->removeAllItems();
    }
}

void dataListSwapImpl(WasmScriptingVM* vm,
                      uint32_t handle,
                      uint32_t index1,
                      uint32_t index2)
{
    auto list = resolveInstanceValue<ViewModelInstanceList>(vm, handle);
    if (list != nullptr)
    {
        list->swap(index1, index2);
    }
}

rcp<ViewModelInstance> resolveVmiArg(WasmScriptingVM* vm, uint32_t vmiHandle)
{
    if (vm == nullptr)
    {
        return nullptr;
    }
    auto host = static_cast<HostViewModelInstance*>(vm->handles().resolve(
        vmiHandle,
        WasmScriptingVM::HandleTable::Tag::viewModelInstance));
    return host != nullptr ? host->instance : nullptr;
}

void dataListInsertImpl(WasmScriptingVM* vm,
                        uint32_t handle,
                        uint32_t vmiHandle,
                        uint32_t index)
{
    auto list = resolveInstanceValue<ViewModelInstanceList>(vm, handle);
    auto instance = resolveVmiArg(vm, vmiHandle);
    if (list == nullptr || instance == nullptr)
    {
        return;
    }
    auto item = make_rcp<ViewModelInstanceListItem>();
    item->viewModelInstance(std::move(instance));
    list->addItemAt(std::move(item), (int)index);
}

void dataListRemoveImpl(WasmScriptingVM* vm,
                        uint32_t handle,
                        uint32_t vmiHandle)
{
    auto list = resolveInstanceValue<ViewModelInstanceList>(vm, handle);
    auto instance = resolveVmiArg(vm, vmiHandle);
    if (list == nullptr || instance == nullptr)
    {
        return;
    }
    for (const auto& item : list->listItems())
    {
        if (item->viewModelInstance() == instance)
        {
            list->removeItem(item);
            break;
        }
    }
}

void dataListRemoveAtImpl(WasmScriptingVM* vm, uint32_t handle, uint32_t index)
{
    auto list = resolveInstanceValue<ViewModelInstanceList>(vm, handle);
    if (list != nullptr)
    {
        list->removeItem((int)index);
    }
}

void dataViewModelSetImpl(WasmScriptingVM* vm,
                          uint32_t handle,
                          uint32_t vmiHandle)
{
    auto value = resolveInstanceValue<ViewModelInstanceViewModel>(vm, handle);
    auto instance = resolveVmiArg(vm, vmiHandle);
    if (value == nullptr || instance == nullptr ||
        value->parentViewModelInstance() == nullptr)
    {
        return;
    }
    value->parentViewModelInstance()->replaceViewModelByProperty(
        value,
        std::move(instance));
}

uint32_t dataViewModelGetImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto value = resolveInstanceValue<ViewModelInstanceViewModel>(vm, handle);
    if (vm == nullptr || value == nullptr)
    {
        return 0;
    }
    auto reference = value->referenceViewModelInstance();
    if (reference == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(
        WasmScriptingVM::HandleTable::Tag::viewModelInstance,
        new HostViewModelInstance{std::move(reference)});
}

uint32_t dataVmiPropertyImpl(WasmScriptingVM* vm,
                             uint32_t vmiHandle,
                             const char* name,
                             uint32_t length,
                             uint32_t* kindOut,
                             uint32_t kindCount)
{
    if (kindCount > 0)
    {
        kindOut[0] = DataPropertyWire::kindNone;
    }
    if (vm == nullptr)
    {
        return 0;
    }
    auto host = static_cast<HostViewModelInstance*>(vm->handles().resolve(
        vmiHandle,
        WasmScriptingVM::HandleTable::Tag::viewModelInstance));
    if (host == nullptr)
    {
        return 0;
    }
    auto value = host->instance->propertyValue(std::string(name, length));
    if (value == nullptr)
    {
        return 0;
    }
    uint32_t kind = DataPropertyWire::kindNone;
    switch (value->coreType())
    {
        case ViewModelInstanceNumberBase::typeKey:
            kind = DataPropertyWire::kindNumber;
            break;
        case ViewModelInstanceBooleanBase::typeKey:
            kind = DataPropertyWire::kindBoolean;
            break;
        case ViewModelInstanceStringBase::typeKey:
            kind = DataPropertyWire::kindString;
            break;
        case ViewModelInstanceTriggerBase::typeKey:
            kind = DataPropertyWire::kindTrigger;
            break;
        case ViewModelInstanceColorBase::typeKey:
            kind = DataPropertyWire::kindColor;
            break;
        case ViewModelInstanceViewModelBase::typeKey:
            kind = DataPropertyWire::kindViewModel;
            break;
        case ViewModelInstanceListBase::typeKey:
            kind = DataPropertyWire::kindList;
            break;
        case ViewModelInstanceEnumBase::typeKey:
            kind = DataPropertyWire::kindEnum;
            break;
        case ViewModelInstanceAssetImageBase::typeKey:
            kind = DataPropertyWire::kindImage;
            break;
        case ViewModelInstanceAssetFontBase::typeKey:
            kind = DataPropertyWire::kindFont;
            break;
        case ViewModelInstanceAssetBlobBase::typeKey:
            kind = DataPropertyWire::kindBlob;
            break;
        case ViewModelInstanceSymbolListIndexBase::typeKey:
            if (kindCount > 0)
            {
                kindOut[0] = DataPropertyWire::kindSymbolListIndex;
            }
            if (kindCount > 1)
            {
                kindOut[1] =
                    (uint32_t)value->as<ViewModelInstanceSymbolListIndex>()
                        ->propertyValue();
            }
            return 0;
        default:
            return 0;
    }
    if (kindCount > 0)
    {
        kindOut[0] = kind;
    }
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::instanceValue,
                              new HostInstanceValue{ref_rcp(value)});
}

uint32_t dataVmiInstanceImpl(WasmScriptingVM* vm,
                             uint32_t vmiHandle,
                             const char* name,
                             uint32_t nameLength)
{
    if (vm == nullptr)
    {
        return 0;
    }
    auto host = static_cast<HostViewModelInstance*>(vm->handles().resolve(
        vmiHandle,
        WasmScriptingVM::HandleTable::Tag::viewModelInstance));
    if (host == nullptr || host->instance->viewModel() == nullptr)
    {
        return 0;
    }
    ViewModel* viewModel = host->instance->viewModel();
    auto instance =
        nameLength != 0
            ? viewModel->createFromInstance(std::string(name, nameLength))
            : nullptr;
    if (instance == nullptr)
    {
        instance = viewModel->createInstance();
    }
    if (instance == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(
        WasmScriptingVM::HandleTable::Tag::viewModelInstance,
        new HostViewModelInstance{std::move(instance)});
}

uint32_t dataVmiSymbolIndexImpl(WasmScriptingVM* vm, uint32_t vmiHandle)
{
    if (vm == nullptr)
    {
        return ~0u;
    }
    auto host = static_cast<HostViewModelInstance*>(vm->handles().resolve(
        vmiHandle,
        WasmScriptingVM::HandleTable::Tag::viewModelInstance));
    if (host == nullptr)
    {
        return ~0u;
    }
    auto prop = host->instance->propertyValue(SymbolType::itemIndex);
    if (prop != nullptr && prop->is<ViewModelInstanceSymbolListIndex>())
    {
        return (uint32_t)prop->as<ViewModelInstanceSymbolListIndex>()
            ->propertyValue();
    }
    return ~0u;
}

uint32_t dataVmiEqualImpl(WasmScriptingVM* vm, uint32_t a, uint32_t b)
{
    auto lhs = resolveVmiArg(vm, a);
    auto rhs = resolveVmiArg(vm, b);
    return lhs != nullptr && lhs == rhs ? 1 : 0;
}

void dataListRemoveAllOfImpl(WasmScriptingVM* vm,
                             uint32_t handle,
                             uint32_t vmiHandle)
{
    auto list = resolveInstanceValue<ViewModelInstanceList>(vm, handle);
    auto instance = resolveVmiArg(vm, vmiHandle);
    if (list != nullptr && instance != nullptr)
    {
        list->removeAllItemsWithViewModelInstance(instance.get());
    }
}

uint32_t dataListGetImpl(WasmScriptingVM* vm, uint32_t handle, uint32_t index)
{
    auto list = resolveInstanceValue<ViewModelInstanceList>(vm, handle);
    if (list == nullptr || index >= list->listItems().size())
    {
        return 0;
    }
    return mintRemovedItem(vm, list->listItems()[index]);
}

uint32_t dataEnumValuesImpl(WasmScriptingVM* vm,
                            uint32_t handle,
                            char* buffer,
                            uint32_t capacity)
{
    auto value = resolveInstanceValue<ViewModelInstanceEnum>(vm, handle);
    if (value == nullptr || value->viewModelProperty() == nullptr ||
        !value->viewModelProperty()->is<ViewModelPropertyEnum>())
    {
        return 0;
    }
    auto dataEnum =
        value->viewModelProperty()->as<ViewModelPropertyEnum>()->dataEnum();
    if (dataEnum == nullptr)
    {
        return 0;
    }
    std::string joined;
    for (auto& entry : dataEnum->values())
    {
        if (!joined.empty())
        {
            joined += '\n';
        }
        joined += entry->key();
    }
    return fillModuleString(vm, joined, buffer, capacity);
}

uint32_t dataRootViewModelImpl(WasmScriptingVM* vm, uint32_t objectHandle)
{
    if (vm == nullptr)
    {
        return 0;
    }
    auto object = static_cast<ScriptedObject*>(
        vm->handles().resolve(objectHandle,
                              WasmScriptingVM::HandleTable::Tag::object));
    auto instance = object != nullptr && object->dataContext() != nullptr
                        ? object->dataContext()->rootViewModelInstance()
                        : nullptr;
    if (instance == nullptr)
    {
        vm->noteMissingRequestedData();
        return 0;
    }
    return vm->handles().mint(
        WasmScriptingVM::HandleTable::Tag::viewModelInstance,
        new HostViewModelInstance{std::move(instance)});
}

uint32_t dataGlobalViewModelImpl(WasmScriptingVM* vm,
                                 uint32_t objectHandle,
                                 const char* name,
                                 uint32_t nameLength)
{
    if (vm == nullptr)
    {
        return 0;
    }
    auto object = static_cast<ScriptedObject*>(
        vm->handles().resolve(objectHandle,
                              WasmScriptingVM::HandleTable::Tag::object));
    if (object == nullptr || object->scriptAsset() == nullptr ||
        object->scriptAsset()->file() == nullptr ||
        object->dataContext() == nullptr)
    {
        return 0;
    }
    std::string key(name, nameLength);
    auto instance = object->dataContext()->resolveGlobalViewModel(
        object->scriptAsset()->file(),
        key.c_str());
    if (instance == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(
        WasmScriptingVM::HandleTable::Tag::viewModelInstance,
        new HostViewModelInstance{std::move(instance)});
}

uint32_t dataGlobalViewModelNamesImpl(WasmScriptingVM* vm,
                                      uint32_t objectHandle,
                                      char* buffer,
                                      uint32_t capacity)
{
    if (vm == nullptr)
    {
        return 0;
    }
    auto object = static_cast<ScriptedObject*>(
        vm->handles().resolve(objectHandle,
                              WasmScriptingVM::HandleTable::Tag::object));
    if (object == nullptr || object->scriptAsset() == nullptr ||
        object->scriptAsset()->file() == nullptr)
    {
        return 0;
    }
    std::string joined;
    for (auto& name : object->scriptAsset()->file()->globalViewModelNames())
    {
        if (!joined.empty())
        {
            joined += '\n';
        }
        joined += name;
    }
    return fillModuleString(vm, joined, buffer, capacity);
}

struct HostDataContext
{
    rcp<DataContext> context;
};

uint32_t dataContextImpl(WasmScriptingVM* vm, uint32_t objectHandle)
{
    if (vm == nullptr)
    {
        return 0;
    }
    auto object = static_cast<ScriptedObject*>(
        vm->handles().resolve(objectHandle,
                              WasmScriptingVM::HandleTable::Tag::object));
    if (object == nullptr || object->dataContext() == nullptr)
    {
        vm->noteMissingRequestedData();
        return 0;
    }
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::dataContext,
                              new HostDataContext{object->dataContext()});
}

uint32_t dataContextParentImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return 0;
    }
    auto host = static_cast<HostDataContext*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::dataContext));
    if (host == nullptr || host->context->parent() == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::dataContext,
                              new HostDataContext{host->context->parent()});
}

uint32_t dataContextViewModelImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return 0;
    }
    auto host = static_cast<HostDataContext*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::dataContext));
    if (host == nullptr)
    {
        return 0;
    }
    auto instance = host->context->mainViewModelInstance();
    if (instance == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(
        WasmScriptingVM::HandleTable::Tag::viewModelInstance,
        new HostViewModelInstance{std::move(instance)});
}

void dataContextReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return;
    }
    delete static_cast<HostDataContext*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::dataContext));
    vm->handles().release(handle,
                          WasmScriptingVM::HandleTable::Tag::dataContext);
}

// --- rive_artboard_v1: host-owned artboard inputs ---------------------------

// Mirrors ScriptReffedArtboard: the host owns the instance, its default state
// machine, and the bound view model instance; module userdata share it by ref.
struct HostArtboard : public RefCnt<HostArtboard>
{
    File* file = nullptr;
    std::unique_ptr<ArtboardInstance> artboard;
    std::unique_ptr<StateMachineInstance> stateMachine;
    rcp<ViewModelInstance> viewModelInstance;
    rcp<DataContext> parentDataContext;

    HostArtboard(File* fileValue,
                 std::unique_ptr<ArtboardInstance>&& artboardInstance,
                 rcp<ViewModelInstance> boundInstance,
                 rcp<DataContext> parent) :
        file(fileValue),
        artboard(std::move(artboardInstance)),
        stateMachine(artboard->defaultStateMachine()),
        parentDataContext(std::move(parent))
    {
        // A scripted artboard is a root: nothing hosts it in another
        // artboard's focus tree, so it owns its FocusManager and builds its
        // own focus tree.
        artboard->buildFocusTree(artboard->ensureFocusManager(), nullptr);
        viewModelInstance = boundInstance != nullptr
                                ? std::move(boundInstance)
                                : file->createViewModelInstance(artboard.get());
        if (stateMachine != nullptr && viewModelInstance != nullptr)
        {
            if (parentDataContext != nullptr)
            {
                auto dataContext = make_rcp<DataContext>(viewModelInstance);
                dataContext->parent(parentDataContext);
                stateMachine->bindDataContext(dataContext);
            }
            else
            {
                stateMachine->bindViewModelInstance(viewModelInstance);
            }
        }
    }

    ~HostArtboard()
    {
        // State machine before artboard; its destructor touches the artboard.
        stateMachine = nullptr;
        artboard = nullptr;
    }

    bool advance(float seconds)
    {
        if (stateMachine != nullptr)
        {
            // Bound view models advance with the host frame, not here.
            return stateMachine->advanceAndApply(seconds, false);
        }
        return artboard->advance(seconds);
    }
};

HostArtboard* resolveArtboard(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return nullptr;
    }
    return static_cast<HostArtboard*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::artboard));
}

void artboardReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto host = resolveArtboard(vm, handle);
    if (vm == nullptr || host == nullptr)
    {
        return;
    }
    vm->handles().release(handle, WasmScriptingVM::HandleTable::Tag::artboard);
    host->unref();
}

uint32_t artboardAdvanceImpl(WasmScriptingVM* vm,
                             uint32_t handle,
                             float seconds)
{
    auto host = resolveArtboard(vm, handle);
    return host != nullptr && host->advance(seconds) ? 1 : 0;
}

void artboardDrawImpl(WasmScriptingVM* vm,
                      uint32_t handle,
                      uint32_t rendererHandle)
{
    auto host = resolveArtboard(vm, handle);
    if (vm == nullptr || host == nullptr)
    {
        return;
    }
    auto renderer = static_cast<Renderer*>(
        vm->handles().resolve(rendererHandle,
                              WasmScriptingVM::HandleTable::Tag::renderer));
    if (renderer != nullptr)
    {
        host->artboard->drawInternal(renderer);
    }
}

namespace
{
struct WasmDrawVisit
{
    WasmScriptingVM* vm;
    bool failed = false;
};

void wasmDrawVisitor(void* context, Drawable* drawable, Renderer* renderer)
{
    auto visit = static_cast<WasmDrawVisit*>(context);
    if (visit->failed)
    {
        drawable->draw(renderer);
        return;
    }
    auto& handles = visit->vm->handles();
    uint32_t handle =
        handles.mint(WasmScriptingVM::HandleTable::Tag::drawable, drawable);
    auto& saves = visit->vm->visitSaves();
    WasmScriptingVM::VisitSaves outer = saves;
    saves = {renderer, 0};
    visit->failed = !visit->vm->notifyDrawVisit(handle);
    if (visit->failed)
    {
        for (; saves.open > 0; saves.open--)
        {
            renderer->restore();
        }
    }
    saves = outer;
    handles.release(handle, WasmScriptingVM::HandleTable::Tag::drawable);
}

Drawable* resolveDrawable(WasmScriptingVM* vm, uint32_t handle)
{
    return vm == nullptr ? nullptr
                         : static_cast<Drawable*>(vm->handles().resolve(
                               handle,
                               WasmScriptingVM::HandleTable::Tag::drawable));
}

CustomProperty* resolveDrawableProperty(WasmScriptingVM* vm,
                                        uint32_t handle,
                                        uint32_t key)
{
    auto drawable = resolveDrawable(vm, handle);
    return drawable != nullptr ? drawable->customProperty(key) : nullptr;
}
} // namespace

void artboardDrawVisitImpl(WasmScriptingVM* vm,
                           uint32_t handle,
                           uint32_t rendererHandle)
{
    auto host = resolveArtboard(vm, handle);
    auto renderer = resolveRenderer(vm, rendererHandle);
    if (host == nullptr || renderer == nullptr)
    {
        return;
    }
    WasmDrawVisit visit = {vm};
    host->artboard->drawInternal(renderer, wasmDrawVisitor, &visit, host->file);
}

void artboardDrawModulatedImpl(WasmScriptingVM* vm,
                               uint32_t handle,
                               uint32_t rendererHandle,
                               uint32_t key)
{
    auto host = resolveArtboard(vm, handle);
    auto renderer = resolveRenderer(vm, rendererHandle);
    if (host != nullptr && renderer != nullptr)
    {
        host->artboard->drawModulated(renderer, key, host->file);
    }
}

void artboardDrawableDrawImpl(WasmScriptingVM* vm,
                              uint32_t handle,
                              uint32_t rendererHandle)
{
    auto drawable = resolveDrawable(vm, handle);
    auto renderer = resolveRenderer(vm, rendererHandle);
    if (drawable != nullptr && renderer != nullptr)
    {
        drawable->draw(renderer);
    }
}

uint32_t artboardPropertyKeyImpl(WasmScriptingVM* vm,
                                 uint32_t handle,
                                 const char* name,
                                 uint32_t length)
{
    auto host = resolveArtboard(vm, handle);
    return File::customPropertyKey(host != nullptr ? host->file : nullptr,
                                   name,
                                   length);
}

uint32_t artboardDrawableValueImpl(WasmScriptingVM* vm,
                                   uint32_t handle,
                                   uint32_t key,
                                   uint32_t* out,
                                   uint32_t outCount)
{
    auto property = resolveDrawableProperty(vm, handle, key);
    if (property == nullptr)
    {
        return 0;
    }
    CustomPropertyKind kind = property->kind();
    uint32_t bits = 0;
    switch (kind)
    {
        case CustomPropertyKind::number:
        {
            float value = property->as<CustomPropertyNumber>()->propertyValue();
            memcpy(&bits, &value, sizeof(bits));
            break;
        }
        case CustomPropertyKind::boolean:
            bits = property->as<CustomPropertyBoolean>()->propertyValue();
            break;
        case CustomPropertyKind::color:
            bits = property->as<CustomPropertyColor>()->propertyValue();
            break;
        default:
            // No reader takes the others' bits, the kind answers has().
            break;
    }
    if (outCount > 0)
    {
        out[0] = bits;
    }
    return (uint32_t)kind + 1;
}

uint32_t artboardDrawableStringImpl(WasmScriptingVM* vm,
                                    uint32_t handle,
                                    uint32_t key,
                                    char* out,
                                    uint32_t outCount)
{
    auto property = resolveDrawableProperty(vm, handle, key);
    if (property == nullptr || !property->is<CustomPropertyString>())
    {
        return ~0u;
    }
    return fillModuleString(
        vm,
        property->as<CustomPropertyString>()->propertyValue(),
        out,
        outCount);
}

#ifdef WITH_RIVE_TOOLS
uint32_t artboardDrawablePropertiesImpl(WasmScriptingVM* vm,
                                        uint32_t handle,
                                        char* out,
                                        uint32_t outCount)
{
    auto drawable = resolveDrawable(vm, handle);
    auto file =
        drawable != nullptr ? drawable->artboard()->drawVisitorFile() : nullptr;
    auto manifest = file != nullptr ? file->manifest() : nullptr;
    if (manifest == nullptr)
    {
        return 0;
    }
    std::string packed;
    for (auto child : drawable->children())
    {
        auto property = CustomProperty::tagging(child);
        if (property == nullptr)
        {
            continue;
        }
        packed.push_back((char)property->kind());
        packed.append(manifest->resolveName(property->nameId()));
        packed.push_back('\0');
    }
    memcpy(out, packed.data(), std::min<size_t>(packed.size(), outCount));
    return (uint32_t)packed.size();
}
#endif

uint32_t artboardInstanceImpl(WasmScriptingVM* vm,
                              uint32_t handle,
                              uint32_t vmiHandle)
{
    auto host = resolveArtboard(vm, handle);
    if (vm == nullptr || host == nullptr)
    {
        return 0;
    }
    auto clone = host->artboard->instance();
    clone->frameOrigin(false);
    auto vmi = vmiHandle != 0 ? resolveVmiArg(vm, vmiHandle) : nullptr;
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::artboard,
                              new HostArtboard(host->file,
                                               std::move(clone),
                                               std::move(vmi),
                                               host->parentDataContext));
}

uint32_t artboardDataImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto host = resolveArtboard(vm, handle);
    if (vm == nullptr || host == nullptr || host->viewModelInstance == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(
        WasmScriptingVM::HandleTable::Tag::viewModelInstance,
        new HostViewModelInstance{host->viewModelInstance});
}

float artboardWidthImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto host = resolveArtboard(vm, handle);
    return host != nullptr ? host->artboard->width() : 0.0f;
}

float artboardHeightImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto host = resolveArtboard(vm, handle);
    return host != nullptr ? host->artboard->height() : 0.0f;
}

void artboardSetWidthImpl(WasmScriptingVM* vm, uint32_t handle, float value)
{
    auto host = resolveArtboard(vm, handle);
    if (host != nullptr)
    {
        host->artboard->width(value);
    }
}

void artboardSetHeightImpl(WasmScriptingVM* vm, uint32_t handle, float value)
{
    auto host = resolveArtboard(vm, handle);
    if (host != nullptr)
    {
        host->artboard->height(value);
    }
}

uint32_t artboardFrameOriginImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto host = resolveArtboard(vm, handle);
    return host != nullptr && host->artboard->frameOrigin() ? 1 : 0;
}

void artboardSetFrameOriginImpl(WasmScriptingVM* vm,
                                uint32_t handle,
                                uint32_t value)
{
    auto host = resolveArtboard(vm, handle);
    if (host != nullptr)
    {
        host->artboard->frameOrigin(value != 0);
    }
}

void artboardBoundsImpl(WasmScriptingVM* vm,
                        uint32_t handle,
                        float* out,
                        uint32_t outCount)
{
    auto host = resolveArtboard(vm, handle);
    if (host == nullptr || outCount < 4)
    {
        return;
    }
    const AABB& bounds = host->artboard->bounds();
    out[0] = bounds.min().x;
    out[1] = bounds.min().y;
    out[2] = bounds.max().x;
    out[3] = bounds.max().y;
}

uint32_t artboardPointerEventImpl(WasmScriptingVM* vm,
                                  uint32_t handle,
                                  uint32_t kind,
                                  uint32_t pointerId,
                                  float x,
                                  float y)
{
    auto host = resolveArtboard(vm, handle);
    if (host == nullptr || host->stateMachine == nullptr)
    {
        return 0;
    }
    Vec2D position(x, y);
    switch (kind)
    {
        case ArtboardWire::pointerDown:
            return (uint32_t)(int)host->stateMachine->pointerDown(
                position,
                (uint8_t)pointerId);
        case ArtboardWire::pointerMove:
            return (uint32_t)(int)host->stateMachine->pointerMove(
                position,
                0,
                (uint8_t)pointerId);
        case ArtboardWire::pointerUp:
            return (uint32_t)(int)host->stateMachine->pointerUp(
                position,
                (uint8_t)pointerId);
        case ArtboardWire::pointerExit:
            return (uint32_t)(int)host->stateMachine->pointerExit(
                position,
                (uint8_t)pointerId);
    }
    return 0;
}

// Animations pin their artboard so the instance they play into outlives them.
struct HostAnimation
{
    rcp<HostArtboard> owner;
    std::unique_ptr<LinearAnimationInstance> animation;

    float duration() const
    {
        return (float)animation->duration() / (float)animation->fps();
    }
};

uint32_t artboardAnimationImpl(WasmScriptingVM* vm,
                               uint32_t handle,
                               const char* name,
                               uint32_t length)
{
    auto host = resolveArtboard(vm, handle);
    if (vm == nullptr || host == nullptr)
    {
        return 0;
    }
    auto animation = host->artboard->animationNamed(std::string(name, length));
    if (animation == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(
        WasmScriptingVM::HandleTable::Tag::animation,
        new HostAnimation{ref_rcp(host), std::move(animation)});
}

HostAnimation* resolveAnimation(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return nullptr;
    }
    return static_cast<HostAnimation*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::animation));
}

void artboardAnimationReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto host = resolveAnimation(vm, handle);
    if (vm == nullptr || host == nullptr)
    {
        return;
    }
    vm->handles().release(handle, WasmScriptingVM::HandleTable::Tag::animation);
    delete host;
}

float artboardAnimationDurationImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto host = resolveAnimation(vm, handle);
    return host != nullptr ? host->duration() : 0.0f;
}

uint32_t artboardAnimationAdvanceImpl(WasmScriptingVM* vm,
                                      uint32_t handle,
                                      float seconds)
{
    auto host = resolveAnimation(vm, handle);
    if (host == nullptr)
    {
        return 0;
    }
    bool advanced = host->animation->advance(seconds);
    host->animation->apply();
    return advanced ? 1 : 0;
}

void artboardAnimationSetTimeImpl(WasmScriptingVM* vm,
                                  uint32_t handle,
                                  float value,
                                  uint32_t mode)
{
    auto host = resolveAnimation(vm, handle);
    if (host == nullptr)
    {
        return;
    }
    float seconds = value;
    if (mode == ArtboardWire::timeFrames)
    {
        seconds = value / (float)host->animation->fps();
    }
    else if (mode == ArtboardWire::timePercentage)
    {
        seconds = value * host->duration();
    }
    host->animation->time(
        host->animation->animation()->globalToLocalSeconds(seconds));
    host->animation->apply();
}

// Nodes pin their artboard; the component pointer lives inside its instance.
// Without an owner the node borrows a path effect's shape for that update.
struct HostNode
{
    rcp<HostArtboard> owner;
    TransformComponent* component;
};

uint32_t mintNode(WasmScriptingVM* vm,
                  rcp<HostArtboard> owner,
                  TransformComponent* component)
{
    bool borrowed = owner == nullptr;
    uint32_t handle =
        vm->handles().mint(WasmScriptingVM::HandleTable::Tag::node,
                           new HostNode{std::move(owner), component});
    if (borrowed)
    {
        vm->scopedNodes().push_back(handle);
    }
    return handle;
}

uint32_t artboardNodeImpl(WasmScriptingVM* vm,
                          uint32_t handle,
                          const char* name,
                          uint32_t length)
{
    auto host = resolveArtboard(vm, handle);
    if (vm == nullptr || host == nullptr)
    {
        return 0;
    }
    auto component =
        host->artboard->find<TransformComponent>(std::string(name, length));
    if (component == nullptr)
    {
        return 0;
    }
    return mintNode(vm, ref_rcp(host), component);
}

void artboardAddToPathImpl(WasmScriptingVM* vm,
                           uint32_t handle,
                           uint32_t pathHandle,
                           float xx,
                           float xy,
                           float yx,
                           float yy,
                           float tx,
                           float ty)
{
    auto host = resolveArtboard(vm, handle);
    auto hostPath = resolvePath(vm, pathHandle);
    if (host == nullptr || hostPath == nullptr)
    {
        return;
    }
    Mat2D transform(xx, xy, yx, yy, tx, ty);
    host->artboard->addToRawPath(hostPath->raw, &transform);
    hostPath->renderDirty = true;
}

uint32_t artboardGamepadEventImpl(WasmScriptingVM* vm,
                                  uint32_t handle,
                                  const uint8_t* payload,
                                  uint32_t byteCount)
{
    auto host = resolveArtboard(vm, handle);
    GamepadWire wire;
    if (host == nullptr || host->stateMachine == nullptr ||
        !decodeGamepadWire(payload, byteCount, wire))
    {
        return 0;
    }
    auto invocation = decodeGamepadInvocation(wire, payload);
    if (!invocation)
    {
        return 0;
    }
    // The Luau artboard:gamepad* path: focus first, then every scripted
    // drawable that did not already take it.
    ScriptedDrawable* dispatched = nullptr;
    if (auto* focus = host->stateMachine->focusManager())
    {
        (void)focus->gamepadDispatch(*invocation, &dispatched);
    }
    return (uint32_t)host->stateMachine->broadcastGamepadToScriptedDrawables(
        *invocation,
        dispatched);
}

HostNode* resolveNode(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return nullptr;
    }
    return static_cast<HostNode*>(
        vm->handles().resolve(handle, WasmScriptingVM::HandleTable::Tag::node));
}

void artboardNodeReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto host = resolveNode(vm, handle);
    // A borrowed node is released when its path effect update returns.
    if (host == nullptr || host->owner == nullptr)
    {
        return;
    }
    vm->handles().release(handle, WasmScriptingVM::HandleTable::Tag::node);
    delete host;
}

void artboardNodeTransformImpl(WasmScriptingVM* vm,
                               uint32_t handle,
                               float* out,
                               uint32_t outCount)
{
    if (outCount < 5)
    {
        return;
    }
    auto host = resolveNode(vm, handle);
    // A stale node reads as the identity, the same on every lane.
    if (host == nullptr)
    {
        const float identity[5] = {0, 0, 0, 1, 1};
        memcpy(out, identity, sizeof(identity));
        return;
    }
    out[0] = host->component->x();
    out[1] = host->component->y();
    out[2] = host->component->rotation();
    out[3] = host->component->scaleX();
    out[4] = host->component->scaleY();
}

// x/y writes only land on Node and RootBone, like the Luau lane.
void nodeSetX(TransformComponent* component, float value)
{
    if (component->is<Node>())
    {
        component->as<Node>()->x(value);
    }
    else if (component->is<RootBone>())
    {
        component->as<RootBone>()->x(value);
    }
}

void nodeSetY(TransformComponent* component, float value)
{
    if (component->is<Node>())
    {
        component->as<Node>()->y(value);
    }
    else if (component->is<RootBone>())
    {
        component->as<RootBone>()->y(value);
    }
}

void artboardNodeSetImpl(WasmScriptingVM* vm,
                         uint32_t handle,
                         uint32_t field,
                         float v0,
                         float v1)
{
    auto host = resolveNode(vm, handle);
    if (host == nullptr)
    {
        return;
    }
    TransformComponent* component = host->component;
    switch (field)
    {
        case ArtboardWire::nodeX:
            nodeSetX(component, v0);
            break;
        case ArtboardWire::nodeY:
            nodeSetY(component, v0);
            break;
        case ArtboardWire::nodeRotation:
            component->rotation(v0);
            break;
        case ArtboardWire::nodeScaleX:
            component->scaleX(v0);
            break;
        case ArtboardWire::nodeScaleY:
            component->scaleY(v0);
            break;
        case ArtboardWire::nodePosition:
            nodeSetX(component, v0);
            nodeSetY(component, v1);
            break;
        case ArtboardWire::nodeScale:
            component->scaleX(v0);
            component->scaleY(v1);
            break;
    }
}

void artboardNodeWorldTransformImpl(WasmScriptingVM* vm,
                                    uint32_t handle,
                                    float* out,
                                    uint32_t outCount)
{
    if (outCount < 6)
    {
        return;
    }
    auto host = resolveNode(vm, handle);
    Mat2D world = host != nullptr ? host->component->worldTransform() : Mat2D();
    for (int i = 0; i < 6; i++)
    {
        out[i] = world[i];
    }
}

void artboardNodeSetWorldTransformImpl(WasmScriptingVM* vm,
                                       uint32_t handle,
                                       const float* values,
                                       uint32_t floatCount)
{
    auto host = resolveNode(vm, handle);
    if (host == nullptr || floatCount < 6)
    {
        return;
    }
    host->component->mutableWorldTransform() =
        Mat2D(values[0], values[1], values[2], values[3], values[4], values[5]);
}

void artboardNodeDecomposeImpl(WasmScriptingVM* vm,
                               uint32_t handle,
                               const float* values,
                               uint32_t floatCount)
{
    auto host = resolveNode(vm, handle);
    if (host == nullptr || floatCount < 6)
    {
        return;
    }
    TransformComponent* component = host->component;
    Mat2D world =
        getParentWorld(*component).invertOrIdentity() *
        Mat2D(values[0], values[1], values[2], values[3], values[4], values[5]);
    TransformComponents components = world.decompose();
    nodeSetX(component, components.x());
    nodeSetY(component, components.y());
    component->scaleX(components.scaleX());
    component->scaleY(components.scaleY());
    component->rotation(components.rotation());
}

uint32_t artboardNodePathVerbsImpl(WasmScriptingVM* vm,
                                   uint32_t handle,
                                   uint8_t* out,
                                   uint32_t outCount)
{
    auto host = resolveNode(vm, handle);
    if (host == nullptr || !host->component->is<Path>())
    {
        return ~0u;
    }
    return fillOut(pathVerbBytes(host->component->as<Path>()->rawPath()),
                   out,
                   outCount);
}

uint32_t artboardNodePathPointsImpl(WasmScriptingVM* vm,
                                    uint32_t handle,
                                    float* out,
                                    uint32_t outCount)
{
    auto host = resolveNode(vm, handle);
    if (host == nullptr || !host->component->is<Path>())
    {
        return ~0u;
    }
    return fillOut(pathFloats(host->component->as<Path>()->rawPath()),
                   out,
                   outCount);
}

uint32_t artboardNodePaintImpl(WasmScriptingVM* vm,
                               uint32_t handle,
                               uint32_t* out,
                               uint32_t outCount)
{
    auto host = resolveNode(vm, handle);
    if (host == nullptr || !host->component->is<ShapePaint>() || outCount < 7)
    {
        return 0;
    }
    // The same snapshot ScriptedPaintData(const ShapePaint*) takes.
    auto shapePaint = host->component->as<ShapePaint>();
    uint32_t style = (uint32_t)RenderPaintStyle::fill;
    uint32_t join = (uint32_t)StrokeJoin::miter;
    uint32_t cap = (uint32_t)StrokeCap::butt;
    float thickness = 1.0f;
    float featherStrength = 0.0f;
    uint32_t color = 0xFF000000;
    if (shapePaint->is<Stroke>())
    {
        auto stroke = shapePaint->as<Stroke>();
        style = (uint32_t)RenderPaintStyle::stroke;
        thickness = stroke->thickness();
        cap = stroke->cap();
        join = stroke->join();
    }
    for (auto& child : shapePaint->children())
    {
        if (child->is<SolidColor>())
        {
            color = child->as<SolidColor>()->colorValue();
            break;
        }
    }
    if (shapePaint->feather() != nullptr)
    {
        featherStrength = shapePaint->feather()->strength();
    }
    out[0] = style;
    out[1] = join;
    out[2] = cap;
    out[3] = (uint32_t)shapePaint->blendModeValue();
    out[4] = color;
    memcpy(&out[5], &thickness, sizeof(float));
    memcpy(&out[6], &featherStrength, sizeof(float));
    return 1;
}

uint32_t artboardNodeChildrenImpl(WasmScriptingVM* vm,
                                  uint32_t handle,
                                  uint32_t* out,
                                  uint32_t outCount)
{
    auto host = resolveNode(vm, handle);
    if (host == nullptr || !host->component->is<ContainerComponent>())
    {
        return 0;
    }
    auto& children = host->component->as<ContainerComponent>()->children();
    uint32_t count = 0;
    for (auto child : children)
    {
        if (child->is<TransformComponent>())
        {
            count++;
        }
    }
    if (count > outCount)
    {
        return count;
    }
    uint32_t index = 0;
    for (auto child : children)
    {
        if (child->is<TransformComponent>())
        {
            out[index++] =
                mintNode(vm, host->owner, child->as<TransformComponent>());
        }
    }
    return count;
}

uint32_t artboardNodeParentImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto host = resolveNode(vm, handle);
    if (host == nullptr)
    {
        return 0;
    }
    auto parent = host->component->parent();
    if (parent == nullptr || !parent->is<TransformComponent>())
    {
        return 0;
    }
    return mintNode(vm, host->owner, parent->as<TransformComponent>());
}

// --- rive_data_v1 asset properties (image/font/blob) ------------------------

struct HostFont
{
    rcp<Font> font;
#ifdef WITH_RIVE_TEXT
    RawPath glyphPath;
    uint32_t glyphPathId = 0;
    bool hasGlyphPath = false;
#endif
};

uint32_t dataVmiImageImpl(WasmScriptingVM* vm,
                          uint32_t vmiHandle,
                          const char* name,
                          uint32_t length)
{
    return mintInstanceValue<ViewModelInstanceAssetImage>(vm,
                                                          vmiHandle,
                                                          name,
                                                          length);
}

uint32_t dataVmiFontImpl(WasmScriptingVM* vm,
                         uint32_t vmiHandle,
                         const char* name,
                         uint32_t length)
{
    return mintInstanceValue<ViewModelInstanceAssetFont>(vm,
                                                         vmiHandle,
                                                         name,
                                                         length);
}

uint32_t dataVmiBlobImpl(WasmScriptingVM* vm,
                         uint32_t vmiHandle,
                         const char* name,
                         uint32_t length)
{
    return mintInstanceValue<ViewModelInstanceAssetBlob>(vm,
                                                         vmiHandle,
                                                         name,
                                                         length);
}

// Mirrors ScriptedPropertyImage::pushValue: the instance's embedded asset
// first, else the id-bound asset through the file registry.
uint32_t dataImageGetImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto value = resolveInstanceValue<ViewModelInstanceAssetImage>(vm, handle);
    if (vm == nullptr || value == nullptr)
    {
        return 0;
    }
    RenderImage* renderImage = nullptr;
    if (auto asset = value->asset())
    {
        renderImage = asset->renderImage();
    }
    if (renderImage == nullptr && vm->file() != nullptr)
    {
        auto fileAsset = vm->file()->asset(value->propertyValue());
        if (fileAsset != nullptr && fileAsset->is<ImageAsset>())
        {
            renderImage = fileAsset->as<ImageAsset>()->renderImage();
        }
    }
    if (renderImage == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::image,
                              new HostImage{ref_rcp(renderImage)});
}

void dataImageSetImpl(WasmScriptingVM* vm,
                      uint32_t handle,
                      uint32_t imageHandle)
{
    auto value = resolveInstanceValue<ViewModelInstanceAssetImage>(vm, handle);
    if (value == nullptr)
    {
        return;
    }
    auto host = resolveImage(vm, imageHandle);
    value->value(host != nullptr ? host->image.get() : nullptr);
}

uint32_t dataFontGetImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto value = resolveInstanceValue<ViewModelInstanceAssetFont>(vm, handle);
    if (vm == nullptr || value == nullptr)
    {
        return 0;
    }
    rcp<Font> font;
    if (auto asset = value->asset())
    {
        font = asset->font();
    }
    if (font == nullptr && vm->file() != nullptr)
    {
        auto fileAsset = vm->file()->asset(value->propertyValue());
        if (fileAsset != nullptr && fileAsset->is<FontAsset>())
        {
            font = fileAsset->as<FontAsset>()->font();
        }
    }
    if (font == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::font,
                              new HostFont{std::move(font)});
}

void dataFontSetImpl(WasmScriptingVM* vm, uint32_t handle, uint32_t fontHandle)
{
    auto value = resolveInstanceValue<ViewModelInstanceAssetFont>(vm, handle);
    if (vm == nullptr || value == nullptr)
    {
        return;
    }
    auto host = static_cast<HostFont*>(
        vm->handles().resolve(fontHandle,
                              WasmScriptingVM::HandleTable::Tag::font));
    value->value(host != nullptr ? host->font.get() : nullptr);
}

HostFont* resolveFont(WasmScriptingVM* vm, uint32_t handle);

void dataFontReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    // A font the teardown sweep already freed resolves to nothing here.
    auto host = resolveFont(vm, handle);
    if (host == nullptr)
    {
        return;
    }
    delete host;
    vm->handles().release(handle, WasmScriptingVM::HandleTable::Tag::font);
}

// --- rive_font_v1 / rive_text_v1
// ----------------------------------------------

HostFont* resolveFont(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return nullptr;
    }
    return static_cast<HostFont*>(
        vm->handles().resolve(handle, WasmScriptingVM::HandleTable::Tag::font));
}

uint32_t mintFont(WasmScriptingVM* vm, rcp<Font> font)
{
    if (font == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::font,
                              new HostFont{std::move(font)});
}

uint32_t floatBits(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

float bitsFloat(uint32_t bits)
{
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

uint32_t fontFromAssetImpl(WasmScriptingVM* vm,
                           uint32_t objectHandle,
                           const char* name,
                           uint32_t length)
{
    auto asset = findFileAsset<FontAsset>(vm, objectHandle, name, length);
    return asset == nullptr ? 0 : mintFont(vm, asset->font());
}

void fontReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    dataFontReleaseImpl(vm, handle);
}

void fontMetricsImpl(WasmScriptingVM* vm,
                     uint32_t handle,
                     float* out,
                     uint32_t outCount)
{
    auto host = resolveFont(vm, handle);
    if (host == nullptr || out == nullptr || outCount < 4)
    {
        return;
    }
    const Font::LineMetrics& metrics = host->font->lineMetrics();
    out[0] = metrics.ascent;
    out[1] = metrics.descent;
    out[2] = metrics.capHeight;
    out[3] = metrics.xHeight;
}

uint32_t fontWeightImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto host = resolveFont(vm, handle);
    return host == nullptr ? 0 : host->font->getWeight();
}

uint32_t fontIsItalicImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto host = resolveFont(vm, handle);
    return host != nullptr && host->font->isItalic() ? 1 : 0;
}

uint32_t fontAxisCountImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto host = resolveFont(vm, handle);
    return host == nullptr ? 0 : host->font->getAxisCount();
}

void fontAxisImpl(WasmScriptingVM* vm,
                  uint32_t handle,
                  uint32_t index,
                  float* out,
                  uint32_t outCount)
{
    auto host = resolveFont(vm, handle);
    if (host == nullptr || out == nullptr || outCount < 4 ||
        index >= host->font->getAxisCount())
    {
        return;
    }
    Font::Axis axis = host->font->getAxis((uint16_t)index);
    out[0] = bitsFloat(axis.tag);
    out[1] = axis.min;
    out[2] = axis.def;
    out[3] = axis.max;
}

float fontAxisValueImpl(WasmScriptingVM* vm, uint32_t handle, uint32_t tag)
{
    auto host = resolveFont(vm, handle);
    return host == nullptr ? 0.0f : host->font->getAxisValue(tag);
}

uint32_t fontFeaturesImpl(WasmScriptingVM* vm,
                          uint32_t handle,
                          uint32_t* out,
                          uint32_t outCount)
{
    auto host = resolveFont(vm, handle);
    if (host == nullptr)
    {
        return 0;
    }
    SimpleArray<uint32_t> features = host->font->features();
    return fillOut(Span<const uint32_t>(features.data(), features.size()),
                   out,
                   outCount);
}

uint32_t fontHasGlyphImpl(WasmScriptingVM* vm,
                          uint32_t handle,
                          uint32_t codepoint)
{
    auto host = resolveFont(vm, handle);
    return host != nullptr && host->font->hasGlyph(codepoint) ? 1 : 0;
}

uint32_t fontWithOptionsImpl(WasmScriptingVM* vm,
                             uint32_t handle,
                             const uint32_t* coords,
                             uint32_t coordCount,
                             const uint32_t* features,
                             uint32_t featureCount)
{
    auto host = resolveFont(vm, handle);
    if (host == nullptr)
    {
        return 0;
    }
    std::vector<Font::Coord> axes;
    for (uint32_t i = 0; i + 1 < coordCount; i += 2)
    {
        axes.push_back({coords[i], bitsFloat(coords[i + 1])});
    }
    std::vector<Font::Feature> settings;
    for (uint32_t i = 0; i + 1 < featureCount; i += 2)
    {
        settings.push_back({features[i], features[i + 1]});
    }
    return mintFont(vm, host->font->withOptions(axes, settings));
}

#ifdef WITH_RIVE_TEXT
uint32_t fontDecodeImpl(WasmScriptingVM* vm,
                        const uint8_t* bytes,
                        uint32_t byteCount)
{
    if (vm == nullptr || bytes == nullptr)
    {
        return 0;
    }
    return mintFont(vm, HBFont::Decode(Span<const uint8_t>(bytes, byteCount)));
}

// The outline crosses in two calls, so the last one is kept for the second.
const RawPath& fontGlyph(HostFont* host, uint32_t glyph)
{
    if (!host->hasGlyphPath || host->glyphPathId != glyph)
    {
        host->glyphPath = RawText::glyphPath(*host->font, (GlyphID)glyph);
        host->glyphPathId = glyph;
        host->hasGlyphPath = true;
    }
    return host->glyphPath;
}

uint32_t fontGlyphVerbsImpl(WasmScriptingVM* vm,
                            uint32_t handle,
                            uint32_t glyph,
                            uint8_t* out,
                            uint32_t outCount)
{
    auto host = resolveFont(vm, handle);
    if (host == nullptr)
    {
        return 0;
    }
    return fillOut(pathVerbBytes(fontGlyph(host, glyph)), out, outCount);
}

uint32_t fontGlyphPointsImpl(WasmScriptingVM* vm,
                             uint32_t handle,
                             uint32_t glyph,
                             float* out,
                             uint32_t outCount)
{
    auto host = resolveFont(vm, handle);
    if (host == nullptr)
    {
        return 0;
    }
    return fillOut(pathFloats(fontGlyph(host, glyph)), out, outCount);
}

// A Text builtin: the layout plus the flat records the module reads it
// through, rebuilt after any change.
struct HostText
{
    HostText(Factory* factory) : text(factory) {}
    RawText text;
    std::vector<const GlyphRun*> runs;
    std::vector<uint32_t> lineWords;
    std::vector<uint32_t> runWords;
    std::vector<uint32_t> glyphWords;
    // Fonts the layout produced, fallbacks included, handed to the module as
    // handles this text owns.
    std::unordered_map<const Font*, uint32_t> fontHandles;
    // RawText revision the records were built from; ~0u before the first.
    uint32_t snapshotRevision = ~0u;
};

HostText* resolveText(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return nullptr;
    }
    return static_cast<HostText*>(
        vm->handles().resolve(handle, WasmScriptingVM::HandleTable::Tag::text));
}

void deleteHostText(WasmScriptingVM* vm, HostText* host)
{
    if (host == nullptr)
    {
        return;
    }
    for (auto& pair : host->fontHandles)
    {
        dataFontReleaseImpl(vm, pair.second);
    }
    delete host;
}

void buildTextSnapshot(WasmScriptingVM* vm, HostText* host)
{
    if (host->snapshotRevision == host->text.revision())
    {
        return;
    }
    host->snapshotRevision = host->text.revision();
    host->runs.clear();
    host->lineWords.clear();
    host->runWords.clear();
    host->glyphWords.clear();

    RawText& text = host->text;
    const GlyphLookup& lookup = text.layoutView().glyphLookup();
    std::unordered_map<const GlyphRun*, uint32_t> runIndices;
    uint32_t glyphCount = 0;
    for (uint32_t line = 0; line < text.lineCount(); line++)
    {
        const OrderedLine& orderedLine = text.orderedLines()[line];
        uint32_t glyphStart = glyphCount;
        text.forEachGlyph(
            line,
            [&](const GlyphRun& run, uint32_t glyphIndex, Vec2D position) {
                auto indexed =
                    runIndices.emplace(&run, (uint32_t)host->runs.size());
                if (indexed.second)
                {
                    host->runs.push_back(&run);
                }
                glyphCount++;
                GlyphID glyphId = run.glyphs[glyphIndex];
                uint32_t flags =
                    (run.level & 1) | (run.font->isColorGlyph(glyphId) ? 2 : 0);
                host->glyphWords.insert(host->glyphWords.end(),
                                        {glyphId,
                                         run.textIndices[glyphIndex],
                                         indexed.first->second,
                                         flags,
                                         floatBits(position.x),
                                         floatBits(position.y),
                                         floatBits(run.advances[glyphIndex])});
                return true;
            });
        host->lineWords.insert(host->lineWords.end(),
                               {glyphStart,
                                glyphCount - glyphStart,
                                (uint32_t)text.lineDirection(line),
                                orderedLine.firstCodePointIndex(lookup),
                                orderedLine.lastCodePointIndex(lookup),
                                floatBits(orderedLine.glyphLine().startX),
                                floatBits(text.lineTop(line)),
                                floatBits(orderedLine.y()),
                                floatBits(orderedLine.bottom())});
    }
    for (const GlyphRun* run : host->runs)
    {
        auto found = host->fontHandles.find(run->font.get());
        if (found == host->fontHandles.end())
        {
            found = host->fontHandles
                        .emplace(run->font.get(), mintFont(vm, run->font))
                        .first;
        }
        host->runWords.insert(host->runWords.end(),
                              {found->second,
                               (uint32_t)(run->level & 1),
                               floatBits(run->size),
                               floatBits(run->lineHeight),
                               floatBits(run->letterSpacing)});
    }
}

uint32_t textNewImpl(WasmScriptingVM* vm)
{
    if (vm == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::text,
                              new HostText(vm->factory()));
}

void textReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    deleteHostText(vm, resolveText(vm, handle));
    if (vm != nullptr)
    {
        vm->handles().release(handle, WasmScriptingVM::HandleTable::Tag::text);
    }
}

void textAppendImpl(WasmScriptingVM* vm,
                    uint32_t handle,
                    const char* chars,
                    uint32_t length,
                    uint32_t fontHandle,
                    uint32_t paintHandle,
                    float size,
                    float lineHeight,
                    float letterSpacing,
                    uint32_t foreground)
{
    auto host = resolveText(vm, handle);
    auto font = resolveFont(vm, fontHandle);
    if (host == nullptr || font == nullptr)
    {
        return;
    }
    RenderPaint* paint = resolvePaint(vm, paintHandle);
    host->text.append(std::string(chars, length),
                      paint != nullptr ? ref_rcp(paint) : nullptr,
                      font->font,
                      size,
                      lineHeight,
                      letterSpacing,
                      foreground);
}

void textClearImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (auto host = resolveText(vm, handle))
    {
        host->text.clear();
    }
}

void textLayoutImpl(WasmScriptingVM* vm,
                    uint32_t handle,
                    const rive_text_layout_desc_v1* desc,
                    uint32_t descByteCount)
{
    auto host = resolveText(vm, handle);
    if (host == nullptr || desc == nullptr ||
        descByteCount < sizeof(rive_text_layout_desc_v1))
    {
        return;
    }
    RawText& text = host->text;
    text.sizingIndex((int)desc->sizing);
    text.overflowIndex((int)desc->overflow);
    text.alignIndex((int)desc->align);
    text.wrapIndex((int)desc->wrap);
    text.wordBreakIndex((int)desc->wordBreak);
    text.originIndex((int)desc->origin);
    text.directionIndex((int)desc->direction);
    text.maxWidth(desc->maxWidth);
    text.maxHeight(desc->maxHeight);
    text.paragraphSpacing(desc->paragraphSpacing);
}

void textDrawImpl(WasmScriptingVM* vm,
                  uint32_t handle,
                  uint32_t rendererHandle,
                  uint32_t paintHandle)
{
    auto host = resolveText(vm, handle);
    auto renderer = resolveRenderer(vm, rendererHandle);
    if (host == nullptr || renderer == nullptr)
    {
        return;
    }
    RenderPaint* paint = resolvePaint(vm, paintHandle);
    host->text.render(renderer, paint != nullptr ? ref_rcp(paint) : nullptr);
}

void textBoundsImpl(WasmScriptingVM* vm,
                    uint32_t handle,
                    float* out,
                    uint32_t outCount)
{
    auto host = resolveText(vm, handle);
    if (host == nullptr || out == nullptr || outCount < 4)
    {
        return;
    }
    AABB bounds = host->text.bounds();
    out[0] = bounds.minX;
    out[1] = bounds.minY;
    out[2] = bounds.maxX;
    out[3] = bounds.maxY;
}

uint32_t textLengthImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto host = resolveText(vm, handle);
    return host == nullptr ? 0 : (uint32_t)host->text.length();
}

uint32_t textLinesImpl(WasmScriptingVM* vm,
                       uint32_t handle,
                       uint32_t* out,
                       uint32_t outCount)
{
    auto host = resolveText(vm, handle);
    if (host == nullptr)
    {
        return 0;
    }
    buildTextSnapshot(vm, host);
    return fillOut(host->lineWords, out, outCount);
}

uint32_t textRunsImpl(WasmScriptingVM* vm,
                      uint32_t handle,
                      uint32_t* out,
                      uint32_t outCount)
{
    auto host = resolveText(vm, handle);
    if (host == nullptr)
    {
        return 0;
    }
    buildTextSnapshot(vm, host);
    return fillOut(host->runWords, out, outCount);
}

uint32_t textGlyphsImpl(WasmScriptingVM* vm,
                        uint32_t handle,
                        uint32_t* out,
                        uint32_t outCount)
{
    auto host = resolveText(vm, handle);
    if (host == nullptr)
    {
        return 0;
    }
    buildTextSnapshot(vm, host);
    return fillOut(host->glyphWords, out, outCount);
}

uint32_t textHitTestImpl(WasmScriptingVM* vm, uint32_t handle, float x, float y)
{
    auto host = resolveText(vm, handle);
    if (host == nullptr)
    {
        return 0;
    }
    TextLayoutView view = host->text.layoutView();
    return CursorPosition::fromTranslation(Vec2D(x, y), view).codePointIndex();
}

uint32_t textCaretImpl(WasmScriptingVM* vm,
                       uint32_t handle,
                       uint32_t index,
                       float* out,
                       uint32_t outCount)
{
    auto host = resolveText(vm, handle);
    if (host == nullptr || out == nullptr || outCount < 3)
    {
        return 0;
    }
    TextLayoutView view = host->text.layoutView();
    auto visual = CursorPosition::atIndex(index, view).visualPosition(view);
    if (!visual.found())
    {
        return 0;
    }
    out[0] = visual.x();
    out[1] = visual.top();
    out[2] = visual.bottom();
    return 1;
}

uint32_t textSelectionRectsImpl(WasmScriptingVM* vm,
                                uint32_t handle,
                                uint32_t from,
                                uint32_t to,
                                float* out,
                                uint32_t outCount)
{
    auto host = resolveText(vm, handle);
    if (host == nullptr)
    {
        return 0;
    }
    TextLayoutView view = host->text.layoutView();
    Cursor cursor(CursorPosition::atIndex(from, view),
                  CursorPosition::atIndex(to, view));
    std::vector<AABB> rects;
    cursor.selectionRects(rects, view);
    std::vector<float> words;
    for (const AABB& rect : rects)
    {
        words.insert(words.end(), {rect.minX, rect.minY, rect.maxX, rect.maxY});
    }
    return fillOut(words, out, outCount);
}
#else
struct HostText
{};
void deleteHostText(WasmScriptingVM*, HostText*) {}
#endif

// Mirrors ScriptedPropertyBlob::pushValue: a non-null instance asset (even
// empty) wins, else the id-bound asset through the file registry.
rcp<FileAsset> resolveBlobAsset(WasmScriptingVM* vm, uint32_t handle)
{
    auto value = resolveInstanceValue<ViewModelInstanceAssetBlob>(vm, handle);
    if (vm == nullptr || value == nullptr)
    {
        return nullptr;
    }
    if (auto asset = value->asset())
    {
        return asset;
    }
    if (vm->file() != nullptr)
    {
        auto fileAsset = vm->file()->asset(value->propertyValue());
        if (fileAsset != nullptr && fileAsset->is<BlobAsset>())
        {
            return fileAsset;
        }
    }
    return nullptr;
}

uint32_t blobAssetBytesImpl(WasmScriptingVM* vm,
                            uint32_t objectHandle,
                            const char* name,
                            uint32_t nameLength,
                            uint8_t* out,
                            uint32_t outCount)
{
    auto asset = findFileAsset<BlobAsset>(
        vm,
        objectHandle,
        name,
        nameLength,
        [](BlobAsset* blob) { return !blob->bytes().empty(); });
    if (asset == nullptr)
    {
        return 0;
    }
    auto bytes = asset->bytes();
    if (bytes.size() <= outCount)
    {
        memcpy(out, bytes.data(), bytes.size());
    }
    return (uint32_t)bytes.size();
}

// --- rive_file_v1 and artboard properties: context:decodeFile ---------------
//
// The Luau lane's decodeFile, RiveFile, BindableArtboard and
// Property<BindableArtboard> (lua_rive_file.cpp, ScriptedPropertyArtboard),
// with the file and its bindables held host side. Like the Luau lane they
// exist only where fetch() does; elsewhere every op answers none.

struct HostRiveFile
{
    rcp<File> file;
};

using HostBindableArtboard = DecodedBindable;

HostRiveFile* resolveRiveFile(WasmScriptingVM* vm, uint32_t handle)
{
    return vm == nullptr ? nullptr
                         : static_cast<HostRiveFile*>(vm->handles().resolve(
                               handle,
                               WasmScriptingVM::HandleTable::Tag::riveFile));
}

HostBindableArtboard* resolveBindableArtboard(WasmScriptingVM* vm,
                                              uint32_t handle)
{
    return vm == nullptr
               ? nullptr
               : static_cast<HostBindableArtboard*>(vm->handles().resolve(
                     handle,
                     WasmScriptingVM::HandleTable::Tag::bindableArtboard));
}

uint32_t fileDecodeImpl(WasmScriptingVM* vm,
                        const uint8_t* bytes,
                        uint32_t byteCount,
                        uint32_t* statusOut,
                        uint32_t statusCount)
{
    auto status = [&](uint32_t value) {
        if (statusCount > 0)
        {
            statusOut[0] = value;
        }
    };
#ifdef WITH_RIVE_SCRIPTNET
    if (vm == nullptr || vm->factory() == nullptr)
    {
        status(FileDecodeWire::statusNoFactory);
        return 0;
    }
    ImportResult result = ImportResult::malformed;
    auto file = byteCount == 0
                    ? nullptr
                    : importDecodedFile(Span<const uint8_t>(bytes, byteCount),
                                        vm->factory(),
                                        &result);
    if (file == nullptr)
    {
        status(result == ImportResult::unsupportedVersion
                   ? FileDecodeWire::statusUnsupportedVersion
                   : FileDecodeWire::statusMalformed);
        return 0;
    }
    status(FileDecodeWire::statusOk);
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::riveFile,
                              new HostRiveFile{std::move(file)});
#else
    (void)vm;
    (void)bytes;
    (void)byteCount;
    status(FileDecodeWire::statusUnavailable);
    return 0;
#endif
}

void fileReleaseImpl(WasmScriptingVM* vm, uint32_t file)
{
    delete resolveRiveFile(vm, file);
    if (vm != nullptr)
    {
        vm->handles().release(file,
                              WasmScriptingVM::HandleTable::Tag::riveFile);
    }
}

uint32_t fileArtboardCountImpl(WasmScriptingVM* vm, uint32_t file)
{
    auto host = resolveRiveFile(vm, file);
    return host == nullptr ? 0 : (uint32_t)host->file->artboardCount();
}

// One name at a time: names are file data, so no separator is safe.
uint32_t fileArtboardNameImpl(WasmScriptingVM* vm,
                              uint32_t file,
                              uint32_t index,
                              char* buffer,
                              uint32_t capacity)
{
    auto host = resolveRiveFile(vm, file);
    if (host == nullptr || index >= host->file->artboardCount())
    {
        return 0;
    }
    return fillModuleString(vm,
                            host->file->artboard(index)->name(),
                            buffer,
                            capacity);
}

uint32_t fileBindableImpl(WasmScriptingVM* vm,
                          uint32_t file,
                          const char* name,
                          uint32_t nameLength,
                          uint32_t useDefault)
{
    auto host = resolveRiveFile(vm, file);
    if (host == nullptr)
    {
        return 0;
    }
    std::string named(name, nameLength);
    DecodedBindable decoded =
        decodedBindable(*host->file, useDefault != 0 ? nullptr : named.c_str());
    if (decoded.artboard == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(
        WasmScriptingVM::HandleTable::Tag::bindableArtboard,
        new HostBindableArtboard(std::move(decoded)));
}

void fileBindableReleaseImpl(WasmScriptingVM* vm, uint32_t bindable)
{
    delete resolveBindableArtboard(vm, bindable);
    if (vm != nullptr)
    {
        vm->handles().release(
            bindable,
            WasmScriptingVM::HandleTable::Tag::bindableArtboard);
    }
}

uint32_t fileBindableNameImpl(WasmScriptingVM* vm,
                              uint32_t bindable,
                              char* buffer,
                              uint32_t capacity)
{
    auto host = resolveBindableArtboard(vm, bindable);
    if (host == nullptr)
    {
        return 0;
    }
    return fillModuleString(vm,
                            host->artboard->artboard()->name(),
                            buffer,
                            capacity);
}

uint32_t fileBindableDataImpl(WasmScriptingVM* vm, uint32_t bindable)
{
    auto host = resolveBindableArtboard(vm, bindable);
    if (host == nullptr || host->viewModel == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(
        WasmScriptingVM::HandleTable::Tag::viewModelInstance,
        new HostViewModelInstance{host->viewModel});
}

uint32_t fileBindableEqualImpl(WasmScriptingVM* vm, uint32_t a, uint32_t b)
{
    auto first = resolveBindableArtboard(vm, a);
    auto second = resolveBindableArtboard(vm, b);
    return first != nullptr && second != nullptr &&
                   first->artboard == second->artboard
               ? 1
               : 0;
}

uint32_t dataVmiArtboardImpl(WasmScriptingVM* vm,
                             uint32_t vmiHandle,
                             const char* name,
                             uint32_t length)
{
#ifdef WITH_RIVE_SCRIPTNET
    return mintInstanceValue<ViewModelInstanceArtboard>(vm,
                                                        vmiHandle,
                                                        name,
                                                        length);
#else
    (void)vm;
    (void)vmiHandle;
    (void)name;
    (void)length;
    return 0;
#endif
}

// Mirrors ScriptedPropertyArtboard::pushValue: nil while the property names
// one of the host file's own artboards.
uint32_t dataArtboardGetImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto value = resolveInstanceValue<ViewModelInstanceArtboard>(vm, handle);
    if (value == nullptr || value->asset() == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(
        WasmScriptingVM::HandleTable::Tag::bindableArtboard,
        new HostBindableArtboard{value->asset(),
                                 value->boundViewModelInstance()});
}

// Mirrors ScriptedPropertyArtboard::setValue: the artboard's own view model
// goes in first, since asset() notifies and whatever mounts the artboard
// binds it to that instance.
void dataArtboardSetImpl(WasmScriptingVM* vm,
                         uint32_t handle,
                         uint32_t bindable)
{
    auto value = resolveInstanceValue<ViewModelInstanceArtboard>(vm, handle);
    if (value == nullptr)
    {
        return;
    }
    auto host = resolveBindableArtboard(vm, bindable);
    value->boundViewModelInstance(host != nullptr ? host->viewModel : nullptr);
    value->asset(host != nullptr ? host->artboard : nullptr);
}

// --- rive_net_v1: fetch() --------------------------------------------------

uint32_t netFetchImpl(WasmScriptingVM* vm,
                      const uint8_t* request,
                      uint32_t requestCount,
                      uint32_t token)
{
    if (vm == nullptr)
    {
        return 0;
    }
    return vm->startFetch(Span<const uint8_t>(request, requestCount), token)
               ? 1
               : 0;
}

void netFetchCancelImpl(WasmScriptingVM* vm, uint32_t token)
{
    if (vm != nullptr)
    {
        vm->cancelFetch(token);
    }
}

// --- rive_audio_v1 ---------------------------------------------------------

#ifdef WITH_RIVE_AUDIO
struct HostAudioSource
{
    rcp<AudioSource> source;
};

struct HostAudioSound
{
    rcp<AudioSound> sound;
};

HostAudioSource* resolveAudioSource(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return nullptr;
    }
    return static_cast<HostAudioSource*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::audioSource));
}

HostAudioSound* resolveAudioSound(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return nullptr;
    }
    return static_cast<HostAudioSound*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::audioSound));
}

// The Luau play and playFrame shapes: relative times offset from the
// engine clock, the sound minted at the artboard-free volume of 1. Tools
// builds refuse to play while playback is paused.
uint32_t audioPlaySound(WasmScriptingVM* vm, rcp<AudioSound> sound)
{
    if (sound == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::audioSound,
                              new HostAudioSound{std::move(sound)});
}

rcp<AudioEngine> audioPlayEngine(WasmScriptingVM* vm, HostAudioSource* host)
{
    if (host == nullptr)
    {
        return nullptr;
    }
#ifdef WITH_RIVE_TOOLS
    if (!vm->isPlaying())
    {
        return nullptr;
    }
#endif
    return AudioEngine::RuntimeEngine(true);
}

uint32_t audioPlaySeconds(WasmScriptingVM* vm,
                          uint32_t sourceHandle,
                          float seconds,
                          bool relative)
{
    auto host = resolveAudioSource(vm, sourceHandle);
    auto engine = audioPlayEngine(vm, host);
    if (engine == nullptr)
    {
        return 0;
    }
    if (relative)
    {
        seconds += engine->timeInSeconds();
    }
    return audioPlaySound(vm, engine->playSeconds(host->source, seconds, 0, 0));
}

uint32_t audioPlayFrames(WasmScriptingVM* vm,
                         uint32_t sourceHandle,
                         double frames,
                         bool relative)
{
    auto host = resolveAudioSource(vm, sourceHandle);
    auto engine = audioPlayEngine(vm, host);
    if (engine == nullptr)
    {
        return 0;
    }
    uint64_t startTime = (uint64_t)frames;
    if (relative)
    {
        startTime += engine->timeInFrames();
    }
    return audioPlaySound(vm, engine->play(host->source, startTime, 0, 0));
}
#endif

uint32_t audioSourceImpl(WasmScriptingVM* vm,
                         uint32_t objectHandle,
                         const char* name,
                         uint32_t nameLength)
{
#ifdef WITH_RIVE_AUDIO
    auto asset = findFileAsset<AudioAsset>(
        vm,
        objectHandle,
        name,
        nameLength,
        [](AudioAsset* audio) { return audio->audioSource() != nullptr; });
    if (asset == nullptr)
    {
        return 0;
    }
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::audioSource,
                              new HostAudioSource{asset->audioSource()});
#endif
    return 0;
}

void audioSourceReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
#ifdef WITH_RIVE_AUDIO
    auto host = resolveAudioSource(vm, handle);
    if (host == nullptr)
    {
        return;
    }
    vm->handles().release(handle,
                          WasmScriptingVM::HandleTable::Tag::audioSource);
    delete host;
#endif
}

float audioSourceDurationImpl(WasmScriptingVM* vm, uint32_t handle)
{
#ifdef WITH_RIVE_AUDIO
    auto host = resolveAudioSource(vm, handle);
    return host != nullptr ? host->source->duration() : 0.0f;
#else
    return 0.0f;
#endif
}

uint32_t audioSourceSampleRateImpl(WasmScriptingVM* vm, uint32_t handle)
{
#ifdef WITH_RIVE_AUDIO
    auto host = resolveAudioSource(vm, handle);
    return host != nullptr ? host->source->sampleRate() : 0;
#else
    return 0;
#endif
}

uint32_t audioSourceChannelsImpl(WasmScriptingVM* vm, uint32_t handle)
{
#ifdef WITH_RIVE_AUDIO
    auto host = resolveAudioSource(vm, handle);
    return host != nullptr ? host->source->channels() : 0;
#else
    return 0;
#endif
}

uint32_t audioPlayImpl(WasmScriptingVM* vm, uint32_t source)
{
#ifdef WITH_RIVE_AUDIO
    return audioPlaySeconds(vm, source, 0.0f, true);
#else
    return 0;
#endif
}

uint32_t audioPlayAtTimeImpl(WasmScriptingVM* vm,
                             uint32_t source,
                             float seconds)
{
#ifdef WITH_RIVE_AUDIO
    return audioPlaySeconds(vm, source, seconds, false);
#else
    return 0;
#endif
}

uint32_t audioPlayInTimeImpl(WasmScriptingVM* vm,
                             uint32_t source,
                             float seconds)
{
#ifdef WITH_RIVE_AUDIO
    return audioPlaySeconds(vm, source, seconds, true);
#else
    return 0;
#endif
}

uint32_t audioPlayAtFrameImpl(WasmScriptingVM* vm,
                              uint32_t source,
                              double frame)
{
#ifdef WITH_RIVE_AUDIO
    return audioPlayFrames(vm, source, frame, false);
#else
    return 0;
#endif
}

uint32_t audioPlayInFrameImpl(WasmScriptingVM* vm,
                              uint32_t source,
                              double frame)
{
#ifdef WITH_RIVE_AUDIO
    return audioPlayFrames(vm, source, frame, true);
#else
    return 0;
#endif
}

float audioTimeImpl(WasmScriptingVM* vm)
{
#ifdef WITH_RIVE_AUDIO
    auto engine = AudioEngine::RuntimeEngine(true);
    return engine != nullptr ? engine->timeInSeconds() : 0.0f;
#else
    return 0.0f;
#endif
}

double audioTimeFrameImpl(WasmScriptingVM* vm)
{
#ifdef WITH_RIVE_AUDIO
    auto engine = AudioEngine::RuntimeEngine(true);
    return engine != nullptr ? (double)engine->timeInFrames() : 0.0;
#else
    return 0.0;
#endif
}

uint32_t audioSampleRateImpl(WasmScriptingVM* vm)
{
#ifdef WITH_RIVE_AUDIO
    auto engine = AudioEngine::RuntimeEngine(true);
    return engine != nullptr ? engine->sampleRate() : 0;
#else
    return 0;
#endif
}

void audioSoundReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
#ifdef WITH_RIVE_AUDIO
    auto host = resolveAudioSound(vm, handle);
    if (host == nullptr)
    {
        return;
    }
    vm->handles().release(handle,
                          WasmScriptingVM::HandleTable::Tag::audioSound);
    delete host;
#endif
}

void audioSoundPlayImpl(WasmScriptingVM* vm, uint32_t handle)
{
#ifdef WITH_RIVE_AUDIO
    auto host = resolveAudioSound(vm, handle);
    if (host != nullptr)
    {
        host->sound->play();
    }
#endif
}

void audioSoundPauseImpl(WasmScriptingVM* vm, uint32_t handle)
{
#ifdef WITH_RIVE_AUDIO
    auto host = resolveAudioSound(vm, handle);
    if (host != nullptr)
    {
        host->sound->pause();
    }
#endif
}

void audioSoundResumeImpl(WasmScriptingVM* vm, uint32_t handle)
{
#ifdef WITH_RIVE_AUDIO
    auto host = resolveAudioSound(vm, handle);
    if (host != nullptr)
    {
        host->sound->resume();
    }
#endif
}

void audioSoundStopImpl(WasmScriptingVM* vm,
                        uint32_t handle,
                        uint32_t fadeFrames)
{
#ifdef WITH_RIVE_AUDIO
    auto host = resolveAudioSound(vm, handle);
    if (host != nullptr)
    {
        host->sound->stop(fadeFrames);
    }
#endif
}

uint32_t audioSoundSeekImpl(WasmScriptingVM* vm, uint32_t handle, float seconds)
{
#ifdef WITH_RIVE_AUDIO
    auto host = resolveAudioSound(vm, handle);
    return host != nullptr && host->sound->seekSeconds(seconds) ? 1 : 0;
#else
    return 0;
#endif
}

uint32_t audioSoundSeekFrameImpl(WasmScriptingVM* vm,
                                 uint32_t handle,
                                 double frame)
{
#ifdef WITH_RIVE_AUDIO
    auto host = resolveAudioSound(vm, handle);
    return host != nullptr && host->sound->seek((uint64_t)frame) ? 1 : 0;
#else
    return 0;
#endif
}

uint32_t audioSoundCompletedImpl(WasmScriptingVM* vm, uint32_t handle)
{
#ifdef WITH_RIVE_AUDIO
    auto host = resolveAudioSound(vm, handle);
    return host != nullptr && host->sound->completed() ? 1 : 0;
#else
    return 0;
#endif
}

float audioSoundTimeImpl(WasmScriptingVM* vm, uint32_t handle)
{
#ifdef WITH_RIVE_AUDIO
    auto host = resolveAudioSound(vm, handle);
    return host != nullptr ? host->sound->timeInSeconds() : 0.0f;
#else
    return 0.0f;
#endif
}

double audioSoundTimeFrameImpl(WasmScriptingVM* vm, uint32_t handle)
{
#ifdef WITH_RIVE_AUDIO
    auto host = resolveAudioSound(vm, handle);
    return host != nullptr ? (double)host->sound->timeInFrames() : 0.0;
#else
    return 0.0;
#endif
}

float audioSoundVolumeImpl(WasmScriptingVM* vm, uint32_t handle)
{
#ifdef WITH_RIVE_AUDIO
    auto host = resolveAudioSound(vm, handle);
    return host != nullptr ? host->sound->volume() : 0.0f;
#else
    return 0.0f;
#endif
}

void audioSoundSetVolumeImpl(WasmScriptingVM* vm, uint32_t handle, float value)
{
#ifdef WITH_RIVE_AUDIO
    auto host = resolveAudioSound(vm, handle);
    if (host != nullptr)
    {
        host->sound->volume(value);
    }
#endif
}

uint32_t dataBlobPresentImpl(WasmScriptingVM* vm, uint32_t handle)
{
    return resolveBlobAsset(vm, handle) != nullptr ? 1 : 0;
}

uint32_t dataBlobGetImpl(WasmScriptingVM* vm,
                         uint32_t handle,
                         uint8_t* buffer,
                         uint32_t capacity)
{
    auto asset = resolveBlobAsset(vm, handle);
    if (asset == nullptr)
    {
        return 0;
    }
    auto bytes = asset->as<BlobAsset>()->bytes();
    size_t copied = bytes.size() < capacity ? bytes.size() : capacity;
    memcpy(buffer, bytes.data(), copied);
    return (uint32_t)bytes.size();
}

uint32_t dataBlobNameImpl(WasmScriptingVM* vm,
                          uint32_t handle,
                          char* buffer,
                          uint32_t capacity)
{
    auto asset = resolveBlobAsset(vm, handle);
    if (asset == nullptr)
    {
        return 0;
    }
    return fillModuleString(vm, asset->name(), buffer, capacity);
}

void dataBlobSetImpl(WasmScriptingVM* vm,
                     uint32_t handle,
                     const uint8_t* bytes,
                     uint32_t byteCount)
{
    auto value = resolveInstanceValue<ViewModelInstanceAssetBlob>(vm, handle);
    if (value == nullptr)
    {
        return;
    }
    auto asset = make_rcp<BlobAsset>();
    SimpleArray<uint8_t> data(bytes, byteCount);
    asset->decode(data, nullptr);
    value->value(asset.get());
}

void dataBlobClearImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto value = resolveInstanceValue<ViewModelInstanceAssetBlob>(vm, handle);
    if (value != nullptr)
    {
        value->value(nullptr);
    }
}

HostInstanceValue* resolveHostValue(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return nullptr;
    }
    return static_cast<HostInstanceValue*>(vm->handles().resolve(
        handle,
        WasmScriptingVM::HandleTable::Tag::instanceValue));
}

void dataWatchImpl(WasmScriptingVM* vm, uint32_t handle, uint32_t token)
{
    auto host = resolveHostValue(vm, handle);
    if (host == nullptr || host->delegate != nullptr)
    {
        return;
    }
    host->delegate = new HostValueDelegate();
    host->delegate->vm = vm;
    host->delegate->token = token;
    host->value->addDelegate(host->delegate);
}

void dataUnwatchImpl(WasmScriptingVM* vm, uint32_t handle)
{
    auto host = resolveHostValue(vm, handle);
    if (host == nullptr || host->delegate == nullptr)
    {
        return;
    }
    host->value->removeDelegate(host->delegate);
    delete host->delegate;
    host->delegate = nullptr;
}

bool ensureRuntime()
{
    static bool initialized = false;
    static bool ok = false;
    if (initialized)
    {
        return ok;
    }
    initialized = true;

    RuntimeInitArgs args;
    memset(&args, 0, sizeof(args));
    args.mem_alloc_type = Alloc_With_System_Allocator;
    if (!wasm_runtime_full_init(&args))
    {
        return false;
    }
    // Memory grows on demand, so a failed grow is the only sign of a
    // ceiling or device limit before the module traps.
    wasm_runtime_set_enlarge_mem_error_callback(
        [](uint32_t incPages,
           uint64_t currentBytes,
           uint32_t,
           enlarge_memory_error_reason_t reason,
           wasm_module_inst_t,
           wasm_exec_env_t,
           void*) {
            fprintf(stderr,
                    "wasm memory: growing %llu MB by %u pages failed: %s\n",
                    (unsigned long long)(currentBytes >> 20),
                    incPages,
                    reason == MAX_SIZE_REACHED
                        ? "the module hit its wasmMaxPages ceiling"
                        : "the device could not provide the memory");
        },
        nullptr);
    wasm_runtime_register_natives("env",
                                  kEnvNatives,
                                  sizeof(kEnvNatives) / sizeof(NativeSymbol));
    wasm_runtime_register_natives("wasi_snapshot_preview1",
                                  kWasiNatives,
                                  sizeof(kWasiNatives) / sizeof(NativeSymbol));
    if (!registerRiveBindingNatives())
    {
        return false;
    }
    ok = true;
    return ok;
}

} // namespace

uint32_t WasmScriptingVM::HandleTable::mint(Tag tag, void* object)
{
    uint32_t slot;
    if (!freeSlots.empty())
    {
        slot = freeSlots.back();
        freeSlots.pop_back();
    }
    else
    {
        slot = (uint32_t)slots.size();
        slots.push_back({});
    }
    slots[slot].tag = tag;
    slots[slot].object = object;
    if (holdsViewModel(tag))
    {
        slots[slot].viewModelIndex = (uint32_t)viewModelSlots.size();
        viewModelSlots.push_back(slot);
    }
    // Handle 0 is never valid; slots bias by one.
    return ((slot + 1) & 0xffffff) | ((uint32_t)slots[slot].generation << 24);
}

void* WasmScriptingVM::HandleTable::resolve(uint32_t handle, Tag tag) const
{
    uint32_t slot = (handle & 0xffffff);
    if (slot == 0 || slot > slots.size())
    {
        return nullptr;
    }
    const Slot& entry = slots[slot - 1];
    if (entry.tag != tag || entry.generation != (uint8_t)(handle >> 24))
    {
        return nullptr;
    }
    return entry.object;
}

void WasmScriptingVM::HandleTable::release(uint32_t handle, Tag tag)
{
    uint32_t slot = (handle & 0xffffff);
    if (slot == 0 || slot > slots.size())
    {
        return;
    }
    Slot& entry = slots[slot - 1];
    // Tag mismatch must not free the slot: a mistyped guest release would
    // stale the correctly-typed handle and leak its host wrapper.
    if (entry.generation != (uint8_t)(handle >> 24) || entry.tag != tag)
    {
        return;
    }
    if (holdsViewModel(tag))
    {
        uint32_t moved = viewModelSlots.back();
        viewModelSlots[entry.viewModelIndex] = moved;
        slots[moved].viewModelIndex = entry.viewModelIndex;
        viewModelSlots.pop_back();
    }
    entry.tag = Tag::empty;
    entry.object = nullptr;
    entry.generation++;
    freeSlots.push_back(slot - 1);
}

int WasmScriptingVM::sm_defaultTimeoutMs = 200;

WasmScriptingVM::WasmScriptingVM() = default;

std::unique_ptr<WasmScriptingVM> WasmScriptingVM::make(
    Span<const uint8_t> module,
    Factory* factory,
    std::string& outError,
    std::function<void(const char*, size_t)> print)
{
    std::unique_ptr<WasmScriptingVM> vm(new WasmScriptingVM());
    vm->m_factory = factory;
    vm->m_print = std::move(print);
    if (s_bootHook)
    {
        s_bootHook(*vm, module);
    }
    if (!vm->init(module))
    {
        outError = vm->m_lastError;
        return nullptr;
    }
    return vm;
}

void WasmScriptingVM::callDraw(ScriptedObject* object,
                               int selfRef,
                               Renderer* renderer)
{
    // The handle is scoped to this call; releasing bumps the generation so a
    // stashed renderer goes stale instead of ghost drawing.
    uint32_t handle = m_handles.mint(HandleTable::Tag::renderer, renderer);
    uint32_t args[3] = {m_L, (uint32_t)selfRef, handle};
    callModule("host_obj_draw", 3, args);
    m_handles.release(handle, HandleTable::Tag::renderer);
}

WasmScriptingVM::~WasmScriptingVM()
{
    if (m_budgetSlot != nullptr)
    {
        ScriptWatchdog::instance().remove(m_budgetSlot.get());
    }
    if (m_debugHooks != nullptr)
    {
        m_debugHooks->onDetach(*this);
        m_debugHooks = nullptr;
    }
    // Flag in-flight decodes cancelled before teardown so a later poll
    // cannot call back into this dead VM, mirroring the Luau backend's
    // shutdownAsyncForState.
    if (m_decodeOwnerId != 0)
    {
        auto& pool = getGlobalWorkPoolIfExists();
        if (pool != nullptr)
        {
            pool->cancelAllForOwner(m_decodeOwnerId);
        }
    }
#ifdef WITH_RIVE_SCRIPTNET
    // Same for fetches: their listeners hold this VM.
    if (m_netOwnerId != 0)
    {
        scriptnet::cancelAllForOwner(m_netOwnerId);
    }
#endif
    // Coverage measurement: RIVE_WASM_EXEC_STATS=1 dumps how many frames ran
    // compiled, interpreted, and as guard-exit continuations.
    if (getenv("RIVE_WASM_EXEC_STATS") != nullptr && m_state != nullptr &&
        m_state->execEnv != nullptr)
    {
        uint32_t args[1] = {0};
        uint32_t compiled = callModule("host_exec_stats", 1, args);
        args[0] = 1;
        uint32_t interp = callModule("host_exec_stats", 1, args);
        args[0] = 2;
        uint32_t guardExits = callModule("host_exec_stats", 1, args);
        fprintf(stderr,
                "wasm exec stats: compiled=%u interp=%u guardExits=%u\n",
                compiled,
                interp,
                guardExits);
    }
    // The module never runs guest finalizers at teardown, so host objects
    // behind live handles are swept here; instance values in particular must
    // detach their delegates before the VM goes away.
    for (HandleTable::Slot& slot : m_handles.slots)
    {
        switch (slot.tag)
        {
            case HandleTable::Tag::instanceValue:
                delete static_cast<HostInstanceValue*>(slot.object);
                break;
            case HandleTable::Tag::viewModelInstance:
                delete static_cast<HostViewModelInstance*>(slot.object);
                break;
            case HandleTable::Tag::path:
                delete static_cast<HostPath*>(slot.object);
                break;
            case HandleTable::Tag::measure:
                delete static_cast<HostMeasure*>(slot.object);
                break;
            case HandleTable::Tag::paint:
                delete static_cast<HostPaint*>(slot.object);
                break;
            case HandleTable::Tag::shader:
                delete static_cast<HostShader*>(slot.object);
                break;
            case HandleTable::Tag::image:
                delete static_cast<HostImage*>(slot.object);
                break;
            case HandleTable::Tag::font:
                delete static_cast<HostFont*>(slot.object);
                break;
            case HandleTable::Tag::text:
                deleteHostText(this, static_cast<HostText*>(slot.object));
                break;
            case HandleTable::Tag::buffer:
                delete static_cast<HostBuffer*>(slot.object);
                break;
            case HandleTable::Tag::meshInstances:
                delete static_cast<HostMeshInstances*>(slot.object);
                break;
            case HandleTable::Tag::dataContext:
                delete static_cast<HostDataContext*>(slot.object);
                break;
            case HandleTable::Tag::artboard:
                // Refcounted: animations and nodes may still hold it.
                static_cast<HostArtboard*>(slot.object)->unref();
                break;
            case HandleTable::Tag::animation:
                delete static_cast<HostAnimation*>(slot.object);
                break;
            case HandleTable::Tag::node:
                delete static_cast<HostNode*>(slot.object);
                break;
            case HandleTable::Tag::riveFile:
                delete static_cast<HostRiveFile*>(slot.object);
                break;
            case HandleTable::Tag::bindableArtboard:
                delete static_cast<HostBindableArtboard*>(slot.object);
                break;
#ifdef WITH_RIVE_AUDIO
            case HandleTable::Tag::audioSource:
                delete static_cast<HostAudioSource*>(slot.object);
                break;
            case HandleTable::Tag::audioSound:
                delete static_cast<HostAudioSound*>(slot.object);
                break;
#endif
#ifdef RIVE_CANVAS
            case HandleTable::Tag::canvas:
                delete static_cast<HostCanvas*>(slot.object);
                break;
#endif
#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
            case HandleTable::Tag::gpuPass:
                delete static_cast<HostGpuPass*>(slot.object);
                break;
            case HandleTable::Tag::gpuCanvas:
                delete static_cast<HostGpuCanvas*>(slot.object);
                break;
            case HandleTable::Tag::gpuBuffer:
                delete static_cast<HostGpuBuffer*>(slot.object);
                break;
            case HandleTable::Tag::gpuTexture:
                delete static_cast<HostGpuTexture*>(slot.object);
                break;
            case HandleTable::Tag::gpuSampler:
                delete static_cast<HostGpuSampler*>(slot.object);
                break;
            case HandleTable::Tag::gpuTextureView:
                delete static_cast<HostGpuTextureView*>(slot.object);
                break;
            case HandleTable::Tag::gpuShaderModule:
                delete static_cast<HostGpuShaderModule*>(slot.object);
                break;
            case HandleTable::Tag::gpuBindGroupLayout:
                delete static_cast<HostGpuBindGroupLayout*>(slot.object);
                break;
            case HandleTable::Tag::gpuBindGroup:
                delete static_cast<HostGpuBindGroup*>(slot.object);
                break;
            case HandleTable::Tag::gpuPipeline:
                delete static_cast<HostGpuPipeline*>(slot.object);
                break;
#endif
            default:
                break;
        }
        slot.tag = HandleTable::Tag::empty;
        slot.object = nullptr;
    }
}

bool WasmScriptingVM::notifyDrawVisit(uint32_t drawable)
{
    uint32_t args[2] = {m_L, drawable};
    return valid() && callModule("host_draw_visit", 2, args) != 0;
}

void WasmScriptingVM::notifyDataValueChanged(uint32_t token)
{
    if (!valid())
    {
        return;
    }
    uint32_t args[2] = {m_L, token};
    callModule("host_data_value_changed", 2, args);
}

// --- context:decodeImage over the module ABI ---------------------------------

#ifdef RIVE_DECODERS
namespace
{

// Mirrors the Luau backend's ImageDecodeTask (lua_image_decode.cpp): same
// Bitmap::decode, same premultiply, same error string, same WorkPool, so the
// wasm lane settles on the same advance as the Luau lane.
class WasmImageDecodeTask : public WorkTask
{
public:
    std::vector<uint8_t> m_encodedData;
    WasmScriptingVM* m_vm = nullptr;
    uint32_t m_token = 0;
    std::unique_ptr<Bitmap> m_bitmap;

    bool execute() override
    {
        m_bitmap = Bitmap::decode(m_encodedData.data(), m_encodedData.size());
        if (!m_bitmap)
        {
            m_errorMessage = "failed to decode image data";
            return false;
        }
        if (m_bitmap->pixelFormat() != Bitmap::PixelFormat::RGBAPremul)
        {
            m_bitmap->pixelFormat(Bitmap::PixelFormat::RGBAPremul);
        }
        return true;
    }

    void onComplete() override
    {
        if (m_vm != nullptr)
        {
            m_vm->resolveImageDecode(m_token,
                                     m_bitmap->width(),
                                     m_bitmap->height(),
                                     m_bitmap->bytes(),
                                     m_bitmap->width() * m_bitmap->height() *
                                         4);
        }
        m_bitmap.reset();
        m_encodedData.clear();
        m_encodedData.shrink_to_fit();
    }

    void onError(const std::string& error) override
    {
        if (m_vm != nullptr)
        {
            m_vm->rejectImageDecode(m_token, error.c_str());
        }
    }

    void onCancel() override { m_vm = nullptr; }
};

} // namespace
#endif // RIVE_DECODERS

bool WasmScriptingVM::startImageDecode(const uint8_t* bytes,
                                       uint32_t byteCount,
                                       uint32_t token)
{
#ifndef RIVE_DECODERS
    return false;
#else
    if (m_decodeOwnerId == 0)
    {
        m_decodeOwnerId = WorkPool::nextOwnerId();
    }
    auto task = make_rcp<WasmImageDecodeTask>();
    task->m_encodedData.assign(bytes, bytes + byteCount);
    task->m_vm = this;
    task->m_token = token;
    task->setOwnerId(m_decodeOwnerId);
    m_pendingDecodes[token] = task;
    getGlobalWorkPool()->submit(std::move(task));
    return true;
#endif
}

void WasmScriptingVM::cancelImageDecode(uint32_t token)
{
    auto it = m_pendingDecodes.find(token);
    if (it != m_pendingDecodes.end())
    {
        it->second->cancel();
        m_pendingDecodes.erase(it);
    }
}

void WasmScriptingVM::deliverDecodeResult(const DecodeResult& result)
{
    if (result.ok)
    {
        uint32_t byteCount = (uint32_t)result.pixels.size();
        uint32_t sizeArgs[1] = {byteCount};
        uint32_t pixelsPtr = callModule("malloc", 1, sizeArgs);
        if (pixelsPtr == 0)
        {
            DecodeResult failure;
            failure.token = result.token;
            failure.error = "failed to allocate decoded pixels";
            deliverDecodeResult(failure);
            return;
        }
        memcpy(resolveModuleWritePtr(pixelsPtr, byteCount),
               result.pixels.data(),
               byteCount);
        uint32_t args[6] = {m_L,
                            result.token,
                            result.width,
                            result.height,
                            pixelsPtr,
                            byteCount};
        callModule("host_image_decoded", 6, args);
        guestFree(pixelsPtr);
        return;
    }
    uint32_t messagePtr = guestString(result.error.c_str());
    if (messagePtr == 0)
    {
        return;
    }
    uint32_t args[3] = {m_L, result.token, messagePtr};
    callModule("host_image_decode_failed", 3, args);
    guestFree(messagePtr);
}

void WasmScriptingVM::resolveImageDecode(uint32_t token,
                                         uint32_t width,
                                         uint32_t height,
                                         const uint8_t* pixels,
                                         uint32_t byteCount)
{
    if (inModuleCall())
    {
        // The decoder frees its pixels once this returns.
        std::vector<uint8_t> owned(pixels, pixels + byteCount);
        m_heldOutcomes.push_back(
            [this, token, width, height, owned = std::move(owned)] {
                resolveImageDecode(token,
                                   width,
                                   height,
                                   owned.data(),
                                   (uint32_t)owned.size());
            });
        return;
    }
    if (m_pendingDecodes.erase(token) == 0 || !valid())
    {
        return;
    }
    DecodeResult result;
    result.ok = true;
    result.token = token;
    result.width = width;
    result.height = height;
    result.pixels = Span<const uint8_t>(pixels, byteCount);
    deliverDecodeResult(result);
}

void WasmScriptingVM::rejectImageDecode(uint32_t token, const char* message)
{
    if (inModuleCall())
    {
        m_heldOutcomes.push_back([this, token, error = std::string(message)] {
            rejectImageDecode(token, error.c_str());
        });
        return;
    }
    if (m_pendingDecodes.erase(token) == 0 || !valid())
    {
        return;
    }
    DecodeResult result;
    result.token = token;
    result.error = message;
    deliverDecodeResult(result);
}

// --- fetch() over the module ABI --------------------------------------------

#ifdef WITH_RIVE_SCRIPTNET
namespace
{
// The Luau lane's rejection value: "<code>: <message>".
std::string fetchRejection(scriptnet::NetErrorCode code,
                           const std::string& message)
{
    return std::string(scriptnet::netErrorCodeName(code)) + ": " + message;
}

// Carries one request's outcome back to the VM that started it. scriptnet
// delivers on this VM's thread from its poll; teardown cancels every
// outstanding request first, which clears the VM pointer here.
class WasmFetchListener : public scriptnet::FetchListener
{
public:
    WasmFetchListener(WasmScriptingVM* vm, uint32_t token) :
        m_vm(vm), m_token(token)
    {}

    void onResponse(scriptnet::HttpResponse&& response) override
    {
        if (m_vm != nullptr)
        {
            m_vm->resolveFetch(m_token, std::move(response));
        }
    }

    void onError(const scriptnet::NetError& error) override
    {
        if (m_vm != nullptr)
        {
            m_vm->rejectFetch(m_token,
                              (uint32_t)error.code,
                              fetchRejection(error.code, error.message));
        }
    }

    void onCancel() override { m_vm = nullptr; }

private:
    WasmScriptingVM* m_vm;
    uint32_t m_token;
};
} // namespace
#endif

bool WasmScriptingVM::startFetch(Span<const uint8_t> wire, uint32_t token)
{
#ifdef WITH_RIVE_SCRIPTNET
    // Reusing a live token would leave its first request uncancellable.
    if (m_pendingFetches.count(token) != 0)
    {
        return false;
    }
    scriptnet::HttpRequest request;
    // The module's fetch() writes the stream from arguments it validated, so
    // a bad one is a bug; it goes out empty and the policy refuses it,
    // settling like any other refusal.
    if (!NetWire::decode(wire, request))
    {
        request = scriptnet::HttpRequest();
    }
    if (m_netOwnerId == 0)
    {
        m_netOwnerId = WorkPool::nextOwnerId();
    }
    m_pendingFetches[token] =
        scriptnet::fetch(m_netOwnerId,
                         std::move(request),
                         make_rcp<WasmFetchListener>(this, token));
    return true;
#else
    (void)wire;
    (void)token;
    return false;
#endif
}

void WasmScriptingVM::cancelFetch(uint32_t token)
{
    auto it = m_pendingFetches.find(token);
    if (it == m_pendingFetches.end())
    {
        return;
    }
    uint32_t id = it->second;
    m_pendingFetches.erase(it);
#ifdef WITH_RIVE_SCRIPTNET
    scriptnet::cancel(id);
#else
    (void)id;
#endif
}

void WasmScriptingVM::resolveFetch(uint32_t token,
                                   scriptnet::HttpResponse&& response)
{
    if (inModuleCall())
    {
        m_heldOutcomes.push_back(
            [this, token, response = std::move(response)]() mutable {
                resolveFetch(token, std::move(response));
            });
        return;
    }
    // Cancelled while held.
    if (m_pendingFetches.erase(token) == 0 || !valid())
    {
        return;
    }
    size_t size = NetWire::encodedSize(response);
    uint32_t byteCount = (uint32_t)size;
    uint32_t sizeArgs[1] = {byteCount};
    uint32_t responsePtr =
        size == byteCount ? callModule("malloc", 1, sizeArgs) : 0;
    if (responsePtr == 0)
    {
#ifdef WITH_RIVE_SCRIPTNET
        deliverFetchFailure(
            token,
            (uint32_t)scriptnet::NetErrorCode::TooLarge,
            fetchRejection(scriptnet::NetErrorCode::TooLarge,
                           "the response does not fit in script memory"));
#endif
        return;
    }
    NetWire::encode(response,
                    (uint8_t*)resolveModuleWritePtr(responsePtr, byteCount));
    uint32_t args[4] = {m_L, token, responsePtr, byteCount};
    callModule("host_fetch_resolved", 4, args);
    guestFree(responsePtr);
}

void WasmScriptingVM::rejectFetch(uint32_t token,
                                  uint32_t code,
                                  const std::string& message)
{
    if (inModuleCall())
    {
        m_heldOutcomes.push_back([this, token, code, message] {
            rejectFetch(token, code, message);
        });
        return;
    }
    if (m_pendingFetches.erase(token) != 0 && valid())
    {
        deliverFetchFailure(token, code, message);
    }
}

void WasmScriptingVM::deliverFetchFailure(uint32_t token,
                                          uint32_t code,
                                          const std::string& message)
{
    uint32_t messagePtr = guestString(message.c_str());
    if (messagePtr == 0)
    {
        return;
    }
    uint32_t args[4] = {m_L, token, code, messagePtr};
    callModule("host_fetch_failed", 4, args);
    guestFree(messagePtr);
}

bool WasmScriptingVM::inModuleCall() const
{
    return m_state != nullptr && m_state->callDepth != 0;
}

void WasmScriptingVM::deliverHeldOutcomes()
{
    if (inModuleCall() || m_heldOutcomes.empty())
    {
        return;
    }
    // Deliveries can hold more, which wait for the next pass.
    std::vector<std::function<void()>> held;
    held.swap(m_heldOutcomes);
    for (auto& deliver : held)
    {
        deliver();
    }
}

void WasmScriptingVM::setBootHook(BootHook hook)
{
    s_bootHook = std::move(hook);
}

void WasmScriptingVM::setTimeoutMs(int ms)
{
    m_timeoutMs = ms;
    if (valid())
    {
        uint32_t args[2] = {m_L, (uint32_t)ms};
        callModule("host_set_timeout", 2, args);
    }
}

void WasmScriptingVM::advanceDetachedViewModels()
{
    // Only detached roots; instances with parents are already reached
    // through the bound tree or their detached-root ancestor.
    auto advanceDetached = [](rcp<ViewModelInstance> instance) {
        if (instance != nullptr && !instance->hasParents())
        {
            instance->advanced();
        }
    };
    // Indexed and holding the instance, since a changed callback can mint or
    // release, like ViewModelInstance::advanced.
    for (size_t i = 0; i < m_handles.viewModelSlots.size(); i++)
    {
        HandleTable::Slot slot = m_handles.slots[m_handles.viewModelSlots[i]];
        switch (slot.tag)
        {
            case HandleTable::Tag::viewModelInstance:
                advanceDetached(
                    static_cast<HostViewModelInstance*>(slot.object)->instance);
                break;
            case HandleTable::Tag::artboard:
                // Artboard inputs keep their bound instance advancing, like
                // the Luau context's tracked instances.
                advanceDetached(
                    static_cast<HostArtboard*>(slot.object)->viewModelInstance);
                break;
            default:
                break;
        }
    }
}

#if WASM_ENABLE_PRELINKED_AOT != 0
// Builds the section list for a module whose AOT text is linked into this
// binary and loads it. The relocation section is dropped: the native linker
// already applied every fixup, which is what makes the text usable without
// runtime executable memory.
static wasm_module_t loadPrelinkedModule(const PrelinkedAotModule& prelinked,
                                         char* error,
                                         uint32_t errorSize)
{
    constexpr uint32_t kSectionText = 2;
    constexpr uint32_t kSectionRelocation = 5;
    // .aot container: 8 byte magic+version header, then type/size framed
    // sections. The from-sections loader never sees the header, so an
    // artifact from an incompatible wamrc must be rejected here.
    if (prelinked.aotSize < 8)
    {
        snprintf(error, errorSize, "prelinked aot container too small");
        return nullptr;
    }
    uint32_t magic;
    uint32_t version;
    memcpy(&magic, prelinked.aot, 4);
    memcpy(&version, prelinked.aot + 4, 4);
    if (magic != AOT_MAGIC_NUMBER || version != AOT_CURRENT_VERSION)
    {
        snprintf(error,
                 errorSize,
                 "prelinked aot container magic/version mismatch "
                 "(%08x v%u, runtime expects v%u)",
                 magic,
                 version,
                 (uint32_t)AOT_CURRENT_VERSION);
        return nullptr;
    }
    const uint8_t* p = prelinked.aot + 8;
    const uint8_t* end = prelinked.aot + prelinked.aotSize;
    std::vector<wasm_section_t> sections;
    while (true)
    {
        // The container aligns every u32 read; section headers land on
        // 4 byte boundaries relative to the (aligned) container base.
        // Compare as integers first: the aligned address may lie past end,
        // where pointer arithmetic is undefined.
        uintptr_t aligned = ((uintptr_t)p + 3) & ~(uintptr_t)3;
        if (aligned > (uintptr_t)end || (uintptr_t)end - aligned < 8)
        {
            break;
        }
        p = (const uint8_t*)aligned;
        uint32_t type;
        uint32_t size;
        memcpy(&type, p, 4);
        memcpy(&size, p + 4, 4);
        p += 8;
        if (size > (size_t)(end - p))
        {
            snprintf(error, errorSize, "prelinked aot container truncated");
            return nullptr;
        }
        if (type != kSectionRelocation)
        {
            wasm_section_t section = {};
            section.section_type = (int)type;
            if (type == kSectionText)
            {
                section.section_body = const_cast<uint8_t*>(prelinked.text);
                section.section_body_size = (uint32_t)prelinked.textSize;
            }
            else
            {
                section.section_body = const_cast<uint8_t*>(p);
                section.section_body_size = size;
            }
            sections.push_back(section);
        }
        p += size;
    }
    if (sections.empty())
    {
        snprintf(error, errorSize, "prelinked aot container empty");
        return nullptr;
    }
    for (size_t i = 0; i + 1 < sections.size(); i++)
    {
        sections[i].next = &sections[i + 1];
    }
    sections.back().next = nullptr;
    return wasm_runtime_load_from_sections(sections.data(),
                                           true,
                                           error,
                                           errorSize);
}
#endif

bool WasmScriptingVM::init(Span<const uint8_t> module)
{
    if (!ensureRuntime())
    {
        m_lastError = "wamr runtime init failed";
        return false;
    }

    // A debugger pins before init; a rejected artifact must not unpin it.
    const bool callerPinned = m_tierPinned;
    // wasm_runtime_load keeps referencing the buffer, so hold a copy.
    m_moduleBytes.assign(module.begin(), module.end());
    // Snapshot before load: the loader mutates m_moduleBytes in place
    // (null-terminating names), which corrupts it for wamrc. The AOT lane
    // compiles from this pristine copy instead.
    std::vector<uint8_t> pristineBytes(module.begin(), module.end());
    // One content hash serves the dev AOT artifact lookup and the shared
    // module cache key; the key folds in which artifact actually loads so
    // interp and AOT lanes never share an entry.
    uint64_t moduleKey = wasmModuleKey(
        Span<const uint8_t>(m_moduleBytes.data(), m_moduleBytes.size()));
    m_state = std::make_unique<WamrState>();
    m_moduleKey = moduleKey;
    char error[256] = {0};
    // Dev AOT lane: with RIVE_WASM_AOT_CACHE set, swap in a wamrc-compiled
    // artifact when one exists for these bytes, else dump the wasm so it can
    // be compiled offline. wasm_runtime_load sniffs the AOT magic.
    char aotPath[1024] = {0};
    bool haveAot = false;
    bool haveHwAot = false;
    bool haveO0Aot = false;
    if (const char* cacheDir = getenv("RIVE_WASM_AOT_CACHE"))
    {
#ifdef RIVE_WASM_HW_BOUNDS
        // Hw artifacts (wamrc --bounds-checks=0 --stack-bounds-checks=1)
        // rely on the guard-page trap handler, so only builds carrying it
        // ever probe the .hw.aot name; sw-only builds cannot load them.
        snprintf(aotPath,
                 sizeof(aotPath),
                 "%s/%s",
                 cacheDir,
                 aotArtifactName(moduleKey, ".hw").c_str());
        struct stat hwAotStat;
        haveHwAot = stat(aotPath, &hwAotStat) == 0;
#endif
        if (!haveHwAot)
        {
            snprintf(aotPath,
                     sizeof(aotPath),
                     "%s/%s",
                     cacheDir,
                     aotArtifactName(moduleKey).c_str());
            struct stat aotStat;
            haveAot = stat(aotPath, &aotStat) == 0;
        }
        if (!haveAot && !haveHwAot)
        {
            char dumpPath[1024];
            snprintf(dumpPath,
                     sizeof(dumpPath),
                     "%s/%s",
                     cacheDir,
                     wasmModuleName(moduleKey).c_str());
            struct stat dumpStat;
            if (stat(dumpPath, &dumpStat) != 0)
            {
                if (FILE* dump = fopen(dumpPath, "wb"))
                {
                    fwrite(m_moduleBytes.data(), 1, m_moduleBytes.size(), dump);
                    fclose(dump);
                    fprintf(stderr,
                            "wasm aot: dumped %s for wamrc\n",
                            dumpPath);
                }
            }
        }
    }
    // Ladder cache: content already compiled in a prior session or by a
    // background schedule boots straight onto the artifact - the
    // undo/reopen-instant and permanently-hot-vm_host path.
    // Only the documented values opt into sync boot; any other string (a
    // typo, an empty value) leaves the module on the async ladder rather
    // than silently blocking the load for seconds.
    const char* syncEnv = getenv("RIVE_WASM_AOT_SYNC");
    bool syncO0 = syncEnv != nullptr && strcmp(syncEnv, "o0") == 0;
    bool syncO3 = syncEnv != nullptr && strcmp(syncEnv, "o3") == 0;
    // Debug boot ignores any faster artifact a prior run left in a shared
    // cache; the point is to run -O0, so only the sync-o0 path below acts.
    bool debugBoot = syncO0;
    if (!haveAot && !haveHwAot)
    {
        auto& ladder = ModuleTierLadder::instance();
        if (ladder.enabled())
        {
#ifdef RIVE_WASM_HW_BOUNDS
            std::string hwPath =
                debugBoot ? std::string()
                          : ladder.artifactPath(moduleKey, TierSpecies::hw);
            if (!hwPath.empty() && hwPath.size() < sizeof(aotPath))
            {
                memcpy(aotPath, hwPath.c_str(), hwPath.size() + 1);
                haveHwAot = true;
            }
#endif
            if (!haveHwAot && !debugBoot)
            {
                std::string path =
                    ladder.artifactPath(moduleKey, TierSpecies::o3);
                if (!path.empty() && path.size() < sizeof(aotPath))
                {
                    memcpy(aotPath, path.c_str(), path.size() + 1);
                    haveAot = true;
                }
            }
            // Sync AOT boot, opted in by RIVE_WASM_AOT_SYNC=o3|o0: on a
            // cache miss, compile on this thread and boot the artifact
            // instead of the interpreter. Project-sized modules compile in
            // seconds and the interpreter is never representative of real
            // performance; hosts with large modules stay on the async
            // ladder by not setting this.
            if (!haveAot && !haveHwAot && (syncO0 || syncO3))
            {
#ifdef RIVE_WASM_HW_BOUNDS
                TierSpecies top = TierSpecies::hw;
#else
                TierSpecies top = TierSpecies::o3;
#endif
                TierSpecies species = syncO0 ? TierSpecies::o0 : top;
                auto syncStart = std::chrono::steady_clock::now();
                std::string path = ladder.compileSync(
                    moduleKey,
                    Span<const uint8_t>(pristineBytes.data(),
                                        pristineBytes.size()),
                    species);
                if (!path.empty() && path.size() < sizeof(aotPath))
                {
                    memcpy(aotPath, path.c_str(), path.size() + 1);
                    haveHwAot = species == TierSpecies::hw;
                    haveO0Aot = species == TierSpecies::o0;
                    haveAot = !haveHwAot && !haveO0Aot;
                    m_tierPinned = m_tierPinned || syncO0;
                    auto syncMs =
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - syncStart)
                            .count();
                    fprintf(stderr,
                            "wasm aot: sync compiled %s in %lld ms\n",
                            path.c_str(),
                            (long long)syncMs);
                }
                else
                {
                    fprintf(stderr,
                            "wasm aot: sync compile failed, booting interp\n");
                }
            }
        }
    }
#if WASM_ENABLE_PRELINKED_AOT != 0
    // Offline-compiled code linked into this binary; the only AOT shape on
    // platforms without runtime executable memory. The file lanes above never
    // fire there, but an explicit artifact still wins if one appears.
    const PrelinkedAotModule* prelinked =
        haveAot || haveHwAot || haveO0Aot
            ? nullptr
            : findPrelinkedAotModule(moduleKey, m_moduleBytes.size());
    if (prelinked != nullptr)
    {
        moduleKey ^= 0x94d049bb133111ebull;
    }
#endif
    if (haveAot || haveHwAot || haveO0Aot)
    {
        moduleKey ^= 0x9e3779b97f4a7c15ull;
    }
    if (haveHwAot)
    {
        // Distinct key: hw and sw artifacts of the same bytes must never
        // share a cached module, the flag below lives on the module.
        moduleKey ^= 0xc2b2ae3d27d4eb4full;
    }
    if (haveO0Aot)
    {
        // Same rule for the -O0 species: its cached module reports aotO0
        // so later VMs still schedule the -O3 upgrade.
        moduleKey ^= 0x27d4eb2f165667c5ull;
    }
    auto& cache = sharedModuleCache();
    bool adoptedFromCache = false;
    auto adoptCached = [&]() {
        auto cached = cache.find(moduleKey);
        if (cached == cache.end())
        {
            return false;
        }
        SharedWasmModule& entry = cached->second;
        m_state->module = entry.module;
        m_state->ownsModule = false;
        m_tier = entry.tier;
        // The VM's own copy is redundant against the cache entry, but the
        // tier ladder still needs the pristine bytes; entries live for the
        // process.
        m_scheduleBytes = Span<const uint8_t>(entry.pristineBytes.data(),
                                              entry.pristineBytes.size());
        m_moduleBytes.clear();
        adoptedFromCache = true;
        return true;
    };
    if (!adoptCached())
    {
        if (haveAot || haveHwAot || haveO0Aot)
        {
            if (FILE* aot = fopen(aotPath, "rb"))
            {
                fseek(aot, 0, SEEK_END);
                long size = ftell(aot);
                fseek(aot, 0, SEEK_SET);
                m_moduleBytes.resize(size);
                size_t read = fread(m_moduleBytes.data(), 1, size, aot);
                fclose(aot);
                fprintf(stderr,
                        "wasm aot: loaded %s (%zu bytes)\n",
                        aotPath,
                        read);
                m_tier =
                    haveO0Aot ? ExecutionTier::aotO0 : ExecutionTier::aotO3;
            }
        }
#if WASM_ENABLE_PRELINKED_AOT != 0
        if (prelinked != nullptr)
        {
            m_state->module =
                loadPrelinkedModule(*prelinked, error, sizeof(error));
            if (m_state->module != nullptr)
            {
                m_tier = ExecutionTier::aotO3;
                fprintf(stderr,
                        "wasm aot: prelinked module %016llx (%zu byte text)\n",
                        (unsigned long long)m_moduleKey,
                        prelinked->textSize);
            }
            else
            {
                // A stale or incompatible bake must not fail the file; drop
                // to the wasm bytes under the plain key.
                fprintf(stderr,
                        "wasm aot: prelinked module rejected (%s); "
                        "falling back\n",
                        error);
                prelinked = nullptr;
                moduleKey ^= 0x94d049bb133111ebull;
                adoptCached();
            }
        }
        if (m_state->module == nullptr)
#endif
        {
            auto loadBytes = [&]() {
                if (!haveAot && !haveHwAot && !haveO0Aot)
                {
                    // The load below rewrites the buffer in place; wamrc
                    // needs the module as it is now.
                    ModuleTierLadder::instance().stagePristine(
                        m_moduleKey,
                        Span<const uint8_t>(m_moduleBytes.data(),
                                            m_moduleBytes.size()));
                }
                m_state->module =
                    wasm_runtime_load(m_moduleBytes.data(),
                                      (uint32_t)m_moduleBytes.size(),
                                      error,
                                      sizeof(error));
            };
            loadBytes();
            if (m_state->module == nullptr &&
                (haveAot || haveHwAot || haveO0Aot))
            {
                // A stale artifact, say from another WAMR, must not fail the
                // file; drop to the wasm bytes under the plain key.
                fprintf(stderr,
                        "wasm aot: %s rejected (%s); falling back\n",
                        aotPath,
                        error);
                // Every later load would read and reject it again.
                remove(aotPath);
                haveAot = haveHwAot = haveO0Aot = false;
                m_tier = ExecutionTier::interp;
                // The pin held the rejected artifact's tier, not this one.
                m_tierPinned = callerPinned;
                moduleKey = m_moduleKey;
                if (!adoptCached())
                {
                    m_moduleBytes = pristineBytes;
                    loadBytes();
                }
            }
        }
        if (m_state->module != nullptr && !adoptedFromCache)
        {
            if (haveHwAot)
            {
                wasm_runtime_set_module_hw_bounds(m_state->module, true);
            }
            SharedWasmModule entry;
            entry.bytes = std::move(m_moduleBytes);
            entry.pristineBytes = std::move(pristineBytes);
            entry.module = m_state->module;
            entry.tier = m_tier;
            auto inserted = cache.emplace(moduleKey, std::move(entry));
            m_state->ownsModule = false;
            m_scheduleBytes = Span<const uint8_t>(
                inserted.first->second.pristineBytes.data(),
                inserted.first->second.pristineBytes.size());
        }
    }
    if (m_state->module == nullptr)
    {
        m_lastError = std::string("module load failed: ") + error;
        return false;
    }
    // Unresolved function imports link fine and trap only when first called,
    // with no diagnostic; report them here where the failure is actionable.
    int32_t importCount = wasm_runtime_get_import_count(m_state->module);
    for (int32_t i = 0; i < importCount; i++)
    {
        wasm_import_t import;
        wasm_runtime_get_import_type(m_state->module, i, &import);
        if (import.kind == WASM_IMPORT_EXPORT_KIND_FUNC && !import.linked)
        {
            m_unresolvedImports.push_back(std::string(import.module_name) +
                                          "." + import.name);
        }
    }
    // Read off the module, not the instance: module start runs every
    // script's top level, which already passes strings.
    int32_t exportCount = wasm_runtime_get_export_count(m_state->module);
    for (int32_t i = 0; i < exportCount; i++)
    {
        wasm_export_t moduleExport;
        wasm_runtime_get_export_type(m_state->module, i, &moduleExport);
        m_utf16Strings |= strcmp(moduleExport.name, "__riveUtf16Strings") == 0;
        m_utf16HostStrings |=
            strcmp(moduleExport.name, "__riveUtf16HostStrings") == 0;
        m_hostBudget |= strcmp(moduleExport.name, "__riveHostBudget") == 0;
    }
    s_bootPrint = &m_print;
    s_booting = this;
    // Module start is a call into the module like any other: the top
    // level's probes need the bracket, and its trap is the instantiate
    // error.
    if (m_debugHooks != nullptr)
    {
        m_debugHooks->onCallBegin(*this);
    }
    m_state->instance = wasm_runtime_instantiate(m_state->module,
                                                 512 * 1024,
                                                 0,
                                                 error,
                                                 sizeof(error));
    if (m_debugHooks != nullptr)
    {
        m_debugHooks->onCallEnd(*this,
                                m_state->instance == nullptr ? error : nullptr);
    }
    s_booting = nullptr;
    s_bootPrint = nullptr;
    if (m_state->instance == nullptr)
    {
        // The runtime's exception buffer truncates what start threw.
        m_lastError = std::string("module instantiate failed: ") +
                      (m_moduleErrorDetail.empty() ? std::string(error)
                                                   : m_moduleErrorDetail);
        return false;
    }
    m_state->execEnv =
        wasm_runtime_create_exec_env(m_state->instance, 512 * 1024);
    if (m_state->execEnv == nullptr)
    {
        m_lastError = "exec env creation failed";
        return false;
    }
    wasm_runtime_set_user_data(m_state->execEnv, this);

    // The leak watch for rasc-linked modules (riveRegister marks one): the
    // stub baseline never frees, and sustained growth on a collecting
    // module is references accumulating.
    const char* leakEnv = getenv("RIVE_WASM_LEAK_WARN");
    m_leakWatch = wasm_runtime_lookup_function(m_state->instance,
                                               "riveRegister") != nullptr &&
                  (leakEnv == nullptr || strcmp(leakEnv, "0") != 0);
    m_collectedRuntime =
        wasm_runtime_lookup_function(m_state->instance, "__riveCollected") !=
        nullptr;
    m_handleWatch = wasm_runtime_lookup_function(m_state->instance,
                                                 "riveRegister") != nullptr &&
                    (leakEnv == nullptr || strcmp(leakEnv, "0") != 0);
    // Frame collector modules scavenge at every boundary instead of
    // rewinding or finalizing.
    m_frameMinor = wasm_runtime_lookup_function(m_state->instance,
                                                "__riveFrameMinor") != nullptr;
    m_frameMajorsProbe =
        wasm_runtime_lookup_function(m_state->instance, "__riveFrameMajors") !=
        nullptr;
    // Stub modules report their bump position, finer than page counts.
    m_heapUsedProbe = wasm_runtime_lookup_function(m_state->instance,
                                                   "__riveHeapUsed") != nullptr;

    callModule("__wasm_call_ctors", 0, nullptr);
    m_L = callModule("host_newstate", 0, nullptr);
    if (m_L == 0)
    {
        m_lastError = "host_newstate failed";
        return false;
    }
    uint32_t largs[1] = {m_L};
    // Same execution budget as the Luau backend's timed pcall.
    uint32_t timeoutArgs[2] = {m_L, (uint32_t)m_timeoutMs};
    callModule("host_set_timeout", 2, timeoutArgs);
    callModule("host_install_require", 1, largs);
    callModule("host_install_rive_math", 1, largs);
    callModule("host_install_rive_input", 1, largs);
    callModule("host_install_rive_renderer", 1, largs);
    callModule("host_install_rive_data", 1, largs);
    callModule("host_install_rive_gpu", 1, largs);
    // Last like the Luau backend's lualibs order, and before host_seal_env
    // so the Promise/async/await globals do not clear the safe-env flag.
    callModule("host_install_rive_promise", 1, largs);
    m_embeddedModules = (int)callModule("host_register_embedded", 1, largs);
    callModule("host_seal_env", 1, largs);
    return true;
}

void WasmScriptingVM::scheduleTierCompiles(const std::string& laneId)
{
    auto& ladder = ModuleTierLadder::instance();
    if (!ladder.enabled() || m_tier == ExecutionTier::aotO3 || m_tierPinned)
    {
        return;
    }
    Span<const uint8_t> bytes = m_scheduleBytes;
    if (bytes.size() == 0)
    {
        bytes = Span<const uint8_t>(m_moduleBytes.data(), m_moduleBytes.size());
    }
    if (bytes.size() == 0)
    {
        return;
    }
    ladder.schedule(laneId, m_moduleKey, bytes);
}

bool WasmScriptingVM::maybeUpgradeTier()
{
    auto& ladder = ModuleTierLadder::instance();
    if (!ladder.enabled() || m_tier == ExecutionTier::aotO3 || m_tierPinned)
    {
        return false;
    }
    ExecutionTier target = ExecutionTier::aotO3;
    bool hwBounds = false;
    std::string path;
#ifdef RIVE_WASM_HW_BOUNDS
    path = ladder.artifactPath(m_moduleKey, TierSpecies::hw);
    hwBounds = !path.empty();
#endif
    if (path.empty())
    {
        path = ladder.artifactPath(m_moduleKey, TierSpecies::o3);
    }
    if (path.empty() && m_tier < ExecutionTier::aotO0)
    {
        target = ExecutionTier::aotO0;
        path = ladder.artifactPath(m_moduleKey, TierSpecies::o0);
    }
    if (path.empty())
    {
        return false;
    }
    FILE* f = fopen(path.c_str(), "rb");
    if (f == nullptr)
    {
        return false;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> artifact(size);
    size_t read = fread(artifact.data(), 1, size, f);
    fclose(f);
    if (read != (size_t)size)
    {
        return false;
    }
    std::string error;
    if (!applyTierArtifact(
            Span<const uint8_t>(artifact.data(), artifact.size()),
            target,
            error,
            hwBounds))
    {
        fprintf(stderr, "wasm tier swap failed: %s\n", error.c_str());
        return false;
    }
    return true;
}

const char* WasmScriptingVM::frameBoundary()
{
    if (!valid())
    {
        return nullptr;
    }
    if (m_frameMinor)
    {
        uint32_t promoted = callModule("__riveFrameMinor", 0, nullptr);
        if (getenv("RIVE_FRAME_GC_DEBUG") != nullptr)
        {
            fprintf(stderr, "frameMinor promoted=%u\n", promoted);
            callModule("__riveFrameVerify", 0, nullptr);
        }
        if (!m_frameMinorAnnounced)
        {
            m_frameMinorAnnounced = true;
            return "script frame gc: scavenging per frame";
        }
        if (const char* warning = handleLeakWarning())
        {
            return warning;
        }
        return heapGrowthWarning();
    }
    if (const char* warning = handleLeakWarning())
    {
        return warning;
    }
    return heapGrowthWarning();
}

const char* WasmScriptingVM::heapGrowthWarning()
{
    if (!m_leakWatch || !m_advancedOnce)
    {
        return nullptr;
    }
    wasm_memory_inst_t memory =
        wasm_runtime_get_default_memory(m_state->instance);
    if (memory == nullptr)
    {
        return nullptr;
    }
    // The stub bump position beats page counts when available.
    uint32_t pages =
        m_heapUsedProbe
            ? std::max(1u, callModule("__riveHeapUsed", 0, nullptr) >> 16)
            : (uint32_t)wasm_memory_get_cur_page_count(memory);
    if (m_leakBaselinePages == 0)
    {
        m_leakBaselinePages = pages;
        m_leakFirstBaselinePages = pages;
        return nullptr;
    }
    m_leakFrames++;
    m_leakTotalFrames++;
    // 8MB past baseline over at least two seconds of frames: far beyond any
    // one-time warmup we have measured, reached in seconds by a per-frame
    // leak (box2d hand-optimized leaked ~2.1MB/s). The watch re-arms after
    // each warning, so a leaking session keeps hearing about it every 8MB
    // instead of dying hours after a single line scrolled away.
    constexpr uint32_t kLeakWarnPages = 128;
    constexpr uint32_t kLeakWarnMinFrames = 120;
    if (m_leakFrames < kLeakWarnMinFrames ||
        pages < m_leakBaselinePages + kLeakWarnPages)
    {
        return nullptr;
    }
    if (m_collectedRuntime && !m_leakArmedCollected)
    {
        // Linear memory never shrinks, so a collected runtime's one-time
        // spike would read as growth forever. Demand a second growing
        // window before the first warning.
        m_leakArmedCollected = true;
        m_leakBaselinePages = pages;
        m_leakFrames = 0;
        return nullptr;
    }
    uint32_t grownMB = (pages - m_leakBaselinePages) / 16;
    uint32_t frames = m_leakFrames;
    m_leakBaselinePages = pages;
    m_leakFrames = 0;
    m_leakWarningCount++;

    // Time to the wasmMaxPages trap from the average growth rate at 60fps.
    // Growth arrives in page-doubling steps, so this is an estimate.
    char projection[96] = {0};
    uint32_t maxPages = (uint32_t)wasm_memory_get_max_page_count(memory);
    double pagesPerFrame =
        (double)(pages - m_leakFirstBaselinePages) / (double)m_leakTotalFrames;
    if (maxPages > pages && pagesPerFrame > 0.0)
    {
        double minutes =
            (double)(maxPages - pages) / pagesPerFrame / (60.0 * 60.0);
        snprintf(projection,
                 sizeof(projection),
                 "; at this rate every frame traps in roughly %.0f minutes",
                 minutes < 1.0 ? 1.0 : minutes);
    }
    char buffer[384];
    if (m_collectedRuntime)
    {
        snprintf(buffer,
                 sizeof(buffer),
                 "script heap grew %uMB over %u frames%s; the collector is "
                 "running, so something is accumulating references (a "
                 "growing array, map, or cache). RIVE_WASM_LEAK_WARN=0 "
                 "silences this.",
                 grownMB,
                 frames,
                 projection);
    }
    else
    {
        snprintf(buffer,
                 sizeof(buffer),
                 "script heap grew %uMB over %u frames%s; the stub runtime "
                 "never frees, so per-frame allocations leak. Set "
                 "wasmRuntime: frame. RIVE_WASM_LEAK_WARN=0 silences this.",
                 grownMB,
                 frames,
                 projection);
    }
    m_leakWarning = buffer;
    return m_leakWarning.c_str();
}

static const char* handleTagName(WasmScriptingVM::HandleTable::Tag tag)
{
    using Tag = WasmScriptingVM::HandleTable::Tag;
    switch (tag)
    {
        case Tag::path:
            return "path";
        case Tag::measure:
            return "measure";
        case Tag::paint:
            return "paint";
        case Tag::renderer:
            return "renderer";
        case Tag::shader:
            return "shader";
        case Tag::object:
            return "object";
        case Tag::viewModelInstance:
            return "viewModelInstance";
        case Tag::instanceValue:
            return "instanceValue";
        case Tag::image:
            return "image";
        case Tag::font:
            return "font";
        case Tag::text:
            return "text";
        case Tag::buffer:
            return "buffer";
        case Tag::meshInstances:
            return "meshInstances";
        case Tag::canvas:
            return "canvas";
        case Tag::gpuCanvas:
            return "gpuCanvas";
        case Tag::gpuPass:
            return "gpuPass";
        case Tag::gpuBuffer:
            return "gpuBuffer";
        case Tag::gpuTexture:
            return "gpuTexture";
        case Tag::gpuSampler:
            return "gpuSampler";
        case Tag::gpuTextureView:
            return "gpuTextureView";
        case Tag::gpuShaderModule:
            return "gpuShaderModule";
        case Tag::gpuBindGroupLayout:
            return "gpuBindGroupLayout";
        case Tag::gpuBindGroup:
            return "gpuBindGroup";
        case Tag::gpuPipeline:
            return "gpuPipeline";
        case Tag::dataContext:
            return "dataContext";
        case Tag::artboard:
            return "artboard";
        case Tag::animation:
            return "animation";
        case Tag::node:
            return "node";
        case Tag::transitionChild:
            return "transitionChild";
        case Tag::audioSource:
            return "audioSource";
        case Tag::audioSound:
            return "audioSound";
        case Tag::drawable:
            return "drawable";
        case Tag::riveFile:
            return "riveFile";
        case Tag::bindableArtboard:
            return "bindableArtboard";
        case Tag::empty:
        case Tag::count:
            break;
    }
    return "unknown";
}

uint32_t WasmScriptingVM::memoryPages() const
{
    if (m_state == nullptr || m_state->instance == nullptr)
    {
        return 0;
    }
    wasm_memory_inst_t memory =
        wasm_runtime_get_default_memory(m_state->instance);
    return memory == nullptr ? 0
                             : (uint32_t)wasm_memory_get_cur_page_count(memory);
}

uint32_t WasmScriptingVM::frameMajors()
{
    return m_frameMajorsProbe ? callModule("__riveFrameMajors", 0, nullptr) : 0;
}

const char* WasmScriptingVM::handleLeakWarning()
{
    if (!m_handleWatch || !m_advancedOnce)
    {
        return nullptr;
    }
    uint32_t live =
        (uint32_t)(m_handles.slots.size() - m_handles.freeSlots.size());
    if (m_handleBaselineLive == 0)
    {
        // Bias by one so a zero-handle module still records its baseline.
        m_handleBaselineLive = live + 1;
        return nullptr;
    }
    m_handleFrames++;
    // Hundreds of handles past warmup over two seconds of frames is a
    // per-frame mint with no release, not a working set.
    constexpr uint32_t kHandleWarnCount = 512;
    constexpr uint32_t kHandleWarnMinFrames = 120;
    if (live + 1 < m_handleBaselineLive + kHandleWarnCount)
    {
        m_handleCollectPending = false;
        return nullptr;
    }
    if (m_handleFrames < kHandleWarnMinFrames)
    {
        return nullptr;
    }
    // Wrappers that outlived a boundary die in the old region, which only
    // a major sweeps, and its size trigger cannot see the host side of a
    // handle. Ask for one and only warn if the growth survives it. A bake
    // without the majors counter cannot say when that is, so it stays quiet.
    if (m_collectedRuntime && !m_frameMajorsProbe)
    {
        return nullptr;
    }
    if (m_collectedRuntime && !m_handleCollectPending)
    {
        m_handleCollectPending = true;
        m_handleCollectMajors = frameMajors();
        callModule("__riveCollect", 0, nullptr);
        return nullptr;
    }
    // A sliced major spans boundaries, so wait for the count to move.
    if (m_handleCollectPending && frameMajors() == m_handleCollectMajors)
    {
        return nullptr;
    }
    m_handleCollectPending = false;
    // Re-arm so a leaking session keeps warning every 512 handles.
    uint32_t grown = live + 1 - m_handleBaselineLive;
    uint32_t frames = m_handleFrames;
    m_handleBaselineLive = live + 1;
    m_handleFrames = 0;
    uint32_t counts[(size_t)HandleTable::Tag::count] = {0};
    for (const HandleTable::Slot& slot : m_handles.slots)
    {
        if (slot.tag != HandleTable::Tag::empty)
        {
            counts[(size_t)slot.tag]++;
        }
    }
    size_t top = 0;
    for (size_t i = 1; i < sizeof(counts) / sizeof(counts[0]); i++)
    {
        if (counts[i] > counts[top])
        {
            top = i;
        }
    }
    char buffer[256];
    snprintf(buffer,
             sizeof(buffer),
             "script leaked %u host handles over %u frames (most: %u %s); "
             "resources created per frame need release() or finish(). "
             "RIVE_WASM_LEAK_WARN=0 silences this.",
             grown,
             frames,
             counts[top],
             handleTagName((HandleTable::Tag)top));
    m_leakWarning = buffer;
    return m_leakWarning.c_str();
}

bool WasmScriptingVM::applyTierArtifact(Span<const uint8_t> artifactBytes,
                                        ExecutionTier tier,
                                        std::string& error,
                                        bool hwBounds)
{
    auto next = std::make_unique<WamrState>();
    next->artifactBytes.assign(artifactBytes.begin(), artifactBytes.end());
    char loadError[256] = {0};
    next->module = wasm_runtime_load(next->artifactBytes.data(),
                                     (uint32_t)next->artifactBytes.size(),
                                     loadError,
                                     sizeof(loadError));
    if (next->module == nullptr)
    {
        error = std::string("artifact load failed: ") + loadError;
        return false;
    }
    if (hwBounds)
    {
        wasm_runtime_set_module_hw_bounds(next->module, true);
    }
    next->instance = wasm_runtime_instantiate(next->module,
                                              512 * 1024,
                                              0,
                                              loadError,
                                              sizeof(loadError));
    if (next->instance == nullptr)
    {
        error = std::string("artifact instantiate failed: ") + loadError;
        return false;
    }
    // No ctors on the target: every byte of initialized state arrives from
    // the live instance.
    if (!wamrTransplantState(m_state->instance, next->instance, error))
    {
        return false;
    }
    next->execEnv = wasm_runtime_create_exec_env(next->instance, 512 * 1024);
    if (next->execEnv == nullptr)
    {
        error = "artifact exec env creation failed";
        return false;
    }
    wasm_runtime_set_user_data(next->execEnv, this);
    next->guestNames = std::move(m_state->guestNames);
    m_state = std::move(next);
    m_tier = tier;
    return true;
}

bool WasmScriptingVM::registerBytecode(const std::string& name,
                                       Span<const uint8_t> bytecode)
{
    uint32_t nameSizeArgs[1] = {(uint32_t)name.size() + 1};
    uint32_t namePtr = callModule("malloc", 1, nameSizeArgs);
    uint32_t bcSizeArgs[1] = {(uint32_t)bytecode.size()};
    uint32_t bcPtr = callModule("malloc", 1, bcSizeArgs);
    if (namePtr == 0 || bcPtr == 0)
    {
        m_lastError = "bytecode allocation failed";
        return false;
    }
    memcpy(resolveModulePtr(namePtr, (uint32_t)name.size() + 1),
           name.c_str(),
           name.size() + 1);
    memcpy(resolveModulePtr(bcPtr, (uint32_t)bytecode.size()),
           bytecode.data(),
           bytecode.size());

    uint32_t args[4] = {m_L, namePtr, bcPtr, (uint32_t)bytecode.size()};
    callModule("host_register_module", 4, args);

    uint32_t freeName[1] = {namePtr};
    callModule("free", 1, freeName);
    // The registry keeps its own copy via lua_pushlstring; the staging
    // buffer frees.
    uint32_t freeBc[1] = {bcPtr};
    callModule("free", 1, freeBc);
    return true;
}

#ifdef WITH_RIVE_TOOLS
void WasmScriptingVM::registerShaderRstb(std::string name,
                                         std::vector<uint8_t> bytes)
{
    m_shaderRstbs[std::move(name)] = std::move(bytes);
}

const std::vector<uint8_t>* WasmScriptingVM::findShaderRstb(
    const std::string& name) const
{
    auto it = m_shaderRstbs.find(name);
    if (it != m_shaderRstbs.end())
    {
        return &it->second;
    }
    // Registered names are library-mangled paths; scripts ask by short name.
    for (const auto& entry : m_shaderRstbs)
    {
        size_t slash = entry.first.rfind('/');
        if (slash != std::string::npos && entry.first.substr(slash + 1) == name)
        {
            return &entry.second;
        }
    }
    return nullptr;
}
#endif

bool WasmScriptingVM::requireModule(const std::string& name, int* outResultRef)
{
    uint32_t namePtr = guestString(name.c_str());
    if (namePtr == 0)
    {
        m_lastError = "module name allocation failed";
        return false;
    }

    uint32_t requireArgs[2] = {m_L, namePtr};
    uint32_t status = 0;
    CallOutcome outcome =
        callModuleChecked("host_require", 2, requireArgs, &status);

    guestFree(namePtr);

    if (outcome != CallOutcome::ok)
    {
        // Folding a trap into "status 0" once read as a successful require
        // with no generator; fail the require instead.
        m_lastError = outcome == CallOutcome::trapped
                          ? "module require trapped"
                          : "module has no host_require export";
        uint32_t topArgs[2] = {m_L, 0};
        callModule("host_settop", 2, topArgs);
        return false;
    }
    if (status != 0)
    {
        uint32_t strArgs[2] = {m_L, (uint32_t)-1};
        uint32_t messagePtr = callModule("host_tostring", 2, strArgs);
        const char* message = messagePtr != 0
                                  ? (const char*)resolveModulePtr(messagePtr, 1)
                                  : nullptr;
        m_lastError = message != nullptr ? message : "module execution failed";
    }
    else if (outResultRef != nullptr)
    {
        uint32_t refArgs[1] = {m_L};
        *outResultRef = (int)callModule("host_ref", 1, refArgs);
    }
    uint32_t topArgs[2] = {m_L, 0};
    callModule("host_settop", 2, topArgs);
    return status == 0;
}

// --- ScriptBackend over the module's host_obj_* exports ---------------------

bool WasmScriptingVM::valid() const
{
    return m_state != nullptr && m_state->execEnv != nullptr && m_L != 0 &&
           !m_poisoned;
}

void WasmScriptingVM::releaseRef(int ref)
{
    if (!valid() || ref == 0)
    {
        return;
    }
    auto it = m_contextObjects.find(ref);
    if (it != m_contextObjects.end())
    {
        m_handles.release(it->second, HandleTable::Tag::object);
        m_contextObjects.erase(it);
    }
    m_inputSlots.erase(ref);
    uint32_t args[2] = {m_L, (uint32_t)ref};
    // Listeners owned by this script instance stop immediately, mirroring the
    // Luau backend's tracked property dispose.
    callModule("host_obj_dispose", 2, args);
    callModule("host_unref", 2, args);
}

int WasmScriptingVM::instantiate(int generatorRef,
                                 ScriptedObject* object,
                                 int* outContextRef,
                                 ScriptedContext** outContextPtr)
{
    uint32_t objectHandle = m_handles.mint(HandleTable::Tag::object, object);
    uint32_t args[3] = {m_L, (uint32_t)generatorRef, objectHandle};
    int selfRef = (int)callModule("host_obj_instantiate", 3, args);
    if (selfRef == 0)
    {
        m_handles.release(objectHandle, HandleTable::Tag::object);
        return 0;
    }
    uint32_t contextArgs[1] = {m_L};
    *outContextRef = (int)callModule("host_obj_context", 1, contextArgs);
    if (*outContextRef != 0)
    {
        m_contextObjects[*outContextRef] = objectHandle;
    }
    else
    {
        m_handles.release(objectHandle, HandleTable::Tag::object);
    }
    // The wasm context lives entirely in the module until the binding layer
    // moves in; there is no host-side ScriptedContext.
    *outContextPtr = nullptr;
    return selfRef;
}

ScriptBackend::InitResult WasmScriptingVM::callUserInit(ScriptedObject* object,
                                                        int selfRef,
                                                        int contextRef)
{
    // callRet folds a trap into 0, which here would read as notImplemented
    // and mark a crashed init as done; call directly so failure is failure.
    wasm_function_inst_t f =
        wasm_runtime_lookup_function(m_state->instance, "host_obj_user_init");
    if (f == nullptr)
    {
        return InitResult::notImplemented;
    }
    uint32_t buf[3] = {m_L, (uint32_t)selfRef, (uint32_t)contextRef};
    uint32_t status = 2;
    // An init can run a nested one; each reads only its own misses.
    bool outerMissing = m_missingRequestedData;
    m_missingRequestedData = false;
    ScriptCallScope callScope(this);
    if (!wasm_runtime_call_wasm(m_state->execEnv, f, 3, buf))
    {
        const char* exception =
            fullTrapMessage(wasm_runtime_get_exception(m_state->instance));
        m_lastError = exception != nullptr ? exception : "script init trapped";
        fprintf(stderr, "script init trapped: %s\n", m_lastError.c_str());
#if WASM_ENABLE_DUMP_CALL_STACK
        wasm_runtime_dump_call_stack(m_state->execEnv);
#endif
        wasm_runtime_clear_exception(m_state->instance);
    }
    else
    {
        status = buf[0];
    }
    bool missing = m_missingRequestedData;
    m_missingRequestedData = outerMissing;
    switch (status)
    {
        case 0:
            return InitResult::notImplemented;
        case 1:
            return missing ? InitResult::failed : InitResult::succeeded;
        default:
            return InitResult::failed;
    }
}

bool WasmScriptingVM::callAdvance(ScriptedObject* object,
                                  int selfRef,
                                  float elapsedSeconds)
{
    wasm_function_inst_t f =
        wasm_runtime_lookup_function(m_state->instance, "host_obj_advance");
    if (f == nullptr)
    {
        return false;
    }
    wasm_val_t args[3];
    args[0].kind = WASM_I32;
    args[0].of.i32 = (int32_t)m_L;
    args[1].kind = WASM_I32;
    args[1].of.i32 = selfRef;
    args[2].kind = WASM_F64;
    args[2].of.f64 = elapsedSeconds;
    wasm_val_t results[1];
    results[0].kind = WASM_I32;
    ScriptCallScope callScope(this);
    if (!wasm_runtime_call_wasm_a(m_state->execEnv, f, 1, results, 3, args))
    {
        reportTrap("host_obj_advance");
        return false;
    }
    if (!m_advancedOnce)
    {
        m_advancedOnce = true;
    }
    return results[0].of.i32 != 0;
}

void WasmScriptingVM::callUpdate(ScriptedObject* object, int selfRef)
{
    // Marks issued during update must not re-arm it, like the Luau backend.
    object->setInUpdatePhase(true);
    uint32_t args[2] = {m_L, (uint32_t)selfRef};
    callModule("host_obj_update", 2, args);
    object->setInUpdatePhase(false);
}

void WasmScriptingVM::callTrigger(ScriptedObject* object,
                                  int selfRef,
                                  const char* name)
{
    uint32_t arg = 0;
    uint32_t owned = 0;
    if (!inputArg(selfRef, name, arg, owned))
    {
        return;
    }
    uint32_t args[3] = {m_L, (uint32_t)selfRef, arg};
    callModule("host_obj_trigger", 3, args);
    guestFree(owned);
}

bool WasmScriptingVM::callNumberMethod(ScriptedObject* object,
                                       int selfRef,
                                       const char* name,
                                       const float* args,
                                       size_t argCount,
                                       float* outResult)
{
    GuestName nameCopy(this, name);
    uint32_t namePtr = nameCopy.ptr;
    if (namePtr == 0)
    {
        return false;
    }
    uint32_t scratchSize = (uint32_t)(argCount * sizeof(float)) + sizeof(float);
    uint32_t sizeArgs[1] = {scratchSize};
    uint32_t scratch = callModule("malloc", 1, sizeArgs);
    if (scratch == 0)
    {
        return false;
    }
    memcpy(resolveModulePtr(scratch, scratchSize),
           args,
           argCount * sizeof(float));
    uint32_t outPtr = scratch + (uint32_t)(argCount * sizeof(float));
    uint32_t callArgs[6] =
        {m_L, (uint32_t)selfRef, namePtr, scratch, (uint32_t)argCount, outPtr};
    uint32_t ok = callModule("host_obj_number_method", 6, callArgs);
    if (ok != 0)
    {
        memcpy(outResult,
               resolveModulePtr(outPtr, sizeof(float)),
               sizeof(float));
    }
    guestFree(scratch);
    return ok != 0;
}

bool WasmScriptingVM::callBooleanMethod(ScriptedObject* object,
                                        int selfRef,
                                        const char* name)
{
    GuestName nameCopy(this, name);
    uint32_t namePtr = nameCopy.ptr;
    if (namePtr == 0)
    {
        return false;
    }
    uint32_t args[3] = {m_L, (uint32_t)selfRef, namePtr};
    uint32_t result = callModule("host_obj_boolean_method", 3, args);
    return result != 0;
}

bool WasmScriptingVM::callPathEffectUpdate(ScriptedObject* object,
                                           int selfRef,
                                           const RawPath& sourcePath,
                                           const ShapePaint* shapePaint,
                                           RawPath* outPath)
{
    if (!valid() || shapePaint == nullptr || outPath == nullptr)
    {
        return false;
    }
    // The exact subset the Luau lane's ScriptedPaintData(ShapePaint*)
    // snapshot captures; gradients stay host side there too.
    PathEffectPaintWire paint;
    paint.style = (uint32_t)RenderPaintStyle::fill;
    paint.blendMode = (uint32_t)BlendMode::srcOver;
    paint.thickness = 1;
    paint.color = 0xFF000000;
    if (shapePaint->is<Stroke>())
    {
        auto stroke = shapePaint->as<Stroke>();
        paint.style = (uint32_t)RenderPaintStyle::stroke;
        paint.thickness = stroke->thickness();
        paint.cap = stroke->cap();
        paint.join = stroke->join();
    }
    for (auto& child : shapePaint->children())
    {
        if (child->is<SolidColor>())
        {
            paint.color = child->as<SolidColor>()->colorValue();
            break;
        }
    }
    if (shapePaint->feather() != nullptr)
    {
        paint.feather = shapePaint->feather()->strength();
    }
    paint.blendMode = (uint32_t)shapePaint->blendModeValue();

    uint32_t verbCount = (uint32_t)sourcePath.verbs().size();
    uint32_t floatCount = (uint32_t)sourcePath.points().size() * 2;
    uint32_t pointBytes = floatCount * (uint32_t)sizeof(float);
    // One staging block, float-aligned parts first: [paint][points][verbs].
    uint32_t byteCount = (uint32_t)sizeof(paint) + pointBytes + verbCount;
    uint32_t sizeArgs[1] = {byteCount};
    uint32_t dataPtr = callModule("malloc", 1, sizeArgs);
    if (dataPtr == 0)
    {
        return false;
    }
    uint8_t* out = (uint8_t*)resolveModulePtr(dataPtr, byteCount);
    memcpy(out, &paint, sizeof(paint));
    memcpy(out + sizeof(paint), sourcePath.points().data(), pointBytes);
    memcpy(out + sizeof(paint) + pointBytes,
           sourcePath.verbs().data(),
           verbCount);
    m_pathEffectOut = outPath;
    // _v2 also gets a node over the shape, the Luau lane's NodeData; modules
    // built before it get the paint alone.
    bool withNode =
        wasm_runtime_lookup_function(m_state->instance,
                                     "host_obj_path_effect_v2") != nullptr;
    size_t scopeStart = m_scopedNodes.size();
    uint32_t nodeHandle = 0;
    TransformComponent* shape = shapePaint->parentTransformComponent();
    if (withNode && shape != nullptr)
    {
        nodeHandle = mintNode(this, nullptr, shape);
    }
    uint32_t args[8] = {m_L,
                        (uint32_t)selfRef,
                        dataPtr + (uint32_t)sizeof(paint) + pointBytes,
                        verbCount,
                        dataPtr + (uint32_t)sizeof(paint),
                        floatCount,
                        dataPtr,
                        nodeHandle};
    uint32_t ok = withNode ? callModule("host_obj_path_effect_v2", 8, args)
                           : callModule("host_obj_path_effect", 7, args);
    releaseScopedNodes(scopeStart);
    m_pathEffectOut = nullptr;
    guestFree(dataPtr);
    return ok != 0;
}

void WasmScriptingVM::releaseScopedNodes(size_t from)
{
    for (size_t i = from; i < m_scopedNodes.size(); i++)
    {
        uint32_t handle = m_scopedNodes[i];
        if (auto node = static_cast<HostNode*>(
                m_handles.resolve(handle, HandleTable::Tag::node)))
        {
            m_handles.release(handle, HandleTable::Tag::node);
            delete node;
        }
    }
    m_scopedNodes.resize(from);
}

bool WasmScriptingVM::callDataConvert(ScriptedObject* object,
                                      int selfRef,
                                      const char* method,
                                      DataValue* input,
                                      ScriptDataResult* outResult)
{
    if (!valid() || input == nullptr)
    {
        return false;
    }
    DataConvertWire wire;
    std::string text;
    if (input->is<DataValueNumber>())
    {
        wire.kind = DataConvertWire::kindNumber;
        wire.number = input->as<DataValueNumber>()->value();
    }
    else if (input->is<DataValueString>())
    {
        wire.kind = DataConvertWire::kindString;
        const std::string& value = input->as<DataValueString>()->value();
        text = moduleStringBytes(this, value.data(), value.size());
        wire.stringLength = (uint32_t)text.size();
    }
    else if (input->is<DataValueBoolean>())
    {
        wire.kind = DataConvertWire::kindBoolean;
        wire.boolean = input->as<DataValueBoolean>()->value() ? 1 : 0;
    }
    else if (input->is<DataValueColor>())
    {
        wire.kind = DataConvertWire::kindColor;
        wire.color = input->as<DataValueColor>()->value();
    }
    else
    {
        // Unsupported input kinds report handled with no result, so the
        // converter yields an empty DataValue like the Luau backend.
        return true;
    }
    GuestName methodCopy(this, method);
    uint32_t methodPtr = methodCopy.ptr;
    if (methodPtr == 0)
    {
        return false;
    }
    uint32_t byteCount = (uint32_t)sizeof(wire) + wire.stringLength;
    uint32_t sizeArgs[1] = {byteCount};
    uint32_t wirePtr = callModule("malloc", 1, sizeArgs);
    if (wirePtr == 0)
    {
        return false;
    }
    uint8_t* out = (uint8_t*)resolveModulePtr(wirePtr, byteCount);
    memcpy(out, &wire, sizeof(wire));
    memcpy(out + sizeof(wire), text.data(), wire.stringLength);
    m_convertResultOut = outResult;
    uint32_t args[4] = {m_L, (uint32_t)selfRef, methodPtr, wirePtr};
    uint32_t ran = callModule("host_obj_data_convert", 4, args);
    m_convertResultOut = nullptr;
    guestFree(wirePtr);
    return ran != 0;
}

bool WasmScriptingVM::callPointerEvent(ScriptedObject* object,
                                       int selfRef,
                                       const char* method,
                                       int pointerId,
                                       Vec2D localPosition,
                                       ListenerType hitType,
                                       float timeStamp,
                                       HitResult* outResult)
{
    if (!valid())
    {
        return false;
    }
    // _v2 carries the listener type and time stamp; modules built before it
    // export only the six-argument name, which must be called as such.
    wasm_function_inst_t f =
        wasm_runtime_lookup_function(m_state->instance,
                                     "host_obj_pointer_event_v2");
    bool legacy = f == nullptr;
    if (legacy)
    {
        f = wasm_runtime_lookup_function(m_state->instance,
                                         "host_obj_pointer_event");
    }
    if (f == nullptr)
    {
        return false;
    }
    GuestName methodCopy(this, method);
    uint32_t methodPtr = methodCopy.ptr;
    if (methodPtr == 0)
    {
        return false;
    }
    wasm_val_t args[8];
    args[0].kind = WASM_I32;
    args[0].of.i32 = (int32_t)m_L;
    args[1].kind = WASM_I32;
    args[1].of.i32 = selfRef;
    args[2].kind = WASM_I32;
    args[2].of.i32 = (int32_t)methodPtr;
    args[3].kind = WASM_I32;
    args[3].of.i32 = pointerId;
    args[4].kind = WASM_F64;
    args[4].of.f64 = localPosition.x;
    args[5].kind = WASM_F64;
    args[5].of.f64 = localPosition.y;
    args[6].kind = WASM_I32;
    args[6].of.i32 = (int32_t)hitType;
    args[7].kind = WASM_F64;
    args[7].of.f64 = timeStamp;
    wasm_val_t results[1];
    results[0].kind = WASM_I32;
    ScriptCallScope callScope(this);
    if (!wasm_runtime_call_wasm_a(m_state->execEnv,
                                  f,
                                  1,
                                  results,
                                  legacy ? 6 : 8,
                                  args))
    {
        reportTrap("host_obj_pointer_event");
        return false;
    }
    if (results[0].of.i32 == 0)
    {
        return false;
    }
    *outResult = (HitResult)(results[0].of.i32 - 1);
    return true;
}

bool WasmScriptingVM::callKeyboardEvent(ScriptedObject* object,
                                        int selfRef,
                                        Key key,
                                        KeyModifiers modifiers,
                                        bool isPressed,
                                        bool isRepeat)
{
    if (!valid())
    {
        return false;
    }
    uint32_t args[6] = {m_L,
                        (uint32_t)selfRef,
                        (uint32_t)key,
                        (uint32_t)modifiers,
                        isPressed ? 1u : 0u,
                        isRepeat ? 1u : 0u};
    return callModule("host_obj_keyboard_event", 6, args) != 0;
}

bool WasmScriptingVM::callTextEvent(ScriptedObject* object,
                                    int selfRef,
                                    const std::string& text)
{
    if (!valid())
    {
        return false;
    }
    // Length crosses explicitly, so embedded nulls survive.
    std::string bytes = moduleStringBytes(this, text.data(), text.size());
    uint32_t sizeArgs[1] = {(uint32_t)bytes.size() + 1};
    uint32_t textPtr = callModule("malloc", 1, sizeArgs);
    if (textPtr == 0)
    {
        return false;
    }
    memcpy(resolveModulePtr(textPtr, (uint32_t)bytes.size() + 1),
           bytes.data(),
           bytes.size());
    uint32_t args[4] = {m_L,
                        (uint32_t)selfRef,
                        textPtr,
                        (uint32_t)bytes.size()};
    uint32_t result = callModule("host_obj_text_event", 4, args);
    guestFree(textPtr);
    return result != 0;
}

bool WasmScriptingVM::callGamepadEvent(ScriptedObject* object,
                                       int selfRef,
                                       const char* method,
                                       const ListenerInvocation& invocation)
{
    if (!valid())
    {
        return false;
    }
    GamepadWire wire;
    const GamepadSnapshot* snapshot = nullptr;
    if (!packGamepadWire(invocation, wire, snapshot))
    {
        return false;
    }
    GuestName methodCopy(this, method);
    uint32_t methodPtr = methodCopy.ptr;
    if (methodPtr == 0)
    {
        return false;
    }
    uint32_t byteCount = gamepadPayloadSize(wire);
    uint32_t sizeArgs[1] = {byteCount};
    uint32_t dataPtr = callModule("malloc", 1, sizeArgs);
    if (dataPtr == 0)
    {
        return false;
    }
    writeGamepadPayload((uint8_t*)resolveModulePtr(dataPtr, byteCount),
                        wire,
                        snapshot);
    uint32_t args[5] = {m_L, (uint32_t)selfRef, methodPtr, dataPtr, byteCount};
    uint32_t result = callModule("host_obj_gamepad_event", 5, args);
    guestFree(dataPtr);
    return result != 0;
}

void WasmScriptingVM::callListenerPerform(ScriptedObject* object,
                                          int selfRef,
                                          const ListenerInvocation& invocation)
{
    if (!valid())
    {
        return;
    }
    ListenerWire wire;
    wire.kind = (uint32_t)invocation.kind();
    std::string textBytes;
    const std::string* text = nullptr;
    GamepadWire gamepad;
    const GamepadSnapshot* snapshot = nullptr;
    bool hasGamepad = false;
    if (const PointerInvocation* p = invocation.asPointer())
    {
        wire.posX = p->position.x;
        wire.posY = p->position.y;
        wire.prevX = p->previousPosition.x;
        wire.prevY = p->previousPosition.y;
        wire.pointerId = p->pointerId;
        wire.hitEvent = (uint32_t)p->hitEvent;
        wire.timeStamp = p->timeStamp;
    }
    else if (const KeyboardInvocation* k = invocation.asKeyboard())
    {
        wire.key = (uint32_t)k->key;
        wire.modifiers = (uint32_t)k->modifiers;
        wire.isPressed = k->isPressed ? 1 : 0;
        wire.isRepeat = k->isRepeat ? 1 : 0;
    }
    else if (const TextInputInvocation* t = invocation.asTextInput())
    {
        textBytes = moduleStringBytes(this, t->text.data(), t->text.size());
        text = &textBytes;
        wire.textLength = (uint32_t)textBytes.size();
    }
    else if (const FocusInvocation* f = invocation.asFocus())
    {
        wire.isFocus = f->isFocus ? 1 : 0;
    }
    else if (const ReportedEventInvocation* e = invocation.asReportedEvent())
    {
        wire.delaySeconds = e->delaySeconds;
    }
    else if (const SemanticInvocation* s = invocation.asSemantic())
    {
        wire.semanticAction = (uint32_t)s->actionType;
    }
    else
    {
        // viewModelChange and none carry only their kind; the gamepad kinds
        // append their own payload.
        hasGamepad = packGamepadWire(invocation, gamepad, snapshot);
    }
    uint32_t tailBytes = text != nullptr
                             ? wire.textLength
                             : (hasGamepad ? gamepadPayloadSize(gamepad) : 0);
    uint32_t byteCount = (uint32_t)sizeof(wire) + tailBytes;
    uint32_t sizeArgs[1] = {byteCount};
    uint32_t dataPtr = callModule("malloc", 1, sizeArgs);
    if (dataPtr == 0)
    {
        return;
    }
    uint8_t* out = (uint8_t*)resolveModulePtr(dataPtr, byteCount);
    memcpy(out, &wire, sizeof(wire));
    if (text != nullptr)
    {
        memcpy(out + sizeof(wire), text->data(), wire.textLength);
    }
    else if (hasGamepad)
    {
        writeGamepadPayload(out + sizeof(wire), gamepad, snapshot);
    }
    uint32_t args[4] = {m_L, (uint32_t)selfRef, dataPtr, byteCount};
    callModule("host_obj_listener_perform", 4, args);
    guestFree(dataPtr);
}

// A module predating Transition cannot draw the incoming child, so the
// runtime keeps drawing it.
bool WasmScriptingVM::transitionManagesTo(int selfRef)
{
    if (!valid() || wasm_runtime_lookup_function(
                        m_state->instance,
                        "host_obj_transition_manages_to") == nullptr)
    {
        return false;
    }
    uint32_t args[2] = {m_L, (uint32_t)selfRef};
    return callModule("host_obj_transition_manages_to", 2, args) != 0;
}

uint32_t WasmScriptingVM::mintTransitionChild(const TransitionChildRef& ref)
{
    return ref.artboard == nullptr
               ? 0
               : m_handles.mint(HandleTable::Tag::transitionChild,
                                const_cast<TransitionChildRef*>(&ref));
}

void WasmScriptingVM::releaseTransitionChild(uint32_t handle)
{
    if (handle != 0)
    {
        m_handles.release(handle, HandleTable::Tag::transitionChild);
    }
}

void WasmScriptingVM::callTransitionChanged(ScriptedObject* object,
                                            int selfRef,
                                            const TransitionChildRef& from,
                                            const TransitionChildRef& to,
                                            int direction)
{
    if (!valid())
    {
        return;
    }
    uint32_t fromHandle = mintTransitionChild(from);
    uint32_t toHandle = mintTransitionChild(to);
    uint32_t args[5] = {m_L,
                        (uint32_t)selfRef,
                        fromHandle,
                        toHandle,
                        (uint32_t)direction};
    callModule("host_obj_transition_changed", 5, args);
    releaseTransitionChild(fromHandle);
    releaseTransitionChild(toHandle);
}

void WasmScriptingVM::callTransitionDraw(ScriptedObject* object,
                                         int selfRef,
                                         Renderer* renderer,
                                         const TransitionChildRef& from,
                                         const TransitionChildRef& to)
{
    if (!valid())
    {
        return;
    }
    uint32_t rendererHandle =
        m_handles.mint(HandleTable::Tag::renderer, renderer);
    uint32_t fromHandle = mintTransitionChild(from);
    uint32_t toHandle = mintTransitionChild(to);
    uint32_t args[5] = {m_L,
                        (uint32_t)selfRef,
                        rendererHandle,
                        fromHandle,
                        toHandle};
    callModule("host_obj_transition_draw", 5, args);
    releaseTransitionChild(fromHandle);
    releaseTransitionChild(toHandle);
    m_handles.release(rendererHandle, HandleTable::Tag::renderer);
}

void WasmScriptingVM::callLayoutResize(ScriptedObject* object,
                                       int selfRef,
                                       Vec2D size)
{
    if (!valid())
    {
        return;
    }
    // _v2 carries the surface scale; vm modules built before it export
    // only the four-argument name, which must be called as such.
    wasm_function_inst_t f =
        wasm_runtime_lookup_function(m_state->instance,
                                     "host_obj_layout_resize_v2");
    bool legacy = f == nullptr;
    if (legacy)
    {
        f = wasm_runtime_lookup_function(m_state->instance,
                                         "host_obj_layout_resize");
    }
    if (f == nullptr)
    {
        return;
    }
    wasm_val_t args[5];
    args[0].kind = WASM_I32;
    args[0].of.i32 = (int32_t)m_L;
    args[1].kind = WASM_I32;
    args[1].of.i32 = selfRef;
    args[2].kind = WASM_F64;
    args[2].of.f64 = size.x;
    args[3].kind = WASM_F64;
    args[3].of.f64 = size.y;
    args[4].kind = WASM_F64;
    args[4].of.f64 = displayScale();
    ScriptCallScope callScope(this);
    if (!wasm_runtime_call_wasm_a(m_state->execEnv,
                                  f,
                                  0,
                                  nullptr,
                                  legacy ? 4 : 5,
                                  args))
    {
        reportTrap("host_obj_layout_resize");
    }
}

bool WasmScriptingVM::callLayoutMeasure(ScriptedObject* object,
                                        int selfRef,
                                        Vec2D* outSize)
{
    if (!valid())
    {
        return false;
    }
    uint32_t sizeArgs[1] = {2 * sizeof(float)};
    uint32_t scratch = callModule("malloc", 1, sizeArgs);
    if (scratch == 0)
    {
        return false;
    }
    // Prefill so the module's error paths leave outSize untouched, like the
    // Luau backend.
    float* out = (float*)resolveModulePtr(scratch, 2 * sizeof(float));
    out[0] = outSize->x;
    out[1] = outSize->y;
    uint32_t args[3] = {m_L, (uint32_t)selfRef, scratch};
    uint32_t measured = callModule("host_obj_layout_measure", 3, args);
    if (measured != 0)
    {
        out = (float*)resolveModulePtr(scratch, 2 * sizeof(float));
        outSize->x = out[0];
        outSize->y = out[1];
    }
    guestFree(scratch);
    return measured != 0;
}

int32_t WasmScriptingVM::inputSlot(int selfRef, const char* name)
{
    auto& slots = m_inputSlots[selfRef];
    auto it = slots.find(name);
    if (it != slots.end())
    {
        return it->second;
    }
    int32_t slot = -1;
    uint32_t namePtr = guestString(name);
    if (namePtr != 0)
    {
        uint32_t args[3] = {m_L, (uint32_t)selfRef, namePtr};
        slot = (int32_t)callModule("host_obj_input_slot", 3, args);
        guestFree(namePtr);
    }
    slots.emplace(name, slot);
    return slot;
}

bool WasmScriptingVM::legacyInputs() const
{
    return wasm_runtime_lookup_function(m_state->instance,
                                        "host_obj_input_slot") == nullptr;
}

bool WasmScriptingVM::inputArg(int selfRef,
                               const char* name,
                               uint32_t& arg,
                               uint32_t& owned)
{
    owned = 0;
    if (legacyInputs())
    {
        owned = arg = guestString(name);
        return arg != 0;
    }
    int32_t slot = inputSlot(selfRef, name);
    if (slot < 0)
    {
        return false;
    }
    arg = (uint32_t)slot;
    return true;
}

void WasmScriptingVM::setInputBoolean(int selfRef, const char* name, bool value)
{
    uint32_t arg = 0;
    uint32_t owned = 0;
    if (!inputArg(selfRef, name, arg, owned))
    {
        return;
    }
    uint32_t args[4] = {m_L, (uint32_t)selfRef, arg, value ? 1u : 0u};
    callModule("host_obj_set_boolean", 4, args);
    guestFree(owned);
}

void WasmScriptingVM::setInputNumber(int selfRef, const char* name, float value)
{
    uint32_t arg = 0;
    uint32_t owned = 0;
    if (!inputArg(selfRef, name, arg, owned))
    {
        return;
    }
    wasm_function_inst_t f =
        wasm_runtime_lookup_function(m_state->instance, "host_obj_set_number");
    if (f != nullptr)
    {
        wasm_val_t args[4];
        args[0].kind = WASM_I32;
        args[0].of.i32 = (int32_t)m_L;
        args[1].kind = WASM_I32;
        args[1].of.i32 = selfRef;
        args[2].kind = WASM_I32;
        args[2].of.i32 = (int32_t)arg;
        args[3].kind = WASM_F64;
        args[3].of.f64 = value;
        ScriptCallScope callScope(this);
        if (!wasm_runtime_call_wasm_a(m_state->execEnv, f, 0, nullptr, 4, args))
        {
            reportTrap("host_obj_set_number");
        }
    }
    guestFree(owned);
}

void WasmScriptingVM::setInputUnsigned(int selfRef,
                                       const char* name,
                                       uint32_t value)
{
    uint32_t arg = 0;
    uint32_t owned = 0;
    if (!inputArg(selfRef, name, arg, owned))
    {
        return;
    }
    uint32_t args[4] = {m_L, (uint32_t)selfRef, arg, value};
    callModule(owned != 0 ? "host_obj_set_unsigned" : "host_obj_set_color",
               4,
               args);
    guestFree(owned);
}

void WasmScriptingVM::setInputString(int selfRef,
                                     const char* name,
                                     const char* value)
{
    uint32_t arg = 0;
    uint32_t owned = 0;
    if (!inputArg(selfRef, name, arg, owned))
    {
        return;
    }
    uint32_t valuePtr = guestString(value);
    if (valuePtr != 0)
    {
        uint32_t args[4] = {m_L, (uint32_t)selfRef, arg, valuePtr};
        callModule("host_obj_set_string", 4, args);
        guestFree(valuePtr);
    }
    guestFree(owned);
}

void WasmScriptingVM::setInputViewModel(int selfRef,
                                        const char* name,
                                        ViewModelInstanceValue* value)
{
    if (!valid() || value == nullptr)
    {
        return;
    }
    switch (value->coreType())
    {
        case ViewModelInstanceViewModelBase::typeKey:
        {
            auto viewModel = value->as<ViewModelInstanceViewModel>();
            auto vmi = viewModel->referenceViewModelInstance();
            if (vmi == nullptr)
            {
                fprintf(stderr,
                        "setInputViewModel - passed in a "
                        "ViewModelInstanceViewModel with no associated "
                        "ViewModelInstance.\n");
                return;
            }
            uint32_t arg = 0;
            uint32_t owned = 0;
            if (!inputArg(selfRef, name, arg, owned))
            {
                return;
            }
            // The module's view model userdata owns the handle and releases
            // it from its finalizer.
            uint32_t handle =
                m_handles.mint(HandleTable::Tag::viewModelInstance,
                               new HostViewModelInstance{std::move(vmi)});
            uint32_t args[4] = {m_L, (uint32_t)selfRef, arg, handle};
            callModule("host_obj_set_view_model", 4, args);
            guestFree(owned);
            break;
        }
        default:
            // Nothing assigned; leave self untouched, like the Luau backend.
            break;
    }
}

void WasmScriptingVM::setInputArtboard(int selfRef,
                                       const char* name,
                                       ScriptedObject* object,
                                       Artboard* artboard)
{
    if (!valid() || object == nullptr || object->scriptAsset() == nullptr ||
        object->scriptAsset()->file() == nullptr || artboard == nullptr)
    {
        return;
    }
    uint32_t arg = 0;
    uint32_t owned = 0;
    if (!inputArg(selfRef, name, arg, owned))
    {
        return;
    }
    auto artboardInstance = artboard->instance();
    artboardInstance->frameOrigin(false);
    // The module's artboard userdata owns the handle and releases it from
    // its finalizer.
    uint32_t handle =
        m_handles.mint(HandleTable::Tag::artboard,
                       new HostArtboard(object->scriptAsset()->file(),
                                        std::move(artboardInstance),
                                        nullptr,
                                        object->dataContext()));
    uint32_t args[4] = {m_L, (uint32_t)selfRef, arg, handle};
    callModule("host_obj_set_artboard", 4, args);
    guestFree(owned);
}

uint32_t WasmScriptingVM::guestString(const char* text, const char* allocator)
{
    if (text == nullptr)
    {
        return 0;
    }
    std::string bytes = moduleStringBytes(this, text, strlen(text));
    // The terminator is one unit wide.
    bytes.append(m_utf16HostStrings ? 2 : 1, '\0');
    uint32_t sizeArgs[1] = {(uint32_t)bytes.size()};
    uint32_t ptr = callModule(allocator, 1, sizeArgs);
    if (ptr != 0)
    {
        memcpy(resolveModuleWritePtr(ptr, (uint32_t)bytes.size()),
               bytes.data(),
               bytes.size());
    }
    return ptr;
}

WasmScriptingVM::GuestName::GuestName(WasmScriptingVM* vm, const char* name) :
    m_vm(vm)
{
    if (vm->m_state->lookupExport("__riveRawAlloc") == nullptr)
    {
        ptr = vm->guestString(name);
        m_owned = true;
        return;
    }
    auto& names = vm->m_state->guestNames;
    auto it = names.find(name);
    if (it != names.end())
    {
        ptr = it->second;
        return;
    }
    ptr = vm->guestString(name, "__riveRawAlloc");
    if (ptr != 0)
    {
        names.emplace(name, ptr);
    }
}

WasmScriptingVM::GuestName::~GuestName()
{
    if (m_owned)
    {
        m_vm->guestFree(ptr);
    }
}

void WasmScriptingVM::guestFree(uint32_t ptr)
{
    if (ptr == 0)
    {
        return;
    }
    uint32_t args[1] = {ptr};
    callModule("free", 1, args);
}

// Browser-lane exports over the same impl cores; nothing off emscripten.
#include "wasm_natives_web_gen.hpp"

#endif
