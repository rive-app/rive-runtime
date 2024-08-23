//
// Copyright 2022 Rive
//

#ifndef _RIVE_SOKOL_TESS_RENDERER_HPP_
#define _RIVE_SOKOL_TESS_RENDERER_HPP_

#include "rive/tess/tess_renderer.hpp"
#include "sokol_gfx.h"

namespace rive
{

// The actual graphics device image.
class SokolRenderImageResource : public RefCnt<SokolRenderImageResource>
{
private:
    sg_image m_gpuResource;

public:
    // bytes is expected to be tightly packed RGBA*width*height.
    SokolRenderImageResource(const uint8_t* bytes, uint32_t width, uint32_t height);
    ~SokolRenderImageResource();

    sg_image image() const { return m_gpuResource; }
};

// The unique render image associated with a given source Rive asset. Can be stored in sub-region of
// an actual graphics device image (SokolRenderImageResource).
class SokolRenderImage : public lite_rtti_override<RenderImage, SokolRenderImage>
{
private:
    rcp<SokolRenderImageResource> m_gpuImage;
    sg_buffer m_vertexBuffer;
    sg_buffer m_uvBuffer;

public:
    // Needed by std::unique_ptr
    // SokolRenderImage() {}

    SokolRenderImage(rcp<SokolRenderImageResource> image,
                     uint32_t width,
                     uint32_t height,
                     const Mat2D& uvTransform);

    ~SokolRenderImage() override;

    sg_image image() const { return m_gpuImage->image(); }
    sg_buffer vertexBuffer() const { return m_vertexBuffer; }
    sg_buffer uvBuffer() const { return m_uvBuffer; }
};

class SokolTessRenderer : public TessRenderer
{
private:
    static const std::size_t maxClippingPaths = 16;
    sg_pipeline m_meshPipeline;
    sg_pipeline m_currentPipeline = {0};
    int m_clipCount = 0;

    // Src Over Pipelines
    sg_pipeline m_pathPipeline[maxClippingPaths + 1];

    // Screen Pipelines
    sg_pipeline m_pathScreenPipeline[maxClippingPaths + 1];

    // Additive
    sg_pipeline m_pathAdditivePipeline[maxClippingPaths + 1];

    // Multiply
    sg_pipeline m_pathMultiplyPipeline[maxClippingPaths + 1];

    sg_pipeline m_incClipPipeline;
    sg_pipeline m_decClipPipeline;
    sg_buffer m_boundsIndices;

    std::vector<SubPath> m_ClipPaths;

    void applyClipping();
    void setPipeline(sg_pipeline pipeline);

public:
    SokolTessRenderer();
    ~SokolTessRenderer();
    void orthographicProjection(float left,
                                float right,
                                float bottom,
                                float top,
                                float near,
                                float far) override;
    void drawPath(RenderPath* path, RenderPaint* paint) override;
    void drawImage(const RenderImage*, BlendMode, float opacity) override;
    void drawImageMesh(const RenderImage*,
                       rcp<RenderBuffer> vertices_f32,
                       rcp<RenderBuffer> uvCoords_f32,
                       rcp<RenderBuffer> indices_u16,
                       uint32_t vertexCount,
                       uint32_t indexCount,
                       BlendMode,
                       float opacity) override;
    void restore() override;
    void reset();
};
} // namespace rive
#endif
