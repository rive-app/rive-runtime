#include "rive/tess/sokol/sokol_tess_renderer.hpp"
#include "rive/tess/sokol/sokol_factory.hpp"
#include "rive/tess/tess_render_path.hpp"
#include "rive/tess/contour_stroke.hpp"
#include "generated/shader.h"
#include <unordered_set>

using namespace rive;

static void fillColorBuffer(float* buffer, ColorInt value)
{
    buffer[0] = (float)colorRed(value) / 0xFF;
    buffer[1] = (float)colorGreen(value) / 0xFF;
    buffer[2] = (float)colorBlue(value) / 0xFF;
    buffer[3] = colorOpacity(value);
}

class SokolRenderPath : public lite_rtti_override<TessRenderPath, SokolRenderPath>
{
public:
    SokolRenderPath() {}
    SokolRenderPath(RawPath& rawPath, FillRule fillRule) : lite_rtti_override(rawPath, fillRule) {}

    ~SokolRenderPath()
    {
        sg_destroy_buffer(m_vertexBuffer);
        sg_destroy_buffer(m_indexBuffer);
    }

private:
    std::vector<Vec2D> m_vertices;
    std::vector<uint16_t> m_indices;

    sg_buffer m_vertexBuffer = {0};
    sg_buffer m_indexBuffer = {0};

    std::size_t m_boundsIndex = 0;

protected:
    void addTriangles(rive::Span<const rive::Vec2D> vts, rive::Span<const uint16_t> idx) override
    {
        m_vertices.insert(m_vertices.end(), vts.begin(), vts.end());
        m_indices.insert(m_indices.end(), idx.begin(), idx.end());
    }

    void setTriangulatedBounds(const AABB& value) override
    {
        m_boundsIndex = m_vertices.size();
        m_vertices.emplace_back(Vec2D(value.minX, value.minY));
        m_vertices.emplace_back(Vec2D(value.maxX, value.minY));
        m_vertices.emplace_back(Vec2D(value.maxX, value.maxY));
        m_vertices.emplace_back(Vec2D(value.minX, value.maxY));
    }

public:
    void rewind() override
    {
        TessRenderPath::rewind();
        m_vertices.clear();
        m_indices.clear();
    }

    void drawStroke(ContourStroke* stroke)
    {
        if (isContainer())
        {
            for (auto& subPath : m_subPaths)
            {
                LITE_RTTI_CAST_OR_CONTINUE(sokolPath, SokolRenderPath*, subPath.path());
                sokolPath->drawStroke(stroke);
            }
            return;
        }
        std::size_t start, end;
        stroke->nextRenderOffset(start, end);
        sg_draw(start < 2 ? 0 : (start - 2) * 3, end - start < 2 ? 0 : (end - start - 2) * 3, 1);
    }

    void drawFill()
    {
        if (triangulate())
        {
            sg_destroy_buffer(m_vertexBuffer);
            sg_destroy_buffer(m_indexBuffer);
            if (m_indices.size() == 0 || m_vertices.size() == 0)
            {
                m_vertexBuffer = {0};
                m_indexBuffer = {0};
                return;
            }

            m_vertexBuffer = sg_make_buffer((sg_buffer_desc){
                .type = SG_BUFFERTYPE_VERTEXBUFFER,
                .data =
                    {
                        m_vertices.data(),
                        m_vertices.size() * sizeof(Vec2D),
                    },
            });

            m_indexBuffer = sg_make_buffer((sg_buffer_desc){
                .type = SG_BUFFERTYPE_INDEXBUFFER,
                .data =
                    {
                        m_indices.data(),
                        m_indices.size() * sizeof(uint16_t),
                    },
            });
        }

        if (m_vertexBuffer.id == 0)
        {
            return;
        }

        sg_bindings bind = {
            .vertex_buffers[0] = m_vertexBuffer,
            .index_buffer = m_indexBuffer,
        };

        sg_apply_bindings(&bind);
        sg_draw(0, m_indices.size(), 1);
    }

    void drawBounds(const sg_buffer& boundsIndexBuffer)
    {
        if (m_vertexBuffer.id == 0)
        {
            return;
        }
        sg_bindings bind = {
            .vertex_buffers[0] = m_vertexBuffer,
            .vertex_buffer_offsets[0] = (int)(m_boundsIndex * sizeof(Vec2D)),
            .index_buffer = boundsIndexBuffer,
        };

        sg_apply_bindings(&bind);
        sg_draw(0, 6, 1);
    }
};

