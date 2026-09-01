#ifdef WITH_RIVE_SCRIPTING_WASM

#include "rive/wasm/module_tier_ladder.hpp"
#include "rive/wasm/wamr_state_transplant.hpp"
#include "rive/wasm/wasm_scripting_vm.hpp"

#include "rive/animation/linear_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/listener_invocation.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/assets/blob_asset.hpp"
#include "rive/bones/root_bone.hpp"
#include "rive/constraints/constraint.hpp"
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
#include "rive/renderer.hpp"
#include "rive/scripted/scripted_object.hpp"
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

#ifdef RIVE_DECODERS
#include "rive/decoders/bitmap_decoder.hpp"

#include <chrono>
#include <sys/stat.h>
#endif

#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/ore/ore_context.hpp"
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
#endif

#include "wasm_export.h"

#include <chrono>
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

} // namespace

struct rive::WasmScriptingVMNatives
{
    static void print(WasmScriptingVM* vm, const char* data, size_t size);
};

// Loaded modules are immutable and shared: fast-interp translation costs
// ~11ms per load while instantiation is microseconds, and every file
// instance of the same content reloads identical bytes. Entries live for
// the process; the byte buffer must outlive the module (wasm_runtime_load
// keeps referencing it).
static void pregrowAotMemory(wasm_module_inst_t instance);

struct SharedWasmModule
{
    std::vector<uint8_t> bytes;
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

uint32_t WasmScriptingVM::callModule(const char* name,
                                     uint32_t argc,
                                     uint32_t* argv)
{
    uint32_t result = 0;
    callModuleChecked(name, argc, argv, &result);
    return result;
}

WasmScriptingVM::CallOutcome WasmScriptingVM::callModuleChecked(
    const char* name,
    uint32_t argc,
    uint32_t* argv,
    uint32_t* result)
{
    wasm_module_inst_t inst = m_state->instance;
    wasm_function_inst_t f = wasm_runtime_lookup_function(inst, name);
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
    if (!wasm_runtime_call_wasm(m_state->execEnv, f, argc, buf))
    {
        // A silent fold hides real traps; name them so a script that dies
        // mid-call is diagnosable instead of a mystery no-op.
        const char* exception = wasm_runtime_get_exception(inst);
        if (exception != nullptr)
        {
            fprintf(stderr, "wasm call trapped in %s: %s\n", name, exception);
            if (m_leakWarningCount > 0 && !m_leakTrapContextPrinted)
            {
                // A bare trap after leak warnings is almost always the
                // memory ceiling; say so once for hosts that dropped the
                // warning strings.
                m_leakTrapContextPrinted = true;
                fprintf(stderr,
                        "wasm call trapped after %u script heap leak "
                        "warnings; the module likely hit its wasmMaxPages "
                        "ceiling\n",
                        m_leakWarningCount);
            }
            wasm_runtime_clear_exception(inst);
        }
        return CallOutcome::trapped;
    }
    *result = buf[0];
    return CallOutcome::ok;
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

void WasmScriptingVM::raiseModuleError(const char* message)
{
    wasm_runtime_set_exception(m_state->instance, message);
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

uint32_t strftimeNative(wasm_exec_env_t env,
                        uint32_t a,
                        uint32_t b,
                        uint32_t c,
                        uint32_t d)
{
    return 0;
}
void tzsetJs(wasm_exec_env_t env,
             uint32_t a,
             uint32_t b,
             uint32_t c,
             uint32_t d)
{}
void localtimeJs(wasm_exec_env_t env, uint32_t a, uint32_t b, uint32_t c) {}
void gmtimeJs(wasm_exec_env_t env, uint32_t a, uint32_t b, uint32_t c) {}

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

NativeSymbol kEnvNatives[] = {
    {"invoke_vii", (void*)invokeViiNative, "(iii)", nullptr},
    {"_emscripten_throw_longjmp", (void*)throwLongjmpNative, "()", nullptr},
    {"_abort_js", (void*)abortJs, "()", nullptr},
    {"_emscripten_memcpy_js", (void*)memcpyJs, "(iii)", nullptr},
    {"emscripten_get_now", (void*)getNow, "()F", nullptr},
    {"emscripten_date_now", (void*)dateNow, "()F", nullptr},
    {"emscripten_resize_heap", (void*)resizeHeap, "(i)i", nullptr},
    {"strftime", (void*)strftimeNative, "(iiii)i", nullptr},
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

NativeSymbol kWasiNatives[] = {
    {"fd_write", (void*)fdWrite, "(i*i*)i", nullptr},
    {"fd_seek", (void*)fdSeek, "(iiiii)i", nullptr},
};

// --- rive_path/paint/renderer_v1: handle-backed render objects --------------

// The module's ModuleRenderPath mirror; geometry arrives through update and
// rebuilds the real render path.
struct HostPath
{
    rcp<RenderPath> path;
};

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
    RawPath rawPath(Span<const PathVerb>((const PathVerb*)verbs, verbCount),
                    Span<const Vec2D>((const Vec2D*)points, floatCount / 2));
    hostPath->path = vm->factory()->makeRenderPath(rawPath, (FillRule)fillRule);
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

struct HostImage
{
    rcp<RenderImage> image;
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
    std::string key(name, length);
    for (const auto& asset : object->scriptAsset()->file()->assets())
    {
        if (asset->is<ImageAsset>() && asset->name() == key)
        {
            RenderImage* renderImage = asset->as<ImageAsset>()->renderImage();
            if (renderImage == nullptr)
            {
                return 0;
            }
            return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::image,
                                      new HostImage{ref_rcp(renderImage)});
        }
    }
    return 0;
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
    ore::Context* context;

    ~HostGpuPass()
    {
        // Mirror the Luau wrapper: never leave the active-pass slot dangling.
        if (context != nullptr && pass != nullptr &&
            context->activeRenderPass() == pass.get())
        {
            context->setActiveRenderPass(nullptr);
        }
    }
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

uint32_t gpuCanvasNewImpl(WasmScriptingVM* vm, uint32_t width, uint32_t height)
{
    if (vm == nullptr || vm->factory() == nullptr || width == 0 || height == 0)
    {
        return 0;
    }
    auto* renderContext =
        static_cast<gpu::RenderContext*>(vm->factory()->renderContext());
    ore::Context* oreContext = gpuOreContext(vm);
    if (renderContext == nullptr || oreContext == nullptr)
    {
        return 0;
    }
    auto canvas = vm->factory()->deferredCanvasHost() != nullptr
                      ? renderContext->makeDeferredRenderCanvas(width, height)
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
    props[0] = host->canvas->width();
    props[1] = host->canvas->height();
    props[2] = (uint32_t)oreContext->canvasTargetFormat();
    props[3] = 1;
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
                                  host->canvas->renderImage()))});
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
    auto canvas = vm->factory()->deferredCanvasHost() != nullptr
                      ? renderContext->makeDeferredRenderCanvas(width, height)
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
    return 19;
}

