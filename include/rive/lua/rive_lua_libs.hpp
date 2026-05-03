#ifdef WITH_RIVE_SCRIPTING
#ifndef _RIVE_LUA_LIBS_HPP_
#define _RIVE_LUA_LIBS_HPP_
#include "lua.h"
#include "lualib.h"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/lua/lua_state.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/renderer.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/math/contour_measure.hpp"
#include "rive/math/path_measure.hpp"
#include "rive/shapes/paint/image_sampler.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/viewmodel/data_enum.hpp"
#include "rive/viewmodel/viewmodel_instance_asset_image.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "rive/viewmodel/viewmodel_instance_color.hpp"
#include "rive/viewmodel/viewmodel_instance_enum.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance_color.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_string.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/data_bind/data_values/data_value_boolean.hpp"
#include "rive/data_bind/data_values/data_value_color.hpp"
#include "rive/data_bind/data_values/data_value_list.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/animation/listener_invocation.hpp"
#include "rive/hit_result.hpp"
#include "rive/lua/scripting_vm.hpp"
#include "rive/refcnt.hpp"
#ifdef WITH_RIVE_AUDIO
#include "rive/audio/audio_engine.hpp"
#include "rive/audio/audio_source.hpp"
#include "rive/audio/audio_sound.hpp"
#endif
#ifdef WITH_RIVE_TOOLS
#include "rive/core/binary_writer.hpp"
#include "rive/core/vector_binary_stream.hpp"
#endif

#include <chrono>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <string>
#include <vector>

static const int maxCStack = 8000;
static const int luaGlobalsIndex = -maxCStack - 2002;
static const int luaRegistryIndex = -maxCStack - 2000;

namespace rive
{
class Artboard;
class ArtboardInstance;
class Factory;
class File;
class ModuleDetails;
class ScriptedObject;
class StateMachineInstance;
class TransformComponent;
enum class LuaAtoms : int16_t
{
    // Vector
    length,
    lengthSquared,
    normalized,
    distance,
    distanceSquared,
    dot,
    lerp,

    // Path
    moveTo,
    lineTo,
    quadTo,
    cubicTo,
    close,
    reset,
    add,
    contours,
    measure,

    // Path Command
    type,
    points,

    // Mat2D
    invert,
    isIdentity,

    // Image
    width,
    height,

    // ImageSampler
    clamp,
    repeat,
    mirror,
    bilinear,
    nearest,

    // Paint
    style,
    join,
    cap,
    thickness,
    blendMode,
    feather,
    gradient,
    color,

    stroke,
    fill,
    miter,
    round,
    bevel,
    butt,
    square,
    srcOver,
    screen,
    overlay,
    darken,
    lighten,
    colorDodge,
    colorBurn,
    hardLight,
    softLight,
    difference,
    exclusion,
    multiply,
    hue,
    saturation,
    luminosity,
    copy,

    // Renderer
    drawPath,
    drawImage,
    drawImageMesh,
    clipPath,
    save,
    restore,
    transform,

    // Scripted Properties
    value,
    red,
    green,
    blue,
    alpha,
    getNumber,
    getTrigger,
    getString,
    getBoolean,
    getColor,
    getList,
    getViewModel,
    getEnum,
    getIndex,
    getImage,
    values,
    addListener,
    removeListener,
    fire,
    push,
    insert,
    shift,
    pop,
    swap,
    clear,

    // Artboards
    draw,
    advance,
    frameOrigin,
    data,
    instance,
    animation,
    newAtom,
    bounds,
    pointerDown,
    pointerMove,
    pointerUp,
    pointerExit,
    addToPath,
    name,

    // Scripted DataValues
    isNumber,
    isString,
    isBoolean,
    isColor,

    // inputs
    hit,
    id,
    position,

    // nodes
    rotation,
    scale,
    worldTransform,
    scaleX,
    scaleY,
    decompose,
    children,
    parent,
    node,
    paint,
    asPaint,
    asPath,

    // PathMeasure/ContourMeasure
    positionAndTangent,
    warp,
    extract,
    next,
    isClosed,

    // Scripted Context
    markNeedsUpdate,
    viewModel,
    rootViewModel,
    image,
    blob,
    size,
    dataContext,
    audio,
    play,
    playAtTime,
    playInTime,
    playAtFrame,
    playInFrame,
    stop,
    pause,
    resume,
    seek,
    seekFrame,
    volume,
    completed,
    time,
    timeFrame,
    sampleRate,

    // Animation
    duration,
    setTime,
    setTimeFrames,
    setTimePercentage,

    // PointerEvent (append to avoid shifting existing atom ids)
    previousPosition,
    timeStamp,

    // ScriptedInvocation / listener payloads (append only)
    isPointerEvent,
    isKeyboardEvent,
    isTextInput,
    isFocus,
    isReportedEvent,
    isViewModelChange,
    isNone,
    isGamepad,
    asPointerEvent,
    asKeyboardEvent,
    asTextInput,
    asFocus,
    asReportedEvent,
    asViewModelChange,
    asGamepad,
    asNone,
    key,
    alt,
    control,
    meta,
    text,
    phase,
    delaySeconds,
    deviceId,
    buttonMask,
    axis0,
    remove,
    removeAt,
    removeAllOf,

    // GPU bindings
    write,
    upload,
    view,
    setPipeline,
    setVertexBuffer,
    setIndexBuffer,
    setBindGroup,
    setViewport,
    setScissorRect,
    setStencilReference,
    setBlendColor,
    drawIndexed,
    finish,
    beginRenderPass,
    beginFrame,
    endFrame,
    colorView,
    depthView,
    resize,
    canvas,
    gpuCanvas,
    drawCanvas,
    features,
    loadShader,
    format,
    preferredCanvasFormat,

    // Promise
    andThen,
    catch_,
    finally_,
    cancel,
    onCancel,
    getStatus,

    // Image decode
    decodeImage,
};

struct ScriptedMat2D
{
    static constexpr uint8_t luaTag = LUA_T_COUNT + 1;
    static constexpr const char* luaName = "Mat2D";
    static constexpr bool hasMetatable = true;

    ScriptedMat2D(float x1, float y1, float x2, float y2, float tx, float ty) :
        value(x1, y1, x2, y2, tx, ty)
    {}

    ScriptedMat2D(const Mat2D& mat) : value(mat) {}

    ScriptedMat2D() {}

    rive::Mat2D value;
};

static_assert(std::is_trivially_destructible<ScriptedMat2D>::value,
              "ScriptedMat2D must be trivially destructible");

class ScriptedPathCommand
{
public:
    ScriptedPathCommand(std::string type, std::vector<Vec2D> points = {}) :
        m_type(type), m_points(points)
    {}
    static constexpr uint8_t luaTag = LUA_T_COUNT + 29;
    static constexpr const char* luaName = "PathCommand";
    static constexpr bool hasMetatable = true;
    std::string type() { return m_type; }
    std::vector<Vec2D> points() { return m_points; }

private:
    std::string m_type;
    std::vector<Vec2D> m_points;
};

class ScriptedPathData
{
public:
    ScriptedPathData() {}
    ScriptedPathData(const RawPath* path);
    int totalCommands();
    void markDirty() { m_isRenderPathDirty = true; }
    RawPath rawPath;
    static constexpr uint8_t luaTag = LUA_T_COUNT + 30;
    static constexpr const char* luaName = "PathData";
    static constexpr bool hasMetatable = true;
    RenderPath* renderPath(lua_State* L);

protected:
    rcp<RenderPath> m_renderPath;

