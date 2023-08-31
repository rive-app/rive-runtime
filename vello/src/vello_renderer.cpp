#include "vello_renderer.hpp"

VelloPath::~VelloPath() { vello_path_release(m_path); }

void VelloPath::rewind() { vello_path_rewind(m_path); }

void VelloPath::addRenderPath(RenderPath* path, const Mat2D& transform)
{
    vello_path_extend(m_path, static_cast<VelloPath*>(path)->m_path, transform.values());
}

void VelloPath::fillRule(FillRule value) { vello_path_set_fill_rule(m_path, value); }

void VelloPath::moveTo(float x, float y) { vello_path_move_to(m_path, x, y); }

void VelloPath::lineTo(float x, float y) { vello_path_line_to(m_path, x, y); }

void VelloPath::cubicTo(float ox, float oy, float ix, float iy, float x, float y)
{
    vello_path_cubic_to(m_path, ox, oy, ix, iy, x, y);
}

void VelloPath::close() { vello_path_close(m_path); }

VelloGradient::~VelloGradient() { vello_gradient_release(m_gradient); }

VelloImage::~VelloImage() { vello_image_release(m_image); }

VelloPaint::~VelloPaint() { vello_paint_release(m_paint); }

void VelloPaint::style(RenderPaintStyle style) { vello_paint_set_style(m_paint, style); }

void VelloPaint::color(unsigned int value) { vello_paint_set_color(m_paint, value); }

void VelloPaint::thickness(float value) { vello_paint_set_thickness(m_paint, value); }

void VelloPaint::join(StrokeJoin value) { vello_paint_set_join(m_paint, value); }

void VelloPaint::cap(StrokeCap value) { vello_paint_set_cap(m_paint, value); }

void VelloPaint::blendMode(BlendMode value) { vello_paint_set_blend_mode(m_paint, value); }

void VelloPaint::shader(rcp<RenderShader> shader)
{
    VelloGradient* gradient = (VelloGradient*)shader.get();

    if (gradient != nullptr)
    {
        vello_paint_set_gradient(m_paint, gradient->gradient());
    }
}

void VelloRenderer::save() { vello_renderer_save(m_renderer); }

void VelloRenderer::restore() { vello_renderer_restore(m_renderer); }

void VelloRenderer::transform(const Mat2D& transform)
{
    vello_renderer_transform(m_renderer, transform.values());
}

void VelloRenderer::clipPath(RenderPath* path)
{
    vello_renderer_clip_path(m_renderer, static_cast<VelloPath*>(path)->path());
}

void VelloRenderer::drawPath(RenderPath* path, RenderPaint* paint)
{
    vello_renderer_draw_path(m_renderer,
                             static_cast<VelloPath*>(path)->path(),
                             static_cast<VelloPaint*>(paint)->paint());
}

void VelloRenderer::drawImage(const RenderImage* image, BlendMode blend_mode, float opacity)
{
    vello_renderer_draw_image(m_renderer,
                              static_cast<const VelloImage*>(image)->image(),
                              blend_mode,
                              opacity);
}

void VelloRenderer::drawImageMesh(const RenderImage* image,
                                  rcp<RenderBuffer> vertices,
                                  rcp<RenderBuffer> uvCoords,
                                  rcp<RenderBuffer> indices,
                                  uint32_t vertexCount,
                                  uint32_t indexCount,
                                  BlendMode blendMode,
                                  float opacity)
{
    vello_renderer_draw_image_mesh(m_renderer,
                                   static_cast<const VelloImage*>(image)->image(),
                                   DataRenderBuffer::Cast(vertices.get())->vecs(),
                                   vertexCount,
                                   DataRenderBuffer::Cast(uvCoords.get())->vecs(),
                                   vertexCount,
                                   DataRenderBuffer::Cast(indices.get())->u16s(),
                                   indexCount,
                                   blendMode,
                                   opacity);
}

rcp<RenderBuffer> VelloFactory::makeRenderBuffer(RenderBufferType type,
                                                 RenderBufferFlags flags,
                                                 size_t sizeInBytes)
{
    return make_rcp<DataRenderBuffer>(type, flags, sizeInBytes);
}

rcp<RenderShader> VelloFactory::makeLinearGradient(float sx,
                                                   float sy,
                                                   float ex,
                                                   float ey,
                                                   const ColorInt colors[], // [count]
                                                   const float stops[],     // [count]
                                                   size_t count)
{
    RawVelloGradient gradient = vello_gradient_new_linear(sx, sy, ex, ey, colors, stops, count);
    return rcp<RenderShader>(new VelloGradient(std::move(gradient)));
}

rcp<RenderShader> VelloFactory::makeRadialGradient(float cx,
                                                   float cy,
                                                   float radius,
                                                   const ColorInt colors[], // [count]
                                                   const float stops[],     // [count]
                                                   size_t count)
{
    RawVelloGradient gradient = vello_gradient_new_radial(cx, cy, radius, colors, stops, count);
    return rcp<RenderShader>(new VelloGradient(std::move(gradient)));
}

std::unique_ptr<RenderPath> VelloFactory::makeRenderPath(RawPath& rawPath, FillRule fillRule)
{
    std::unique_ptr<VelloPath> path = std::make_unique<VelloPath>();

    for (auto [verb, points] : rawPath)
    {
        switch (verb)
        {
            case PathVerb::move:
                path->move(points[0]);
                break;
            case PathVerb::line:
                path->line(points[1]);
                break;
            case PathVerb::cubic:
                path->cubic(points[1], points[2], points[3]);
                break;
            case PathVerb::close:
                path->close();
                break;
        }
    }

    return path;
}

std::unique_ptr<RenderPath> VelloFactory::makeEmptyRenderPath()
{
    return std::make_unique<VelloPath>();
}

std::unique_ptr<RenderPaint> VelloFactory::makeRenderPaint()
{
    return std::make_unique<VelloPaint>();
}

std::unique_ptr<RenderImage> VelloFactory::decodeImage(Span<const uint8_t> encoded)
{
    return std::make_unique<VelloImage>(vello_image_new(encoded.data(), encoded.size()));
}

static VelloFactory factory;
rive::Factory* ViewerContent::RiveFactory() { return &factory; }
