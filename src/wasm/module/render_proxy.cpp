#ifdef RIVE_WASM_MODULE
// In-module implementations of the abstract render interfaces the bindings
// call through: geometry stays in module memory as RawPath and syncs to the
// host lazily; every native contact is a u32 handle crossing a rive_*_v1
// import. Compiled only into the script module (never the runtime).
#include "rive/factory.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/renderer.hpp"
#include "rive/wasm/module_render.hpp"
#include "rive/wasm/rive_bindings_v1.h"

#include <stdio.h>

using namespace rive;

namespace
{

class ModuleRenderPath : public RenderPath
{
public:
    ModuleRenderPath() = default;
    ModuleRenderPath(RawPath& rawPath, FillRule rule) :
        m_rawPath(rawPath), m_fillRule(rule)
    {}
    ~ModuleRenderPath() override
    {
        if (m_handle != 0)
        {
            rive_path_release(m_handle);
        }
    }

    void rewind() override
    {
        m_rawPath.rewind();
        m_dirty = true;
    }
    void fillRule(FillRule value) override
    {
        m_fillRule = value;
        m_dirty = true;
    }
    void addPath(CommandPath* path, const Mat2D& transform) override
    {
        addRenderPath(path->renderPath(), transform);
    }
    void addRenderPath(const RenderPath* path, const Mat2D& transform) override
    {
        auto modulePath = static_cast<const ModuleRenderPath*>(path);
        m_rawPath.addPath(modulePath->m_rawPath, &transform);
        m_dirty = true;
    }
    void moveTo(float x, float y) override
    {
        m_rawPath.moveTo(x, y);
        m_dirty = true;
    }
    void lineTo(float x, float y) override
    {
        m_rawPath.lineTo(x, y);
        m_dirty = true;
    }
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y)
        override
    {
        m_rawPath.cubicTo(ox, oy, ix, iy, x, y);
        m_dirty = true;
    }
    void close() override
    {
        m_rawPath.close();
        m_dirty = true;
    }
    void addRawPath(const RawPath& path) override
    {
        m_rawPath.addPath(path, nullptr);
        m_dirty = true;
    }

    // Pushes geometry to the host when it changed since the last draw.
    uint32_t sync()
    {
        if (m_handle == 0)
        {
            m_handle = rive_path_new();
            m_dirty = true;
        }
        if (m_dirty)
        {
            rive_path_update(m_handle,
                             (const uint8_t*)m_rawPath.verbs().data(),
                             (uint32_t)m_rawPath.verbs().size(),
                             (const float*)m_rawPath.points().data(),
                             (uint32_t)m_rawPath.points().size() * 2,
                             (uint32_t)m_fillRule);
            m_dirty = false;
        }
        return m_handle;
    }

private:
    RawPath m_rawPath;
    FillRule m_fillRule = FillRule::nonZero;
    uint32_t m_handle = 0;
    bool m_dirty = false;
};

class ModuleRenderPaint : public RenderPaint
{
public:
    ModuleRenderPaint() : m_handle(rive_paint_new()) {}
    ~ModuleRenderPaint() override { rive_paint_release(m_handle); }

    void style(RenderPaintStyle value) override
    {
        rive_paint_style(m_handle, (uint32_t)value);
    }
    void color(ColorInt value) override { rive_paint_color(m_handle, value); }
    void thickness(float value) override
    {
        rive_paint_thickness(m_handle, value);
    }
    void join(StrokeJoin value) override
    {
        rive_paint_join(m_handle, (uint32_t)value);
    }
    void cap(StrokeCap value) override
    {
        rive_paint_cap(m_handle, (uint32_t)value);
    }
    void feather(float value) override { rive_paint_feather(m_handle, value); }
    void blendMode(BlendMode value) override
    {
        rive_paint_blend_mode(m_handle, (uint32_t)value);
    }
    void shader(rcp<RenderShader> value) override;
    void invalidateStroke() override {}

    uint32_t handle() const { return m_handle; }

private:
    rcp<RenderShader> m_shader;
    uint32_t m_handle;
};

class ModuleRenderShader : public RenderShader
{
public:
    ModuleRenderShader(uint32_t handle) : m_handle(handle) {}
    ~ModuleRenderShader() override { rive_shader_release(m_handle); }
    uint32_t handle() const { return m_handle; }

private:
    uint32_t m_handle;
};

// Out of line so ModuleRenderShader's handle accessor is visible.
void ModuleRenderPaint::shader(rcp<RenderShader> value)
{
    // Pinned so the host handle outlives the assignment.
    m_shader = value;
    rive_paint_shader(
        m_handle,
        value != nullptr
            ? static_cast<ModuleRenderShader*>(value.get())->handle()
            : 0);
}

// Contents live in module memory and push to the host on unmap.
class ModuleRenderBuffer : public RenderBuffer
{
public:
    ModuleRenderBuffer(RenderBufferType type,
                       RenderBufferFlags flags,
                       size_t sizeInBytes) :
        RenderBuffer(type, flags, sizeInBytes),
        m_handle(rive_buffer_new((uint32_t)type,
                                 (uint32_t)flags,
                                 (uint32_t)sizeInBytes)),
        m_store(new uint8_t[sizeInBytes])
    {}
    ~ModuleRenderBuffer() override
    {
        rive_buffer_release(m_handle);
        delete[] m_store;
    }
    uint32_t handle() const { return m_handle; }

protected:
    void* onMap() override { return m_store; }
    void onUnmap() override
    {
        rive_buffer_update(m_handle, m_store, (uint32_t)sizeInBytes());
    }

private:
    uint32_t m_handle;
    uint8_t* m_store;
};

