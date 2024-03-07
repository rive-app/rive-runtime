/*
 * Copyright 2022 Rive
 */

#include "rive/pls/gl/pls_render_context_gl_impl.hpp"

#include "gl_utils.hpp"
#include "rive/pls/gl/pls_render_target_gl.hpp"
#include "shaders/constants.glsl"

#include "shaders/out/generated/glsl.exports.h"

#ifdef RIVE_WEBGL
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

EM_JS(bool,
      webgl_shader_pixel_local_storage_is_coherent_js,
      (EMSCRIPTEN_WEBGL_CONTEXT_HANDLE contextHandle),
      {
          const gl = GL.getContext(contextHandle).GLctx;
          const ext = gl.getExtension("WEBGL_shader_pixel_local_storage");
          return ext && ext.isCoherent();
      });

EM_JS(void,
      framebufferTexturePixelLocalStorageWEBGL,
      (EMSCRIPTEN_WEBGL_CONTEXT_HANDLE contextHandle,
       GLint plane,
       GLuint backingtexture,
       GLint level,
       GLint layer),
      {
          const gl = GL.getContext(contextHandle).GLctx;
          const ext = Module["ctx"].getExtension("WEBGL_shader_pixel_local_storage");
          if (ext)
          {
              ext.framebufferTexturePixelLocalStorageWEBGL(plane,
                                                           GL.textures[backingtexture],
                                                           level,
                                                           layer);
          }
      });

EM_JS(void,
      framebufferPixelLocalClearValuefvWEBGL,
      (EMSCRIPTEN_WEBGL_CONTEXT_HANDLE contextHandle,
       GLint plane,
       float r,
       float g,
       float b,
       float a),
      {
          const gl = GL.getContext(contextHandle).GLctx;
          const ext = Module["ctx"].getExtension("WEBGL_shader_pixel_local_storage");
          if (ext)
          {
              ext.framebufferPixelLocalClearValuefvWEBGL(plane, [ r, g, b, a ]);
          }
      });

EM_JS(void,
      beginPixelLocalStorageWEBGL,
      (EMSCRIPTEN_WEBGL_CONTEXT_HANDLE contextHandle, uint32_t n, uint32_t loadopsIdx),
      {
          const gl = GL.getContext(contextHandle).GLctx;
          const ext = Module["ctx"].getExtension("WEBGL_shader_pixel_local_storage");
          if (ext)
          {
              ext.beginPixelLocalStorageWEBGL(Module.HEAPU32.subarray(loadopsIdx, loadopsIdx + n));
          }
      });

EM_JS(void,
      endPixelLocalStorageWEBGL,
      (EMSCRIPTEN_WEBGL_CONTEXT_HANDLE contextHandle, uint32_t n, uint32_t storeopsIdx),
      {
          const gl = GL.getContext(contextHandle).GLctx;
          const ext = Module["ctx"].getExtension("WEBGL_shader_pixel_local_storage");
          if (ext)
          {
              ext.endPixelLocalStorageWEBGL(Module.HEAPU32.subarray(storeopsIdx, storeopsIdx + n));
          }
      });

EM_JS(void,
      provokingVertexWEBGL,
      (EMSCRIPTEN_WEBGL_CONTEXT_HANDLE contextHandle, GLenum provokeMode),
      {
          const gl = GL.getContext(contextHandle).GLctx;
          const ext = Module["ctx"].getExtension("WEBGL_provoking_vertex");
          if (ext)
          {
              ext.provokingVertexWEBGL(provokeMode);
          }
      });

bool webgl_shader_pixel_local_storage_is_coherent()
{
    return webgl_shader_pixel_local_storage_is_coherent_js(emscripten_webgl_get_current_context());
}

void glFramebufferTexturePixelLocalStorageANGLE(GLint plane,
                                                GLuint backingtexture,
                                                GLint level,
                                                GLint layer)
{
    framebufferTexturePixelLocalStorageWEBGL(emscripten_webgl_get_current_context(),
                                             plane,
                                             backingtexture,
                                             level,
                                             layer);
}

