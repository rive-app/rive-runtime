#include "rive/tess/sokol/sokol_tess_renderer.hpp"
#include "rive/tess/sokol/sokol_factory.hpp"
#include "generated/shader.h"

using namespace rive;

SokolRenderImage::SokolRenderImage(sg_image image) : m_Image(image) {}

class SokolBuffer : public RenderBuffer {
private:
    sg_buffer m_Buffer;

public:
    SokolBuffer(size_t count, const sg_buffer_desc& desc) :
        RenderBuffer(count), m_Buffer(sg_make_buffer(desc)) {}
    ~SokolBuffer() { sg_destroy_buffer(m_Buffer); }

    sg_buffer buffer() { return m_Buffer; }
};

rcp<RenderBuffer> SokolFactory::makeBufferU16(Span<const uint16_t> span) {
    return rcp<RenderBuffer>(new SokolBuffer(span.size(),
                                             (sg_buffer_desc){
                                                 .type = SG_BUFFERTYPE_INDEXBUFFER,
                                                 .data =
                                                     {
                                                         span.data(),
                                                         span.size_bytes(),
                                                     },
                                             }));
}

rcp<RenderBuffer> SokolFactory::makeBufferU32(Span<const uint32_t> span) {
    return rcp<RenderBuffer>(new SokolBuffer(span.size(),
                                             (sg_buffer_desc){
                                                 .type = SG_BUFFERTYPE_INDEXBUFFER,
                                                 .data =
                                                     {
                                                         span.data(),
                                                         span.size_bytes(),
                                                     },
                                             }));
}
rcp<RenderBuffer> SokolFactory::makeBufferF32(Span<const float> span) {
    return rcp<RenderBuffer>(new SokolBuffer(span.size(),
                                             (sg_buffer_desc){
                                                 .type = SG_BUFFERTYPE_VERTEXBUFFER,
                                                 .data =
                                                     {
                                                         span.data(),
                                                         span.size_bytes(),
                                                     },
                                             }));
}

SokolTessRenderer::SokolTessRenderer() {
    m_MeshPipeline = sg_make_pipeline((sg_pipeline_desc){
        .layout =
            {
                .attrs =
                    {
                        [ATTR_vs_pos] = {.format = SG_VERTEXFORMAT_FLOAT2, .buffer_index = 0},
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
}

void SokolTessRenderer::orthographicProjection(float left,
                                               float right,
                                               float bottom,
                                               float top,
                                               float near,
                                               float far) {
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

void SokolTessRenderer::drawImage(const RenderImage*, BlendMode, float opacity) {}
void SokolTessRenderer::drawImageMesh(const RenderImage* renderImage,
                                      rcp<RenderBuffer> vertices_f32,
                                      rcp<RenderBuffer> uvCoords_f32,
                                      rcp<RenderBuffer> indices_u16,
                                      BlendMode blendMode,
                                      float opacity) {
    vs_params_t vs_params;

    const Mat2D& world = transform();
    vs_params.mvp = m_Projection * world;

    sg_apply_pipeline(m_MeshPipeline);
    sg_bindings bind = {
        .vertex_buffers[0] = static_cast<SokolBuffer*>(vertices_f32.get())->buffer(),
        .vertex_buffers[1] = static_cast<SokolBuffer*>(uvCoords_f32.get())->buffer(),
        .index_buffer = static_cast<SokolBuffer*>(indices_u16.get())->buffer(),
        .fs_images[SLOT_tex] = static_cast<const SokolRenderImage*>(renderImage)->image(),
    };

    sg_apply_bindings(&bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, SG_RANGE_REF(vs_params));
    // printf("INDICES: %zu\n", indices_u16->count());
    sg_draw(0, indices_u16->count(), 1);
}