class ModuleRenderImage : public RenderImage
{
public:
    ModuleRenderImage(uint32_t handle) : m_handle(handle)
    {
        m_Width = (int)rive_image_width(handle);
        m_Height = (int)rive_image_height(handle);
    }
    ~ModuleRenderImage() override { rive_image_release(m_handle); }
    uint32_t handle() const { return m_handle; }

private:
    uint32_t m_handle;
};

class ModuleFactory : public Factory
{
public:
#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
    ore::Context* ore() override { return wasmModuleOreContext(); }
#endif
    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType type,
                                       RenderBufferFlags flags,
                                       size_t sizeInBytes) override
    {
        return make_rcp<ModuleRenderBuffer>(type, flags, sizeInBytes);
    }
    rcp<RenderShader> makeLinearGradient(float sx,
                                         float sy,
                                         float ex,
                                         float ey,
                                         const ColorInt colors[],
                                         const float stops[],
                                         size_t count) override
    {
        return make_rcp<ModuleRenderShader>(
            rive_shader_linear(sx, sy, ex, ey, colors, stops, (uint32_t)count));
    }
    rcp<RenderShader> makeRadialGradient(float cx,
                                         float cy,
                                         float radius,
                                         const ColorInt colors[],
                                         const float stops[],
                                         size_t count) override
    {
        return make_rcp<ModuleRenderShader>(
            rive_shader_radial(cx, cy, radius, colors, stops, (uint32_t)count));
    }
    rcp<RenderPath> makeRenderPath(RawPath& rawPath, FillRule rule) override
    {
        return make_rcp<ModuleRenderPath>(rawPath, rule);
    }
    rcp<RenderPath> makeEmptyRenderPath() override
    {
        return make_rcp<ModuleRenderPath>();
    }
    rcp<RenderPaint> makeRenderPaint() override
    {
        return make_rcp<ModuleRenderPaint>();
    }
    rcp<RenderImage> decodeImage(Span<const uint8_t>) override
    {
        return nullptr;
    }
};

class ModuleRenderer : public Renderer
{
public:
    ModuleRenderer(uint32_t handle) : m_handle(handle) {}
    void save() override { rive_renderer_save(m_handle); }
    void restore() override { rive_renderer_restore(m_handle); }
    void transform(const Mat2D& transform) override
    {
        rive_renderer_transform(m_handle,
                                transform.xx(),
                                transform.xy(),
                                transform.yx(),
                                transform.yy(),
                                transform.tx(),
                                transform.ty());
    }
    void drawPath(RenderPath* path, RenderPaint* paint) override
    {
        rive_renderer_draw_path(
            m_handle,
            static_cast<ModuleRenderPath*>(path)->sync(),
            static_cast<ModuleRenderPaint*>(paint)->handle());
    }
    void clipPath(RenderPath* path) override
    {
        rive_renderer_clip_path(m_handle,
                                static_cast<ModuleRenderPath*>(path)->sync());
    }
    void drawImage(const RenderImage* image,
                   ImageSampler sampler,
                   BlendMode blend,
                   float opacity) override
    {
        rive_renderer_draw_image(
            m_handle,
            static_cast<const ModuleRenderImage*>(image)->handle(),
            sampler.asKey(),
            (uint32_t)blend,
            opacity);
    }
    void drawImageMesh(const RenderImage* image,
                       ImageSampler sampler,
                       rcp<RenderBuffer> vertices,
                       rcp<RenderBuffer> uvCoords,
                       rcp<RenderBuffer> indices,
                       uint32_t vertexCount,
                       uint32_t indexCount,
                       BlendMode blend,
                       float opacity) override
    {
        rive_renderer_draw_image_mesh(
            m_handle,
            static_cast<const ModuleRenderImage*>(image)->handle(),
            sampler.asKey(),
            static_cast<ModuleRenderBuffer*>(vertices.get())->handle(),
            static_cast<ModuleRenderBuffer*>(uvCoords.get())->handle(),
            static_cast<ModuleRenderBuffer*>(indices.get())->handle(),
            (uint32_t)blend,
            opacity);
    }
    void modulateOpacity(float) override {}
    uint32_t handle() const { return m_handle; }

private:
    uint32_t m_handle;
};

} // namespace

namespace rive
{
uint32_t wasmModuleImageHandle(RenderImage* image)
{
    return image != nullptr ? static_cast<ModuleRenderImage*>(image)->handle()
                            : 0;
}

Factory* wasmModuleFactory()
{
    static ModuleFactory factory;
    return &factory;
}

std::unique_ptr<Renderer> makeWasmModuleRenderer(uint32_t handle)
{
    return std::make_unique<ModuleRenderer>(handle);
}

uint32_t wasmModuleRendererHandle(Renderer* renderer)
{
    return renderer != nullptr
               ? static_cast<ModuleRenderer*>(renderer)->handle()
               : 0;
}

rcp<RenderImage> makeWasmModuleRenderImage(uint32_t handle)
{
    return make_rcp<ModuleRenderImage>(handle);
}
} // namespace rive

#endif
