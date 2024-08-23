#include "rive/tess/tess_renderer.hpp"
#include <cassert>

using namespace rive;

TessRenderer::TessRenderer() { m_Stack.emplace_back(RenderState()); }

void TessRenderer::projection(const Mat4& value) { m_Projection = value; }

void TessRenderer::save() { m_Stack.push_back(m_Stack.back()); }
void TessRenderer::restore()
{
    assert(m_Stack.size() > 1);
    RenderState& state = m_Stack.back();
    m_Stack.pop_back();

    // We can only add clipping paths so if they're still the same, nothing has
    // changed.
    if (state.clipPaths.size() != m_Stack.back().clipPaths.size())
    {
        m_IsClippingDirty = true;
    }
}

void TessRenderer::transform(const Mat2D& transform)
{
    Mat2D& stackMat = m_Stack.back().transform;
    stackMat = stackMat * transform;
}

void TessRenderer::clipPath(RenderPath* path)
{

    RenderState& state = m_Stack.back();
    state.clipPaths.emplace_back(SubPath(path, state.transform));
    m_IsClippingDirty = true;
}

void TessRenderer::drawImage(const RenderImage*, BlendMode, float opacity) {}
void TessRenderer::drawImageMesh(const RenderImage*,
                                 rcp<RenderBuffer> vertices_f32,
                                 rcp<RenderBuffer> uvCoords_f32,
                                 rcp<RenderBuffer> indices_u16,
                                 uint32_t vertexCount,
                                 uint32_t indexCount,
                                 BlendMode,
                                 float opacity)
{}