// Returns a full-formed RenderPath -- can be treated as immutable
rcp<RenderPath> SokolFactory::makeRenderPath(RawPath& rawPath, FillRule rule)
{
    return make_rcp<SokolRenderPath>(rawPath, rule);
}

rcp<RenderPath> SokolFactory::makeEmptyRenderPath() { return make_rcp<SokolRenderPath>(); }

class SokolBuffer : public lite_rtti_override<RenderBuffer, SokolBuffer>
{
public:
    SokolBuffer(RenderBufferType type, RenderBufferFlags renderBufferFlags, size_t sizeInBytes) :
        lite_rtti_override(type, renderBufferFlags, sizeInBytes),
        m_mappedMemory(new char[sizeInBytes])
    {
        // If the buffer will be immutable, defer creation until the client unmaps for the only time
        // and we have our initial data.
        if (!(flags() & RenderBufferFlags::mappedOnceAtInitialization))
        {
            m_buffer = sg_make_buffer(makeNoDataBufferDesc());
        }
    }
    ~SokolBuffer() { sg_destroy_buffer(m_buffer); }

    sg_buffer buffer()
    {
        // In the case of RenderBufferFlags::mappedOnceAtInitialization, the client is expected to
        // map()/unmap() before we need to access this buffer for rendering.
        assert(m_buffer.id);
        return m_buffer;
    }

protected:
    void* onMap() override
    {
        // An immutable buffer is only mapped once, and then we delete m_mappedMemory.
        assert(m_mappedMemory);
        return m_mappedMemory.get();
    }

    void onUnmap() override
    {
        if (flags() & RenderBufferFlags::mappedOnceAtInitialization)
        {
            // We are an immutable buffer. Creation was deferred until now.
            assert(!m_buffer.id);
            assert(m_mappedMemory);
            sg_buffer_desc bufferDesc = makeNoDataBufferDesc();
            bufferDesc.data = {m_mappedMemory.get(), sizeInBytes()};
            m_buffer = sg_make_buffer(bufferDesc);
            m_mappedMemory.reset(); // This buffer will never be mapped again.
        }
        else
        {
            assert(m_buffer.id);
            sg_update_buffer(m_buffer,
                             sg_range{.ptr = m_mappedMemory.get(), .size = sizeInBytes()});
        }
    }

private:
    sg_buffer_desc makeNoDataBufferDesc() const
    {
        return {
            .size = sizeInBytes(),
            .usage = (flags() & RenderBufferFlags::mappedOnceAtInitialization) ? SG_USAGE_IMMUTABLE
                                                                               : SG_USAGE_STREAM,
            .type = (type() == RenderBufferType::index) ? SG_BUFFERTYPE_INDEXBUFFER
                                                        : SG_BUFFERTYPE_VERTEXBUFFER,
        };
    };

    sg_buffer m_buffer{};
    std::unique_ptr<char[]> m_mappedMemory;
};

rcp<RenderBuffer> SokolFactory::makeRenderBuffer(RenderBufferType type,
                                                 RenderBufferFlags flags,
                                                 size_t sizeInBytes)
{
    return make_rcp<SokolBuffer>(type, flags, sizeInBytes);
}

sg_pipeline vectorPipeline(sg_shader shader,
                           sg_blend_state blend,
                           sg_stencil_state stencil,
                           sg_color_mask colorMask = SG_COLORMASK_RGBA)
{
    return sg_make_pipeline((sg_pipeline_desc){
        .layout =
            {
                .attrs =
                    {
                        [ATTR_vs_path_position] =
                            {
                                .format = SG_VERTEXFORMAT_FLOAT2,
                                .buffer_index = 0,
                            },
                    },
            },
        .shader = shader,
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_NONE,
        .depth =
            {
                .compare = SG_COMPAREFUNC_ALWAYS,
                .write_enabled = false,
            },
        .colors =
            {
                [0] =
                    {
                        .write_mask = colorMask,
                        .blend = blend,
                    },
            },
        .stencil = stencil,
        .label = "path-pipeline",
    });
}

