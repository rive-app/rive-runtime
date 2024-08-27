/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/gl/render_context_gl_impl.hpp"

#include "rive/renderer/gl/load_store_actions_ext.hpp"
#include "rive/renderer/gl/gl_utils.hpp"
#include "rive/math/simd.hpp"
#include "rive/renderer/gl/render_target_gl.hpp"
#include <sstream>

#include "generated/shaders/pls_load_store_ext.exports.h"

namespace rive::gpu
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
                        gpu::ShaderFeatures combinedShaderFeatures,
                        rcp<GLState> state) :
        m_state(std::move(state))
    {
        m_id = glCreateProgram();
        glAttachShader(m_id, vertexShader);

        std::ostringstream glsl;
        glsl << "#version 300 es\n";
        glsl << "#define " GLSL_FRAGMENT "\n";
        if (combinedShaderFeatures & gpu::ShaderFeatures::ENABLE_CLIPPING)
        {
            glsl << "#define " GLSL_ENABLE_CLIPPING "\n";
        }
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
    PLSImplEXTNative(const GLCapabilities& capabilities) : m_capabilities(capabilities) {}

    ~PLSImplEXTNative()
    {
        for (GLuint shader : m_plsLoadStoreVertexShaders)
        {
            if (shader != 0)
            {
                glDeleteShader(shader);
            }
        }
        m_state->deleteVAO(m_plsLoadStoreVAO);
    }

    void init(rcp<GLState> state) override { m_state = std::move(state); }

    bool supportsRasterOrdering(const GLCapabilities&) const override { return true; }

    void activatePixelLocalStorage(PLSRenderContextGLImpl* impl,
                                   const FlushDescriptor& desc) override
    {
        assert(impl->m_capabilities.EXT_shader_pixel_local_storage);
        assert(impl->m_capabilities.EXT_shader_framebuffer_fetch ||
               impl->m_capabilities.ARM_shader_framebuffer_fetch);

        auto renderTarget = static_cast<PLSRenderTargetGL*>(desc.renderTarget);
        renderTarget->bindDestinationFramebuffer(GL_FRAMEBUFFER);
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
            const PLSLoadStoreProgram& plsProgram =
                findLoadStoreProgram(actions, desc.combinedShaderFeatures);
            m_state->bindProgram(plsProgram.id());
            if (plsProgram.clearColorUniLocation() >= 0)
            {
                glUniform4fv(plsProgram.clearColorUniLocation(), 1, clearColor4f.data());
            }
            m_state->bindVAO(m_plsLoadStoreVAO);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    }

    void deactivatePixelLocalStorage(PLSRenderContextGLImpl* impl,
                                     const FlushDescriptor& desc) override
    {
        // Issue a fullscreen draw that transfers the color information in pixel local storage to
        // the main framebuffer.
        LoadStoreActionsEXT actions = LoadStoreActionsEXT::storeColor;
        m_state->bindProgram(findLoadStoreProgram(actions, desc.combinedShaderFeatures).id());
        m_state->bindVAO(m_plsLoadStoreVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glDisable(GL_SHADER_PIXEL_LOCAL_STORAGE_EXT);
    }

    void pushShaderDefines(gpu::InterlockMode, std::vector<const char*>* defines) const override
    {
        defines->push_back(GLSL_PLS_IMPL_EXT_NATIVE);
    }

private:
    const PLSLoadStoreProgram& findLoadStoreProgram(LoadStoreActionsEXT actions,
                                                    gpu::ShaderFeatures combinedShaderFeatures)
    {
        bool hasClipping = combinedShaderFeatures & gpu::ShaderFeatures::ENABLE_CLIPPING;
        uint32_t programKey =
            (static_cast<uint32_t>(actions) << 1) | static_cast<uint32_t>(hasClipping);

        if (m_plsLoadStoreVertexShaders[hasClipping] == 0)
        {
            std::ostringstream glsl;
            glsl << "#version 300 es\n";
            glsl << "#define " GLSL_VERTEX "\n";
            if (hasClipping)
            {
                glsl << "#define " GLSL_ENABLE_CLIPPING "\n";
            }
            BuildLoadStoreEXTGLSL(glsl, LoadStoreActionsEXT::none);
            m_plsLoadStoreVertexShaders[hasClipping] =
                glutils::CompileRawGLSL(GL_VERTEX_SHADER, glsl.str().c_str());
            glGenVertexArrays(1, &m_plsLoadStoreVAO);
        }

        return m_plsLoadStorePrograms
            .try_emplace(programKey,
                         actions,
                         m_plsLoadStoreVertexShaders[hasClipping],
                         combinedShaderFeatures,
                         m_state)
            .first->second;
    }

    const GLCapabilities m_capabilities;
    std::map<uint32_t, PLSLoadStoreProgram> m_plsLoadStorePrograms;
    GLuint m_plsLoadStoreVertexShaders[2] = {0}; // One with a clip plane and one without.
    GLuint m_plsLoadStoreVAO = 0;
    rcp<GLState> m_state;
};

std::unique_ptr<PLSRenderContextGLImpl::PLSImpl> PLSRenderContextGLImpl::MakePLSImplEXTNative(
    const GLCapabilities& capabilities)
{
    return std::make_unique<PLSImplEXTNative>(capabilities);
}
} // namespace rive::gpu
