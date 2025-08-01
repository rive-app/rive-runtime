#ifdef WITH_RIVE_SCRIPTING
#ifndef _RIVE_LUA_LIBS_HPP_
#define _RIVE_LUA_LIBS_HPP_
#include "lua.h"
#include "lualib.h"
#include "rive/math/raw_path.hpp"
#include "rive/renderer.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/shapes/paint/image_sampler.hpp"
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
    trilinear,
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
    value
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

struct ScriptingPropertyFloat
{
    static constexpr uint8_t luaTag = LUA_T_COUNT + 10;
    static constexpr const char* luaName = "Property<number>";
    static constexpr bool hasMetatable = true;
    float value;
};

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

struct ScriptingContext
{
    Factory* factory;
};

class ScriptingVM
{
public:
    ScriptingVM(Factory* factory);
    ~ScriptingVM();

    ScriptingContext& context() { return m_context; }
    lua_State* state() { return m_state; }

    static void init(lua_State* state, ScriptingContext* context);
    static bool registerModule(lua_State* state,
                               const char* name,
                               Span<uint8_t> bytecode);

    bool registerModule(const char* name, Span<uint8_t> bytecode);

private:
    lua_State* m_state;
    ScriptingContext m_context;
};

} // namespace rive
#endif
#endif
