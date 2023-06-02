/*
 * Copyright 2022 Rive
 */

#include "rive/pls/gl/pls_render_context_gl.hpp"

#include "gl/buffer_ring_gl.hpp"

#include <stdio.h>

#ifdef RIVE_WASM
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

#ifdef RIVE_WASM
EM_JS(void, set_provoking_vertex_if_supported, (GLenum convention), {
    const ext = Module["ctx"].getExtension("WEBGL_provoking_vertex");
    if (ext)
    {
        ext.provokingVertexWEBGL(convention);
    }
});
#endif

#ifdef RIVE_ANGLE
static void set_provoking_vertex_if_supported(GLenum convention)
{
    if (GLAD_GL_ANGLE_provoking_vertex)
    {
        glProvokingVertexANGLE(convention);
    }
}
#endif

namespace rive::pls
{
std::unique_ptr<PLSRenderContextGL> PLSRenderContextGL::Make()
{
#ifdef RIVE_WASM
    if (!emscripten_webgl_enable_WEBGL_shader_pixel_local_storage(
            emscripten_webgl_get_current_context()))
    {
        printf("WEBGL_shader_pixel_local_storage not supported.\n");
        exit(-1);
    }

    if (!emscripten_webgl_shader_pixel_local_storage_is_coherent())
    {
        printf("WEBGL_shader_pixel_local_storage is not coherent\n");
        return nullptr;
    }
#endif

#ifdef RIVE_ANGLE
    if (!GLAD_GL_ANGLE_shader_pixel_local_storage ||
        !GLAD_GL_ANGLE_shader_pixel_local_storage_coherent)
    {
        printf("ANGLE_shader_pixel_local_storage_coherent not supported.\n");
        exit(-1);
        return nullptr;
    }
#endif

    // D3D and Metal both have a provoking vertex convention of "first" for flat varyings, and it's
    // very costly for ANGLE to implement the OpenGL convention of "last" on these backends. To
    // workaround this, ANGLE provides the ANGLE_provoking_vertex extension. When this extension is
    // present, we can just set the provoking vertex to "first" and trust that it will be fast.
    set_provoking_vertex_if_supported(GL_FIRST_VERTEX_CONVENTION_WEBGL);

    PlatformFeatures platformFeatures;
    GLenum rendererToken = GL_RENDERER;
#ifdef RIVE_WASM
    if (emscripten_webgl_enable_extension(emscripten_webgl_get_current_context(),
                                          "WEBGL_debug_renderer_info"))
    {
        rendererToken = GL_UNMASKED_RENDERER_WEBGL;
    }
#endif
    const char* rendererString = reinterpret_cast<const char*>(glGetString(rendererToken));
    if (strstr(rendererString, "Apple") && strstr(rendererString, "Metal"))
    {
        // In Metal, non-flat varyings preserve their exact value if all vertices in the triangle
        // emit the same value, and we also see a small (5-10%) improvement from not using flat
        // varyings.
        platformFeatures.avoidFlatVaryings = true;
    }

    return std::unique_ptr<PLSRenderContextGL>(new PLSRenderContextGL(platformFeatures));
}

rcp<PLSRenderTargetGL> PLSRenderContextGL::wrapGLRenderTarget(GLuint framebufferID,
                                                              size_t width,
                                                              size_t height)
{
    // WEBGL_shader_pixel_local_storage can't load or store to framebuffers.
    return nullptr;
}

rcp<PLSRenderTargetGL> PLSRenderContextGL::makeOffscreenRenderTarget(size_t width, size_t height)
{
    return rcp<PLSRenderTargetGL>(
        new PLSRenderTargetGL(width,
                              height,
                              PLSRenderTargetGL::CoverageBackingType::texture,
                              m_platformFeatures));
}

void PLSRenderContextGL::activatePixelLocalStorage(LoadAction loadAction,
                                                   const ShaderFeatures& shaderFeatures,
                                                   const DrawProgram& drawProgram)
{
    // Bind these before activating pixel local storage, so they don't count against our GL call
    // count in Chrome while pixel local storage is active, reducing our chances of the render
    // pass being interrupted.
    glBindVertexArray(m_drawVAO);
    static_cast<const UniformBufferGL*>(drawUniformBufferRing())
        ->bindToUniformBlock(kUniformBlockIdx);
    drawProgram.bind();

    if (loadAction == LoadAction::clear)
    {
        float clearColor4f[4];
        UnpackColorToRGBA32F(frameDescriptor().clearColor, clearColor4f);
        glFramebufferPixelLocalClearValuefvWEBGL(kFramebufferPlaneIdx, clearColor4f);
    }

    GLenum loadOps[4] = {
        (GLenum)(loadAction == LoadAction::clear ? GL_LOAD_OP_CLEAR_WEBGL : GL_LOAD_OP_LOAD_WEBGL),
        GL_LOAD_OP_ZERO_WEBGL,
        GL_DONT_CARE,
        GL_LOAD_OP_ZERO_WEBGL};

    glBeginPixelLocalStorageWEBGL(shaderFeatures.programFeatures.enablePathClipping ? 4 : 3,
                                  loadOps);
}

void PLSRenderContextGL::deactivatePixelLocalStorage(const ShaderFeatures& shaderFeatures)
{
    constexpr static GLenum kStoreOps[4] = {GL_STORE_OP_STORE_WEBGL,
                                            GL_DONT_CARE,
                                            GL_DONT_CARE,
                                            GL_DONT_CARE};
    glEndPixelLocalStorageWEBGL(shaderFeatures.programFeatures.enablePathClipping ? 4 : 3,
                                kStoreOps);
}
} // namespace rive::pls