SokolTessRenderer::SokolTessRenderer()
{
    m_meshPipeline = sg_make_pipeline((sg_pipeline_desc){
        .layout =
            {
                .attrs =
                    {
                        [ATTR_vs_position] = {.format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 0},
                        [ATTR_vs_texcoord0] = {.format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 1},
                    },
            },
        .shader = sg_make_shader(rive_tess_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_NONE,
        .depth =
            {
                .compare = SG_COMPAREFUNC_ALWAYS,
                .write_enabled = false,
            },
        .colors =
            {
                [0] =
                    {
                        .write_mask = SG_COLORMASK_RGBA,
                        .blend =
                            {
                                .enabled = true,
                                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                            },
                    },
            },
        .label = "mesh-pipeline",
    });

    auto uberShader = sg_make_shader(rive_tess_path_shader_desc(sg_query_backend()));

    assert(maxClippingPaths < 256);

    // Src Over Pipelines
    {
        m_pathPipeline[0] = vectorPipeline(uberShader,
                                           {
                                               .enabled = true,
                                               .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                                               .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                                           },
                                           {
                                               .enabled = false,
                                           });

        for (std::size_t i = 1; i <= maxClippingPaths; i++)
        {
            m_pathPipeline[i] =
                vectorPipeline(uberShader,
                               {
                                   .enabled = true,
                                   .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                                   .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                               },
                               {
                                   .enabled = true,
                                   .ref = (uint8_t)i,
                                   .read_mask = 0xFF,
                                   .write_mask = 0x00,
                                   .front =
                                       {
                                           .compare = SG_COMPAREFUNC_EQUAL,
                                       },
                                   .back =
                                       {
                                           .compare = SG_COMPAREFUNC_EQUAL,
                                       },
                               });
        }
    }

    // Screen Pipelines
    {
        m_pathScreenPipeline[0] =
            vectorPipeline(uberShader,
                           {
                               .enabled = true,
                               .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                               .dst_factor_rgb = SG_BLENDFACTOR_ONE,
                               .src_factor_alpha = SG_BLENDFACTOR_SRC_ALPHA,
                               .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                           },
                           {
                               .enabled = false,
                           });

        for (std::size_t i = 1; i <= maxClippingPaths; i++)
        {
            m_pathScreenPipeline[i] =
                vectorPipeline(uberShader,
                               {
                                   .enabled = true,
                                   .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                                   .dst_factor_rgb = SG_BLENDFACTOR_ONE,
                                   .src_factor_alpha = SG_BLENDFACTOR_SRC_ALPHA,
                                   .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                               },
                               {
                                   .enabled = true,
                                   .ref = (uint8_t)i,
                                   .read_mask = 0xFF,
                                   .write_mask = 0x00,
                                   .front =
                                       {
                                           .compare = SG_COMPAREFUNC_EQUAL,
                                       },
                                   .back =
                                       {
                                           .compare = SG_COMPAREFUNC_EQUAL,
                                       },
                               });
        }
    }

    // Additive Pipelines
    {
        m_pathAdditivePipeline[0] = vectorPipeline(uberShader,
                                                   {
                                                       .enabled = true,
                                                       .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                                                       .dst_factor_rgb = SG_BLENDFACTOR_ONE,
                                                   },
                                                   {
                                                       .enabled = false,
                                                   });

        for (std::size_t i = 1; i <= maxClippingPaths; i++)
        {
            m_pathAdditivePipeline[i] =
                vectorPipeline(uberShader,
                               {
                                   .enabled = true,
                                   .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                                   .dst_factor_rgb = SG_BLENDFACTOR_ONE,
                               },
                               {
                                   .enabled = true,
                                   .ref = (uint8_t)i,
                                   .read_mask = 0xFF,
                                   .write_mask = 0x00,
                                   .front =
                                       {
                                           .compare = SG_COMPAREFUNC_EQUAL,
                                       },
                                   .back =
                                       {
                                           .compare = SG_COMPAREFUNC_EQUAL,
                                       },
                               });
        }
    }

    // Multiply Pipelines
    {
        m_pathMultiplyPipeline[0] =
            vectorPipeline(uberShader,
                           {
                               .enabled = true,
                               .src_factor_rgb = SG_BLENDFACTOR_DST_COLOR,
                               .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                           },
                           {
                               .enabled = false,
                           });

        for (std::size_t i = 1; i <= maxClippingPaths; i++)
        {
            m_pathMultiplyPipeline[i] =
                vectorPipeline(uberShader,
                               {
                                   .enabled = true,
                                   .src_factor_rgb = SG_BLENDFACTOR_DST_COLOR,
                                   .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                               },
                               {
                                   .enabled = true,
                                   .ref = (uint8_t)i,
                                   .read_mask = 0xFF,
                                   .write_mask = 0x00,
                                   .front =
                                       {
                                           .compare = SG_COMPAREFUNC_EQUAL,
                                       },
                                   .back =
                                       {
                                           .compare = SG_COMPAREFUNC_EQUAL,
                                       },
                               });
        }
    }

    m_incClipPipeline = vectorPipeline(uberShader,
                                       {
                                           .enabled = false,
                                       },
                                       {
                                           .enabled = true,
                                           .read_mask = 0xFF,
                                           .write_mask = 0xFF,
                                           .front =
                                               {
                                                   .compare = SG_COMPAREFUNC_ALWAYS,
                                                   .depth_fail_op = SG_STENCILOP_KEEP,
                                                   .fail_op = SG_STENCILOP_KEEP,
                                                   .pass_op = SG_STENCILOP_INCR_CLAMP,
                                               },
                                           .back =
                                               {
                                                   .compare = SG_COMPAREFUNC_ALWAYS,
                                                   .depth_fail_op = SG_STENCILOP_KEEP,
                                                   .fail_op = SG_STENCILOP_KEEP,
                                                   .pass_op = SG_STENCILOP_INCR_CLAMP,
                                               },
                                       },
                                       SG_COLORMASK_NONE);

    m_decClipPipeline = vectorPipeline(uberShader,
                                       {
                                           .enabled = false,
                                       },
                                       {
                                           .enabled = true,
                                           .read_mask = 0xFF,
                                           .write_mask = 0xFF,
                                           .front =
                                               {
                                                   .compare = SG_COMPAREFUNC_ALWAYS,
                                                   .depth_fail_op = SG_STENCILOP_KEEP,
                                                   .fail_op = SG_STENCILOP_KEEP,
                                                   .pass_op = SG_STENCILOP_DECR_CLAMP,
                                               },
                                           .back =
                                               {
                                                   .compare = SG_COMPAREFUNC_ALWAYS,
                                                   .depth_fail_op = SG_STENCILOP_KEEP,
                                                   .fail_op = SG_STENCILOP_KEEP,
                                                   .pass_op = SG_STENCILOP_DECR_CLAMP,
                                               },
                                       },
                                       SG_COLORMASK_NONE);

    uint16_t indices[] = {0, 1, 2, 0, 2, 3};

    m_boundsIndices = sg_make_buffer((sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = SG_RANGE(indices),
    });
}

SokolTessRenderer::~SokolTessRenderer()
{
    sg_destroy_buffer(m_boundsIndices);
    sg_destroy_pipeline(m_meshPipeline);
    sg_destroy_pipeline(m_incClipPipeline);
    sg_destroy_pipeline(m_decClipPipeline);
    for (std::size_t i = 0; i <= maxClippingPaths; i++)
    {
        sg_destroy_pipeline(m_pathPipeline[i]);
        sg_destroy_pipeline(m_pathScreenPipeline[i]);
    }
}

void SokolTessRenderer::orthographicProjection(float left,
                                               float right,
                                               float bottom,
                                               float top,
                                               float near,
                                               float far)
{
    m_Projection[0] = 2.0f / (right - left);
    m_Projection[1] = 0.0f;
    m_Projection[2] = 0.0f;
    m_Projection[3] = 0.0f;

    m_Projection[4] = 0.0f;
    m_Projection[5] = 2.0f / (top - bottom);
    m_Projection[6] = 0.0f;
    m_Projection[7] = 0.0f;

#ifdef SOKOL_GLCORE33
    m_Projection[8] = 0.0f;
    m_Projection[9] = 0.0f;
    m_Projection[10] = 2.0f / (near - far);
    m_Projection[11] = 0.0f;

    m_Projection[12] = (right + left) / (left - right);
    m_Projection[13] = (top + bottom) / (bottom - top);
    m_Projection[14] = (far + near) / (near - far);
    m_Projection[15] = 1.0f;
#else
    // NDC are slightly different in Metal:
    // https://metashapes.com/blog/opengl-metal-projection-matrix-problem/
    m_Projection[8] = 0.0f;
    m_Projection[9] = 0.0f;
    m_Projection[10] = 1.0f / (far - near);
    m_Projection[11] = 0.0f;

    m_Projection[12] = (right + left) / (left - right);
    m_Projection[13] = (top + bottom) / (bottom - top);
    m_Projection[14] = near / (near - far);
    m_Projection[15] = 1.0f;
#endif
    // for (int i = 0; i < 4; i++) {
    //     int b = i * 4;
    //     printf("%f\t%f\t%f\t%f\n",
    //            m_Projection[b],
    //            m_Projection[b + 1],
    //            m_Projection[b + 2],
    //            m_Projection[b + 3]);
    // }
}

void SokolTessRenderer::drawImage(const RenderImage* image, BlendMode, float opacity)
{
    LITE_RTTI_CAST_OR_RETURN(sokolImage, const SokolRenderImage*, image);

    vs_params_t vs_params;

    const Mat2D& world = transform();
    vs_params.mvp = m_Projection * world;

    setPipeline(m_meshPipeline);
    sg_bindings bind = {
        .vertex_buffers[0] = sokolImage->vertexBuffer(),
        .vertex_buffers[1] = sokolImage->uvBuffer(),
        .index_buffer = m_boundsIndices,
        .fs_images[SLOT_tex] = sokolImage->image(),
    };

    sg_apply_bindings(&bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, SG_RANGE_REF(vs_params));
    sg_draw(0, 6, 1);
}

void SokolTessRenderer::drawImageMesh(const RenderImage* renderImage,
                                      rcp<RenderBuffer> vertices_f32,
                                      rcp<RenderBuffer> uvCoords_f32,
                                      rcp<RenderBuffer> indices_u16,
                                      uint32_t vertexCount,
                                      uint32_t indexCount,
                                      BlendMode blendMode,
                                      float opacity)
{
    LITE_RTTI_CAST_OR_RETURN(sokolVertices, SokolBuffer*, vertices_f32.get());
    LITE_RTTI_CAST_OR_RETURN(sokolUVCoords, SokolBuffer*, uvCoords_f32.get());
    LITE_RTTI_CAST_OR_RETURN(sokolIndices, SokolBuffer*, indices_u16.get());
    LITE_RTTI_CAST_OR_RETURN(sokolRenderImage, const SokolRenderImage*, renderImage);

    vs_params_t vs_params;

    const Mat2D& world = transform();
    vs_params.mvp = m_Projection * world;

    setPipeline(m_meshPipeline);
    sg_bindings bind = {
        .vertex_buffers[0] = sokolVertices->buffer(),
        .vertex_buffers[1] = sokolUVCoords->buffer(),
        .index_buffer = sokolIndices->buffer(),
        .fs_images[SLOT_tex] = sokolRenderImage->image(),
    };

    sg_apply_bindings(&bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, SG_RANGE_REF(vs_params));
    sg_draw(0, indexCount, 1);
}

class SokolGradient : public lite_rtti_override<RenderShader, SokolGradient>
{
private:
    Vec2D m_start;
    Vec2D m_end;
    int m_type;
    std::vector<float> m_colors;
    std::vector<float> m_stops;
    bool m_isVisible = false;

private:
    // General gradient
    SokolGradient(int type, const ColorInt colors[], const float stops[], size_t count) :
        m_type(type)
    {
        m_stops.resize(count);
        m_colors.resize(count * 4);
        for (int i = 0, colorIndex = 0; i < count; i++, colorIndex += 4)
        {
            fillColorBuffer(&m_colors[colorIndex], colors[i]);
            m_stops[i] = stops[i];
            if (m_colors[colorIndex + 3] > 0.0f)
            {
                m_isVisible = true;
            }
        }
    }

public:
    // Linear gradient
    SokolGradient(float sx,
                  float sy,
                  float ex,
                  float ey,
                  const ColorInt colors[],
                  const float stops[],
                  size_t count) :
        SokolGradient(1, colors, stops, count)
    {
        m_start = Vec2D(sx, sy);
        m_end = Vec2D(ex, ey);
    }

    SokolGradient(float cx,
                  float cy,
                  float radius,
                  const ColorInt colors[], // [count]
                  const float stops[],     // [count]
                  size_t count) :
        SokolGradient(2, colors, stops, count)
    {
        m_start = Vec2D(cx, cy);
        m_end = Vec2D(cx + radius, cy);
    }

    void bind(vs_path_params_t& vertexUniforms, fs_path_uniforms_t& fragmentUniforms)
    {
        auto stopCount = m_stops.size();
        vertexUniforms.fillType = fragmentUniforms.fillType = m_type;
        vertexUniforms.gradientStart = m_start;
        vertexUniforms.gradientEnd = m_end;
        fragmentUniforms.stopCount = stopCount;
        for (int i = 0; i < stopCount; i++)
        {
            auto colorBufferIndex = i * 4;
            for (int j = 0; j < 4; j++)
            {
                fragmentUniforms.colors[i][j] = m_colors[colorBufferIndex + j];
            }
            fragmentUniforms.stops[i / 4][i % 4] = m_stops[i];
        }
    }
};

rcp<RenderShader> SokolFactory::makeLinearGradient(float sx,
                                                   float sy,
                                                   float ex,
                                                   float ey,
                                                   const ColorInt colors[],
                                                   const float stops[],
                                                   size_t count)
{
    return rcp<RenderShader>(new SokolGradient(sx, sy, ex, ey, colors, stops, count));
}

rcp<RenderShader> SokolFactory::makeRadialGradient(float cx,
                                                   float cy,
                                                   float radius,
                                                   const ColorInt colors[], // [count]
                                                   const float stops[],     // [count]
                                                   size_t count)
{
    return rcp<RenderShader>(new SokolGradient(cx, cy, radius, colors, stops, count));
}

class SokolRenderPaint : public lite_rtti_override<RenderPaint, SokolRenderPaint>
{
private:
    fs_path_uniforms_t m_uniforms = {0};
    rcp<SokolGradient> m_shader;
    RenderPaintStyle m_style;
    std::unique_ptr<ContourStroke> m_stroke;
    bool m_strokeDirty = false;
    float m_strokeThickness = 0.0f;
    StrokeJoin m_strokeJoin;
    StrokeCap m_strokeCap;

    sg_buffer m_strokeVertexBuffer = {0};
    sg_buffer m_strokeIndexBuffer = {0};
    std::vector<std::size_t> m_StrokeOffsets;

    BlendMode m_blendMode = BlendMode::srcOver;

public:
    ~SokolRenderPaint() override
    {
        sg_destroy_buffer(m_strokeVertexBuffer);
        sg_destroy_buffer(m_strokeIndexBuffer);
    }

    void color(ColorInt value) override
    {
        fillColorBuffer(m_uniforms.colors[0], value);
        m_uniforms.fillType = 0;
    }

    void style(RenderPaintStyle value) override
    {
        m_style = value;

        switch (m_style)
        {
            case RenderPaintStyle::fill:
                m_stroke = nullptr;
                m_strokeDirty = false;
                break;
            case RenderPaintStyle::stroke:
                m_stroke = rivestd::make_unique<ContourStroke>();
                m_strokeDirty = true;
                break;
        }
    }

    RenderPaintStyle style() const { return m_style; }

    void thickness(float value) override
    {
        m_strokeThickness = value;
        m_strokeDirty = true;
    }

    void join(StrokeJoin value) override
    {
        m_strokeJoin = value;
        m_strokeDirty = true;
    }

    void cap(StrokeCap value) override
    {
        m_strokeCap = value;
        m_strokeDirty = true;
    }

    void invalidateStroke() override
    {
        if (m_stroke)
        {
            m_strokeDirty = true;
        }
    }

    void blendMode(BlendMode value) override { m_blendMode = value; }
    BlendMode blendMode() const { return m_blendMode; }

    void shader(rcp<RenderShader> shader) override
    {
        m_shader = lite_rtti_rcp_cast<SokolGradient>(std::move(shader));
    }

    void draw(vs_path_params_t& vertexUniforms, SokolRenderPath* path)
    {
        if (m_shader)
        {
            m_shader->bind(vertexUniforms, m_uniforms);
        }

        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_path_params, SG_RANGE_REF(vertexUniforms));
        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_fs_path_uniforms, SG_RANGE_REF(m_uniforms));
        if (m_stroke != nullptr)
        {
            if (m_strokeDirty)
            {
                static Mat2D identity;
                m_stroke->reset();
                path->extrudeStroke(m_stroke.get(),
                                    m_strokeJoin,
                                    m_strokeCap,
                                    m_strokeThickness / 2.0f,
                                    identity);
                m_strokeDirty = false;

                const std::vector<Vec2D>& strip = m_stroke->triangleStrip();

                sg_destroy_buffer(m_strokeVertexBuffer);
                sg_destroy_buffer(m_strokeIndexBuffer);
                auto size = strip.size();
                if (size <= 2)
                {
                    m_strokeVertexBuffer = {0};
                    m_strokeIndexBuffer = {0};
                    return;
                }

                m_strokeVertexBuffer = sg_make_buffer((sg_buffer_desc){
                    .type = SG_BUFFERTYPE_VERTEXBUFFER,
                    .data =
                        {
                            strip.data(),
                            strip.size() * sizeof(Vec2D),
                        },
                });

                // Let's use a tris index buffer so we can keep the same sokol pipeline.
                std::vector<uint16_t> indices;

                // Build them by stroke offsets (where each offset represents a sub-path, or a move
                // to)
                m_stroke->resetRenderOffset();
                m_StrokeOffsets.clear();
                while (true)
                {
                    std::size_t strokeStart, strokeEnd;
                    if (!m_stroke->nextRenderOffset(strokeStart, strokeEnd))
                    {
                        break;
                    }
                    std::size_t length = strokeEnd - strokeStart;
                    if (length > 2)
                    {
                        for (std::size_t i = 0, end = length - 2; i < end; i++)
                        {
                            if ((i % 2) == 1)
                            {
                                indices.push_back(i + strokeStart);
                                indices.push_back(i + 1 + strokeStart);
                                indices.push_back(i + 2 + strokeStart);
                            }
                            else
                            {
                                indices.push_back(i + strokeStart);
                                indices.push_back(i + 2 + strokeStart);
                                indices.push_back(i + 1 + strokeStart);
                            }
                        }
                        m_StrokeOffsets.push_back(indices.size());
                    }
                }

                m_strokeIndexBuffer = sg_make_buffer((sg_buffer_desc){
                    .type = SG_BUFFERTYPE_INDEXBUFFER,
                    .data =
                        {
                            indices.data(),
                            indices.size() * sizeof(uint16_t),
                        },
                });
            }
            if (m_strokeVertexBuffer.id == 0)
            {
                return;
            }

            sg_bindings bind = {
                .vertex_buffers[0] = m_strokeVertexBuffer,
                .index_buffer = m_strokeIndexBuffer,
            };

            sg_apply_bindings(&bind);

            m_stroke->resetRenderOffset();
            // path->drawStroke(m_stroke.get());
            std::size_t start = 0;
            for (auto end : m_StrokeOffsets)
            {
                sg_draw(start, end - start, 1);
                start = end;
            }
        }
        else
        {
            path->drawFill();
        }
    }
};

