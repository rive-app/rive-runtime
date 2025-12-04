/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/gl/render_context_gl_impl.hpp"

#include "rive/renderer/gl/load_store_actions_ext.hpp"
#include "rive/renderer/gl/gl_utils.hpp"
#include "rive/renderer/gl/render_target_gl.hpp"
#include "shaders/constants.glsl"
#include <map>
#include <sstream>

#include "generated/shaders/pls_load_store_ext.glsl.exports.h"

namespace rive::gpu
{
// Wraps an EXT_shader_pixel_local_storage load/store program, described by a
// set of LoadStoreActions.
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
        BuildLoadStoreEXTGLSL(glsl, actions);
        GLuint fragmentShader =
            glutils::CompileRawGLSL(GL_FRAGMENT_SHADER, glsl.str().c_str());
        glAttachShader(m_id, fragmentShader);
        glDeleteShader(fragmentShader);

        glutils::LinkProgram(m_id);
        if (actions & LoadStoreActionsEXT::clearColor)
        {
            m_colorClearUniLocation =
                glGetUniformLocation(m_id, GLSL_clearColor);
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

class RenderContextGLImpl::PLSImplEXTNative
    : public RenderContextGLImpl::PixelLocalStorageImpl
{
public:
    PLSImplEXTNative(const GLCapabilities& capabilities) :
        m_capabilities(capabilities)
    {}

    ~PLSImplEXTNative()
    {
        if (m_plsLoadStoreVertexShader != 0)
        {
            glDeleteShader(m_plsLoadStoreVertexShader);
        }
        m_state->deleteVAO(m_plsLoadStoreVAO);
    }

    void init(rcp<GLState> state) override { m_state = std::move(state); }

    void getSupportedInterlockModes(
        const GLCapabilities& capabilities,
        PlatformFeatures* platformFeatures) const override
    {
        assert(capabilities.EXT_shader_pixel_local_storage);
        platformFeatures->supportsRasterOrderingMode = true;
        platformFeatures->supportsClockwiseMode = true;
        platformFeatures->supportsClockwiseFixedFunctionMode =
            capabilities.EXT_shader_pixel_local_storage2;
    }

    void applyPipelineStateOverrides(
        const DrawBatch&,
        const FlushDescriptor& desc,
        const PlatformFeatures&,
        PipelineState* pipelineState) const override
    {
        // Vivo Y21 and Oppo Reno 3 Pro both disable writes to pixel local
        // storage when the color mask is off; just leave the color mask enabled
        // in EXT_shader_pixel_local_storage mode.
        pipelineState->colorWriteEnabled = true;
    }

    void activatePixelLocalStorage(RenderContextGLImpl* impl,
                                   const FlushDescriptor& desc) override
    {
        assert(impl->m_capabilities.EXT_shader_pixel_local_storage);
        assert(impl->m_capabilities.EXT_shader_framebuffer_fetch ||
               impl->m_capabilities.ARM_shader_framebuffer_fetch);

        auto renderTarget = static_cast<RenderTargetGL*>(desc.renderTarget);
        renderTarget->bindDestinationFramebuffer(GL_FRAMEBUFFER);
        if (desc.fixedFunctionColorOutput)
        {
            // We need EXT_shader_pixel_local_storage2 for
            // fixedFunctionColorOutput.
            assert(impl->m_capabilities.EXT_shader_pixel_local_storage2);
            assert(desc.interlockMode == gpu::InterlockMode::clockwise);
            assert(!(desc.combinedShaderFeatures &
                     gpu::ShaderFeatures::ENABLE_ADVANCED_BLEND));
            glFramebufferPixelLocalStorageSizeEXT(GL_FRAMEBUFFER,
                                                  2 * sizeof(uint32_t));
        }
        else if (impl->m_capabilities.usePixelLocalStorage2AsWorkaround)
        {
            // PowerVR Rogue GE8300, OpenGL ES 3.2 build 1.10@5187610 has severe
            // pixel local storage corruption issues with our renderer. Using
            // the EXT_shader_pixel_local_storage2 API to set the size is an
            // apparent workaround that comes with worse performance and other,
            // less severe visual artifacts.
            assert(impl->m_capabilities.EXT_shader_pixel_local_storage2);
            glFramebufferPixelLocalStorageSizeEXT(GL_FRAMEBUFFER,
                                                  PLS_PLANE_COUNT *
                                                      sizeof(uint32_t));
        }
        glEnable(GL_SHADER_PIXEL_LOCAL_STORAGE_EXT);

        m_state->setPipelineState(gpu::COLOR_ONLY_PIPELINE_STATE);
        if (desc.fixedFunctionColorOutput)
        {
            // Clear the main framebuffer.
            float cc[4];
            UnpackColorToRGBA32FPremul(desc.colorClearValue, cc);
            glClearColor(cc[0], cc[1], cc[2], cc[3]);
            glClear(GL_COLOR_BUFFER_BIT);

            // Clear PLS using the EXT_shader_pixel_local_storage2 API.
            assert(impl->m_capabilities.EXT_shader_pixel_local_storage2);
            GLuint plsClearValues[2] = {
                desc.coverageClearValue,
                /*clipClearValue=*/0u,
            };
            glClearPixelLocalStorageuiEXT(0, 2, plsClearValues);
        }
        else
        {
            // EXT_shader_pixel_local_storage doesn't have an initialization
            // API. Initialize PLS by drawing a fullscreen quad.
            std::array<float, 4> clearColor4f;
            LoadStoreActionsEXT actions =
                BuildLoadActionsEXT(desc, &clearColor4f);
            const PLSLoadStoreProgram& plsProgram =
                findLoadStoreProgram(actions, desc.combinedShaderFeatures);
            m_state->bindProgram(plsProgram.id());
            if (plsProgram.clearColorUniLocation() >= 0)
            {
                glUniform4fv(plsProgram.clearColorUniLocation(),
                             1,
                             clearColor4f.data());
            }
            m_state->bindVAO(m_plsLoadStoreVAO);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    }

    void deactivatePixelLocalStorage(RenderContextGLImpl* impl,
                                     const FlushDescriptor& desc) override
    {
        if (!desc.fixedFunctionColorOutput)
        {
            // EXT_shader_pixel_local_storage doesn't support concurrent
            // rendering to PLS and the framebuffer. Now that we're done, issue
            // a fullscreen draw that transfers the color information from PLS
            // to the main framebuffer.
            LoadStoreActionsEXT actions = LoadStoreActionsEXT::storeColor;
            m_state->bindProgram(
                findLoadStoreProgram(actions, desc.combinedShaderFeatures)
                    .id());
            m_state->bindVAO(m_plsLoadStoreVAO);
            m_state->setPipelineState(gpu::COLOR_ONLY_PIPELINE_STATE);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }

        glDisable(GL_SHADER_PIXEL_LOCAL_STORAGE_EXT);
    }

    void pushShaderDefines(gpu::InterlockMode,
                           std::vector<const char*>* defines) const override
    {
        defines->push_back(GLSL_PLS_IMPL_EXT_NATIVE);
    }

private:
    const PLSLoadStoreProgram& findLoadStoreProgram(
        LoadStoreActionsEXT actions,
        gpu::ShaderFeatures combinedShaderFeatures)
    {
        if (m_plsLoadStoreVertexShader == 0)
        {
            std::ostringstream glsl;
            glsl << "#version 300 es\n";
            glsl << "#define " GLSL_VERTEX "\n";
            BuildLoadStoreEXTGLSL(glsl, LoadStoreActionsEXT::none);
            m_plsLoadStoreVertexShader =
                glutils::CompileRawGLSL(GL_VERTEX_SHADER, glsl.str().c_str());
            glGenVertexArrays(1, &m_plsLoadStoreVAO);
        }

        const uint32_t programKey = static_cast<uint32_t>(actions);
        return m_plsLoadStorePrograms
            .try_emplace(programKey,
                         actions,
                         m_plsLoadStoreVertexShader,
                         combinedShaderFeatures,
                         m_state)
            .first->second;
    }

    const GLCapabilities m_capabilities;
    std::map<uint32_t, PLSLoadStoreProgram> m_plsLoadStorePrograms;
    GLuint m_plsLoadStoreVertexShader = 0;
    GLuint m_plsLoadStoreVAO = 0;
    rcp<GLState> m_state;
};

std::unique_ptr<RenderContextGLImpl::PixelLocalStorageImpl>
RenderContextGLImpl::MakePLSImplEXTNative(const GLCapabilities& capabilities)
{
    return std::make_unique<PLSImplEXTNative>(capabilities);
}
} // namespace rive::gpu