    bool m_isRenderPathDirty = true;

private:
    uint64_t m_renderFrameId = 0;
};

class ScriptedPath : public ScriptedPathData
{
public:
    ScriptedPath() {}
    ScriptedPath(const RawPath* path) : ScriptedPathData(path) {}
    static constexpr uint8_t luaTag = LUA_T_COUNT + 2;
    static constexpr const char* luaName = "Path";
    static constexpr bool hasMetatable = true;
};

class ScriptedGradient
{
public:
    rcp<RenderShader> shader;
    static constexpr uint8_t luaTag = LUA_T_COUNT + 3;
    static constexpr const char* luaName = "Gradient";
    static constexpr bool hasMetatable = false;
};

class ScriptedVertexBuffer
{
public:
    std::vector<Vec2D> values;
    rcp<RenderBuffer> vertexBuffer;
    static constexpr uint8_t luaTag = LUA_T_COUNT + 4;
    static constexpr const char* luaName = "VertexBuffer";
    static constexpr bool hasMetatable = true;

    void update(Factory* factory);
};

class ScriptedTriangleBuffer
{
public:
    std::vector<uint16_t> values;
    rcp<RenderBuffer> indexBuffer;
    static constexpr uint8_t luaTag = LUA_T_COUNT + 5;
    static constexpr const char* luaName = "TriangleBuffer";
    static constexpr bool hasMetatable = true;

    uint16_t max = 0;
    void update(Factory* factory);
};

#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
namespace ore
{
class TextureView;
}
#endif

class ScriptedImage
{
public:
    rcp<RenderImage> image;
#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
    rcp<ore::TextureView> cachedOreView; // Cached for Image:view() to avoid
                                         // leaking D3D12 CPU descriptors.
#if defined(ORE_BACKEND_GL)
    // GL only: when the source image is a Rive 2D RenderCanvas, the GL
    // backend's getCanvasImportMirror returns a Y-flipped companion
    // image. We hold a strong rcp here so the companion stays alive for
    // the lifetime of this ScriptedImage. The view in cachedOreView wraps
    // the companion's texture, not the source's.
    rcp<RenderImage> cachedMirrorImage;
#endif
#endif
    // Out-of-line destructor — when ore is enabled, defined in lua_gpu.cpp
    // where ore::TextureView is complete. Otherwise defined in lua_image.cpp.
    ~ScriptedImage();
    // Out-of-line factory — avoids instantiating rcp<TextureView>::~rcp() in
    // TUs that don't include the full ore headers (placement new requires a
    // visible destructor for exception cleanup).
    static ScriptedImage* luaNew(lua_State* L);
    static constexpr uint8_t luaTag = LUA_T_COUNT + 6;
    static constexpr const char* luaName = "Image";
    static constexpr bool hasMetatable = true;
};

class ScriptedBlob
{
public:
    rcp<FileAsset> asset; // Holds ref to keep BlobAsset alive

    static constexpr uint8_t luaTag = LUA_T_COUNT + 35;
    static constexpr const char* luaName = "Blob";
    static constexpr bool hasMetatable = true;
};

#ifdef WITH_RIVE_AUDIO

class ScriptedAudio
{
public:
    static constexpr uint8_t luaTag = LUA_T_COUNT + 40;
    static constexpr const char* luaName = "Audio";
    static constexpr bool hasMetatable = true;
};

class ScriptedAudioSource
{
public:
    static constexpr uint8_t luaTag = LUA_T_COUNT + 38;
    static constexpr const char* luaName = "AudioSource";
    static constexpr bool hasMetatable = true;
    void source(rcp<AudioSource>);
    rcp<AudioSource> source() { return m_source; }
    int play(lua_State*, AudioEngine*);
    int play(lua_State*, AudioEngine*, double, bool);
    int playFrame(lua_State*, AudioEngine*);
    int playFrame(lua_State*, AudioEngine*, uint64_t, bool);

private:
    rcp<AudioSource> m_source;
    int initializeSound(lua_State*, rcp<AudioSound>, Artboard*);
};

class ScriptedAudioSound
{
public:
    ScriptedAudioSound(Artboard* artboard) : m_artboard(artboard) {}
    rcp<AudioSound> sound;
    static constexpr uint8_t luaTag = LUA_T_COUNT + 39;
    static constexpr const char* luaName = "AudioSound";
    static constexpr bool hasMetatable = true;
    Artboard* artboard() { return m_artboard; }

private:
    Artboard* m_artboard = nullptr;
};
#endif

#ifdef RIVE_CANVAS
namespace gpu
{
class RenderCanvas;
class RenderContext;
} // namespace gpu

// Forward-declare RiveRenderer so canvas handles can store a raw pointer to the
// active frame renderer without pulling in the full rive_renderer.hpp header.
class RiveRenderer;

#ifdef RIVE_ORE
namespace ore
{
class Buffer;
class Texture;
class TextureView;
class Sampler;
class BindGroup;
class BindGroupLayout;
class ShaderModule;
class Pipeline;
class RenderPass;
class Context;
} // namespace ore

class ScriptedGPUBuffer
{
public:
    static constexpr uint8_t luaTag = LUA_T_COUNT + 41;
    static constexpr const char* luaName = "GPUBuffer";
    static constexpr bool hasMetatable = true;
    rcp<ore::Buffer> buffer;
    // Set when constructed with `immutable=true`. The Lua wrapper rejects
    // `:write` calls on immutable buffers — matching `BufferDesc::immutable`'s
    // documented contract ("no update() calls allowed after creation").
    bool immutable = false;
};

class ScriptedGPUTexture
{
public:
    static constexpr uint8_t luaTag = LUA_T_COUNT + 42;
    static constexpr const char* luaName = "GPUTexture";
    static constexpr bool hasMetatable = true;
    rcp<ore::Texture> texture;
};

class ScriptedGPUSampler
{
public:
    static constexpr uint8_t luaTag = LUA_T_COUNT + 43;
    static constexpr const char* luaName = "GPUSampler";
    static constexpr bool hasMetatable = false;
    rcp<ore::Sampler> sampler;
};

class ScriptedShader
{
public:
    static constexpr uint8_t luaTag = LUA_T_COUNT + 44;
    static constexpr const char* luaName = "Shader";
    static constexpr bool hasMetatable = false;
    rcp<ore::ShaderModule> module;         // vertex module (or combined)
    rcp<ore::ShaderModule> fragmentModule; // fragment module (null if combined)

    // Returns the module to use for a given pipeline stage.
    ore::ShaderModule* vertexMod() const { return module.get(); }
    ore::ShaderModule* fragmentMod() const
    {
        return fragmentModule ? fragmentModule.get() : module.get();
    }
};

class ScriptedGPUPipeline
{
public:
    static constexpr uint8_t luaTag = LUA_T_COUNT + 45;
    static constexpr const char* luaName = "GPUPipeline";
    static constexpr bool hasMetatable = true;
    rcp<ore::Pipeline> pipeline;
    uint32_t sampleCount = 1;

    // Deep-copied vertex layout data so Pipeline's PipelineDesc pointers
    // remain valid for the lifetime of this object. Stored as raw bytes to
    // avoid pulling ore_types.hpp into this header.
    std::vector<uint8_t> ownedVertexLayoutData;

