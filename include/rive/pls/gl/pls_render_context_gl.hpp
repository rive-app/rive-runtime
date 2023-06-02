/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/math/mat2d.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/pls/gl/gles3.hpp"
#include "rive/pls/gl/pls_render_target_gl.hpp"
#include "rive/pls/buffer_ring.hpp"
#include "rive/pls/pls_render_context.hpp"
#include <map>

namespace rive::pls
{
class PLSPath;
class PLSPaint;
class PLSRenderTargetGL;

// OpenGL backend implementation of PLSRenderContext.
class PLSRenderContextGL : public PLSRenderContext
{
public:
    static std::unique_ptr<PLSRenderContextGL> Make();
    ~PLSRenderContextGL() override;

    // Creates a PLSRenderTarget that draws directly into the given GL framebuffer.
    // Returns null if the framebuffer doesn't support pixel local storage.
    rcp<PLSRenderTargetGL> wrapGLRenderTarget(GLuint framebufferID, size_t width, size_t height);

    // Creates a PLSRenderTarget that draws to a new, offscreen GL framebuffer. This method is
    // guaranteed to succeed, but the caller must call glBlitFramebuffer() to view the rendering
    // results.
    rcp<PLSRenderTargetGL> makeOffscreenRenderTarget(size_t width, size_t height);

protected:
    // We only use one uniform buffer per shader, so for now, we all bind them to block 0.
    constexpr static int kUniformBlockIdx = 0;

    // Wraps a compiled and linked GL program of draw.glsl, with a specific set of features enabled
    // via #define. The set of features to enable is dictated by ShaderFeatures.
    class DrawProgram
    {
    public:
        DrawProgram(const DrawProgram&) = delete;
        DrawProgram& operator=(const DrawProgram&) = delete;
        DrawProgram(PLSRenderContextGL* context, const ShaderFeatures& shaderFeatures);
        ~DrawProgram();

        void bind() const;

    private:
        GLuint m_id;
    };

    class DrawShader;

    PLSRenderContextGL(const PlatformFeatures&);

    const PLSRenderTargetGL* renderTarget() const
    {
        return static_cast<const PLSRenderTargetGL*>(frameDescriptor().renderTarget.get());
    }

    std::unique_ptr<UniformBufferRing> makeUniformBufferRing(size_t sizeInBytes) override;

    std::unique_ptr<TexelBufferRing> makeTexelBufferRing(TexelBufferRing::Format,
                                                         size_t widthInItems,
                                                         size_t height,
                                                         size_t texelsPerItem,
                                                         int textureIdx,
                                                         TexelBufferRing::Filter) override;

    std::unique_ptr<BufferRingImpl> makeVertexBufferRing(size_t capacity,
                                                         size_t itemSizeInBytes) override;

    void allocateTessellationTexture(size_t height) override;

    void onFlush(FlushType,
                 LoadAction,
                 size_t gradSpanCount,
                 size_t gradSpansHeight,
                 size_t tessVertexSpanCount,
                 size_t tessDataHeight,
                 size_t wedgeInstanceCount,
                 const ShaderFeatures&) override;

    void activatePixelLocalStorage(LoadAction, const ShaderFeatures&, const DrawProgram&);
    void deactivatePixelLocalStorage(const ShaderFeatures&);

    // Gradient texture rendering.
    GLuint m_colorRampProgram;
    GLuint m_colorRampVAO;
    GLuint m_colorRampFBO;

    // Tessellation texture rendering.
    GLuint m_tessellateProgram;
    GLuint m_tessellateVAO;
    GLuint m_tessellateFBO;
    GLuint m_tessVertexTexture = 0;

    // Not all programs have a unique vertex shader, so we cache and reuse them where possible.
    std::map<uint64_t, DrawShader> m_vertexShaders;
    std::map<uint64_t, DrawProgram> m_drawPrograms;
    GLuint m_drawVAO;
    GLuint m_pathWedgeVertexBuffer;
    GLuint m_pathWedgeIndexBuffer;
};
} // namespace rive::pls
