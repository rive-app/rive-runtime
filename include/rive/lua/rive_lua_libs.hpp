#ifdef WITH_RIVE_SCRIPTING
#ifndef _RIVE_LUA_LIBS_HPP_
#define _RIVE_LUA_LIBS_HPP_
#include "lua.h"
#include "lualib.h"
#include "rive/lua/lua_state.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/renderer.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/shapes/paint/image_sampler.hpp"
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
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/hit_result.hpp"

#include <chrono>
#include <unordered_map>

static const int maxCStack = 8000;
static const int luaGlobalsIndex = -maxCStack - 2002;
static const int luaRegistryIndex = -maxCStack - 2000;

namespace rive
{
class Factory;
enum class LuaAtoms : int16_t
{
    // Vec2D
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
    addListener,
    removeListener,
    fire,

    // Artboards
    draw,
    advance,
    frameOrigin,
    data,
    instance,
    bounds,

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
    node
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

    rive::Mat2D value;
};

static_assert(std::is_trivially_destructible<ScriptedMat2D>::value,
              "ScriptedMat2D must be trivially destructible");

class ScriptedPath
{
public:
    RawPath rawPath;
    RenderPath* renderPath(lua_State* L);
    static constexpr uint8_t luaTag = LUA_T_COUNT + 2;
    static constexpr const char* luaName = "Path";
    static constexpr bool hasMetatable = true;

    void markDirty() { m_isRenderPathDirty = true; }

private:
    rcp<RenderPath> m_renderPath;

    bool m_isRenderPathDirty = true;
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

class ScriptedImage
{
public:
    rcp<RenderImage> image;
    static constexpr uint8_t luaTag = LUA_T_COUNT + 6;
    static constexpr const char* luaName = "Image";
    static constexpr bool hasMetatable = true;
};

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

class ScriptedPaint
{
public:
    ScriptedPaint(Factory* factory);
    ScriptedPaint(Factory* factory, const ScriptedPaint& source);
    rcp<RenderPaint> renderPaint;

    static constexpr uint8_t luaTag = LUA_T_COUNT + 8;
    static constexpr const char* luaName = "Paint";
    static constexpr bool hasMetatable = true;

    void style(RenderPaintStyle style)
    {
        m_style = style;
        renderPaint->style(style);
    }

    void color(ColorInt value)
    {
        m_color = value;
        renderPaint->color(value);
    }
    void thickness(float value)
    {
        m_thickness = value;
        renderPaint->thickness(value);
    }

    void join(StrokeJoin value)
    {
        m_join = value;
        renderPaint->join(value);
    }

    void cap(StrokeCap value)
    {
        m_cap = value;
        renderPaint->cap(value);
    }

    void feather(float value)
    {
        m_feather = value;
        renderPaint->feather(value);
    }

    void blendMode(BlendMode value)
    {
        m_blendMode = value;
        renderPaint->blendMode(value);
    }

    void gradient(rcp<RenderShader> value)
    {
        m_gradient = value;
        renderPaint->shader(value);
    }

    void pushStyle(lua_State* L);
    void pushJoin(lua_State* L);
    void pushCap(lua_State* L);
    void pushThickness(lua_State* L);
    void pushBlendMode(lua_State* L);
    void pushFeather(lua_State* L);
    void pushGradient(lua_State* L);
    void pushColor(lua_State* L);

private:
    RenderPaintStyle m_style = RenderPaintStyle::fill;
    rcp<RenderShader> m_gradient;
    float m_thickness = 1;
    StrokeJoin m_join = StrokeJoin::miter;
    StrokeCap m_cap = StrokeCap::butt;
    float m_feather = 0;
    BlendMode m_blendMode = BlendMode::srcOver;
    ColorInt m_color = 0xFF000000;
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
    void clipPath(lua_State* L, ScriptedPath* path);
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
                         std::unique_ptr<ArtboardInstance>&& artboardInstance);

    ~ScriptReffedArtboard();
    rive::rcp<rive::File> file() { return m_file; }
    Artboard* artboard() { return m_artboard.get(); }
    StateMachineInstance* stateMachine() { return m_stateMachine.get(); }
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
    ScriptedArtboard(rcp<File> file,
                     std::unique_ptr<ArtboardInstance>&& artboardInstance);

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
    int instance(lua_State* L);

