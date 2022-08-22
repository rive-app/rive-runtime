//
// Copyright 2022 Rive
//

#ifndef _RIVE_SOKOL_TESS_RENDERER_HPP_
#define _RIVE_SOKOL_TESS_RENDERER_HPP_

#include "rive/tess/tess_renderer.hpp"
#include "sokol_gfx.h"

namespace rive {

class SokolRenderImage : public RenderImage {
private:
    sg_image m_image;

public:
    SokolRenderImage(sg_image image);
    sg_image image() const { return m_image; }
};

class SokolTessRenderer : public TessRenderer {
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
                       BlendMode,
                       float opacity) override;
    void restore() override;
    void reset();
};
} // namespace rive
#endif