rcp<RenderPaint> SokolFactory::makeRenderPaint() { return make_rcp<SokolRenderPaint>(); }

void SokolTessRenderer::restore()
{
    TessRenderer::restore();
    if (m_Stack.size() == 1)
    {
        // When we've fully restored, immediately update clip to not wait for next draw.
        applyClipping();
        m_currentPipeline = {0};
    }
}

void SokolTessRenderer::applyClipping()
{
    if (!m_IsClippingDirty)
    {
        return;
    }
    m_IsClippingDirty = false;
    RenderState& state = m_Stack.back();

    auto currentClipLength = m_ClipPaths.size();
    if (currentClipLength == state.clipPaths.size())
    {
        // Same length so now check if they're all the same.
        bool allSame = true;
        for (std::size_t i = 0; i < currentClipLength; i++)
        {
            if (state.clipPaths[i].path() != m_ClipPaths[i].path())
            {
                allSame = false;
                break;
            }
        }
        if (allSame)
        {
            return;
        }
    }

    vs_path_params_t vs_params = {.fillType = 0};
    fs_path_uniforms_t uniforms = {0};

    // Decr any paths from the last clip that are gone.
    std::unordered_set<RenderPath*> alreadyApplied;

    for (auto appliedPath : m_ClipPaths)
    {
        bool decr = true;
        for (auto nextClipPath : state.clipPaths)
        {
            if (nextClipPath.path() == appliedPath.path())
            {
                decr = false;
                alreadyApplied.insert(appliedPath.path());
                break;
            }
        }
        if (decr)
        {
            // Draw appliedPath.path() with decr pipeline
            LITE_RTTI_CAST_OR_CONTINUE(sokolPath, SokolRenderPath*, appliedPath.path());
            setPipeline(m_decClipPipeline);
            vs_params.mvp = m_Projection * appliedPath.transform();
            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_path_params, SG_RANGE_REF(vs_params));
            sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_fs_path_uniforms, SG_RANGE_REF(uniforms));
            sokolPath->drawFill();
        }
    }

    // Incr any paths that are added.
    for (auto nextClipPath : state.clipPaths)
    {
        if (alreadyApplied.count(nextClipPath.path()))
        {
            // Already applied.
            continue;
        }
        // Draw nextClipPath.path() with incr pipeline
        LITE_RTTI_CAST_OR_CONTINUE(sokolPath, SokolRenderPath*, nextClipPath.path());
        setPipeline(m_incClipPipeline);
        vs_params.mvp = m_Projection * nextClipPath.transform();
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_path_params, SG_RANGE_REF(vs_params));
        sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_fs_path_uniforms, SG_RANGE_REF(uniforms));
        sokolPath->drawFill();
    }

    // Pick which pipeline to use for draw path operations.
    // TODO: something similar for draw mesh.
    m_clipCount = state.clipPaths.size();

    m_ClipPaths = state.clipPaths;
}