    // Auto-derived layouts (one per @group(N) in the shader) when the user
    // omits `bindGroupLayouts`. Exposed via `pipeline:getBindGroupLayout(N)`.
    // Empty when explicit layouts were supplied.
    std::vector<rcp<ore::BindGroupLayout>> autoBindGroupLayouts;
};

class ScriptedGPUBindGroup
{
public:
    static constexpr uint8_t luaTag = LUA_T_COUNT + 52;
    static constexpr const char* luaName = "GPUBindGroup";
    static constexpr bool hasMetatable = false;
    ~ScriptedGPUBindGroup(); // Defers destruction via
                             // Context::deferBindGroupDestroy.
    rcp<ore::BindGroup> bindGroup;
};

class ScriptedGPUBindGroupLayout
{
public:
    static constexpr uint8_t luaTag = LUA_T_COUNT + 60;
    static constexpr const char* luaName = "GPUBindGroupLayout";
    static constexpr bool hasMetatable = false;
    rcp<ore::BindGroupLayout> layout;
};

class ScriptedGPURenderPass
{
public:
    static constexpr uint8_t luaTag = LUA_T_COUNT + 46;
    static constexpr const char* luaName = "GPURenderPass";
    static constexpr bool hasMetatable = true;
    ~ScriptedGPURenderPass();
    // `pass` is owned by `ScriptedGPUCanvas::m_activePass`. To prevent
    // the canvas from being GC'd before this render pass — which would
    // delete `m_activePass` and dangle this pointer — the constructor
    // site stores a Lua ref to the owning canvas in `m_canvasRef`,
    // released by `~ScriptedGPURenderPass`.
    ore::RenderPass* pass = nullptr;
    bool m_finished = false;
    bool m_pipelineSet = false;
    uint32_t sampleCount = 1; // for pipeline sampleCount validation
    std::string label;
    uint32_t drawCallCount = 0;
    lua_State* m_L = nullptr;
    int m_canvasRef = LUA_NOREF;
};

class ScriptedGPUTextureView
{
public:
    static constexpr uint8_t luaTag = LUA_T_COUNT + 51;
    static constexpr const char* luaName = "GPUTextureView";
    static constexpr bool hasMetatable = false;
    rcp<ore::TextureView> view;
    // When created from Image:view(), retains the RenderImage so the
    // underlying gpu::Texture stays alive even if the Image is GC'd.
    rcp<RenderImage> retainedImage;
};

class ScriptedGPUCanvas
{
public:
    static constexpr uint8_t luaTag = LUA_T_COUNT + 47;
    static constexpr const char* luaName = "GPUCanvas";
    static constexpr bool hasMetatable = true;
    ~ScriptedGPUCanvas();
    rcp<gpu::RenderCanvas> canvas;
    // 1× resolve target (platform backing texture).
    rcp<ore::TextureView> oreColorView;
    // MSAA color attachment — non-null when canvas was created with sampleCount
    // > 1.
    rcp<ore::Texture> oreMSAAColorTexture;
    rcp<ore::TextureView> oreMSAAColorView;
    rcp<ore::Texture> oreDepthTexture;
    rcp<ore::TextureView> oreDepthView;
    lua_State* m_L = nullptr;
    int m_imageRef = LUA_NOREF;
    ore::RenderPass* m_activePass = nullptr; // heap-allocated, owned by this
    std::string m_activePassLabel; // label of m_activePass for diagnostics
    gpu::RenderContext* renderCtx = nullptr; // needed for resize()
};

#endif // RIVE_ORE

enum class CanvasState
{
    Idle,
    Rendering
};

class ScriptedCanvas
{
public:
    static constexpr uint8_t luaTag = LUA_T_COUNT + 50;
    static constexpr const char* luaName = "Canvas";
    static constexpr bool hasMetatable = true;
    ~ScriptedCanvas();
    rcp<gpu::RenderCanvas> canvas;
    lua_State* m_L = nullptr;
    int m_imageRef = LUA_NOREF;
    gpu::RenderContext* renderCtx = nullptr;
    CanvasState m_state = CanvasState::Idle;
    // Allocated on beginFrame(), deleted on endFrame(). Wraps renderCtx.
    RiveRenderer* m_riveRenderer = nullptr;
    // Lua registry ref to the ScriptedRenderer pushed by beginFrame(),
    // kept alive until endFrame() so the Lua renderer stays valid.
    int m_rendererRef = LUA_NOREF;
};
#endif // RIVE_CANVAS

// ── Promise ────────────────────────────────────────────────────────────────

enum class PromiseState : uint8_t
{
    Pending,
    Fulfilled,
    Rejected,
    Cancelled
};

class ScriptedPromise
{
public:
    static constexpr uint8_t luaTag = LUA_T_COUNT + 53;
    static constexpr const char* luaName = "Promise";
    static constexpr bool hasMetatable = true;

    ScriptedPromise(lua_State* mainThread) : m_state(mainThread) {}
    ~ScriptedPromise();

    void resolve(lua_State* L, int valueIdx);
    // Overload with selfRef: a registry ref to this promise's userdata,
    // needed for Promise/A+ adoption of pending inner promises.
    void resolve(lua_State* L, int valueIdx, int selfRef);
    void reject(lua_State* L, int errorIdx);
    void cancel(lua_State* L);

    bool isFulfilled() const
    {
        return m_promiseState == PromiseState::Fulfilled;
    }
    bool isRejected() const { return m_promiseState == PromiseState::Rejected; }
    bool isCancelled() const
    {
        return m_promiseState == PromiseState::Cancelled;
    }
    bool isPending() const { return m_promiseState == PromiseState::Pending; }
    int resultRef() const { return m_resultRef; }

    lua_State* m_state;
    PromiseState m_promiseState = PromiseState::Pending;
    int m_resultRef = LUA_NOREF;

    struct ThenCallback
    {
        int successRef = LUA_NOREF;
        int failureRef = LUA_NOREF;
        int chainedPromiseRef = LUA_NOREF; // registry ref keeps it alive
        int cancelRef =
            LUA_NOREF; // cleanup callback fired on cancel (async/await)
    };
    struct FinallyCallback
    {
        int callbackRef = LUA_NOREF;
        int chainedPromiseRef = LUA_NOREF; // registry ref keeps it alive
    };

    std::vector<ThenCallback> m_thenCallbacks;
    std::vector<FinallyCallback> m_finallyCallbacks;

    // Parent promise (the promise this was chained FROM via
    // andThen/catch/finally).
    int m_parentRef = LUA_NOREF;

    // Consumer promises (chained FROM this one). Separate refs from
    // ThenCallback.chainedPromiseRef for independent cancel propagation.
    std::vector<int> m_consumerRefs;

    // Cancellation hook: a Lua function called when cancel() fires.
    // Used by decodeImage to cancel in-flight work, and by Promise.all
    // to cancel chained promises.
    int m_onCancelRef = LUA_NOREF;
};

int luaopen_rive_promise(lua_State* L);

// ── ImageSampler ───────────────────────────────────────────────────────────

class ScriptedImageSampler
{
public:
    ImageSampler sampler;

    ScriptedImageSampler(ImageWrap wx, ImageWrap wy, ImageFilter f)
    {
        sampler.wrapX = wx;
        sampler.wrapY = wy;
        sampler.filter = f;
    }

    static constexpr uint8_t luaTag = LUA_T_COUNT + 7;
    static constexpr const char* luaName = "ImageSampler";
    static constexpr bool hasMetatable = false;
};

class ScriptedPaintData
{
public:
    ScriptedPaintData();
    ScriptedPaintData(const ShapePaint* shapePaint);
    virtual ~ScriptedPaintData() = default;

    static constexpr uint8_t luaTag = LUA_T_COUNT + 33;
    static constexpr const char* luaName = "PaintData";
    static constexpr bool hasMetatable = true;

    virtual void style(RenderPaintStyle style) { m_style = style; }

    virtual void color(ColorInt value) { m_color = value; }
    virtual void thickness(float value) { m_thickness = value; }

    virtual void join(StrokeJoin value) { m_join = value; }

    virtual void cap(StrokeCap value) { m_cap = value; }

    virtual void feather(float value) { m_feather = value; }

    virtual void blendMode(BlendMode value) { m_blendMode = value; }

    virtual void gradient(rcp<RenderShader> value) { m_gradient = value; }

