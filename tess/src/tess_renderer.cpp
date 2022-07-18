#include "rive/tess/tess_renderer.hpp"
#include <cassert>

using namespace rive;

TessRenderer::TessRenderer() { m_Stack.emplace_back(RenderState()); }

void TessRenderer::projection(const Mat4& value) { m_Projection = value; }

void TessRenderer::save() { m_Stack.push_back(m_Stack.back()); }
void TessRenderer::restore() {
    assert(m_Stack.size() > 1);
    RenderState& state = m_Stack.back();
    m_Stack.pop_back();

    // We can only add clipping paths so if they're still the same, nothing has
    // changed.
    if (state.clipPaths.size() != m_Stack.back().clipPaths.size()) {
        m_IsClippingDirty = true;
    }
}

bool TessRenderer::needsToApplyClipping() {
    if (!m_IsClippingDirty) {
        return false;
    }

    m_IsClippingDirty = false;
    RenderState& state = m_Stack.back();
    auto currentClipLength = m_ClipPaths.size();
    if (currentClipLength == state.clipPaths.size()) {
        // Same length so now check if they're all the same.
        bool allSame = true;
        for (std::size_t i = 0; i < currentClipLength; i++) {
            if (state.clipPaths[i].path() != m_ClipPaths[i].path()) {
                allSame = false;
                break;
            }
        }
        if (allSame) {
            return false;
        }
    }
    m_ClipPaths = state.clipPaths;

    return true;
}

void TessRenderer::transform(const Mat2D& transform) {
    Mat2D& stackMat = m_Stack.back().transform;
    stackMat = stackMat * transform;
}

void TessRenderer::clipPath(RenderPath* path) {}
void TessRenderer::drawPath(RenderPath* path, RenderPaint* paint) {}
void TessRenderer::drawImage(const RenderImage*, BlendMode, float opacity) {}
void TessRenderer::drawImageMesh(const RenderImage*,
                                 rcp<RenderBuffer> vertices_f32,
                                 rcp<RenderBuffer> uvCoords_f32,
                                 rcp<RenderBuffer> indices_u16,
                                 BlendMode,
                                 float opacity) {}