#ifndef _RIVE_VELLO_RENDERER_HPP_
#define _RIVE_VELLO_RENDERER_HPP_

#include "rive/factory.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/math/path_types.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/renderer.hpp"
#include "utils/factory_utils.hpp"
#include "viewer/viewer_content.hpp"

#include "vello_renderer.hpp"

using namespace rive;

typedef void* RawVelloPath;
typedef void* RawVelloGradient;
typedef void* RawVelloImage;
typedef void* RawVelloPaint;
typedef void* RawVelloRenderer;

extern "C"
{
    typedef void* RawVelloPath;
    typedef void* RawVelloGradient;
    typedef void* RawVelloImage;
    typedef void* RawVelloPaint;
    typedef void* RawVelloRenderer;

    const RawVelloPath vello_path_new();
    void vello_path_release(const RawVelloPath path);
    void vello_path_rewind(const RawVelloPath path);
    void vello_path_extend(const RawVelloPath path, void* other, const float* transform);
    void vello_path_set_fill_rule(const RawVelloPath path, FillRule fill_rule);
    void vello_path_move_to(const RawVelloPath path, float x, float y);
    void vello_path_line_to(const RawVelloPath path, float x, float y);
    void vello_path_cubic_to(const RawVelloPath path,
                             float ox,
                             float oy,
                             float ix,
                             float iy,
                             float x,
                             float y);
    void vello_path_close(const RawVelloPath path);

    const RawVelloGradient vello_gradient_new_linear(float sx,
                                                     float sy,
                                                     float ex,
                                                     float ey,
                                                     const uint32_t* colors_data,
                                                     const float* stops_data,
                                                     size_t len);
    const RawVelloGradient vello_gradient_new_radial(float cx,
                                                     float cy,
                                                     float radius,
                                                     const uint32_t* colors_data,
                                                     const float* stops_data,
                                                     size_t len);
    void vello_gradient_release(const RawVelloGradient gradient);

    const RawVelloImage vello_image_new(const uint8_t* data, size_t len);
    void vello_image_release(const RawVelloImage image);

    const RawVelloPaint vello_paint_new();
    void vello_paint_release(const RawVelloPaint paint);
    void vello_paint_set_style(const RawVelloPaint paint, RenderPaintStyle style);
    void vello_paint_set_color(const RawVelloPaint paint, uint32_t color);
    void vello_paint_set_thickness(const RawVelloPaint paint, float thickness);
    void vello_paint_set_join(const RawVelloPaint paint, StrokeJoin join);
    void vello_paint_set_cap(const RawVelloPaint paint, StrokeCap cap);
    void vello_paint_set_blend_mode(const RawVelloPaint paint, BlendMode blend_mode);
    void vello_paint_set_gradient(const RawVelloPaint paint, const RawVelloGradient gradient);

    const RawVelloRenderer vello_renderer_new();
    void vello_renderer_release(const RawVelloRenderer renderer);
    void vello_renderer_save(const RawVelloRenderer renderer);
    void vello_renderer_restore(const RawVelloRenderer renderer);
    void vello_renderer_transform(const RawVelloRenderer renderer, const float* transform);
    void vello_renderer_clip_path(const RawVelloRenderer renderer, const RawVelloPath path);
    void vello_renderer_draw_path(const RawVelloRenderer renderer,
                                  const RawVelloPath path,
                                  const RawVelloPaint paint);
    void vello_renderer_draw_image(const RawVelloRenderer renderer,
                                   const RawVelloImage image,
                                   BlendMode blend_mode,
                                   float opacity);
    void vello_renderer_draw_image_mesh(const RawVelloRenderer renderer,
                                        const RawVelloImage image,
                                        const Vec2D* vertices_data,
                                        size_t vertices_len,
                                        const Vec2D* uvs_data,
                                        size_t uvs_len,
                                        const uint16_t* indices_data,
                                        size_t indices_len,
                                        BlendMode blend_mode,
                                        float opacity);
}

using namespace rive;

class VelloPath : public RenderPath
{
private:
    RawVelloPath m_path;

public:
    VelloPath() : m_path(vello_path_new()) {}
    VelloPath(RawVelloPath& path) : m_path(std::move(path)) {}
    ~VelloPath() override;

    const RawVelloPath& path() const { return m_path; }

    void rewind() override;
    void addRenderPath(RenderPath* path, const Mat2D& transform) override;
    void fillRule(FillRule value) override;
    void moveTo(float x, float y) override;
    void lineTo(float x, float y) override;
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y) override;
    virtual void close() override;
};

class VelloGradient : public RenderShader
{
private:
    RawVelloGradient m_gradient;

public:
    VelloGradient(RawVelloGradient gradient) : m_gradient(gradient) {}
    ~VelloGradient() override;

    const RawVelloGradient& gradient() const { return m_gradient; }
};

class VelloImage : public RenderImage
{
private:
    RawVelloImage m_image;

public:
    VelloImage(RawVelloImage image) : m_image(image) {}
    ~VelloImage() override;

    const RawVelloImage& image() const { return m_image; }
};

class VelloPaint : public RenderPaint
{
private:
    RawVelloPaint m_paint;

public:
    VelloPaint() : m_paint(vello_paint_new()) {}
    ~VelloPaint() override;

    const RawVelloPaint& paint() const { return m_paint; }

    void style(RenderPaintStyle style) override;
    void color(unsigned int value) override;
    void thickness(float value) override;
    void join(StrokeJoin value) override;
    void cap(StrokeCap value) override;
    void blendMode(BlendMode value) override;
    void shader(rcp<RenderShader>) override;
    void invalidateStroke() override {}
};

class VelloRenderer : public Renderer
{
protected:
    RawVelloRenderer m_renderer;

public:
    VelloRenderer() : m_renderer(vello_renderer_new()) {}
    VelloRenderer(RawVelloRenderer renderer) : m_renderer(std::move(renderer)) {}
    ~VelloRenderer() override {}

    void save() override;
    void restore() override;
    void transform(const Mat2D& transform) override;
    void clipPath(RenderPath* path) override;
    void drawPath(RenderPath* path, RenderPaint* paint) override;
    void drawImage(const RenderImage*, BlendMode, float opacity) override;
    void drawImageMesh(const RenderImage*,
                       rcp<RenderBuffer> vertices_f32,
                       rcp<RenderBuffer> uvCoords_f32,
                       rcp<RenderBuffer> indices_u16,
                       BlendMode,
                       float opacity) override;
};

class VelloFactory : public Factory
{
public:
    rcp<RenderBuffer> makeBufferU16(Span<const uint16_t>) override;
    rcp<RenderBuffer> makeBufferU32(Span<const uint32_t>) override;
    rcp<RenderBuffer> makeBufferF32(Span<const float>) override;

    rcp<RenderShader> makeLinearGradient(float sx,
                                         float sy,
                                         float ex,
                                         float ey,
                                         const ColorInt colors[], // [count]
                                         const float stops[],     // [count]
                                         size_t count) override;

    rcp<RenderShader> makeRadialGradient(float cx,
                                         float cy,
                                         float radius,
                                         const ColorInt colors[], // [count]
                                         const float stops[],     // [count]
                                         size_t count) override;

    std::unique_ptr<RenderPath> makeRenderPath(RawPath&, FillRule) override;

    std::unique_ptr<RenderPath> makeEmptyRenderPath() override;

    std::unique_ptr<RenderPaint> makeRenderPaint() override;

    std::unique_ptr<RenderImage> decodeImage(Span<const uint8_t>) override;
};

#endif