    void pushStyle(lua_State* L);
    void pushJoin(lua_State* L);
    void pushCap(lua_State* L);
    void pushThickness(lua_State* L);
    void pushBlendMode(lua_State* L);
    void pushFeather(lua_State* L);
    void pushGradient(lua_State* L);
    void pushColor(lua_State* L);

protected:
    RenderPaintStyle m_style = RenderPaintStyle::fill;
    rcp<RenderShader> m_gradient;
    float m_thickness = 1;
    StrokeJoin m_join = StrokeJoin::miter;
    StrokeCap m_cap = StrokeCap::butt;
    float m_feather = 0;
    BlendMode m_blendMode = BlendMode::srcOver;
    ColorInt m_color = 0xFF000000;
};

class ScriptedPaint : public ScriptedPaintData
{
public:
    ScriptedPaint(Factory* factory);
    ScriptedPaint(Factory* factory, const ScriptedPaint& source);
    virtual ~ScriptedPaint() = default;
    rcp<RenderPaint> renderPaint;

    static constexpr uint8_t luaTag = LUA_T_COUNT + 8;
    static constexpr const char* luaName = "Paint";
    static constexpr bool hasMetatable = true;

    void style(RenderPaintStyle style) override
    {
        ScriptedPaintData::style(style);
        renderPaint->style(style);
    }

    void color(ColorInt value) override
    {
        ScriptedPaintData::color(value);
        renderPaint->color(value);
    }
    void thickness(float value) override
    {
        ScriptedPaintData::thickness(value);
        renderPaint->thickness(value);
    }

    void join(StrokeJoin value) override
    {
        ScriptedPaintData::join(value);
        renderPaint->join(value);
    }

    void cap(StrokeCap value) override
    {
        ScriptedPaintData::cap(value);
        renderPaint->cap(value);
    }

    void feather(float value) override
    {
        ScriptedPaintData::feather(value);
        renderPaint->feather(value);
    }

    void blendMode(BlendMode value) override
    {
        ScriptedPaintData::blendMode(value);
        renderPaint->blendMode(value);
    }

    void gradient(rcp<RenderShader> value) override
    {
        ScriptedPaintData::gradient(value);
        renderPaint->shader(value);
    }
};

class ScriptedRenderer
{
public:
    ScriptedRenderer(Renderer* renderer) : m_renderer(renderer), m_saveCount(0)
    {}

    // End usage of this ScriptedRenderer.
    bool end();

    void save(lua_State* L);
    void restore(lua_State* L);
    void transform(lua_State* L, const Mat2D& mat2d);
    void clipPath(lua_State* L, ScriptedPathData* path);
    Renderer* validate(lua_State* L);

    static constexpr uint8_t luaTag = LUA_T_COUNT + 9;
    static constexpr const char* luaName = "Renderer";
    static constexpr bool hasMetatable = true;

private:
    // Not owned by the ScriptedRenderer, only valid when passed in.
    Renderer* m_renderer = nullptr;
    uint32_t m_saveCount = 0;
};

class ScriptReffedArtboard : public RefCnt<ScriptReffedArtboard>
{
public:
    ScriptReffedArtboard(rcp<File> file,
                         std::unique_ptr<ArtboardInstance>&& artboardInstance,
                         rcp<ViewModelInstance> viewModelInstance,
                         rcp<DataContext> parentDataContext);

    ~ScriptReffedArtboard();
    rive::rcp<rive::File> file();
    Artboard* artboard();
    StateMachineInstance* stateMachine();
    rcp<ViewModelInstance> viewModelInstance() { return m_viewModelInstance; }

private:
    rcp<File> m_file;
    std::unique_ptr<ArtboardInstance> m_artboard;
    std::unique_ptr<StateMachineInstance> m_stateMachine;
    rcp<ViewModelInstance> m_viewModelInstance;
};

class ScriptedArtboard
{
public:
    ScriptedArtboard(lua_State* L,
                     rcp<File> file,
                     std::unique_ptr<ArtboardInstance>&& artboardInstance,
                     rcp<ViewModelInstance> viewModelInstance,
                     rcp<DataContext> dataContext);
    ~ScriptedArtboard();

    static constexpr uint8_t luaTag = LUA_T_COUNT + 10;
    static constexpr const char* luaName = "Artboard";
    static constexpr bool hasMetatable = true;

    Artboard* artboard() { return m_scriptReffedArtboard->artboard(); }
    StateMachineInstance* stateMachine()
    {
        return m_scriptReffedArtboard->stateMachine();
    }
    rcp<ViewModelInstance> viewModelInstance()
    {
        return m_scriptReffedArtboard->viewModelInstance();
    }

    rcp<ScriptReffedArtboard> scriptReffedArtboard()
    {
        return m_scriptReffedArtboard;
    }

    int pushData(lua_State* L);
    int instance(lua_State* L, rcp<ViewModelInstance> viewModelInstance);
    int animation(lua_State* L, const char* animationName);

    bool advance(float seconds);

    void cleanupDataRef(lua_State* L);

private:
    lua_State* m_state = nullptr;
    rcp<ScriptReffedArtboard> m_scriptReffedArtboard = nullptr;
    rcp<DataContext> m_dataContext = nullptr;
    int m_dataRef = 0;
};

class ScriptedAnimation
{
public:
    ScriptedAnimation(lua_State* L,
                      std::unique_ptr<LinearAnimationInstance> animation);

    static constexpr uint8_t luaTag = LUA_T_COUNT + 32;
    static constexpr const char* luaName = "Animation";
    static constexpr bool hasMetatable = true;
    float duration();
    int advance();
    int setTime(std::string mode);

private:
    lua_State* m_state;
    std::unique_ptr<LinearAnimationInstance> m_animation;
};

// Each listener keeps a registry ref to the property userdata (self) so Luau
// cannot collect ScriptedProperty while callbacks are registered. Released in
// clearListeners / removeListener alongside function and optional userdata
// refs.
struct ScriptedListener
{
    int function;
    int userdata;
    int propertySelfRef;
};

class ScriptedProperty : public ViewModelInstanceValueDelegate
{
public:
    ScriptedProperty(lua_State* L, rcp<ViewModelInstanceValue> value);
    virtual ~ScriptedProperty();
    int addListener();
    int removeListener();
    void clearListeners();
    virtual void dispose();

    void valueChanged() override;

    const lua_State* state() const { return m_state; }

    ViewModelInstanceValue* instanceValue() { return m_instanceValue.get(); }
    ScriptedObject* owner() const { return m_owner; }

private:
    std::vector<ScriptedListener> m_listeners;
    ScriptedObject* m_owner = nullptr;
#ifdef WITH_RIVE_TOOLS
    ScriptingContext* m_orphanContext = nullptr;
#endif
    bool m_disposed = false;

protected:
    lua_State* m_state;
    rcp<ViewModelInstanceValue> m_instanceValue;
};

class ScriptedViewModel
{
public:
    ScriptedViewModel(lua_State* L,
                      rcp<ViewModel> viewModel,
                      rcp<ViewModelInstance> viewModelInstance);
    ~ScriptedViewModel();
    static constexpr uint8_t luaTag = LUA_T_COUNT + 11;
    static constexpr const char* luaName = "ViewModel";
    static constexpr bool hasMetatable = true;
    int pushValue(const char* name, int coreType = 0);
    int pushIndex();
    int instance(lua_State* L);