void glFramebufferPixelLocalClearValuefvANGLE(GLint plane, const GLfloat value[4])
{
    framebufferPixelLocalClearValuefvWEBGL(emscripten_webgl_get_current_context(),
                                           plane,
                                           value[0],
                                           value[1],
                                           value[2],
                                           value[3]);
}

void glBeginPixelLocalStorageANGLE(GLsizei n, const uint32_t loadops[])
{
    beginPixelLocalStorageWEBGL(emscripten_webgl_get_current_context(),
                                n,
                                reinterpret_cast<uintptr_t>(loadops) / sizeof(uint32_t));
}

void glEndPixelLocalStorageANGLE(GLsizei n, const uint32_t storeops[])
{
    endPixelLocalStorageWEBGL(emscripten_webgl_get_current_context(),
                              n,
                              reinterpret_cast<uintptr_t>(storeops) / sizeof(uint32_t));
}

void glProvokingVertexANGLE(GLenum provokeMode)
{
    provokingVertexWEBGL(emscripten_webgl_get_current_context(), provokeMode);
}
#endif // RIVE_WEBGL

namespace rive::pls
{
static GLenum webgl_load_op(pls::LoadAction loadAction)
{
    switch (loadAction)
    {
        case pls::LoadAction::clear:
            return GL_LOAD_OP_CLEAR_ANGLE;
        case pls::LoadAction::preserveRenderTarget:
            return GL_LOAD_OP_LOAD_ANGLE;
        case pls::LoadAction::dontCare:
            return GL_LOAD_OP_ZERO_ANGLE;
    }
    RIVE_UNREACHABLE();
}

class PLSRenderContextGLImpl::PLSImplWebGL : public PLSRenderContextGLImpl::PLSImpl
{
    bool supportsRasterOrdering(const GLCapabilities& capabilities) const override
    {
        return capabilities.ANGLE_shader_pixel_local_storage_coherent;
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
                framebufferRenderTarget->bindDestinationFramebuffer(GL_READ_FRAMEBUFFER);
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
            glFramebufferPixelLocalClearValuefvANGLE(FRAMEBUFFER_PLANE_IDX, clearColor4f);
        }
        GLenum clipLoadAction = (desc.combinedShaderFeatures & pls::ShaderFeatures::ENABLE_CLIPPING)
                                    ? GL_LOAD_OP_ZERO_ANGLE
                                    : GL_DONT_CARE;
        GLenum loadOps[4] = {webgl_load_op(desc.colorLoadAction),
                             GL_LOAD_OP_ZERO_ANGLE,
                             clipLoadAction,
                             GL_DONT_CARE};
        static_assert(FRAMEBUFFER_PLANE_IDX == 0);
        static_assert(COVERAGE_PLANE_IDX == 1);
        static_assert(CLIP_PLANE_IDX == 2);
        static_assert(ORIGINAL_DST_COLOR_PLANE_IDX == 3);
        glBeginPixelLocalStorageANGLE(4, loadOps);
    }

    void deactivatePixelLocalStorage(PLSRenderContextGLImpl*, const FlushDescriptor& desc) override
    {
        constexpr static GLenum kStoreOps[4] = {GL_STORE_OP_STORE_ANGLE,
                                                GL_DONT_CARE,
                                                GL_DONT_CARE,
                                                GL_DONT_CARE};
        static_assert(FRAMEBUFFER_PLANE_IDX == 0);
        static_assert(COVERAGE_PLANE_IDX == 1);
        static_assert(CLIP_PLANE_IDX == 2);
        static_assert(ORIGINAL_DST_COLOR_PLANE_IDX == 3);
        glEndPixelLocalStorageANGLE(4, kStoreOps);

        if (auto framebufferRenderTarget = lite_rtti_cast<FramebufferRenderTargetGL*>(
                static_cast<PLSRenderTargetGL*>(desc.renderTarget)))
        {
            // We rendered to an offscreen texture. Copy back to the external target FBO.
            framebufferRenderTarget->bindInternalFramebuffer(GL_READ_FRAMEBUFFER, 1);
            framebufferRenderTarget->bindDestinationFramebuffer(GL_DRAW_FRAMEBUFFER);
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
} // namespace rive::pls