    bool advance(float seconds);

private:
    rcp<ScriptReffedArtboard> m_scriptReffedArtboard;
    int m_dataRef = 0;
};

struct ScriptedListener
{
    int function;
    int userdata;
};

class ScriptedProperty : public ViewModelInstanceValueDelegate
{
public:
    ScriptedProperty(lua_State* L, rcp<ViewModelInstanceValue> value);
    virtual ~ScriptedProperty();
    int addListener();
    int removeListener();
    void clearListeners();

    void valueChanged() override;

    const lua_State* state() const { return m_state; }

    ViewModelInstanceValue* instanceValue() { return m_instanceValue.get(); }

private:
    std::vector<ScriptedListener> m_listeners;

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

    const lua_State* state() const { return m_state; }

private:
    lua_State* m_state;
    rcp<ViewModel> m_viewModel;
    rcp<ViewModelInstance> m_viewModelInstance;
    std::unordered_map<std::string, int> m_propertyRefs;
};

class ScriptedPropertyViewModel : public ScriptedProperty
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
int rive_lua_pushRef(lua_State* state, int ref);
void rive_lua_pop(lua_State* state, int count);

class ScriptingContext
{
public:
    ScriptingContext(Factory* factory) : m_factory(factory) {}
    Factory* factory() const { return m_factory; }

    virtual void printError(lua_State* state) = 0;
    virtual void printBeginLine(lua_State* state) = 0;
    virtual void print(Span<const char> data) = 0;
    virtual void printEndLine() = 0;
    virtual int pCall(lua_State* state, int nargs, int nresults) = 0;

private:
    Factory* m_factory;
};

class ScriptingVM
{
public:
    ScriptingVM(ScriptingContext* context);
    ~ScriptingVM();

    // ScriptingContext& context() { return m_context; }
    lua_State* state() { return m_state; }

    static void init(lua_State* state, ScriptingContext* context);
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
    static void dumpStack(lua_State* state);

private:
    lua_State* m_state;
    ScriptingContext* m_context;
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
    ScriptedPointerEvent(uint8_t id, Vec2D position) :
        m_id(id), m_position(position)
    {}

    static constexpr uint8_t luaTag = LUA_T_COUNT + 24;
    static constexpr const char* luaName = "PointerEvent";
    static constexpr bool hasMetatable = true;

    uint8_t m_id = 0;
    Vec2D m_position;
    HitResult m_hitResult = HitResult::none;
};

class ScriptedNode
{
public:
    ScriptedNode(rcp<ScriptReffedArtboard> artboard,
                 TransformComponent* component);

    static constexpr uint8_t luaTag = LUA_T_COUNT + 25;
    static constexpr const char* luaName = "Node";
    static constexpr bool hasMetatable = true;

    TransformComponent* component() { return m_component; }
    rcp<ScriptReffedArtboard> artboard() { return m_artboard; }

private:
    rcp<ScriptReffedArtboard> m_artboard;
    TransformComponent* m_component;
};

static void interruptCPP(lua_State* L, int gc);

class CPPRuntimeScriptingContext : public ScriptingContext
{
public:
    CPPRuntimeScriptingContext(Factory* factory) : ScriptingContext(factory) {}

    std::chrono::time_point<std::chrono::steady_clock> executionTime;

    int pCall(lua_State* state, int nargs, int nresults) override;

    void printBeginLine(lua_State* state) override {}
    void print(Span<const char> data) override
    {
        auto message = std::string(data.data(), data.size());
        puts(message.c_str());
    }

    void printEndLine() override { puts("\n"); }

    void printError(lua_State* state) override
    {
        const char* error = lua_tostring(state, -1);
        fprintf(stderr, "%s", error);
    }

    void startTimedExecution(lua_State* state)
    {
        lua_Callbacks* cb = lua_callbacks(state);
        cb->interrupt = interruptCPP;
        executionTime = std::chrono::steady_clock::now();
    }

    void endTimedExecution(lua_State* state)
    {
        lua_Callbacks* cb = lua_callbacks(state);
        cb->interrupt = nullptr;
    }
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
    if (ms > 50)
    {
        lua_Callbacks* cb = lua_callbacks(L);
        cb->interrupt = nullptr;
        // reserve space for error string
        lua_rawcheckstack(L, 1);
        luaL_error(L, "execution took too long");
    }
}
} // namespace rive
#endif
#endif