uint32_t gpuPassBeginImpl(WasmScriptingVM* vm,
                          const rive_gpu_pass_desc_v1* podDesc,
                          uint32_t descByteCount,
                          const rive_gpu_pass_color_attachment_v1* colors,
                          uint32_t colorByteCount)
{
    ore::Context* oreContext = gpuOreContext(vm);
    if (vm == nullptr || oreContext == nullptr ||
        descByteCount < sizeof(*podDesc) ||
        colorByteCount % sizeof(*colors) != 0 ||
        podDesc->colorCount != colorByteCount / sizeof(*colors) ||
        podDesc->colorCount > 4)
    {
        return 0;
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
    // One active encoder per command buffer; finish a stale pass first,
    // matching the Luau binding.
    if (oreContext->activeRenderPass() != nullptr &&
        !oreContext->activeRenderPass()->isFinished())
    {
        oreContext->activeRenderPass()->finish();
        oreContext->setActiveRenderPass(nullptr);
    }
    auto pass =
        ore::cmd::beginRenderPassRecordingOrImmediate(*oreContext, desc);
    if (pass == nullptr)
    {
        return 0;
    }
    oreContext->setActiveRenderPass(pass.get());
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::gpuPass,
                              new HostGpuPass{std::move(pass), oreContext});
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
    // Same dispatch as Image:view() in the Luau binding: deferred images
    // record by resource id, everything else wraps as a canvas image.
    rcp<ore::TextureView> view;
    if (auto* deferredImage =
            lite_rtti_cast<cmd::DeferredRenderImage*>(host->image.get()))
    {
        view = oreContext->recordWrapImageView(deferredImage->id(),
                                               host->image->width(),
                                               host->image->height());
    }
    else
    {
        view = oreContext->recordWrapCanvasImage(host->image.get(),
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

ore::RenderPass* resolvePass(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return nullptr;
    }
    auto host = static_cast<HostGpuPass*>(
        vm->handles().resolve(handle,
                              WasmScriptingVM::HandleTable::Tag::gpuPass));
    return host != nullptr ? host->pass.get() : nullptr;
}

void gpuPassSetPipelineImpl(WasmScriptingVM* vm,
                            uint32_t passHandle,
                            uint32_t pipelineHandle)
{
    auto* pass = resolvePass(vm, passHandle);
    auto pipeline = static_cast<HostGpuPipeline*>(
        vm->handles().resolve(pipelineHandle,
                              WasmScriptingVM::HandleTable::Tag::gpuPipeline));
    if (pass == nullptr || pipeline == nullptr)
    {
        return;
    }
    pass->setPipeline(pipeline->pipeline.get());
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
    auto* pass = resolvePass(vm, passHandle);
    auto bindGroup = static_cast<HostGpuBindGroup*>(
        vm->handles().resolve(bindGroupHandle,
                              WasmScriptingVM::HandleTable::Tag::gpuBindGroup));
    if (pass == nullptr || bindGroup == nullptr ||
        dynamicOffsetByteCount % sizeof(uint32_t) != 0)
    {
        return;
    }
    uint32_t count = dynamicOffsetByteCount / (uint32_t)sizeof(uint32_t);
    pass->setBindGroup(groupIndex,
                       bindGroup->bindGroup.get(),
                       count != 0 ? dynamicOffsets : nullptr,
                       count);
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
    if (auto* pass = resolvePass(vm, passHandle))
    {
        pass->draw(vertexCount, instanceCount, firstVertex, firstInstance);
    }
}

void gpuPassDrawIndexedImpl(WasmScriptingVM* vm,
                            uint32_t passHandle,
                            uint32_t indexCount,
                            uint32_t instanceCount,
                            uint32_t firstIndex,
                            int32_t baseVertex,
                            uint32_t firstInstance)
{
    if (auto* pass = resolvePass(vm, passHandle))
    {
        pass->drawIndexed(indexCount,
                          instanceCount,
                          firstIndex,
                          baseVertex,
                          firstInstance);
    }
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
    if (host->context->activeRenderPass() == host->pass.get())
    {
        host->context->setActiveRenderPass(nullptr);
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
    auto buffer = oreContext->makeBuffer(desc);
    if (buffer == nullptr)
    {
        return 0;
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
    auto texture = oreContext->makeTexture(desc);
    if (texture == nullptr)
    {
        return 0;
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
    // The module validated the region against the texture; recheck the one
    // invariant guest memory enforces nothing about.
    if (uint64_t(region->bytesPerRow) * region->rowsPerImage *
            std::max(1u, region->depth) >
        dataCount)
    {
        return;
    }
    ore::TextureDataDesc upload;
    upload.data = data;
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
    host->texture->upload(upload);
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
    auto sampler = oreContext->makeSampler(desc);
    if (sampler == nullptr)
    {
        return 0;
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
    auto view = oreContext->makeTextureView(desc);
    if (view == nullptr)
    {
        return 0;
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
    auto object = static_cast<ScriptedObject*>(
        vm->handles().resolve(objectHandle,
                              WasmScriptingVM::HandleTable::Tag::object));
    if (object == nullptr || object->scriptAsset() == nullptr ||
        object->scriptAsset()->file() == nullptr)
    {
        return 0;
    }
    std::string key(name, nameLength);
    for (const auto& asset : object->scriptAsset()->file()->assets())
    {
        if (!asset->is<ShaderAsset>() || asset->name() != key)
        {
            continue;
        }
        auto rstb = asset->as<ShaderAsset>()->rstb();
        if (rstb.size() <= outCount)
        {
            memcpy(out, rstb.data(), rstb.size());
        }
        return (uint32_t)rstb.size();
    }
    return 0;
}

uint32_t gpuShaderAssetIdImpl(WasmScriptingVM* vm,
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
        object->scriptAsset()->file() == nullptr)
    {
        return 0;
    }
    std::string key(name, nameLength);
    for (const auto& asset : object->scriptAsset()->file()->assets())
    {
        if (asset->is<ShaderAsset>() && asset->name() == key)
        {
            return asset->assetId();
        }
    }
    return 0;
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
    auto shaderModule = oreContext->makeShaderModule(desc);
    if (shaderModule == nullptr)
    {
        return 0;
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
    }
    ore::BindGroupLayoutDesc desc;
    desc.groupIndex = groupIndex;
    desc.entries = resolved.data();
    desc.entryCount = count;
    auto layout = oreContext->makeBindGroupLayout(desc);
    if (layout == nullptr)
    {
        return 0;
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

    auto bindGroup = oreContext->makeBindGroup(desc);
    if (bindGroup == nullptr)
    {
        return 0;
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

    auto pipeline = oreContext->makePipeline(desc);
    if (pipeline == nullptr)
    {
        return 0;
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
uint32_t gpuFeaturesImpl(WasmScriptingVM*, uint32_t*, uint32_t) { return 0; }
uint32_t gpuPassBeginImpl(WasmScriptingVM*,
                          const rive_gpu_pass_desc_v1*,
                          uint32_t,
                          const rive_gpu_pass_color_attachment_v1*,
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

void rendererSaveImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (auto renderer = resolveRenderer(vm, handle))
    {
        renderer->save();
    }
}
void rendererRestoreImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (auto renderer = resolveRenderer(vm, handle))
    {
        renderer->restore();
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
    auto hostPath = static_cast<HostPath*>(
        vm->handles().resolve(pathHandle,
                              WasmScriptingVM::HandleTable::Tag::path));
    if (hostPath == nullptr || hostPath->path == nullptr)
    {
        return;
    }
    renderer->drawPath(hostPath->path.get(), paint);
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

void rendererClipPathImpl(WasmScriptingVM* vm,
                          uint32_t rendererHandle,
                          uint32_t pathHandle)
{
    auto renderer = resolveRenderer(vm, rendererHandle);
    if (vm == nullptr || renderer == nullptr)
    {
        return;
    }
    auto hostPath = static_cast<HostPath*>(
        vm->handles().resolve(pathHandle,
                              WasmScriptingVM::HandleTable::Tag::path));
    if (hostPath == nullptr || hostPath->path == nullptr)
    {
        return;
    }
    renderer->clipPath(hostPath->path.get());
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
    vm->raiseModuleError("execution exceeded timeout");
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
    if (object == nullptr || object->dataContext() == nullptr)
    {
        return 0;
    }
    auto instance = object->dataContext()->mainViewModelInstance();
    if (instance == nullptr)
    {
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
    const std::string& value = string->propertyValue();
    size_t copied = value.size() < capacity ? value.size() : capacity;
    memcpy(buffer, value.data(), copied);
    return (uint32_t)value.size();
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
    const std::string& key = values[index]->key();
    size_t copied = key.size() < capacity ? key.size() : capacity;
    memcpy(buffer, key.data(), copied);
    return (uint32_t)key.size();
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
    size_t copied = joined.size() < capacity ? joined.size() : capacity;
    memcpy(buffer, joined.data(), copied);
    return (uint32_t)joined.size();
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
    if (object == nullptr || object->dataContext() == nullptr)
    {
        return 0;
    }
    auto instance = object->dataContext()->rootViewModelInstance();
    if (instance == nullptr)
    {
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
    size_t copied = joined.size() < capacity ? joined.size() : capacity;
    memcpy(buffer, joined.data(), copied);
    return (uint32_t)joined.size();
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
struct HostNode
{
    rcp<HostArtboard> owner;
    TransformComponent* component;
};

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
    return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::node,
                              new HostNode{ref_rcp(host), component});
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
    if (vm == nullptr || host == nullptr)
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
    auto host = resolveNode(vm, handle);
    if (host == nullptr || outCount < 5)
    {
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
    auto host = resolveNode(vm, handle);
    if (host == nullptr || outCount < 6)
    {
        return;
    }
    const Mat2D& world = host->component->worldTransform();
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
    const RawPath& raw = host->component->as<Path>()->rawPath();
    size_t count = raw.verbs().size();
    if (count <= outCount)
    {
        memcpy(out, raw.verbs().data(), count);
    }
    return (uint32_t)count;
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
    const RawPath& raw = host->component->as<Path>()->rawPath();
    size_t count = raw.points().size() * 2;
    if (count <= outCount)
    {
        memcpy(out, raw.points().data(), count * sizeof(float));
    }
    return (uint32_t)count;
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

// --- rive_data_v1 asset properties (image/font/blob) ------------------------

struct HostFont
{
    rcp<Font> font;
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

void dataFontReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
    if (vm == nullptr)
    {
        return;
    }
    delete static_cast<HostFont*>(
        vm->handles().resolve(handle, WasmScriptingVM::HandleTable::Tag::font));
    vm->handles().release(handle, WasmScriptingVM::HandleTable::Tag::font);
}

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
    std::string key(name, nameLength);
    for (const auto& asset : object->scriptAsset()->file()->assets())
    {
        if (!asset->is<BlobAsset>() || asset->name() != key ||
            asset->as<BlobAsset>()->bytes().empty())
        {
            continue;
        }
        auto bytes = asset->as<BlobAsset>()->bytes();
        if (bytes.size() <= outCount)
        {
            memcpy(out, bytes.data(), bytes.size());
        }
        return (uint32_t)bytes.size();
    }
    return 0;
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
    const std::string& name = asset->name();
    size_t copied = name.size() < capacity ? name.size() : capacity;
    memcpy(buffer, name.data(), copied);
    return (uint32_t)name.size();
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
            case HandleTable::Tag::buffer:
                delete static_cast<HostBuffer*>(slot.object);
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
        memcpy(resolveModulePtr(pixelsPtr, byteCount),
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
    m_pendingDecodes.erase(token);
    if (!valid())
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
    m_pendingDecodes.erase(token);
    if (!valid())
    {
        return;
    }
    DecodeResult result;
    result.token = token;
    result.error = message;
    deliverDecodeResult(result);
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
    auto advanceDetached = [](const rcp<ViewModelInstance>& instance) {
        if (instance != nullptr && !instance->hasParents())
        {
            instance->advanced();
        }
    };
    for (auto& slot : m_handles.slots)
    {
        if (slot.tag == HandleTable::Tag::viewModelInstance)
        {
            advanceDetached(
                static_cast<HostViewModelInstance*>(slot.object)->instance);
        }
        else if (slot.tag == HandleTable::Tag::artboard)
        {
            // Artboard inputs keep their bound instance advancing, like the
            // Luau context's tracked instances.
            advanceDetached(
                static_cast<HostArtboard*>(slot.object)->viewModelInstance);
        }
    }
}

bool WasmScriptingVM::init(Span<const uint8_t> module)
{
    if (!ensureRuntime())
    {
        m_lastError = "wamr runtime init failed";
        return false;
    }

    // wasm_runtime_load keeps referencing the buffer, so hold a copy.
    m_moduleBytes.assign(module.begin(), module.end());
    // One content hash serves the dev AOT artifact lookup and the shared
    // module cache key; the key folds in which artifact actually loads so
    // interp and AOT lanes never share an entry.
    uint64_t moduleKey = 0xcbf29ce484222325ull;
    for (uint8_t byte : m_moduleBytes)
    {
        moduleKey = (moduleKey ^ byte) * 0x100000001b3ull;
    }
    m_state = std::make_unique<WamrState>();
    m_moduleKey = moduleKey;
    char error[256] = {0};
    // Dev AOT lane: with RIVE_WASM_AOT_CACHE set, swap in a wamrc-compiled
    // artifact when one exists for these bytes, else dump the wasm so it can
    // be compiled offline. wasm_runtime_load sniffs the AOT magic.
    char aotPath[1024] = {0};
    bool haveAot = false;
    bool haveHwAot = false;
    if (const char* cacheDir = getenv("RIVE_WASM_AOT_CACHE"))
    {
#ifdef RIVE_WASM_HW_BOUNDS
        // Hw artifacts (wamrc --bounds-checks=0 --stack-bounds-checks=1)
        // rely on the guard-page trap handler, so only builds carrying it
        // ever probe the .hw.aot name; sw-only builds cannot load them.
        snprintf(aotPath,
                 sizeof(aotPath),
                 "%s/%016llx.hw.aot",
                 cacheDir,
                 (unsigned long long)moduleKey);
        struct stat hwAotStat;
        haveHwAot = stat(aotPath, &hwAotStat) == 0;
#endif
        if (!haveHwAot)
        {
            snprintf(aotPath,
                     sizeof(aotPath),
                     "%s/%016llx.aot",
                     cacheDir,
                     (unsigned long long)moduleKey);
            struct stat aotStat;
            haveAot = stat(aotPath, &aotStat) == 0;
        }
        if (!haveAot && !haveHwAot)
        {
            char dumpPath[1024];
            snprintf(dumpPath,
                     sizeof(dumpPath),
                     "%s/%016llx.wasm",
                     cacheDir,
                     (unsigned long long)moduleKey);
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
    if (!haveAot && !haveHwAot)
    {
        auto& ladder = ModuleTierLadder::instance();
        if (ladder.enabled())
        {
#ifdef RIVE_WASM_HW_BOUNDS
            std::string hwPath =
                ladder.artifactPath(moduleKey, TierSpecies::hw);
            if (!hwPath.empty() && hwPath.size() < sizeof(aotPath))
            {
                memcpy(aotPath, hwPath.c_str(), hwPath.size() + 1);
                haveHwAot = true;
            }
#endif
            if (!haveHwAot)
            {
                std::string path =
                    ladder.artifactPath(moduleKey, TierSpecies::o3);
                if (!path.empty() && path.size() < sizeof(aotPath))
                {
                    memcpy(aotPath, path.c_str(), path.size() + 1);
                    haveAot = true;
                }
            }
        }
    }
    if (haveAot || haveHwAot)
    {
        moduleKey ^= 0x9e3779b97f4a7c15ull;
    }
    if (haveHwAot)
    {
        // Distinct key: hw and sw artifacts of the same bytes must never
        // share a cached module, the flag below lives on the module.
        moduleKey ^= 0xc2b2ae3d27d4eb4full;
    }
    auto& cache = sharedModuleCache();
    auto cached = cache.find(moduleKey);
    if (cached != cache.end())
    {
        m_state->module = cached->second.module;
        m_state->ownsModule = false;
        m_tier = cached->second.tier;
        // The VM's own copy is redundant against the cache entry, but the
        // tier ladder still needs the bytes; entries live for the process.
        m_scheduleBytes = Span<const uint8_t>(cached->second.bytes.data(),
                                              cached->second.bytes.size());
        m_moduleBytes.clear();
    }
    else
    {
        if (haveAot || haveHwAot)
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
                m_tier = ExecutionTier::aotO3;
            }
        }
        if (!haveAot && !haveHwAot)
        {
            // The load below rewrites the buffer in place; wamrc needs the
            // module as it is now.
            ModuleTierLadder::instance().stagePristine(
                m_moduleKey,
                Span<const uint8_t>(m_moduleBytes.data(),
                                    m_moduleBytes.size()));
        }
        m_state->module = wasm_runtime_load(m_moduleBytes.data(),
                                            (uint32_t)m_moduleBytes.size(),
                                            error,
                                            sizeof(error));
        if (m_state->module != nullptr)
        {
            if (haveHwAot)
            {
                wasm_runtime_set_module_hw_bounds(m_state->module, true);
            }
            SharedWasmModule entry;
            entry.bytes = std::move(m_moduleBytes);
            entry.module = m_state->module;
            entry.tier = m_tier;
            auto inserted = cache.emplace(moduleKey, std::move(entry));
            m_state->ownsModule = false;
            m_scheduleBytes =
                Span<const uint8_t>(inserted.first->second.bytes.data(),
                                    inserted.first->second.bytes.size());
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

    s_bootPrint = &m_print;
    m_state->instance = wasm_runtime_instantiate(m_state->module,
                                                 512 * 1024,
                                                 0,
                                                 error,
                                                 sizeof(error));
    s_bootPrint = nullptr;
    if (m_state->instance == nullptr)
    {
        m_lastError = std::string("module instantiate failed: ") + error;
        return false;
    }
    if (m_tier != ExecutionTier::interp && !haveHwAot)
    {
        pregrowAotMemory(m_state->instance);
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
    // Stub modules report their bump position; page counts go blind once
    // the aot lanes pregrow to wasmMaxPages.
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
    if (!ladder.enabled() || m_tier == ExecutionTier::aotO3)
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
    if (!ladder.enabled() || m_tier == ExecutionTier::aotO3)
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
    // The stub bump position beats page counts when available: pregrown
    // aot memory never grows.
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
        case Tag::buffer:
            return "buffer";
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
        case Tag::empty:
            break;
    }
    return "unknown";
}

// Growing linear memory reallocs it, which in-flight AOT frames do not
// tolerate (fields root cause #3): on an artifact, take the module's whole
// declared ceiling up front, while no frames are live, so it never grows
// again. Modules without a declared ceiling keep growth-on-demand.
static void pregrowAotMemory(wasm_module_inst_t instance)
{
    wasm_memory_inst_t memory = wasm_runtime_get_default_memory(instance);
    if (memory == nullptr)
    {
        return;
    }
    uint32_t pages = (uint32_t)wasm_memory_get_cur_page_count(memory);
    uint32_t maxPages = (uint32_t)wasm_memory_get_max_page_count(memory);
    constexpr uint32_t kUnboundedPages = 65536;
    if (maxPages >= kUnboundedPages)
    {
        fprintf(stderr,
                "wasm aot: module declares no wasmMaxPages; memory cannot be "
                "reserved up front, so growth during frames may trap\n");
        return;
    }
    if (maxPages <= pages)
    {
        return;
    }
    if (!wasm_runtime_enlarge_memory(instance, maxPages - pages))
    {
        fprintf(stderr,
                "wasm aot: pregrow to %u pages failed; growth during frames "
                "may trap\n",
                maxPages);
    }
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
    if (m_handleFrames < kHandleWarnMinFrames ||
        live + 1 < m_handleBaselineLive + kHandleWarnCount)
    {
        return nullptr;
    }
    // Re-arm so a leaking session keeps warning every 512 handles.
    uint32_t grown = live + 1 - m_handleBaselineLive;
    uint32_t frames = m_handleFrames;
    m_handleBaselineLive = live + 1;
    m_handleFrames = 0;
    uint32_t counts[(size_t)HandleTable::Tag::node + 1] = {0};
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
    if (!hwBounds)
    {
        // Guard-page memory never moves on growth; only the sw lane needs
        // the ceiling reserved up front.
        pregrowAotMemory(next->instance);
    }
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
    uint32_t sizeArgs[1] = {(uint32_t)name.size() + 1};
    uint32_t namePtr = callModule("malloc", 1, sizeArgs);
    if (namePtr == 0)
    {
        m_lastError = "module name allocation failed";
        return false;
    }
    memcpy(resolveModulePtr(namePtr, (uint32_t)name.size() + 1),
           name.c_str(),
           name.size() + 1);

    uint32_t requireArgs[2] = {m_L, namePtr};
    uint32_t status = 0;
    CallOutcome outcome =
        callModuleChecked("host_require", 2, requireArgs, &status);

    uint32_t freeArgs[1] = {namePtr};
    callModule("free", 1, freeArgs);

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
    return m_state != nullptr && m_state->execEnv != nullptr && m_L != 0;
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
    if (!wasm_runtime_call_wasm(m_state->execEnv, f, 3, buf))
    {
        const char* exception = wasm_runtime_get_exception(m_state->instance);
        fprintf(stderr,
                "script init trapped: %s\n",
                exception != nullptr ? exception : "unknown");
#if WASM_ENABLE_DUMP_CALL_STACK
        wasm_runtime_dump_call_stack(m_state->execEnv);
#endif
        wasm_runtime_clear_exception(m_state->instance);
    }
    else
    {
        status = buf[0];
    }
    switch (status)
    {
        case 0:
            return InitResult::notImplemented;
        case 1:
            return InitResult::succeeded;
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
    if (!wasm_runtime_call_wasm_a(m_state->execEnv, f, 1, results, 3, args))
    {
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
    uint32_t namePtr = guestString(name);
    if (namePtr == 0)
    {
        return;
    }
    uint32_t args[3] = {m_L, (uint32_t)selfRef, namePtr};
    callModule("host_obj_trigger", 3, args);
    guestFree(namePtr);
}

bool WasmScriptingVM::callNumberMethod(ScriptedObject* object,
                                       int selfRef,
                                       const char* name,
                                       const float* args,
                                       size_t argCount,
                                       float* outResult)
{
    uint32_t namePtr = guestString(name);
    if (namePtr == 0)
    {
        return false;
    }
    uint32_t scratchSize = (uint32_t)(argCount * sizeof(float)) + sizeof(float);
    uint32_t sizeArgs[1] = {scratchSize};
    uint32_t scratch = callModule("malloc", 1, sizeArgs);
    if (scratch == 0)
    {
        guestFree(namePtr);
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
    guestFree(namePtr);
    return ok != 0;
}

bool WasmScriptingVM::callBooleanMethod(ScriptedObject* object,
                                        int selfRef,
                                        const char* name)
{
    uint32_t namePtr = guestString(name);
    if (namePtr == 0)
    {
        return false;
    }
    uint32_t args[3] = {m_L, (uint32_t)selfRef, namePtr};
    uint32_t result = callModule("host_obj_boolean_method", 3, args);
    guestFree(namePtr);
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
    uint32_t args[7] = {m_L,
                        (uint32_t)selfRef,
                        dataPtr + (uint32_t)sizeof(paint) + pointBytes,
                        verbCount,
                        dataPtr + (uint32_t)sizeof(paint),
                        floatCount,
                        dataPtr};
    uint32_t ok = callModule("host_obj_path_effect", 7, args);
    m_pathEffectOut = nullptr;
    guestFree(dataPtr);
    return ok != 0;
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
    const std::string* text = nullptr;
    if (input->is<DataValueNumber>())
    {
        wire.kind = DataConvertWire::kindNumber;
        wire.number = input->as<DataValueNumber>()->value();
    }
    else if (input->is<DataValueString>())
    {
        wire.kind = DataConvertWire::kindString;
        text = &input->as<DataValueString>()->value();
        wire.stringLength = (uint32_t)text->size();
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
    uint32_t methodPtr = guestString(method);
    if (methodPtr == 0)
    {
        return false;
    }
    uint32_t byteCount = (uint32_t)sizeof(wire) + wire.stringLength;
    uint32_t sizeArgs[1] = {byteCount};
    uint32_t wirePtr = callModule("malloc", 1, sizeArgs);
    if (wirePtr == 0)
    {
        guestFree(methodPtr);
        return false;
    }
    uint8_t* out = (uint8_t*)resolveModulePtr(wirePtr, byteCount);
    memcpy(out, &wire, sizeof(wire));
    if (text != nullptr)
    {
        memcpy(out + sizeof(wire), text->data(), wire.stringLength);
    }
    m_convertResultOut = outResult;
    uint32_t args[4] = {m_L, (uint32_t)selfRef, methodPtr, wirePtr};
    uint32_t ran = callModule("host_obj_data_convert", 4, args);
    m_convertResultOut = nullptr;
    guestFree(wirePtr);
    guestFree(methodPtr);
    return ran != 0;
}

bool WasmScriptingVM::callPointerEvent(ScriptedObject* object,
                                       int selfRef,
                                       const char* method,
                                       int pointerId,
                                       Vec2D localPosition,
                                       HitResult* outResult)
{
    if (!valid())
    {
        return false;
    }
    wasm_function_inst_t f =
        wasm_runtime_lookup_function(m_state->instance,
                                     "host_obj_pointer_event");
    if (f == nullptr)
    {
        return false;
    }
    uint32_t methodPtr = guestString(method);
    if (methodPtr == 0)
    {
        return false;
    }
    wasm_val_t args[6];
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
    wasm_val_t results[1];
    results[0].kind = WASM_I32;
    bool ok =
        wasm_runtime_call_wasm_a(m_state->execEnv, f, 1, results, 6, args);
    guestFree(methodPtr);
    if (!ok || results[0].of.i32 == 0)
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
    uint32_t sizeArgs[1] = {(uint32_t)text.size() + 1};
    uint32_t textPtr = callModule("malloc", 1, sizeArgs);
    if (textPtr == 0)
    {
        return false;
    }
    memcpy(resolveModulePtr(textPtr, (uint32_t)text.size() + 1),
           text.data(),
           text.size());
    uint32_t args[4] = {m_L, (uint32_t)selfRef, textPtr, (uint32_t)text.size()};
    uint32_t result = callModule("host_obj_text_event", 4, args);
    guestFree(textPtr);
    return result != 0;
}

// Flattens the gamepad alternatives into the wire layout the module rebuilds
// snapshots from; false for non-gamepad kinds. Shared by callGamepadEvent and
// callListenerPerform.
static bool packGamepadWire(const ListenerInvocation& invocation,
                            GamepadWire& wire,
                            const GamepadSnapshot*& snapshot)
{
    if (const GamepadConnectedInvocation* c = invocation.asGamepadConnected())
    {
        wire.kind = GamepadWire::kindConnected;
        snapshot = &c->snapshot;
    }
    else if (const GamepadEventInvocation* e = invocation.asGamepadEvent())
    {
        wire.kind = GamepadWire::kindEvent;
        snapshot = &e->fullState;
        wire.changeKind = (uint32_t)e->change.kind;
        wire.changeIndex = e->change.index;
        wire.changeValue = e->change.value;
        wire.hasStandardButtonIntent = e->hasStandardButtonIntent ? 1 : 0;
        wire.standardButton = (uint32_t)e->standardButton;
        wire.hasStandardAxisIntent = e->hasStandardAxisIntent ? 1 : 0;
        wire.standardAxis = (uint32_t)e->standardAxis;
    }
    else if (const GamepadDisconnectedInvocation* d =
                 invocation.asGamepadDisconnected())
    {
        wire.kind = GamepadWire::kindDisconnected;
        wire.deviceId = d->deviceId;
    }
    else
    {
        return false;
    }
    if (snapshot != nullptr)
    {
        wire.deviceId = snapshot->deviceId;
        wire.mapping = (uint32_t)snapshot->mapping;
        wire.buttonMaskLo = (uint32_t)snapshot->buttonMask;
        wire.buttonMaskHi = (uint32_t)(snapshot->buttonMask >> 32);
        wire.buttonCount = (uint32_t)snapshot->buttonValues.size();
        wire.axisCount = (uint32_t)snapshot->axes.size();
    }
    return true;
}

static void writeGamepadPayload(uint8_t* out,
                                const GamepadWire& wire,
                                const GamepadSnapshot* snapshot)
{
    memcpy(out, &wire, sizeof(wire));
    if (snapshot == nullptr)
    {
        return;
    }
    float* values = (float*)(out + sizeof(wire));
    if (wire.buttonCount != 0)
    {
        memcpy(values,
               snapshot->buttonValues.data(),
               wire.buttonCount * sizeof(float));
    }
    if (wire.axisCount != 0)
    {
        memcpy(values + wire.buttonCount,
               snapshot->axes.data(),
               wire.axisCount * sizeof(float));
    }
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
    uint32_t methodPtr = guestString(method);
    if (methodPtr == 0)
    {
        return false;
    }
    uint32_t byteCount =
        (uint32_t)(sizeof(wire) +
                   (wire.buttonCount + wire.axisCount) * sizeof(float));
    uint32_t sizeArgs[1] = {byteCount};
    uint32_t dataPtr = callModule("malloc", 1, sizeArgs);
    if (dataPtr == 0)
    {
        guestFree(methodPtr);
        return false;
    }
    writeGamepadPayload((uint8_t*)resolveModulePtr(dataPtr, byteCount),
                        wire,
                        snapshot);
    uint32_t args[5] = {m_L, (uint32_t)selfRef, methodPtr, dataPtr, byteCount};
    uint32_t result = callModule("host_obj_gamepad_event", 5, args);
    guestFree(dataPtr);
    guestFree(methodPtr);
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
        text = &t->text;
        wire.textLength = (uint32_t)t->text.size();
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
    uint32_t tailBytes =
        text != nullptr
            ? wire.textLength
            : (hasGamepad ? (uint32_t)(sizeof(gamepad) + (gamepad.buttonCount +
                                                          gamepad.axisCount) *
                                                             sizeof(float))
                          : 0);
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
    wasm_runtime_call_wasm_a(m_state->execEnv,
                             f,
                             0,
                             nullptr,
                             legacy ? 4 : 5,
                             args);
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

void WasmScriptingVM::setInputBoolean(int selfRef, const char* name, bool value)
{
    uint32_t namePtr = guestString(name);
    if (namePtr == 0)
    {
        return;
    }
    uint32_t args[4] = {m_L, (uint32_t)selfRef, namePtr, value ? 1u : 0u};
    callModule("host_obj_set_boolean", 4, args);
    guestFree(namePtr);
}

void WasmScriptingVM::setInputNumber(int selfRef, const char* name, float value)
{
    wasm_function_inst_t f =
        wasm_runtime_lookup_function(m_state->instance, "host_obj_set_number");
    uint32_t namePtr = guestString(name);
    if (f == nullptr || namePtr == 0)
    {
        return;
    }
    wasm_val_t args[4];
    args[0].kind = WASM_I32;
    args[0].of.i32 = (int32_t)m_L;
    args[1].kind = WASM_I32;
    args[1].of.i32 = selfRef;
    args[2].kind = WASM_I32;
    args[2].of.i32 = (int32_t)namePtr;
    args[3].kind = WASM_F64;
    args[3].of.f64 = value;
    wasm_runtime_call_wasm_a(m_state->execEnv, f, 0, nullptr, 4, args);
    guestFree(namePtr);
}

void WasmScriptingVM::setInputUnsigned(int selfRef,
                                       const char* name,
                                       uint32_t value)
{
    uint32_t namePtr = guestString(name);
    if (namePtr == 0)
    {
        return;
    }
    uint32_t args[4] = {m_L, (uint32_t)selfRef, namePtr, value};
    callModule("host_obj_set_unsigned", 4, args);
    guestFree(namePtr);
}

void WasmScriptingVM::setInputString(int selfRef,
                                     const char* name,
                                     const char* value)
{
    uint32_t namePtr = guestString(name);
    uint32_t valuePtr = guestString(value);
    if (namePtr == 0 || valuePtr == 0)
    {
        guestFree(namePtr);
        guestFree(valuePtr);
        return;
    }
    uint32_t args[4] = {m_L, (uint32_t)selfRef, namePtr, valuePtr};
    callModule("host_obj_set_string", 4, args);
    guestFree(valuePtr);
    guestFree(namePtr);
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
            uint32_t namePtr = guestString(name);
            if (namePtr == 0)
            {
                return;
            }
            // The module's view model userdata owns the handle and releases
            // it from its finalizer.
            uint32_t handle =
                m_handles.mint(HandleTable::Tag::viewModelInstance,
                               new HostViewModelInstance{std::move(vmi)});
            uint32_t args[4] = {m_L, (uint32_t)selfRef, namePtr, handle};
            callModule("host_obj_set_view_model", 4, args);
            guestFree(namePtr);
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
    uint32_t namePtr = guestString(name);
    if (namePtr == 0)
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
    uint32_t args[4] = {m_L, (uint32_t)selfRef, namePtr, handle};
    callModule("host_obj_set_artboard", 4, args);
    guestFree(namePtr);
}

uint32_t WasmScriptingVM::guestString(const char* text)
{
    if (text == nullptr)
    {
        return 0;
    }
    size_t size = strlen(text) + 1;
    uint32_t sizeArgs[1] = {(uint32_t)size};
    uint32_t ptr = callModule("malloc", 1, sizeArgs);
    if (ptr != 0)
    {
        memcpy(resolveModulePtr(ptr, (uint32_t)size), text, size);
    }
    return ptr;
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
