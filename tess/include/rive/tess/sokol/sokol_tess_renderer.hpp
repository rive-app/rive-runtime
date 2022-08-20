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
    sg_image m_Image;

public:
    SokolRenderImage(sg_image image);
    sg_image image() const { return m_Image; }
};

class SokolTessRenderer : public TessRenderer {
private:
    sg_pipeline m_MeshPipeline;

public:
    SokolTessRenderer();
    void orthographicProjection(float left,
                                float right,
                                float bottom,
                                float top,
                                float near,
                                float far) override;

    void drawImage(const RenderImage*, BlendMode, float opacity) override;
    void drawImageMesh(const RenderImage*,
                       rcp<RenderBuffer> vertices_f32,
                       rcp<RenderBuffer> uvCoords_f32,
                       rcp<RenderBuffer> indices_u16,
                       BlendMode,
                       float opacity) override;
};
} // namespace rive
#endif
