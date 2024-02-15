/*
 * Copyright 2022 Rive
 */

#include "rive/pls/gl/pls_render_context_gl_impl.hpp"

#include "gl_utils.hpp"
#include "rive/pls/gl/pls_render_target_gl.hpp"
#include "shaders/constants.glsl"

#ifdef RIVE_WEBGL
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

#include "shaders/out/generated/glsl.exports.h"

namespace rive::pls
{
#ifndef GL_WEBGL_shader_pixel_local_storage

// WEBGL_shader_pixel_local_storage bindings aren't in mainline emcsripten yet. Don't implement this
// interface if we don't have bindings.
std::unique_ptr<PLSRenderContextGLImpl::PLSImpl> PLSRenderContextGLImpl::MakePLSImplWebGL()
{
    return nullptr;
}

#else

static GLenum webgl_load_op(pls::LoadAction loadAction)
{
    switch (loadAction)
    {
        case pls::LoadAction::clear:
            return GL_LOAD_OP_CLEAR_WEBGL;
        case pls::LoadAction::preserveRenderTarget:
            return GL_LOAD_OP_LOAD_WEBGL;
        case pls::LoadAction::dontCare:
            return GL_LOAD_OP_ZERO_WEBGL;
    }
    RIVE_UNREACHABLE();
}

class PLSRenderContextGLImpl::PLSImplWebGL : public PLSRenderContextGLImpl::PLSImpl
{
    bool supportsRasterOrdering(const GLCapabilities& capabilities) const override
    {
#ifdef RIVE_WEBGL
        return emscripten_webgl_shader_pixel_local_storage_is_coherent();
#else
        return capabilities.ANGLE_shader_pixel_local_storage_coherent;
#endif
    }

    void activatePixelLocalStorage(PLSRenderContextGLImpl* plsContextImpl,
                                   const FlushDescriptor& desc) override
    {
        auto renderTarget = static_cast<PLSRenderTargetGL*>(desc.renderTarget);
        renderTarget->allocateInternalPLSTextures(desc.interlockMode);

        auto framebufferRenderTarget = lite_rtti_cast<FramebufferRenderTargetGL*>(renderTarget);
        if (framebufferRenderTarget != nullptr)
        {
            // We're targeting an external FBO directly. Make sure to allocate and attach an
            // offscreen target texture.
            framebufferRenderTarget->allocateOffscreenTargetTexture();
            if (desc.colorLoadAction == LoadAction::preserveRenderTarget)
            {
                // Copy the framebuffer's contents to our offscreen texture.
                framebufferRenderTarget->bindExternalFramebuffer(GL_READ_FRAMEBUFFER);
                framebufferRenderTarget->bindInternalFramebuffer(GL_DRAW_FRAMEBUFFER, 1);
                glutils::BlitFramebuffer(desc.renderTargetUpdateBounds, renderTarget->height());
            }
        }

        // Begin pixel local storage.
        renderTarget->bindHeadlessFramebuffer(plsContextImpl->m_capabilities);
        if (desc.colorLoadAction == LoadAction::clear)
        {
            float clearColor4f[4];
            UnpackColorToRGBA32F(desc.clearColor, clearColor4f);
            glFramebufferPixelLocalClearValuefvWEBGL(FRAMEBUFFER_PLANE_IDX, clearColor4f);
        }
        GLenum clipLoadAction = (desc.combinedShaderFeatures & pls::ShaderFeatures::ENABLE_CLIPPING)
                                    ? GL_LOAD_OP_ZERO_WEBGL
                                    : GL_DONT_CARE;
        GLenum loadOps[4] = {webgl_load_op(desc.colorLoadAction),
                             GL_LOAD_OP_ZERO_WEBGL,
                             clipLoadAction,
                             GL_DONT_CARE};
        static_assert(FRAMEBUFFER_PLANE_IDX == 0);
        static_assert(COVERAGE_PLANE_IDX == 1);
        static_assert(CLIP_PLANE_IDX == 2);
        static_assert(ORIGINAL_DST_COLOR_PLANE_IDX == 3);
        glBeginPixelLocalStorageWEBGL(4, loadOps);
    }

    void deactivatePixelLocalStorage(PLSRenderContextGLImpl*, const FlushDescriptor& desc) override
    {
        constexpr static GLenum kStoreOps[4] = {GL_STORE_OP_STORE_WEBGL,
                                                GL_DONT_CARE,
                                                GL_DONT_CARE,
                                                GL_DONT_CARE};
        static_assert(FRAMEBUFFER_PLANE_IDX == 0);
        static_assert(COVERAGE_PLANE_IDX == 1);
        static_assert(CLIP_PLANE_IDX == 2);
        static_assert(ORIGINAL_DST_COLOR_PLANE_IDX == 3);
        glEndPixelLocalStorageWEBGL(4, kStoreOps);

        if (auto framebufferRenderTarget = lite_rtti_cast<FramebufferRenderTargetGL*>(
                static_cast<PLSRenderTargetGL*>(desc.renderTarget)))
        {
            // We rendered to an offscreen texture. Copy back to the external target FBO.
            framebufferRenderTarget->bindInternalFramebuffer(GL_READ_FRAMEBUFFER);
            framebufferRenderTarget->bindExternalFramebuffer(GL_DRAW_FRAMEBUFFER);
            glutils::BlitFramebuffer(desc.renderTargetUpdateBounds,
                                     framebufferRenderTarget->height());
        }
    }

    const char* shaderDefineName() const override { return GLSL_PLS_IMPL_WEBGL; }
};

std::unique_ptr<PLSRenderContextGLImpl::PLSImpl> PLSRenderContextGLImpl::MakePLSImplWebGL()
{
    return std::make_unique<PLSImplWebGL>();
}
#endif
} // namespace rive::pls