    const lua_State* state() const { return m_state; }
    rcp<ViewModelInstance> viewModelInstance() const
    {
        return m_viewModelInstance;
    }
    rcp<ViewModelInstance> mutableViewModelInstance()
    {
        return m_viewModelInstance;
    }

private:
    lua_State* m_state;
    rcp<ViewModel> m_viewModel;
    rcp<ViewModelInstance> m_viewModelInstance;
    std::unordered_map<std::string, int> m_propertyRefs;
};

class ScriptedPropertyViewModel : public ScriptedProperty,
                                  public ViewModelValueDependent
{
public:
    ScriptedPropertyViewModel(lua_State* L,
                              rcp<ViewModel> viewModel,
                              rcp<ViewModelInstanceViewModel> value);
    ~ScriptedPropertyViewModel();
    static constexpr uint8_t luaTag = LUA_T_COUNT + 12;
    static constexpr const char* luaName = "PropertyViewModel";
    static constexpr bool hasMetatable = true;
    int pushValue();
    void setValue(ScriptedViewModel*);
    void dispose() override;
    void relinkDataBind() override;
    void addDirt(ComponentDirt value, bool recurse) override {}
    void clearRef();

private:
    rcp<ViewModel> m_viewModel;
    int m_valueRef = 0;
};

class ScriptedPropertyNumber : public ScriptedProperty
{
public:
    ScriptedPropertyNumber(lua_State* L, rcp<ViewModelInstanceNumber> value);
    static constexpr uint8_t luaTag = LUA_T_COUNT + 13;
    static constexpr const char* luaName = "Property<number>";
    static constexpr bool hasMetatable = true;

    int pushValue();
    void setValue(float value);
};

class ScriptedPropertyTrigger : public ScriptedProperty
{
public:
    ScriptedPropertyTrigger(lua_State* L, rcp<ViewModelInstanceTrigger> value);
    static constexpr uint8_t luaTag = LUA_T_COUNT + 14;
    static constexpr const char* luaName = "PropertyTrigger";
    static constexpr bool hasMetatable = true;
};

class ScriptedPropertyList : public ScriptedProperty
{
public:
    ScriptedPropertyList(lua_State* L, rcp<ViewModelInstanceList> value);
    ~ScriptedPropertyList();
    static constexpr uint8_t luaTag = LUA_T_COUNT + 15;
    static constexpr const char* luaName = "PropertyList";
    static constexpr bool hasMetatable = true;

    int pushLength();
    int pushValue(int index);
    void valueChanged() override;
    void append(ViewModelInstance*);

private:
    bool m_changed = false;
    std::unordered_map<ViewModelInstance*, int> m_propertyRefs;
};

class ScriptedPropertyColor : public ScriptedProperty
{
public:
    ScriptedPropertyColor(lua_State* L, rcp<ViewModelInstanceColor> value);
    static constexpr uint8_t luaTag = LUA_T_COUNT + 16;
    static constexpr const char* luaName = "PropertyColor";
    static constexpr bool hasMetatable = true;

    int pushValue();
    void setValue(unsigned value);
};

class ScriptedPropertyString : public ScriptedProperty
{
public:
    ScriptedPropertyString(lua_State* L, rcp<ViewModelInstanceString> value);
    static constexpr uint8_t luaTag = LUA_T_COUNT + 17;
    static constexpr const char* luaName = "PropertyString";
    static constexpr bool hasMetatable = true;

    int pushValue();
    void setValue(const std::string& value);
};

class ScriptedPropertyBoolean : public ScriptedProperty
{
public:
    ScriptedPropertyBoolean(lua_State* L, rcp<ViewModelInstanceBoolean> value);
    static constexpr uint8_t luaTag = LUA_T_COUNT + 18;
    static constexpr const char* luaName = "Property<bool>";
    static constexpr bool hasMetatable = true;

    int pushValue();
    void setValue(bool value);
};

class ScriptedEnumValues
{
public:
    ScriptedEnumValues(lua_State* L, DataEnum* value) :
        m_state(L), m_dataEnum(value)
    {}
    static constexpr uint8_t luaTag = LUA_T_COUNT + 34;
    static constexpr const char* luaName = "EnumValues";
    static constexpr bool hasMetatable = true;
    void dataEnum(DataEnum* value) { m_dataEnum = value; }
    int pushValue(int index);
    int pushLength();

    const lua_State* state() const { return m_state; }

private:
    lua_State* m_state = nullptr;
    DataEnum* m_dataEnum = nullptr;
};

class ScriptedPropertyEnum : public ScriptedProperty
{
public:
    ScriptedPropertyEnum(lua_State* L, rcp<ViewModelInstanceEnum> value);
    static constexpr uint8_t luaTag = LUA_T_COUNT + 19;
    static constexpr const char* luaName = "Property<enum>";
    static constexpr bool hasMetatable = true;

    int pushValue();
    void setValue(const std::string& value);
};

class ScriptedPropertyImage : public ScriptedProperty
{
public:
    ScriptedPropertyImage(lua_State* L, rcp<ViewModelInstanceAssetImage> value);
    static constexpr uint8_t luaTag = LUA_T_COUNT + 49;
    static constexpr const char* luaName = "Property<Image>";
    static constexpr bool hasMetatable = true;

    int pushValue();
    void setValue(ScriptedImage* scriptedImage);
};

// Make
// ScriptedPropertyViewModel
//      - Nullable ViewModelInstanceValue (ViewModelInstanceViewModel)
//      - Requires ViewModel to know which properties to expect
// ScriptedPropertyArtboard
//      - Nullable ViewModelInstanceValue (ViewModelInstanceArtboard)

// Make renderer: return lua_newrive<ScriptedRenderer>(L, renderer);
template <class T, class... Args>
static T* lua_newrive(lua_State* L, Args&&... args)
{
    if (T::hasMetatable)
    {
        return new (lua_newuserdatataggedwithmetatable(L, sizeof(T), T::luaTag))
            T(std::forward<Args>(args)...);
    }
    else
    {
        return new (lua_newuserdatatagged(L, sizeof(T), T::luaTag))
            T(std::forward<Args>(args)...);
    }
}

BlendMode lua_toblendmode(lua_State* L, int idx);

template <typename T>
static T* lua_torive(lua_State* L, int idx, bool allowNil = false)
{
    T* riveObject = (T*)lua_touserdatatagged(L, idx, T::luaTag);
    if (!allowNil && riveObject == nullptr)
    {
        luaL_typeerror(L, idx, T::luaName);
        return nullptr;
    }
    return riveObject;
}

template <typename T> static void lua_register_rive(lua_State* L)
{
    if (T::hasMetatable)
    {
        // create metatable for T
        luaL_newmetatable(L, T::luaName);
        // lua_createtable(L, 0, 1);

        // push it again as lua_setuserdatametatable pops it
        lua_pushvalue(L, -1);
        lua_setuserdatametatable(L, T::luaTag);
    }
    if (!std::is_trivially_destructible<T>::value)
    {
        // We only need to call the C++ destructor if the object is not
        // trivially destructible.
        lua_setuserdatadtor(L, T::luaTag, [](lua_State* L, void* data) {
            ((T*)data)->~T();
        });
    }
}

inline const Vec2D* lua_checkvec2d(lua_State* L, int stack)
{
    return (const Vec2D*)luaL_checkvector(L, stack);
}

inline const Vec2D* lua_tovec2d(lua_State* L, int stack)
{
    return (const Vec2D*)lua_tovector(L, stack);
}

inline void lua_pushvec2d(lua_State* L, Vec2D vec)
{
    return lua_pushvector2(L, vec.x, vec.y);
}

int luaopen_rive(lua_State* L);
int rive_luaErrorHandler(lua_State* L);
int rive_lua_pcall(lua_State* state, int nargs, int nresults);
int rive_lua_pcall_with_context(lua_State* state,
                                ScriptedObject* scriptedObject,
                                int nargs,
                                int nresults);
int rive_lua_pushRef(lua_State* state, int ref);
void rive_lua_pop(lua_State* state, int count);

class ScriptingContext
{
public:
    ScriptingContext(Factory* factory) : m_factory(factory) {}
    virtual ~ScriptingContext() { shutdownAsync(); }
    Factory* factory() const { return m_factory; }
    ScriptedObject* currentScriptedObject() const
    {
        return m_currentScriptedObject;
    }
    void currentScriptedObject(ScriptedObject* value)
    {
        m_currentScriptedObject = value;
    }

