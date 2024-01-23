/*
 * Copyright 2023 Rive
 */

#include "rive/pls/gl/pls_render_context_gl_impl.hpp"

#include "rive/pls/gl/load_store_actions_ext.hpp"
#include "gl/gl_utils.hpp"
#include "rive/math/simd.hpp"
#include "rive/pls/gl/pls_render_target_gl.hpp"
#include <sstream>

#include "../out/obj/generated/pls_load_store_ext.exports.h"

namespace rive::pls
{
// Wraps an EXT_shader_pixel_local_storage load/store program, described by a set of
// LoadStoreActions.
class PLSLoadStoreProgram
{
public:
    PLSLoadStoreProgram(const PLSLoadStoreProgram&) = delete;
    PLSLoadStoreProgram& operator=(const PLSLoadStoreProgram&) = delete;

    PLSLoadStoreProgram(LoadStoreActionsEXT actions,
                        GLuint vertexShader,
                        const GLCapabilities& extensions,
                        rcp<GLState> state) :
        m_state(std::move(state))
    {
        m_id = glCreateProgram();
        glAttachShader(m_id, vertexShader);

        std::ostringstream glsl;
        glsl << "#version 300 es\n";
        glsl << "#define " GLSL_FRAGMENT "\n";
        BuildLoadStoreEXTGLSL(glsl, actions);
        GLuint fragmentShader = glutils::CompileRawGLSL(GL_FRAGMENT_SHADER, glsl.str().c_str());
        glAttachShader(m_id, fragmentShader);
        glDeleteShader(fragmentShader);

        glutils::LinkProgram(m_id);
        if (actions & LoadStoreActionsEXT::clearColor)
        {
            m_colorClearUniLocation = glGetUniformLocation(m_id, GLSL_clearColor);
        }
    }

    ~PLSLoadStoreProgram() { m_state->deleteProgram(m_id); }

    GLuint id() const { return m_id; }
    GLint clearColorUniLocation() const { return m_colorClearUniLocation; }

private:
    GLuint m_id;
    GLint m_colorClearUniLocation = -1;
    const rcp<GLState> m_state;
};

class PLSRenderContextGLImpl::PLSImplEXTNative : public PLSRenderContextGLImpl::PLSImpl
{
public:
    PLSImplEXTNative(const GLCapabilities& extensions) : m_capabilities(extensions) {}

    ~PLSImplEXTNative()
    {
        glDeleteShader(m_plsLoadStoreVertexShader);
        m_state->deleteVAO(m_plsLoadStoreVAO);
    }

    void init(rcp<GLState> state) override
    {
        // The load/store actions are all done in the fragment shader, so there is one single vertex
        // shader, and we use LoadStoreActionsEXT::none when compiling it.
        std::ostringstream glsl;
        glsl << "#version 300 es\n";
        glsl << "#define " GLSL_VERTEX "\n";
        BuildLoadStoreEXTGLSL(glsl, LoadStoreActionsEXT::none);
        m_plsLoadStoreVertexShader = glutils::CompileRawGLSL(GL_VERTEX_SHADER, glsl.str().c_str());
        glGenVertexArrays(1, &m_plsLoadStoreVAO);
        m_state = std::move(state);
    }

    void activatePixelLocalStorage(PLSRenderContextGLImpl* impl,
                                   const FlushDescriptor& desc) override
    {
        assert(impl->m_capabilities.EXT_shader_pixel_local_storage);
        assert(impl->m_capabilities.EXT_shader_framebuffer_fetch ||
               impl->m_capabilities.ARM_shader_framebuffer_fetch);

        auto renderTarget = static_cast<PLSRenderTargetGL*>(desc.renderTarget);
        if (auto framebufferRenderTarget = lite_rtti_cast<FramebufferRenderTargetGL*>(renderTarget))
        {
            // With EXT_shader_pixel_local_storage we can render directly to any framebuffer.
            framebufferRenderTarget->bindExternalFramebuffer(GL_FRAMEBUFFER);
        }
        else
        {
            renderTarget->bindInternalFramebuffer(GL_FRAMEBUFFER);
        }
        glEnable(GL_SHADER_PIXEL_LOCAL_STORAGE_EXT);

        std::array<float, 4> clearColor4f;
        LoadStoreActionsEXT actions = BuildLoadActionsEXT(desc, &clearColor4f);
        if ((actions & LoadStoreActionsEXT::clearColor) &&
            simd::all(simd::load4f(clearColor4f.data()) == 0))
        {
            // glClear() is only well-defined for EXT_shader_pixel_local_storage when the clear
            // color is zero, and it zeros out every plane of pixel local storage.
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        else
        {
            // Otherwise we have to initialize pixel local storage with a fullscreen draw.
            const PLSLoadStoreProgram& plsProgram = m_plsLoadStorePrograms
                                                        .try_emplace(actions,
                                                                     actions,
                                                                     m_plsLoadStoreVertexShader,
                                                                     m_capabilities,
                                                                     m_state)
                                                        .first->second;
            m_state->bindProgram(plsProgram.id());
            if (plsProgram.clearColorUniLocation() >= 0)
            {
                glUniform4fv(plsProgram.clearColorUniLocation(), 1, clearColor4f.data());
            }
            m_state->bindVAO(m_plsLoadStoreVAO);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    }

    void deactivatePixelLocalStorage(PLSRenderContextGLImpl* impl, const FlushDescriptor&) override
    {
        // Issue a fullscreen draw that transfers the color information in pixel local storage to
        // the main framebuffer.
        LoadStoreActionsEXT actions = LoadStoreActionsEXT::storeColor;
        const PLSLoadStoreProgram& plsProgram =
            m_plsLoadStorePrograms
                .try_emplace(actions, actions, m_plsLoadStoreVertexShader, m_capabilities, m_state)
                .first->second;
        m_state->bindProgram(plsProgram.id());
        m_state->bindVAO(m_plsLoadStoreVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glDisable(GL_SHADER_PIXEL_LOCAL_STORAGE_EXT);
    }

    const char* shaderDefineName() const override { return GLSL_PLS_IMPL_EXT_NATIVE; }

private:
    const GLCapabilities m_capabilities;
    std::map<LoadStoreActionsEXT, PLSLoadStoreProgram> m_plsLoadStorePrograms;
    GLuint m_plsLoadStoreVertexShader = 0;
    GLuint m_plsLoadStoreVAO = 0;
    rcp<GLState> m_state;
};

std::unique_ptr<PLSRenderContextGLImpl::PLSImpl> PLSRenderContextGLImpl::MakePLSImplEXTNative(
    const GLCapabilities& extensions)
{
    return std::make_unique<PLSImplEXTNative>(extensions);
}
} // namespace rive::pls