void SokolTessRenderer::reset() { m_currentPipeline = {0}; }
void SokolTessRenderer::setPipeline(sg_pipeline pipeline)
{
    if (m_currentPipeline.id == pipeline.id)
    {
        return;
    }
    m_currentPipeline = pipeline;
    sg_apply_pipeline(pipeline);
}

void SokolTessRenderer::drawPath(RenderPath* path, RenderPaint* paint)
{
    LITE_RTTI_CAST_OR_RETURN(sokolPath, SokolRenderPath*, path);
    LITE_RTTI_CAST_OR_RETURN(sokolPaint, SokolRenderPaint*, paint);

    applyClipping();
    vs_path_params_t vs_params = {.fillType = 0};
    const Mat2D& world = transform();

    vs_params.mvp = m_Projection * world;
    switch (sokolPaint->blendMode())
    {
        case BlendMode::srcOver:
            setPipeline(m_pathPipeline[m_clipCount]);
            break;
        case BlendMode::screen:
            setPipeline(m_pathScreenPipeline[m_clipCount]);
            break;
        case BlendMode::colorDodge:
            setPipeline(m_pathAdditivePipeline[m_clipCount]);
            break;
        case BlendMode::multiply:
            setPipeline(m_pathMultiplyPipeline[m_clipCount]);
            break;
        default:
            setPipeline(m_pathScreenPipeline[m_clipCount]);
            break;
    }

    sokolPaint->draw(vs_params, sokolPath);
}