    virtual void printError(lua_State* state) = 0;
    virtual void printBeginLine(lua_State* state) = 0;
    virtual void print(Span<const char> data) = 0;
    virtual void printEndLine() = 0;
    virtual int pCall(lua_State* state, int nargs, int nresults) = 0;

    // Add a module to be registered later via performRegistration()
    void addModule(ModuleDetails* moduleDetails);
    // Perform registration of all added modules, handling dependencies and
    // retries
    void performRegistration(lua_State* state);
    // Called when a module is required but not found during registration
    void recordMissingDependency(const std::string& requiringModule,
                                 const std::string& missingModule);

    // Ore GPU context for this VM — set during VM adoption (response phase).
    // Stored as void* to avoid coupling this header to ore headers; callers
    // cast to ore::Context*.
    void setOreContext(void* ctx) { m_oreContext = ctx; }
    void* oreContext() const { return m_oreContext; }

    // GPU render context — set once at startup by the host app.
    // Stored as void* to avoid coupling this header to gpu headers; callers
    // cast to gpu::RenderContext*.
    void setRenderContext(void* ctx) { m_renderContext = ctx; }
    void* renderContext() const { return m_renderContext; }

    // WorkPool for async operations (image decode, etc.).
    // Lazily created on first access. Shared across all contexts via a
    // process-global singleton.
    class WorkPool* workPool();
    uint64_t ownerId() const { return m_ownerId; }

    // Cancel all pending async tasks for this context. Must be called
    // BEFORE lua_close() to prevent callbacks on a dead Lua state.
    void shutdownAsync();

    // Like shutdownAsync but also cancels WASM browser-native decodes
    // that bypass WorkPool. Call from ~ScriptingVM with the main lua_State.
    void shutdownAsyncForState(lua_State* mainThread);

    // WebGL/WASM only: whether an ore frame is currently open for this VM.
    // riveLuaPCall uses this to auto-open a mini-frame when Lua callbacks
    // fire outside the normal render boundary (e.g. Coop stream events).
    void setOreFrameOpen(bool open) { m_oreFrameOpen = open; }
    bool oreFrameOpen() const { return m_oreFrameOpen; }

    // WebGL/WASM only: GL context handle saved at riveGPUBeginFrame so
    // riveGPUEndFrame can restore the caller's context afterwards.
    void setPrevGLContext(intptr_t h) { m_prevGLContext = h; }
    intptr_t prevGLContext() const { return m_prevGLContext; }

#ifdef __EMSCRIPTEN__
    void setGLHandle(int h) { m_glHandle = h; }
    int glHandle() const { return m_glHandle; }
#endif

private:
    bool tryRegisterModule(lua_State* state, ModuleDetails* moduleDetails);
    void sortNextModule(ModuleDetails* module,
                        std::vector<ModuleDetails*>* pendingModules,
                        std::vector<ModuleDetails*>* sortedModules,
                        std::unordered_set<ModuleDetails*>* visitedModules);
    // Called when a module successfully registers
    void onModuleRegistered(ModuleDetails* moduleDetails);

private:
    void* m_oreContext = nullptr;
    void* m_renderContext = nullptr;
    uint64_t m_ownerId = 0;
    bool m_oreFrameOpen = false;
    intptr_t m_prevGLContext = 0;
#ifdef __EMSCRIPTEN__
    int m_glHandle = 0;
#endif

    Factory* m_factory;
    ScriptedObject* m_currentScriptedObject = nullptr;
    std::vector<ModuleDetails*> m_modulesToRegister;
    std::unordered_map<std::string, ModuleDetails*> m_moduleLookup;
    std::unordered_set<ModuleDetails*> m_pendingModules;

#ifdef WITH_RIVE_TOOLS
    // Editor-only: Map from asset ID to generator function ref.
    // Allows direct ScriptedObject reinitialization without regenerating
    // the runtime file.
    std::unordered_map<uint32_t, int> m_assetGeneratorRefs;
    bool m_isPlaying = false;
    std::vector<ScriptedProperty*> m_orphanScriptedProperties;

    // Per-VM RSTB blobs for WGSL shaders compiled during requestVM. Populated
    // by the scripting workspace response phase; looked up by loadShader().
    std::unordered_map<std::string, std::vector<uint8_t>> m_shaderRstbs;

public:
    void setGeneratorRef(uint32_t assetId, int ref);
    int getGeneratorRef(uint32_t assetId) const;
    void clearGeneratorRefs();
    bool hasGeneratorRef(uint32_t assetId) const;
    void isPlaying(bool value) { m_isPlaying = value; }
    bool isPlaying() const { return m_isPlaying; }
    void trackOrphanScriptedProperty(ScriptedProperty* property);
    void untrackOrphanScriptedProperty(ScriptedProperty* property);
    void disposeOrphanScriptedProperties();
    void registerShaderRstb(std::string name, std::vector<uint8_t> bytes);
    const std::vector<uint8_t>* findShaderRstb(const std::string& name) const;
    // Transfers all RSTB blobs out of this context (used during VM adoption
    // to preserve blobs across context replacement).
    std::unordered_map<std::string, std::vector<uint8_t>> takeShaderRstbs()
    {
        return std::move(m_shaderRstbs);
    }
#endif
};

class ScopedScriptedObjectContext
{
public:
    ScopedScriptedObjectContext(ScriptingContext* context,
                                ScriptedObject* scriptedObject) :
        m_context(context),
        m_previous(context == nullptr ? nullptr
                                      : context->currentScriptedObject())
    {
        if (m_context != nullptr)
        {
            m_context->currentScriptedObject(scriptedObject);
        }
    }

    ~ScopedScriptedObjectContext()
    {
        if (m_context != nullptr)
        {
            m_context->currentScriptedObject(m_previous);
        }
    }

private:
    ScriptingContext* m_context;
    ScriptedObject* m_previous;
};

class ScriptedDataValue
{
public:
    ScriptedDataValue(lua_State* L) { m_state = L; }
    virtual ~ScriptedDataValue();
    static constexpr const char* luaName = "DataValue";
    DataValue* dataValue() { return m_dataValue; }
    virtual bool isNumber() { return false; }
    virtual bool isString() { return false; }
    virtual bool isBoolean() { return false; }
    virtual bool isColor() { return false; }

