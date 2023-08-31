//
// Copyright 2022 Rive
//

#ifndef _RIVE_TESS_RENDERER_HPP_
#define _RIVE_TESS_RENDERER_HPP_

#include "rive/renderer.hpp"
#include "rive/tess/sub_path.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/math/mat4.hpp"
#include <vector>
#include <list>

namespace rive
{

struct RenderState
{
    Mat2D transform;
    std::vector<SubPath> clipPaths;
};

class TessRenderer : public Renderer
{
protected:
    Mat4 m_Projection;
    std::list<RenderState> m_Stack;
    bool m_IsClippingDirty = false;

public:
    TessRenderer();

    void projection(const Mat4& value);
    virtual void orthographicProjection(float left,
                                        float right,
                                        float bottom,
                                        float top,
                                        float near,
                                        float far) = 0;

    void save() override;
    void restore() override;
    void transform(const Mat2D& transform) override;
    const Mat2D& transform() { return m_Stack.back().transform; }
    void clipPath(RenderPath* path) override;
    void drawImage(const RenderImage*, BlendMode, float opacity) override;
    void drawImageMesh(const RenderImage*,
                       rcp<RenderBuffer> vertices_f32,
                       rcp<RenderBuffer> uvCoords_f32,
                       rcp<RenderBuffer> indices_u16,
                       uint32_t vertexCount,
                       uint32_t indexCount,
                       BlendMode,
                       float opacity) override;
};
} // namespace rive
#endif