SokolRenderImageResource::SokolRenderImageResource(const uint8_t* bytes,
                                                   uint32_t width,
                                                   uint32_t height) :
    m_gpuResource(sg_make_image((sg_image_desc){
        .width = (int)width,
        .height = (int)height,
        .data.subimage[0][0] = {bytes, width * height * 4},
        .pixel_format = SG_PIXELFORMAT_RGBA8,
    }))
{}
SokolRenderImageResource::~SokolRenderImageResource() { sg_destroy_image(m_gpuResource); }

SokolRenderImage::SokolRenderImage(rcp<SokolRenderImageResource> image,
                                   uint32_t width,
                                   uint32_t height,
                                   const Mat2D& uvTransform) :

    lite_rtti_override(uvTransform), m_gpuImage(image)

{
    float halfWidth = width / 2.0f;
    float halfHeight = height / 2.0f;
    Vec2D points[] = {
        Vec2D(-halfWidth, -halfHeight),
        Vec2D(halfWidth, -halfHeight),
        Vec2D(halfWidth, halfHeight),
        Vec2D(-halfWidth, halfHeight),
    };
    m_vertexBuffer = sg_make_buffer((sg_buffer_desc){
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
        .data = SG_RANGE(points),
    });

    Vec2D uv[] = {
        uvTransform * Vec2D(0.0f, 0.0f),
        uvTransform * Vec2D(1.0f, 0.0f),
        uvTransform * Vec2D(1.0f, 1.0f),
        uvTransform * Vec2D(0.0f, 1.0f),
    };

    m_uvBuffer = sg_make_buffer((sg_buffer_desc){
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
        .data = SG_RANGE(uv),
    });
}

SokolRenderImage::~SokolRenderImage()
{
    sg_destroy_buffer(m_vertexBuffer);
    sg_destroy_buffer(m_uvBuffer);
}