    const lua_State* state() const { return m_state; }

protected:
    lua_State* m_state;
    DataValue* m_dataValue = nullptr;
};

class ScriptedDataValueNumber : public ScriptedDataValue
{
public:
    ScriptedDataValueNumber(lua_State* L, float value) : ScriptedDataValue(L)
    {
        m_dataValue = new DataValueNumber(value);
    }
    static constexpr bool hasMetatable = true;
    static constexpr uint8_t luaTag = LUA_T_COUNT + 20;
    static constexpr const char* luaName = "DataValueNumber";
    bool isNumber() override { return true; }
};

class ScriptedDataValueString : public ScriptedDataValue
{
public:
    ScriptedDataValueString(lua_State* L, std::string value) :
        ScriptedDataValue(L)
    {
        m_dataValue = new DataValueString(value);
    }
    static constexpr bool hasMetatable = true;
    static constexpr uint8_t luaTag = LUA_T_COUNT + 21;
    static constexpr const char* luaName = "DataValueString";
    bool isString() override { return true; }
};

class ScriptedDataValueBoolean : public ScriptedDataValue
{
public:
    ScriptedDataValueBoolean(lua_State* L, bool value) : ScriptedDataValue(L)
    {
        m_dataValue = new DataValueBoolean(value);
    }
    static constexpr bool hasMetatable = true;
    static constexpr uint8_t luaTag = LUA_T_COUNT + 22;
    static constexpr const char* luaName = "DataValueBoolean";
    bool isBoolean() override { return true; }
};

class ScriptedDataValueColor : public ScriptedDataValue
{
public:
    ScriptedDataValueColor(lua_State* L, int value) : ScriptedDataValue(L)
    {
        m_dataValue = new DataValueColor(value);
    }
    static constexpr bool hasMetatable = true;
    static constexpr uint8_t luaTag = LUA_T_COUNT + 23;
    static constexpr const char* luaName = "DataValueColor";
    bool isColor() override { return true; }
};

class ScriptedPointerEvent
{
public:
    ScriptedPointerEvent(uint8_t id,
                         Vec2D position,
                         Vec2D previousPosition = Vec2D(),
                         int hitListenerType = 0,
                         float timeStamp = 0.f) :
        m_id(id),
        m_position(position),
        m_previousPosition(previousPosition),
        m_hitListenerType(hitListenerType),
        m_timeStamp(timeStamp)
    {}

    static constexpr uint8_t luaTag = LUA_T_COUNT + 24;
    static constexpr const char* luaName = "PointerEvent";
    static constexpr bool hasMetatable = true;

    uint8_t m_id = 0;
    Vec2D m_position;
    Vec2D m_previousPosition;
    int m_hitListenerType = 0;
    float m_timeStamp = 0.f;
    HitResult m_hitResult = HitResult::none;
};

class ScriptedNode
{
public:
    ScriptedNode(rcp<ScriptReffedArtboard> artboard,
                 TransformComponent* component);

    static constexpr uint8_t luaTag = LUA_T_COUNT + 25;
    static constexpr const char* luaName = "NodeData";
    static constexpr bool hasMetatable = true;

    TransformComponent* component() { return m_component; }
    rcp<ScriptReffedArtboard> artboard() { return m_artboard; }

    const ShapePaint* shapePaint();
    void shapePaint(const ShapePaint* shapePaint) { m_shapePaint = shapePaint; }

private:
    rcp<ScriptReffedArtboard> m_artboard;
    TransformComponent* m_component = nullptr;
    const ShapePaint* m_shapePaint = nullptr;
};

class ScriptedContourMeasure
{
public:
    ScriptedContourMeasure(rcp<ContourMeasure> measure,
                           rcp<RefCntContourMeasureIter> iter) :
        m_measure(measure), m_iter(iter)
    {}

    static constexpr uint8_t luaTag = LUA_T_COUNT + 26;
    static constexpr const char* luaName = "ContourMeasure";
    static constexpr bool hasMetatable = true;

    ContourMeasure* measure() { return m_measure.get(); }
    rcp<RefCntContourMeasureIter> iter() { return m_iter; }

private:
    rcp<ContourMeasure> m_measure;
    rcp<RefCntContourMeasureIter> m_iter;
};

class ScriptedPathMeasure
{
public:
    ScriptedPathMeasure(PathMeasure measure) : m_measure(std::move(measure)) {}

    static constexpr uint8_t luaTag = LUA_T_COUNT + 27;
    static constexpr const char* luaName = "PathMeasure";
    static constexpr bool hasMetatable = true;

    PathMeasure* measure() { return &m_measure; }

private:
    PathMeasure m_measure;
};

class ScriptedContext
{
public:
    ScriptedContext(ScriptedObject*);
    ScriptedObject* scriptedObject() { return m_scriptedObject; }
    void clearScriptedObject() { m_scriptedObject = nullptr; }
    int pushViewModel(lua_State*);
    int pushRootViewModel(lua_State*);
    int pushDataContext(lua_State*);
    static constexpr uint8_t luaTag = LUA_T_COUNT + 28;
    static constexpr const char* luaName = "Context";
    static constexpr bool hasMetatable = true;
    bool missingRequestedData() { return m_missingRequestedData; }

private:
    ScriptedObject* m_scriptedObject = nullptr;
    bool m_missingRequestedData = false;
};

/// Wraps [`ListenerInvocation`] for `performAction` in scripted listener
/// actions.
class ScriptedInvocation
{
public:
    explicit ScriptedInvocation(ListenerInvocation inv) :
        m_invocation(std::move(inv))
    {}

    ListenerInvocation& invocation() { return m_invocation; }
    const ListenerInvocation& invocation() const { return m_invocation; }

    static constexpr uint8_t luaTag = LUA_T_COUNT + 54;
    static constexpr const char* luaName = "Invocation";
    static constexpr bool hasMetatable = true;

private:
    ListenerInvocation m_invocation;
};

class ScriptedKeyboardInvocation
{
public:
    ScriptedKeyboardInvocation(Key key,
                               KeyModifiers modifiers,
                               bool isPressed,
                               bool isRepeat) :
        m_key(key),
        m_modifiers(modifiers),
        m_isPressed(isPressed),
        m_isRepeat(isRepeat)
    {}

    static constexpr uint8_t luaTag = LUA_T_COUNT + 55;
    static constexpr const char* luaName = "KeyboardInvocation";
    static constexpr bool hasMetatable = true;

    Key m_key;
    KeyModifiers m_modifiers;
    bool m_isPressed;
    bool m_isRepeat;
};

class ScriptedTextInputInvocation
{
public:
    explicit ScriptedTextInputInvocation(std::string text) :
        m_text(std::move(text))
    {}

    static constexpr uint8_t luaTag = LUA_T_COUNT + 56;
    static constexpr const char* luaName = "TextInputInvocation";
    static constexpr bool hasMetatable = true;

    const std::string& text() const { return m_text; }

private:
    std::string m_text;
};

class ScriptedFocusInvocation
{
public:
    explicit ScriptedFocusInvocation(bool isFocus) : m_isFocus(isFocus) {}

    static constexpr uint8_t luaTag = LUA_T_COUNT + 57;
    static constexpr const char* luaName = "FocusInvocation";
    static constexpr bool hasMetatable = true;

    bool m_isFocus;
};

class ScriptedReportedEventInvocation
{
public:
    ScriptedReportedEventInvocation(Event* event, float delaySeconds) :
        m_event(event), m_delaySeconds(delaySeconds)
    {}

    static constexpr uint8_t luaTag = LUA_T_COUNT + 58;
    static constexpr const char* luaName = "ReportedEventInvocation";
    static constexpr bool hasMetatable = true;

    /// Valid only for the duration of the listener callback; do not retain.
    Event* m_event;
    float m_delaySeconds;
};

class ScriptedViewModelChangeInvocation
{
public:
    ScriptedViewModelChangeInvocation() = default;

    static constexpr uint8_t luaTag = LUA_T_COUNT + 59;
    static constexpr const char* luaName = "ViewModelChangeInvocation";
    static constexpr bool hasMetatable = true;
};

class ScriptedGamepadInvocation
{
public:
    ScriptedGamepadInvocation(int deviceId, uint64_t buttonMask, float axis0) :
        m_deviceId(deviceId), m_buttonMask(buttonMask), m_axis0(axis0)
    {}

    static constexpr uint8_t luaTag = LUA_T_COUNT + 60;
    static constexpr const char* luaName = "GamepadInvocation";
    static constexpr bool hasMetatable = true;

    int m_deviceId;
    uint64_t m_buttonMask;
    float m_axis0;
};

class ScriptedNoneInvocation
{
public:
    ScriptedNoneInvocation() = default;

    static constexpr uint8_t luaTag = LUA_T_COUNT + 61;
    static constexpr const char* luaName = "NoneInvocation";
    static constexpr bool hasMetatable = true;
};

void rive_lua_register_listener_invocation_types(lua_State* L);
void rive_lua_push_pointer_arg_for_perform(lua_State* L,
                                           const ListenerInvocation& inv);
void rive_lua_push_scripted_invocation(lua_State* L,
                                       const ListenerInvocation& inv);

static void interruptCPP(lua_State* L, int gc);

#ifdef WITH_RIVE_TOOLS
// Callback type for notifying when console data is available.
// If null, console output goes to stdout.
using ConsoleCallback = void (*)();
#endif

class CPPRuntimeScriptingContext : public ScriptingContext
{
public:
#ifdef WITH_RIVE_TOOLS
    CPPRuntimeScriptingContext(Factory* factory,
                               int timeoutMs = 200,
                               ConsoleCallback consoleCallback = nullptr) :
        ScriptingContext(factory),
        m_timeoutMs(timeoutMs),
        m_consoleCallback(consoleCallback)
    {}
#else
    CPPRuntimeScriptingContext(Factory* factory, int timeoutMs = 200) :
        ScriptingContext(factory), m_timeoutMs(timeoutMs)
    {}
#endif
    virtual ~CPPRuntimeScriptingContext() = default;

    std::chrono::time_point<std::chrono::steady_clock> executionTime;

    int timeoutMs() const { return m_timeoutMs; }
    void setTimeoutMs(int ms) { m_timeoutMs = ms; }

    int pCall(lua_State* state, int nargs, int nresults) override;

    void printBeginLine(lua_State* state) override
    {
#ifdef WITH_RIVE_TOOLS
        BinaryWriter writer(&m_consoleBuffer);
        lua_Debug ar;
        bool hasInfo = lua_getinfo(state, 1, "sl", &ar) != 0;
        writer.write((uint8_t)0);
        writer.write(hasInfo && ar.source != nullptr ? ar.source : "");
        writer.writeVarUint((uint32_t)(hasInfo ? ar.currentline : 0));
#endif
    }

    void print(Span<const char> data) override
    {
#ifdef WITH_RIVE_TOOLS
        if (data.size() == 0)
        {
            return;
        }
        BinaryWriter writer(&m_consoleBuffer);
        writer.writeVarUint((uint64_t)data.size());
        writer.write((const uint8_t*)data.data(), (size_t)data.size());

        if (m_consoleCallback == nullptr)
#endif
        {
            auto message = std::string(data.data(), data.size());
            printf("%s", message.c_str());
        }
    }

    void printEndLine() override
    {
#ifdef WITH_RIVE_TOOLS
        BinaryWriter writer(&m_consoleBuffer);
        writer.writeVarUint((uint32_t)0);

        if (m_consoleCallback != nullptr)
        {
            if (!m_calledConsoleCallback)
            {
                m_calledConsoleCallback = true;
                m_consoleCallback();
            }
        }
        else
#endif
        {
            printf("\n");
        }
    }

    void printError(lua_State* state) override
    {
        const char* error = lua_tostring(state, -1);
        fprintf(stderr, "%s\n", error);
#ifdef WITH_RIVE_TOOLS
        if (error)
        {
            printBeginLine(state);
            print(Span<const char>(error, strlen(error)));
            printEndLine();
        }
#endif
    }

    void startTimedExecution(lua_State* state)
    {
        if (m_timeoutMs == 0)
        {
            return;
        }
        lua_Callbacks* cb = lua_callbacks(state);
        cb->interrupt = interruptCPP;
        executionTime = std::chrono::steady_clock::now();
    }

    void endTimedExecution(lua_State* state)
    {
        if (m_timeoutMs == 0)
        {
            return;
        }
        lua_Callbacks* cb = lua_callbacks(state);
        cb->interrupt = nullptr;
    }

#ifdef WITH_RIVE_TOOLS
    // Console buffer access for editor
    Span<uint8_t> consoleMemory() { return m_consoleBuffer.memory(); }

    void clearConsole()
    {
        m_consoleBuffer.clear();
        m_calledConsoleCallback = false;
    }

    bool hasConsoleCallback() const { return m_consoleCallback != nullptr; }
#endif

private:
    int m_timeoutMs = 200;
#ifdef WITH_RIVE_TOOLS
    ConsoleCallback m_consoleCallback = nullptr;
    VectorBinaryStream m_consoleBuffer;
    bool m_calledConsoleCallback = false;
#endif
};

class ScriptedDataContext
{
public:
    ScriptedDataContext(lua_State* L, rcp<DataContext> dataContext);
    static constexpr uint8_t luaTag = LUA_T_COUNT + 36;
    static constexpr const char* luaName = "DataContext";
    static constexpr bool hasMetatable = true;
    int pushViewModel();
    int pushParent();

    const lua_State* state() const { return m_state; }

private:
    lua_State* m_state = nullptr;
    rcp<DataContext> m_dataContext = nullptr;
};

static void interruptCPP(lua_State* L, int gc)
{
    if (gc >= 0 || !lua_isyieldable(L))
    {
        return;
    }

    CPPRuntimeScriptingContext* context =
        static_cast<CPPRuntimeScriptingContext*>(lua_getthreaddata(L));

    const auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now - context->executionTime)
                  .count();
    if (ms > context->timeoutMs())
    {
        lua_Callbacks* cb = lua_callbacks(L);
        cb->interrupt = nullptr;
        // reserve space for error string
        lua_rawcheckstack(L, 1);

        // Format human-readable error message
        char errorMsg[128];
        int timeoutMs = context->timeoutMs();
        if (timeoutMs >= 1000)
        {
            double seconds = timeoutMs / 1000.0;
            snprintf(errorMsg,
                     sizeof(errorMsg),
                     "execution exceeded %.1f second%s timeout",
                     seconds,
                     seconds == 1.0 ? "" : "s");
        }
        else
        {
            snprintf(errorMsg,
                     sizeof(errorMsg),
                     "execution exceeded %d millisecond%s timeout",
                     timeoutMs,
                     timeoutMs == 1 ? "" : "s");
        }
        luaL_error(L, "%s", errorMsg);
    }
}
} // namespace rive

#ifdef RIVE_CANVAS
#ifdef RIVE_ORE
namespace rive
{
class ShaderAsset;
}
// Load a shader by name into a ScriptedShader (populates both vertex and
// fragment modules for GLSL targets with split entry points).
// Checks ScriptingContext::m_shaderRstbs first (editor path, compiled
// during requestVM), then |fileAsset| if non-null (runtime .riv path).
// Returns false on failure.
bool lua_gpu_load_shader_by_name(rive::ScriptedShader* out,
                                 rive::ScriptingContext* context,
                                 const char* name,
                                 rive::ShaderAsset* fileAsset);

// Build a ScriptedShader directly from raw RSTB bytes. Used by the legacy
// `loadShader` fallback for .riv files that packed WGSL shaders into
// ScriptAsset containers before the ShaderAsset split.
bool lua_gpu_make_shader_from_rstb(rive::ScriptedShader* out,
                                   rive::ScriptingContext* context,
                                   const uint8_t* data,
                                   uint32_t len);

// Compile a shader by name and push the resulting ScriptedShader onto the
// Lua stack. Returns 1 on success, 0 on failure. Declared here (implemented
// in lua_gpu.cpp) so callers that only have a forward-declaration of
// ShaderModule do not need to touch rcp<ShaderModule> directly.
int lua_gpu_push_shader_by_name(lua_State* L, const char* name);
#endif // RIVE_ORE
#endif // RIVE_CANVAS

// Push a GPU features table onto the Lua stack. Queries the ORE context when
// available, otherwise returns conservative defaults. Always returns 1.
// Implemented in lua_scripted_context.cpp.
int lua_push_gpu_features(lua_State* L);

#endif
#endif
