/*
 * Copyright 2022 Rive
 */

#include "rive/renderer/gl/render_context_gl_impl.hpp"

#include "rive/renderer/gl/render_buffer_gl_impl.hpp"
#include "rive/renderer/gl/render_target_gl.hpp"
#include "rive/renderer/draw.hpp"
#include "rive/renderer/texture.hpp"
#include "shaders/constants.glsl"

#include "generated/shaders/advanced_blend.glsl.hpp"
#include "generated/shaders/color_ramp.glsl.hpp"
#include "generated/shaders/constants.glsl.hpp"
#include "generated/shaders/common.glsl.hpp"
#include "generated/shaders/draw_path_common.glsl.hpp"
#include "generated/shaders/draw_path.glsl.hpp"
#include "generated/shaders/draw_image_mesh.glsl.hpp"
#include "generated/shaders/bezier_utils.glsl.hpp"
#include "generated/shaders/tessellate.glsl.hpp"
#include "generated/shaders/render_atlas.glsl.hpp"
#include "generated/shaders/blit_texture_as_draw.glsl.hpp"
#include "generated/shaders/stencil_draw.glsl.hpp"

#ifdef RIVE_WEBGL
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

// In an effort to save space on web, and since web doesn't have ES 3.1 level
// support, don't include the atomic sources.
namespace rive::gpu::glsl
{
const char atomic_draw[] = "";
}
#define DISABLE_PLS_ATOMICS
#else
#include "generated/shaders/atomic_draw.glsl.hpp"
#endif

namespace rive::gpu
{
RenderContextGLImpl::RenderContextGLImpl(
    const char* rendererString,
    GLCapabilities capabilities,
    std::unique_ptr<PixelLocalStorageImpl> plsImpl) :
    m_capabilities(capabilities),
    m_plsImpl(std::move(plsImpl)),
    m_state(make_rcp<GLState>(m_capabilities))

{
    if (m_plsImpl != nullptr)
    {
        m_platformFeatures.supportsRasterOrdering =
            m_plsImpl->supportsRasterOrdering(m_capabilities);
        m_platformFeatures.supportsFragmentShaderAtomics =
            m_plsImpl->supportsFragmentShaderAtomics(m_capabilities);
    }
    if (m_capabilities.KHR_blend_equation_advanced_coherent)
    {
        m_platformFeatures.supportsKHRBlendEquations = true;
    }
    if (m_capabilities.EXT_clip_cull_distance)
    {
        m_platformFeatures.supportsClipPlanes = true;
    }
    if (strstr(rendererString, "Apple") && strstr(rendererString, "Metal"))
    {
        // In Metal, non-flat varyings preserve their exact value if all
        // vertices in the triangle emit the same value, and we also see a small
        // (5-10%) improvement from not using flat varyings.
        m_platformFeatures.avoidFlatVaryings = true;
    }
    if (m_capabilities.isPowerVR)
    {
        // Vivo Y21 (PowerVR Rogue GE8320; OpenGL ES 3.2 build 1.13@5776728a)
        // seems to hit some sort of reset condition that corrupts pixel local
        // storage when rendering a complex feather. For now, feather directly
        // to the screen on PowerVR; always go offscreen.
        m_platformFeatures.alwaysFeatherToAtlas = true;
    }
    m_platformFeatures.clipSpaceBottomUp = true;
    m_platformFeatures.framebufferBottomUp = true;

    GLint maxTextureSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    m_platformFeatures.maxTextureSize = maxTextureSize;

    std::vector<const char*> generalDefines;
    if (!m_capabilities.ARB_shader_storage_buffer_object)
    {
        generalDefines.push_back(GLSL_DISABLE_SHADER_STORAGE_BUFFERS);
    }

    const char* colorRampSources[] = {glsl::constants,
                                      glsl::common,
                                      glsl::color_ramp};
    m_colorRampProgram.compileAndAttachShader(GL_VERTEX_SHADER,
                                              generalDefines.data(),
                                              generalDefines.size(),
                                              colorRampSources,
                                              std::size(colorRampSources),
                                              m_capabilities);
    m_colorRampProgram.compileAndAttachShader(GL_FRAGMENT_SHADER,
                                              generalDefines.data(),
                                              generalDefines.size(),
                                              colorRampSources,
                                              std::size(colorRampSources),
                                              m_capabilities);
    m_colorRampProgram.link();
    glUniformBlockBinding(
        m_colorRampProgram,
        glGetUniformBlockIndex(m_colorRampProgram, GLSL_FlushUniforms),
        FLUSH_UNIFORM_BUFFER_IDX);

    m_state->bindVAO(m_colorRampVAO);
    glEnableVertexAttribArray(0);
    glVertexAttribDivisor(0, 1);

    // Emulate the feather texture1d array as a texture2d since GLES doesn't
    // have texture1d.
    glActiveTexture(GL_TEXTURE0 + FEATHER_TEXTURE_IDX);
    glBindTexture(GL_TEXTURE_2D, m_featherTexture);
    glTexStorage2D(GL_TEXTURE_2D,
                   1,
                   GL_R16F,
                   gpu::GAUSSIAN_TABLE_SIZE,
                   FEATHER_TEXTURE_1D_ARRAY_LENGTH);
    m_state->bindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glTexSubImage2D(GL_TEXTURE_2D,
                    0,
                    0,
                    FEATHER_FUNCTION_ARRAY_INDEX,
                    gpu::GAUSSIAN_TABLE_SIZE,
                    1,
                    GL_RED,
                    GL_HALF_FLOAT,
                    gpu::g_gaussianIntegralTableF16);
    glTexSubImage2D(GL_TEXTURE_2D,
                    0,
                    0,
                    FEATHER_INVERSE_FUNCTION_ARRAY_INDEX,
                    gpu::GAUSSIAN_TABLE_SIZE,
                    1,
                    GL_RED,
                    GL_HALF_FLOAT,
                    gpu::g_inverseGaussianIntegralTableF16);
    glutils::SetTexture2DSamplingParams(GL_LINEAR, GL_LINEAR);

    const char* tessellateSources[] = {glsl::constants,
                                       glsl::common,
                                       glsl::bezier_utils,
                                       glsl::tessellate};
    m_tessellateProgram.compileAndAttachShader(GL_VERTEX_SHADER,
                                               generalDefines.data(),
                                               generalDefines.size(),
                                               tessellateSources,
                                               std::size(tessellateSources),
                                               m_capabilities);
    m_tessellateProgram.compileAndAttachShader(GL_FRAGMENT_SHADER,
                                               generalDefines.data(),
                                               generalDefines.size(),
                                               tessellateSources,
                                               std::size(tessellateSources),
                                               m_capabilities);
    m_tessellateProgram.link();
    m_state->bindProgram(m_tessellateProgram);
    glutils::Uniform1iByName(m_tessellateProgram,
                             GLSL_featherTexture,
                             FEATHER_TEXTURE_IDX);
    glUniformBlockBinding(
        m_tessellateProgram,
        glGetUniformBlockIndex(m_tessellateProgram, GLSL_FlushUniforms),
        FLUSH_UNIFORM_BUFFER_IDX);
    if (!m_capabilities.ARB_shader_storage_buffer_object)
    {
        // Our GL driver doesn't support storage buffers. We polyfill these
        // buffers as textures.
        glutils::Uniform1iByName(m_tessellateProgram,
                                 GLSL_pathBuffer,
                                 PATH_BUFFER_IDX);
        glutils::Uniform1iByName(m_tessellateProgram,
                                 GLSL_contourBuffer,
                                 CONTOUR_BUFFER_IDX);
    }

    m_state->bindVAO(m_tessellateVAO);
    for (int i = 0; i < 4; ++i)
    {
        glEnableVertexAttribArray(i);
        // Draw two instances per TessVertexSpan: one normal and one optional
        // reflection.
        glVertexAttribDivisor(i, 1);
    }

    m_state->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_tessSpanIndexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof(gpu::kTessSpanIndices),
                 gpu::kTessSpanIndices,
                 GL_STATIC_DRAW);

    m_state->bindVAO(m_drawVAO);

    PatchVertex patchVertices[kPatchVertexBufferCount];
    uint16_t patchIndices[kPatchIndexBufferCount];
    GeneratePatchBufferData(patchVertices, patchIndices);

    m_state->bindBuffer(GL_ARRAY_BUFFER, m_patchVerticesBuffer);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(patchVertices),
                 patchVertices,
                 GL_STATIC_DRAW);

    m_state->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_patchIndicesBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof(patchIndices),
                 patchIndices,
                 GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,
                          4,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(PatchVertex),
                          nullptr);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,
                          4,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(PatchVertex),
                          reinterpret_cast<const void*>(sizeof(float) * 4));

    m_state->bindVAO(m_trianglesVAO);
    glEnableVertexAttribArray(0);

    // We draw imageRects when in atomic mode.
    m_state->bindVAO(m_imageRectVAO);

    m_state->bindBuffer(GL_ARRAY_BUFFER, m_imageRectVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(gpu::kImageRectVertices),
                 gpu::kImageRectVertices,
                 GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,
                          4,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(gpu::ImageRectVertex),
                          nullptr);

    m_state->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_imageRectIndexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof(gpu::kImageRectIndices),
                 gpu::kImageRectIndices,
                 GL_STATIC_DRAW);

    m_state->bindVAO(m_imageMeshVAO);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    if (m_plsImpl != nullptr)
    {
        m_plsImpl->init(m_state);
    }
}

RenderContextGLImpl::~RenderContextGLImpl()
{
    glDeleteTextures(1, &m_gradientTexture);
    glDeleteTextures(1, &m_tessVertexTexture);

    // Because glutils wrappers delete GL objects that might affect bindings.
    m_state->invalidate();
}

void RenderContextGLImpl::invalidateGLState()
{
    glActiveTexture(GL_TEXTURE0 + TESS_VERTEX_TEXTURE_IDX);
    glBindTexture(GL_TEXTURE_2D, m_tessVertexTexture);

    glActiveTexture(GL_TEXTURE0 + GRAD_TEXTURE_IDX);
    glBindTexture(GL_TEXTURE_2D, m_gradientTexture);

    glActiveTexture(GL_TEXTURE0 + FEATHER_TEXTURE_IDX);
    glBindTexture(GL_TEXTURE_2D, m_featherTexture);

    glActiveTexture(GL_TEXTURE0 + ATLAS_TEXTURE_IDX);
    glBindTexture(GL_TEXTURE_2D, m_atlasTexture);

    m_state->invalidate();
}

void RenderContextGLImpl::unbindGLInternalResources()
{
    m_state->bindVAO(0);
    m_state->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    m_state->bindBuffer(GL_ARRAY_BUFFER, 0);
    m_state->bindBuffer(GL_UNIFORM_BUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    for (int i = 0; i <= DEFAULT_BINDINGS_SET_SIZE; ++i)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

rcp<RenderBuffer> RenderContextGLImpl::makeRenderBuffer(RenderBufferType type,
                                                        RenderBufferFlags flags,
                                                        size_t sizeInBytes)
{
    return make_rcp<RenderBufferGLImpl>(type, flags, sizeInBytes, m_state);
}

class TextureGLImpl : public Texture
{
public:
    TextureGLImpl(uint32_t width,
                  uint32_t height,
                  GLuint textureID,
                  const GLCapabilities& capabilities) :
        Texture(width, height), m_textureID(textureID)
    {}

    GLuint textureID() const { return m_textureID; }

private:
    GLuint m_textureID = 0;
};

rcp<Texture> RenderContextGLImpl::makeImageTexture(
    uint32_t width,
    uint32_t height,
    uint32_t mipLevelCount,
    const uint8_t imageDataRGBAPremul[])
{
    GLuint textureID;
    glGenTextures(1, &textureID);
    glActiveTexture(GL_TEXTURE0 + IMAGE_TEXTURE_IDX);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexStorage2D(GL_TEXTURE_2D, mipLevelCount, GL_RGBA8, width, height);
    glTexSubImage2D(GL_TEXTURE_2D,
                    0,
                    0,
                    0,
                    width,
                    height,
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    imageDataRGBAPremul);
    glutils::SetTexture2DSamplingParams(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    return adoptImageTexture(width, height, textureID);
}

rcp<Texture> RenderContextGLImpl::adoptImageTexture(uint32_t width,
                                                    uint32_t height,
                                                    GLuint textureID)
{
    return make_rcp<TextureGLImpl>(width, height, textureID, m_capabilities);
}

// BufferRingImpl in GL on a given buffer target. In order to support WebGL2, we
// don't do hardware mapping.
class BufferRingGLImpl : public BufferRing
{
public:
    static std::unique_ptr<BufferRingGLImpl> Make(size_t capacityInBytes,
                                                  GLenum target,
                                                  rcp<GLState> state)
    {
        return capacityInBytes != 0
                   ? std::unique_ptr<BufferRingGLImpl>(
                         new BufferRingGLImpl(target,
                                              capacityInBytes,
                                              std::move(state)))
                   : nullptr;
    }

    ~BufferRingGLImpl()
    {
        for (int i = 0; i < kBufferRingSize; ++i)
        {
            m_state->deleteBuffer(m_ids[i]);
        }
    }

    GLuint submittedBufferID() const { return m_ids[submittedBufferIdx()]; }

protected:
    BufferRingGLImpl(GLenum target,
                     size_t capacityInBytes,
                     rcp<GLState> state) :
        BufferRing(capacityInBytes), m_target(target), m_state(std::move(state))
    {
        glGenBuffers(kBufferRingSize, m_ids);
        for (int i = 0; i < kBufferRingSize; ++i)
        {
            m_state->bindBuffer(m_target, m_ids[i]);
            glBufferData(m_target, capacityInBytes, nullptr, GL_DYNAMIC_DRAW);
        }
    }

    void* onMapBuffer(int bufferIdx, size_t mapSizeInBytes) override
    {
#ifdef RIVE_WEBGL
        // WebGL doesn't support buffer mapping.
        return shadowBuffer();
#else
        m_state->bindBuffer(m_target, m_ids[bufferIdx]);
        return glMapBufferRange(m_target,
                                0,
                                mapSizeInBytes,
                                GL_MAP_WRITE_BIT |
                                    GL_MAP_INVALIDATE_BUFFER_BIT |
                                    GL_MAP_UNSYNCHRONIZED_BIT);
#endif
    }

    void onUnmapAndSubmitBuffer(int bufferIdx, size_t mapSizeInBytes) override
    {
        m_state->bindBuffer(m_target, m_ids[bufferIdx]);
#ifdef RIVE_WEBGL
        // WebGL doesn't support buffer mapping.
        glBufferSubData(m_target, 0, mapSizeInBytes, shadowBuffer());
#else
        glUnmapBuffer(m_target);
#endif
    }

    const GLenum m_target;
    GLuint m_ids[kBufferRingSize];
    const rcp<GLState> m_state;
};

// GL internalformat to use for a texture that polyfills a storage buffer.
static GLenum storage_texture_internalformat(
    gpu::StorageBufferStructure bufferStructure)
{
    switch (bufferStructure)
    {
        case gpu::StorageBufferStructure::uint32x4:
            return GL_RGBA32UI;
        case gpu::StorageBufferStructure::uint32x2:
            return GL_RG32UI;
        case gpu::StorageBufferStructure::float32x4:
            return GL_RGBA32F;
    }
    RIVE_UNREACHABLE();
}

// GL format to use for a texture that polyfills a storage buffer.
static GLenum storage_texture_format(
    gpu::StorageBufferStructure bufferStructure)
{
    switch (bufferStructure)
    {
        case gpu::StorageBufferStructure::uint32x4:
            return GL_RGBA_INTEGER;
        case gpu::StorageBufferStructure::uint32x2:
            return GL_RG_INTEGER;
        case gpu::StorageBufferStructure::float32x4:
            return GL_RGBA;
    }
    RIVE_UNREACHABLE();
}

// GL type to use for a texture that polyfills a storage buffer.
static GLenum storage_texture_type(gpu::StorageBufferStructure bufferStructure)
{
    switch (bufferStructure)
    {
        case gpu::StorageBufferStructure::uint32x4:
            return GL_UNSIGNED_INT;
        case gpu::StorageBufferStructure::uint32x2:
            return GL_UNSIGNED_INT;
        case gpu::StorageBufferStructure::float32x4:
            return GL_FLOAT;
    }
    RIVE_UNREACHABLE();
}

class StorageBufferRingGLImpl : public BufferRingGLImpl
{
public:
    StorageBufferRingGLImpl(size_t capacityInBytes,
                            gpu::StorageBufferStructure bufferStructure,
                            rcp<GLState> state) :
        BufferRingGLImpl(
            // If we don't support storage buffers, instead make a pixel-unpack
            // buffer that will be used to copy data into the polyfill texture.
            GL_SHADER_STORAGE_BUFFER,
            capacityInBytes,
            std::move(state)),
        m_bufferStructure(bufferStructure)
    {}

    void bindToRenderContext(uint32_t bindingIdx,
                             size_t bindingSizeInBytes,
                             size_t offsetSizeInBytes) const
    {
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                          bindingIdx,
                          submittedBufferID(),
                          offsetSizeInBytes,
                          bindingSizeInBytes);
    }

protected:
    const gpu::StorageBufferStructure m_bufferStructure;
};

class TexelBufferRingWebGL : public BufferRing
{
public:
    TexelBufferRingWebGL(size_t capacityInBytes,
                         gpu::StorageBufferStructure bufferStructure,
                         rcp<GLState> state) :
        BufferRing(
            gpu::StorageTextureBufferSize(capacityInBytes, bufferStructure)),
        m_bufferStructure(bufferStructure),
        m_state(std::move(state))
    {
        auto [width, height] =
            gpu::StorageTextureSize(capacityInBytes, m_bufferStructure);
        GLenum internalformat =
            storage_texture_internalformat(m_bufferStructure);
        glGenTextures(gpu::kBufferRingSize, m_textures);
        glActiveTexture(GL_TEXTURE0);
        for (size_t i = 0; i < gpu::kBufferRingSize; ++i)
        {
            glBindTexture(GL_TEXTURE_2D, m_textures[i]);
            glTexStorage2D(GL_TEXTURE_2D, 1, internalformat, width, height);
            glutils::SetTexture2DSamplingParams(GL_NEAREST, GL_NEAREST);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    ~TexelBufferRingWebGL()
    {
        glDeleteTextures(gpu::kBufferRingSize, m_textures);
    }

    void* onMapBuffer(int bufferIdx, size_t mapSizeInBytes) override
    {
        return shadowBuffer();
    }
    void onUnmapAndSubmitBuffer(int bufferIdx, size_t mapSizeInBytes) override
    {}

    void bindToRenderContext(uint32_t bindingIdx,
                             size_t bindingSizeInBytes,
                             size_t offsetSizeInBytes) const
    {
        auto [updateWidth, updateHeight] =
            gpu::StorageTextureSize(bindingSizeInBytes, m_bufferStructure);
        glActiveTexture(GL_TEXTURE0 + bindingIdx);
        glBindTexture(GL_TEXTURE_2D, m_textures[submittedBufferIdx()]);
        glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        0,
                        0,
                        updateWidth,
                        updateHeight,
                        storage_texture_format(m_bufferStructure),
                        storage_texture_type(m_bufferStructure),
                        shadowBuffer() + offsetSizeInBytes);
    }

protected:
    const gpu::StorageBufferStructure m_bufferStructure;
    const rcp<GLState> m_state;
    GLuint m_textures[gpu::kBufferRingSize];
};

std::unique_ptr<BufferRing> RenderContextGLImpl::makeUniformBufferRing(
    size_t capacityInBytes)
{
    return BufferRingGLImpl::Make(capacityInBytes, GL_UNIFORM_BUFFER, m_state);
}

std::unique_ptr<BufferRing> RenderContextGLImpl::makeStorageBufferRing(
    size_t capacityInBytes,
    gpu::StorageBufferStructure bufferStructure)
{
    if (capacityInBytes == 0)
    {
        return nullptr;
    }
    else if (m_capabilities.ARB_shader_storage_buffer_object)
    {
        return std::make_unique<StorageBufferRingGLImpl>(capacityInBytes,
                                                         bufferStructure,
                                                         m_state);
    }
    else
    {
        return std::make_unique<TexelBufferRingWebGL>(capacityInBytes,
                                                      bufferStructure,
                                                      m_state);
    }
}

std::unique_ptr<BufferRing> RenderContextGLImpl::makeVertexBufferRing(
    size_t capacityInBytes)
{
    return BufferRingGLImpl::Make(capacityInBytes, GL_ARRAY_BUFFER, m_state);
}

void RenderContextGLImpl::resizeGradientTexture(uint32_t width, uint32_t height)
{
    glDeleteTextures(1, &m_gradientTexture);
    if (width == 0 || height == 0)
    {
        m_gradientTexture = 0;
        return;
    }

    glGenTextures(1, &m_gradientTexture);
    glActiveTexture(GL_TEXTURE0 + GRAD_TEXTURE_IDX);
    glBindTexture(GL_TEXTURE_2D, m_gradientTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
    glutils::SetTexture2DSamplingParams(GL_LINEAR, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, m_colorRampFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           m_gradientTexture,
                           0);
}

void RenderContextGLImpl::resizeTessellationTexture(uint32_t width,
                                                    uint32_t height)
{
    glDeleteTextures(1, &m_tessVertexTexture);
    if (width == 0 || height == 0)
    {
        m_tessVertexTexture = 0;
        return;
    }

    glGenTextures(1, &m_tessVertexTexture);
    glActiveTexture(GL_TEXTURE0 + TESS_VERTEX_TEXTURE_IDX);
    glBindTexture(GL_TEXTURE_2D, m_tessVertexTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32UI, width, height);
    glutils::SetTexture2DSamplingParams(GL_NEAREST, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, m_tessellateFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           m_tessVertexTexture,
                           0);
}

void RenderContextGLImpl::AtlasProgram::compile(
    GLuint vertexShaderID,
    const char* defines[],
    size_t numDefines,
    const char* sources[],
    size_t numSources,
    const GLCapabilities& capabilities,
    GLState* state)
{
    m_program = glutils::Program();
    glAttachShader(m_program, vertexShaderID);
    m_program.compileAndAttachShader(GL_FRAGMENT_SHADER,
                                     defines,
                                     numDefines,
                                     sources,
                                     numSources,
                                     capabilities);
    m_program.link();
    state->bindProgram(m_program);
    glUniformBlockBinding(m_program,
                          glGetUniformBlockIndex(m_program, GLSL_FlushUniforms),
                          FLUSH_UNIFORM_BUFFER_IDX);
    glutils::Uniform1iByName(m_program,
                             GLSL_tessVertexTexture,
                             TESS_VERTEX_TEXTURE_IDX);
    glutils::Uniform1iByName(m_program,
                             GLSL_featherTexture,
                             FEATHER_TEXTURE_IDX);
    if (!capabilities.ARB_shader_storage_buffer_object)
    {
        glutils::Uniform1iByName(m_program, GLSL_pathBuffer, PATH_BUFFER_IDX);
        glutils::Uniform1iByName(m_program,
                                 GLSL_contourBuffer,
                                 CONTOUR_BUFFER_IDX);
    }
    if (!capabilities.ANGLE_base_vertex_base_instance_shader_builtin)
    {
        m_baseInstanceUniformLocation =
            glGetUniformLocation(m_program,
                                 glutils::BASE_INSTANCE_UNIFORM_NAME);
    }
}

void RenderContextGLImpl::resizeAtlasTexture(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
    {
        m_atlasTexture = glutils::Texture::Zero();
        return;
    }

    m_atlasTexture = glutils::Texture();
    glActiveTexture(GL_TEXTURE0 + ATLAS_TEXTURE_IDX);
    glBindTexture(GL_TEXTURE_2D, m_atlasTexture);
    glTexStorage2D(
        GL_TEXTURE_2D,
        1,
        m_capabilities.EXT_float_blend
            ? GL_R32F  // fp32 is ideal for the atlas. When there's a lot of
                       // overlap, fp16 can run out of precision.
            : GL_R16F, // FIXME: Fallback for when EXT_color_buffer_half_float
                       // isn't supported.
        width,
        height);
    glutils::SetTexture2DSamplingParams(GL_NEAREST, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, m_atlasFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           m_atlasTexture,
                           0);

    // Don't compile the atlas programs until we get an indication that they
    // will be used.
    if (m_atlasVertexShader == 0)
    {
        std::vector<const char*> defines;
        defines.push_back(GLSL_DRAW_PATH);
        defines.push_back(GLSL_ENABLE_FEATHER);
        defines.push_back(GLSL_ENABLE_INSTANCE_INDEX);
        if (!m_capabilities.ARB_shader_storage_buffer_object)
        {
            defines.push_back(GLSL_DISABLE_SHADER_STORAGE_BUFFERS);
        }
        const char* atlasSources[] = {glsl::constants,
                                      glsl::common,
                                      glsl::draw_path_common,
                                      glsl::render_atlas};
        m_atlasVertexShader.compile(GL_VERTEX_SHADER,
                                    defines.data(),
                                    defines.size(),
                                    atlasSources,
                                    std::size(atlasSources),
                                    m_capabilities);

        defines.push_back(GLSL_ATLAS_FEATHERED_FILL);
        m_atlasFillProgram.compile(m_atlasVertexShader,
                                   defines.data(),
                                   defines.size(),
                                   atlasSources,
                                   std::size(atlasSources),
                                   m_capabilities,
                                   m_state.get());
        defines.pop_back();

        defines.push_back(GLSL_ATLAS_FEATHERED_STROKE);
        m_atlasStrokeProgram.compile(m_atlasVertexShader,
                                     defines.data(),
                                     defines.size(),
                                     atlasSources,
                                     std::size(atlasSources),
                                     m_capabilities,
                                     m_state.get());
        defines.pop_back();
    }
}

RenderContextGLImpl::DrawShader::DrawShader(
    RenderContextGLImpl* renderContextImpl,
    GLenum shaderType,
    gpu::DrawType drawType,
    ShaderFeatures shaderFeatures,
    gpu::InterlockMode interlockMode,
    gpu::ShaderMiscFlags shaderMiscFlags)
{
#ifdef DISABLE_PLS_ATOMICS
    if (interlockMode == gpu::InterlockMode::atomics)
    {
        // Don't draw anything in atomic mode if support for it isn't compiled
        // in.
        return;
    }
#endif

    std::vector<const char*> defines;
    if (renderContextImpl->m_plsImpl != nullptr)
    {
        renderContextImpl->m_plsImpl->pushShaderDefines(interlockMode,
                                                        &defines);
    }
    if (interlockMode == gpu::InterlockMode::atomics)
    {
        // Atomics are currently always done on storage textures.
        defines.push_back(GLSL_USING_PLS_STORAGE_TEXTURES);
    }
    if (shaderMiscFlags & gpu::ShaderMiscFlags::fixedFunctionColorOutput)
    {
        defines.push_back(GLSL_FIXED_FUNCTION_COLOR_OUTPUT);
    }
    if (shaderMiscFlags & gpu::ShaderMiscFlags::clockwiseFill)
    {
        defines.push_back(GLSL_CLOCKWISE_FILL);
    }
    for (size_t i = 0; i < kShaderFeatureCount; ++i)
    {
        ShaderFeatures feature = static_cast<ShaderFeatures>(1 << i);
        if (shaderFeatures & feature)
        {
            assert((kVertexShaderFeaturesMask & feature) ||
                   shaderType == GL_FRAGMENT_SHADER);
            if (interlockMode == gpu::InterlockMode::msaa &&
                feature == gpu::ShaderFeatures::ENABLE_ADVANCED_BLEND &&
                renderContextImpl->m_capabilities
                    .KHR_blend_equation_advanced_coherent)
            {
                defines.push_back(GLSL_ENABLE_KHR_BLEND);
            }
            else
            {
                defines.push_back(GetShaderFeatureGLSLName(feature));
            }
        }
    }
    if (interlockMode == gpu::InterlockMode::msaa)
    {
        defines.push_back(GLSL_RENDER_MODE_MSAA);
    }
    assert(renderContextImpl->platformFeatures().framebufferBottomUp);
    defines.push_back(GLSL_FRAMEBUFFER_BOTTOM_UP);

    std::vector<const char*> sources;
    sources.push_back(glsl::constants);
    sources.push_back(glsl::common);
    if (shaderType == GL_FRAGMENT_SHADER &&
        (shaderFeatures & ShaderFeatures::ENABLE_ADVANCED_BLEND))
    {
        sources.push_back(glsl::advanced_blend);
    }
    if (renderContextImpl->platformFeatures().avoidFlatVaryings)
    {
        sources.push_back("#define " GLSL_OPTIONALLY_FLAT "\n");
    }
    else
    {
        sources.push_back("#define " GLSL_OPTIONALLY_FLAT " flat\n");
    }
    switch (drawType)
    {
        case gpu::DrawType::midpointFanPatches:
        case gpu::DrawType::midpointFanCenterAAPatches:
        case gpu::DrawType::outerCurvePatches:
            if (shaderType == GL_VERTEX_SHADER)
            {
                defines.push_back(GLSL_ENABLE_INSTANCE_INDEX);
            }
            defines.push_back(GLSL_DRAW_PATH);
            sources.push_back(gpu::glsl::draw_path_common);
            sources.push_back(interlockMode == gpu::InterlockMode::atomics
                                  ? gpu::glsl::atomic_draw
                                  : gpu::glsl::draw_path);
            break;
        case gpu::DrawType::stencilClipReset:
            assert(interlockMode == gpu::InterlockMode::msaa);
            sources.push_back(gpu::glsl::stencil_draw);
            break;
        case gpu::DrawType::atlasBlit:
            defines.push_back(GLSL_ATLAS_BLIT);
            [[fallthrough]];
        case gpu::DrawType::interiorTriangulation:
            defines.push_back(GLSL_DRAW_INTERIOR_TRIANGLES);
            sources.push_back(gpu::glsl::draw_path_common);
            sources.push_back(interlockMode == gpu::InterlockMode::atomics
                                  ? gpu::glsl::atomic_draw
                                  : gpu::glsl::draw_path);
            break;
        case gpu::DrawType::imageRect:
            assert(interlockMode == gpu::InterlockMode::atomics);
            defines.push_back(GLSL_DRAW_IMAGE);
            defines.push_back(GLSL_DRAW_IMAGE_RECT);
            sources.push_back(gpu::glsl::draw_path_common);
            sources.push_back(gpu::glsl::atomic_draw);
            break;
        case gpu::DrawType::imageMesh:
            defines.push_back(GLSL_DRAW_IMAGE);
            defines.push_back(GLSL_DRAW_IMAGE_MESH);
            if (interlockMode == gpu::InterlockMode::atomics)
            {
                sources.push_back(gpu::glsl::draw_path_common);
                sources.push_back(gpu::glsl::atomic_draw);
            }
            else
            {
                sources.push_back(gpu::glsl::draw_image_mesh);
            }
            break;
        case gpu::DrawType::atomicResolve:
            assert(interlockMode == gpu::InterlockMode::atomics);
            defines.push_back(GLSL_DRAW_RENDER_TARGET_UPDATE_BOUNDS);
            defines.push_back(GLSL_RESOLVE_PLS);
            if (shaderMiscFlags &
                gpu::ShaderMiscFlags::coalescedResolveAndTransfer)
            {
                assert(shaderType == GL_FRAGMENT_SHADER);
                defines.push_back(GLSL_COALESCED_PLS_RESOLVE_AND_TRANSFER);
            }
            sources.push_back(gpu::glsl::draw_path_common);
            sources.push_back(gpu::glsl::atomic_draw);
            break;
        case gpu::DrawType::atomicInitialize:
            assert(interlockMode == gpu::InterlockMode::atomics);
            RIVE_UNREACHABLE();
    }
    if (!renderContextImpl->m_capabilities.ARB_shader_storage_buffer_object)
    {
        defines.push_back(GLSL_DISABLE_SHADER_STORAGE_BUFFERS);
    }

    m_id = glutils::CompileShader(shaderType,
                                  defines.data(),
                                  defines.size(),
                                  sources.data(),
                                  sources.size(),
                                  renderContextImpl->m_capabilities);
}

RenderContextGLImpl::DrawProgram::DrawProgram(
    RenderContextGLImpl* renderContextImpl,
    gpu::DrawType drawType,
    gpu::ShaderFeatures shaderFeatures,
    gpu::InterlockMode interlockMode,
    gpu::ShaderMiscFlags shaderMiscFlags) :
    m_fragmentShader(renderContextImpl,
                     GL_FRAGMENT_SHADER,
                     drawType,
                     shaderFeatures,
                     interlockMode,
                     shaderMiscFlags),
    m_state(renderContextImpl->m_state)
{
    // Not every vertex shader is unique. Cache them by just the vertex features
    // and reuse when possible.
    ShaderFeatures vertexShaderFeatures =
        shaderFeatures & kVertexShaderFeaturesMask;
    uint32_t vertexShaderKey = gpu::ShaderUniqueKey(drawType,
                                                    vertexShaderFeatures,
                                                    interlockMode,
                                                    gpu::ShaderMiscFlags::none);
    const DrawShader& vertexShader =
        renderContextImpl->m_vertexShaders
            .try_emplace(vertexShaderKey,
                         renderContextImpl,
                         GL_VERTEX_SHADER,
                         drawType,
                         vertexShaderFeatures,
                         interlockMode,
                         gpu::ShaderMiscFlags::none)
            .first->second;

    m_id = glCreateProgram();
    glAttachShader(m_id, vertexShader.id());
    glAttachShader(m_id, m_fragmentShader.id());
    glutils::LinkProgram(m_id);

    m_state->bindProgram(m_id);
    glUniformBlockBinding(m_id,
                          glGetUniformBlockIndex(m_id, GLSL_FlushUniforms),
                          FLUSH_UNIFORM_BUFFER_IDX);

    const bool isImageDraw = gpu::DrawTypeIsImageDraw(drawType);
    const bool isTessellationDraw =
        drawType == gpu::DrawType::midpointFanPatches ||
        drawType == gpu::DrawType::midpointFanCenterAAPatches ||
        drawType == gpu::DrawType::outerCurvePatches;
    const bool isPaintDraw = isTessellationDraw ||
                             drawType == gpu::DrawType::interiorTriangulation ||
                             drawType == gpu::DrawType::atlasBlit;
    if (isImageDraw)
    {
        glUniformBlockBinding(
            m_id,
            glGetUniformBlockIndex(m_id, GLSL_ImageDrawUniforms),
            IMAGE_DRAW_UNIFORM_BUFFER_IDX);
    }
    if (isTessellationDraw)
    {
        glutils::Uniform1iByName(m_id,
                                 GLSL_tessVertexTexture,
                                 TESS_VERTEX_TEXTURE_IDX);
    }
    // Since atomic mode emits the color of the *previous* path, it needs the
    // gradient texture bound for every draw.
    if (isPaintDraw || interlockMode == gpu::InterlockMode::atomics)
    {
        glutils::Uniform1iByName(m_id, GLSL_gradTexture, GRAD_TEXTURE_IDX);
    }
    if ((isTessellationDraw &&
         (shaderFeatures & ShaderFeatures::ENABLE_FEATHER)) ||
        drawType == gpu::DrawType::atlasBlit)
    {
        assert(isPaintDraw || interlockMode == gpu::InterlockMode::atomics);
        glutils::Uniform1iByName(m_id,
                                 GLSL_featherTexture,
                                 FEATHER_TEXTURE_IDX);
    }
    // Atomic mode doesn't support image paints on paths.
    if (drawType == gpu::DrawType::atlasBlit)
    {
        glutils::Uniform1iByName(m_id, GLSL_atlasTexture, ATLAS_TEXTURE_IDX);
    }
    if (isImageDraw ||
        (isPaintDraw && interlockMode != gpu::InterlockMode::atomics))
    {
        glutils::Uniform1iByName(m_id, GLSL_imageTexture, IMAGE_TEXTURE_IDX);
    }
    if (!renderContextImpl->m_capabilities.ARB_shader_storage_buffer_object)
    {
        // Our GL driver doesn't support storage buffers. We polyfill these
        // buffers as textures.
        if (isPaintDraw)
        {
            glutils::Uniform1iByName(m_id, GLSL_pathBuffer, PATH_BUFFER_IDX);
        }
        if (isPaintDraw || interlockMode == gpu::InterlockMode::atomics)
        {
            glutils::Uniform1iByName(m_id, GLSL_paintBuffer, PAINT_BUFFER_IDX);
            glutils::Uniform1iByName(m_id,
                                     GLSL_paintAuxBuffer,
                                     PAINT_AUX_BUFFER_IDX);
        }
        if (isTessellationDraw)
        {
            glutils::Uniform1iByName(m_id,
                                     GLSL_contourBuffer,
                                     CONTOUR_BUFFER_IDX);
        }
    }
    if (interlockMode == gpu::InterlockMode::msaa &&
        (shaderFeatures & gpu::ShaderFeatures::ENABLE_ADVANCED_BLEND) &&
        !renderContextImpl->m_capabilities.KHR_blend_equation_advanced_coherent)
    {
        glutils::Uniform1iByName(m_id,
                                 GLSL_dstColorTexture,
                                 DST_COLOR_TEXTURE_IDX);
    }
    if (!renderContextImpl->m_capabilities
             .ANGLE_base_vertex_base_instance_shader_builtin)
    {
        m_baseInstanceUniformLocation =
            glGetUniformLocation(m_id, glutils::BASE_INSTANCE_UNIFORM_NAME);
    }
}

RenderContextGLImpl::DrawProgram::~DrawProgram()
{
    m_state->deleteProgram(m_id);
}

static GLuint gl_buffer_id(const BufferRing* bufferRing)
{
    return static_cast<const BufferRingGLImpl*>(bufferRing)
        ->submittedBufferID();
}

static void bind_storage_buffer(const GLCapabilities& capabilities,
                                const BufferRing* bufferRing,
                                uint32_t bindingIdx,
                                size_t bindingSizeInBytes,
                                size_t offsetSizeInBytes)
{
    assert(bufferRing != nullptr);
    assert(bindingSizeInBytes > 0);
    if (capabilities.ARB_shader_storage_buffer_object)
    {
        static_cast<const StorageBufferRingGLImpl*>(bufferRing)
            ->bindToRenderContext(bindingIdx,
                                  bindingSizeInBytes,
                                  offsetSizeInBytes);
    }
    else
    {
        static_cast<const TexelBufferRingWebGL*>(bufferRing)
            ->bindToRenderContext(bindingIdx,
                                  bindingSizeInBytes,
                                  offsetSizeInBytes);
    }
}

void RenderContextGLImpl::PixelLocalStorageImpl::ensureRasterOrderingEnabled(
    RenderContextGLImpl* renderContextImpl,
    const gpu::FlushDescriptor& desc,
    bool enabled)
{
    assert(!enabled ||
           supportsRasterOrdering(renderContextImpl->m_capabilities));
    auto rasterOrderState = enabled ? gpu::TriState::yes : gpu::TriState::no;
    if (m_rasterOrderingEnabled != rasterOrderState)
    {
        onEnableRasterOrdering(enabled);
        m_rasterOrderingEnabled = rasterOrderState;
        // We only need a barrier when turning raster ordering OFF, because PLS
        // already inserts the necessary barriers after draws when it's
        // disabled.
        if (m_rasterOrderingEnabled == gpu::TriState::no)
        {
            onBarrier(desc);
        }
    }
}

// Wraps calls to glDrawElementsInstanced*(), as appropriate for the current
// platform. Also includes simple helpers for working with stencil.
class DrawIndexedInstancedHelper
{
public:
    DrawIndexedInstancedHelper(GLint baseInstanceUniformLocation,
                               GLenum primitiveTopology,
                               uint32_t instanceCount,
                               uint32_t baseInstance) :
        m_baseInstanceUniformLocation(baseInstanceUniformLocation),
        m_primitiveTopology(primitiveTopology),
        m_instanceCount(instanceCount),
        m_baseInstance(baseInstance)
    {}

    void setIndexRange(uint32_t indexCount, uint32_t baseIndex)
    {
        m_indexCount = indexCount;
        m_indexOffset =
            reinterpret_cast<const void*>(baseIndex * sizeof(uint16_t));
    }

    void draw(const GLCapabilities& capabilities) const
    {
        assert(capabilities.ANGLE_base_vertex_base_instance_shader_builtin ==
               (m_baseInstanceUniformLocation < 0));
        for (uint32_t baseInstance = m_baseInstance,
                      endInstance = m_baseInstance + m_instanceCount;
             baseInstance < endInstance;)

        {
            uint32_t instanceCount =
                std::min(endInstance - baseInstance,
                         capabilities.maxSupportedInstancesPerDrawCommand);
#ifndef RIVE_WEBGL
            if (capabilities.ANGLE_base_vertex_base_instance_shader_builtin)
            {
                glDrawElementsInstancedBaseInstanceEXT(m_primitiveTopology,
                                                       m_indexCount,
                                                       GL_UNSIGNED_SHORT,
                                                       m_indexOffset,
                                                       instanceCount,
                                                       baseInstance);
            }
            else
#endif
            {
                glUniform1i(m_baseInstanceUniformLocation, baseInstance);
                glDrawElementsInstanced(m_primitiveTopology,
                                        m_indexCount,
                                        GL_UNSIGNED_SHORT,
                                        m_indexOffset,
                                        instanceCount);
            }
            baseInstance += instanceCount;
        }
    }

    void drawWithStencilSettings(GLenum comparator,
                                 GLint ref,
                                 GLuint mask,
                                 GLenum stencilFailOp,
                                 GLenum depthPassStencilFailOp,
                                 GLenum depthPassStencilPassOp,
                                 const GLCapabilities& capabilities) const
    {
        glStencilFunc(comparator, ref, mask);
        glStencilOp(stencilFailOp,
                    depthPassStencilFailOp,
                    depthPassStencilPassOp);
        draw(capabilities);
    }

private:
    const GLint m_baseInstanceUniformLocation;
    const GLenum m_primitiveTopology;
    const uint32_t m_instanceCount;
    const uint32_t m_baseInstance;
    uint32_t m_indexCount = 0;
    const void* m_indexOffset = nullptr;
};

void RenderContextGLImpl::flush(const FlushDescriptor& desc)
{
    assert(desc.interlockMode != gpu::InterlockMode::clockwiseAtomic);
    auto renderTarget = static_cast<RenderTargetGL*>(desc.renderTarget);

    m_state->setWriteMasks(true, true, 0xff);
    m_state->disableBlending();

    // All programs use the same set of per-flush uniforms.
    glBindBufferRange(GL_UNIFORM_BUFFER,
                      FLUSH_UNIFORM_BUFFER_IDX,
                      gl_buffer_id(flushUniformBufferRing()),
                      desc.flushUniformDataOffsetInBytes,
                      sizeof(gpu::FlushUniforms));

    // All programs use the same storage buffers.
    if (desc.pathCount > 0)
    {
        bind_storage_buffer(m_capabilities,
                            pathBufferRing(),
                            PATH_BUFFER_IDX,
                            desc.pathCount * sizeof(gpu::PathData),
                            desc.firstPath * sizeof(gpu::PathData));

        bind_storage_buffer(m_capabilities,
                            paintBufferRing(),
                            PAINT_BUFFER_IDX,
                            desc.pathCount * sizeof(gpu::PaintData),
                            desc.firstPaint * sizeof(gpu::PaintData));

        bind_storage_buffer(m_capabilities,
                            paintAuxBufferRing(),
                            PAINT_AUX_BUFFER_IDX,
                            desc.pathCount * sizeof(gpu::PaintAuxData),
                            desc.firstPaintAux * sizeof(gpu::PaintAuxData));
    }

    if (desc.contourCount > 0)
    {
        bind_storage_buffer(m_capabilities,
                            contourBufferRing(),
                            CONTOUR_BUFFER_IDX,
                            desc.contourCount * sizeof(gpu::ContourData),
                            desc.firstContour * sizeof(gpu::ContourData));
    }

    // Render the complex color ramps into the gradient texture.
    if (desc.gradSpanCount > 0)
    {
        if (m_capabilities.isPowerVR)
        {
            // PowerVR needs an extra little update to the gradient texture to
            // help with synchronization.
            glActiveTexture(GL_TEXTURE0 + GRAD_TEXTURE_IDX);
            uint32_t nullData = 0;
            glTexSubImage2D(GL_TEXTURE_2D,
                            0,
                            0,
                            0,
                            1,
                            1,
                            GL_RGBA,
                            GL_UNSIGNED_BYTE,
                            &nullData);
        }

        m_state->bindBuffer(GL_ARRAY_BUFFER,
                            gl_buffer_id(gradSpanBufferRing()));
        m_state->bindVAO(m_colorRampVAO);
        m_state->setCullFace(GL_BACK);
        glVertexAttribIPointer(
            0,
            4,
            GL_UNSIGNED_INT,
            0,
            reinterpret_cast<const void*>(desc.firstGradSpan *
                                          sizeof(gpu::GradientSpan)));
        glViewport(0, 0, kGradTextureWidth, desc.gradDataHeight);
        glBindFramebuffer(GL_FRAMEBUFFER, m_colorRampFBO);
        m_state->bindProgram(m_colorRampProgram);
        GLenum colorAttachment0 = GL_COLOR_ATTACHMENT0;
        glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, &colorAttachment0);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP,
                              0,
                              gpu::GRAD_SPAN_TRI_STRIP_VERTEX_COUNT,
                              desc.gradSpanCount);
    }

    // Tessellate all curves into vertices in the tessellation texture.
    if (desc.tessVertexSpanCount > 0)
    {
        m_state->bindBuffer(GL_ARRAY_BUFFER,
                            gl_buffer_id(tessSpanBufferRing()));
        m_state->bindVAO(m_tessellateVAO);
        m_state->setCullFace(GL_BACK);
        size_t tessSpanOffsetInBytes =
            desc.firstTessVertexSpan * sizeof(gpu::TessVertexSpan);
        for (GLuint i = 0; i < 3; ++i)
        {
            glVertexAttribPointer(i,
                                  4,
                                  GL_FLOAT,
                                  GL_FALSE,
                                  sizeof(TessVertexSpan),
                                  reinterpret_cast<const void*>(
                                      tessSpanOffsetInBytes + i * 4 * 4));
        }
        glVertexAttribIPointer(
            3,
            4,
            GL_UNSIGNED_INT,
            sizeof(TessVertexSpan),
            reinterpret_cast<const void*>(tessSpanOffsetInBytes +
                                          offsetof(TessVertexSpan, x0x1)));
        glViewport(0, 0, gpu::kTessTextureWidth, desc.tessDataHeight);
        glBindFramebuffer(GL_FRAMEBUFFER, m_tessellateFBO);
        m_state->bindProgram(m_tessellateProgram);
        GLenum colorAttachment0 = GL_COLOR_ATTACHMENT0;
        glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, &colorAttachment0);
        glDrawElementsInstanced(GL_TRIANGLES,
                                std::size(gpu::kTessSpanIndices),
                                GL_UNSIGNED_SHORT,
                                0,
                                desc.tessVertexSpanCount);
    }

    // Render the atlas if we have any offscreen feathers.
    if ((desc.atlasFillBatchCount | desc.atlasStrokeBatchCount) != 0)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_atlasFBO);
        glViewport(0, 0, desc.atlasContentWidth, desc.atlasContentHeight);
        glEnable(GL_SCISSOR_TEST);
        glScissor(0, 0, desc.atlasContentWidth, desc.atlasContentHeight);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        m_state->setCullFace(GL_FRONT); // Inverted because GL is bottom up.
        m_state->bindVAO(m_drawVAO);

        if (desc.atlasFillBatchCount != 0)
        {
            m_state->bindProgram(m_atlasFillProgram);
            m_state->setGLBlendMode(GLState::GLBlendMode::plus);
            for (size_t i = 0; i < desc.atlasFillBatchCount; ++i)
            {
                const gpu::AtlasDrawBatch& fillBatch = desc.atlasFillBatches[i];
                glScissor(fillBatch.scissor.left,
                          fillBatch.scissor.top,
                          fillBatch.scissor.width(),
                          fillBatch.scissor.height());
                DrawIndexedInstancedHelper drawHelper(
                    m_atlasFillProgram.baseInstanceUniformLocation(),
                    GL_TRIANGLES,
                    fillBatch.patchCount,
                    fillBatch.basePatch);
                drawHelper.setIndexRange(
                    gpu::kMidpointFanCenterAAPatchIndexCount,
                    gpu::kMidpointFanCenterAAPatchBaseIndex);
                drawHelper.draw(m_capabilities);
            }
        }

        if (desc.atlasStrokeBatchCount != 0)
        {
            m_state->bindProgram(m_atlasStrokeProgram);
            m_state->setGLBlendMode(GLState::GLBlendMode::max);
            for (size_t i = 0; i < desc.atlasStrokeBatchCount; ++i)
            {
                const gpu::AtlasDrawBatch& strokeBatch =
                    desc.atlasStrokeBatches[i];
                glScissor(strokeBatch.scissor.left,
                          strokeBatch.scissor.top,
                          strokeBatch.scissor.width(),
                          strokeBatch.scissor.height());
                DrawIndexedInstancedHelper drawHelper(
                    m_atlasStrokeProgram.baseInstanceUniformLocation(),
                    GL_TRIANGLES,
                    strokeBatch.patchCount,
                    strokeBatch.basePatch);
                drawHelper.setIndexRange(gpu::kMidpointFanPatchBorderIndexCount,
                                         gpu::kMidpointFanPatchBaseIndex);
                drawHelper.draw(m_capabilities);
            }
        }

        glDisable(GL_SCISSOR_TEST);
    }

    // Bind the currently-submitted buffer in the triangleBufferRing to its
    // vertex array.
    if (desc.hasTriangleVertices)
    {
        m_state->bindVAO(m_trianglesVAO);
        m_state->bindBuffer(GL_ARRAY_BUFFER,
                            gl_buffer_id(triangleBufferRing()));
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    }

    glViewport(0, 0, renderTarget->width(), renderTarget->height());

#ifdef RIVE_DESKTOP_GL
    if (m_capabilities.ANGLE_polygon_mode && desc.wireframe)
    {
        glPolygonModeANGLE(GL_FRONT_AND_BACK, GL_LINE_ANGLE);
        glLineWidth(2);
    }
#endif

    auto msaaResolveAction = RenderTargetGL::MSAAResolveAction::automatic;
    std::array<GLenum, 3> msaaDepthStencilColor;
    if (desc.interlockMode != gpu::InterlockMode::msaa)
    {
        assert(desc.msaaSampleCount == 0);
        m_plsImpl->activatePixelLocalStorage(this, desc);
    }
    else
    {
        assert(desc.msaaSampleCount > 0);
        bool preserveRenderTarget =
            desc.colorLoadAction == gpu::LoadAction::preserveRenderTarget;
        bool isFBO0;
        msaaResolveAction = renderTarget->bindMSAAFramebuffer(
            this,
            desc.msaaSampleCount,
            preserveRenderTarget ? &desc.renderTargetUpdateBounds : nullptr,
            &isFBO0);

        // Hint to tilers to not load unnecessary buffers from memory.
        if (isFBO0)
        {
            msaaDepthStencilColor = {GL_DEPTH, GL_STENCIL, GL_COLOR};
        }
        else
        {
            msaaDepthStencilColor = {GL_DEPTH_ATTACHMENT,
                                     GL_STENCIL_ATTACHMENT,
                                     GL_COLOR_ATTACHMENT0};
        }
        glInvalidateFramebuffer(GL_FRAMEBUFFER,
                                preserveRenderTarget ? 2 : 3,
                                msaaDepthStencilColor.data());

        GLbitfield buffersToClear = GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
        if (desc.colorLoadAction == gpu::LoadAction::clear)
        {
            float cc[4];
            UnpackColorToRGBA32FPremul(desc.colorClearValue, cc);
            glClearColor(cc[0], cc[1], cc[2], cc[3]);
            buffersToClear |= GL_COLOR_BUFFER_BIT;
        }
        glClear(buffersToClear);

        glEnable(GL_STENCIL_TEST);
        glEnable(GL_DEPTH_TEST);

        if (desc.combinedShaderFeatures &
            gpu::ShaderFeatures::ENABLE_ADVANCED_BLEND)
        {
            if (m_capabilities.KHR_blend_equation_advanced_coherent)
            {
                glEnable(GL_BLEND_ADVANCED_COHERENT_KHR);
            }
            else
            {
                // Set up an internal texture to copy the framebuffer into, for
                // in-shader blending.
                renderTarget->bindInternalDstTexture(GL_TEXTURE0 +
                                                     DST_COLOR_TEXTURE_IDX);
            }
        }
    }

    bool clipPlanesEnabled = false;

    // Execute the DrawList.
    for (const DrawBatch& batch : *desc.drawList)
    {
        if (batch.elementCount == 0)
        {
            continue;
        }

        gpu::ShaderFeatures shaderFeatures =
            desc.interlockMode == gpu::InterlockMode::atomics
                ? desc.combinedShaderFeatures
                : batch.shaderFeatures;
        gpu::ShaderMiscFlags shaderMiscFlags = batch.shaderMiscFlags;
        if (m_plsImpl != nullptr)
        {
            shaderMiscFlags |= m_plsImpl->shaderMiscFlags(desc, batch.drawType);
        }
        if (desc.interlockMode == gpu::InterlockMode::rasterOrdering &&
            (batch.drawContents & gpu::DrawContents::clockwiseFill))
        {
            shaderMiscFlags |= gpu::ShaderMiscFlags::clockwiseFill;
        }
        uint32_t fragmentShaderKey = gpu::ShaderUniqueKey(batch.drawType,
                                                          shaderFeatures,
                                                          desc.interlockMode,
                                                          shaderMiscFlags);
        const DrawProgram& drawProgram = m_drawPrograms
                                             .try_emplace(fragmentShaderKey,
                                                          this,
                                                          batch.drawType,
                                                          shaderFeatures,
                                                          desc.interlockMode,
                                                          shaderMiscFlags)
                                             .first->second;
        m_state->bindProgram(drawProgram.id());

        if (auto imageTextureGL =
                static_cast<const TextureGLImpl*>(batch.imageTexture))
        {
            glActiveTexture(GL_TEXTURE0 + IMAGE_TEXTURE_IDX);
            glBindTexture(GL_TEXTURE_2D, imageTextureGL->textureID());
        }

        if (desc.interlockMode == gpu::InterlockMode::msaa)
        {
            // Set up the next blend.
            if (batch.drawContents & gpu::DrawContents::opaquePaint)
            {
                m_state->disableBlending();
            }
            else if (!(batch.drawContents & gpu::DrawContents::advancedBlend))
            {
                assert(batch.firstBlendMode == BlendMode::srcOver);
                m_state->setBlendMode(BlendMode::srcOver);
            }
            else if (m_capabilities.KHR_blend_equation_advanced_coherent)
            {
                // When m_platformFeatures.supportsKHRBlendEquations is true in
                // msaa mode, the renderContext does not combine draws when they
                // have different blend modes.
                m_state->setBlendMode(batch.firstBlendMode);
            }
            else
            {
                if (batch.dstReadList != nullptr)
                {
                    // Read back the framebuffer where we need a dstColor for
                    // blending.
                    renderTarget->bindInternalFramebuffer(
                        GL_DRAW_FRAMEBUFFER,
                        RenderTargetGL::DrawBufferMask::color);
                    for (const Draw* draw = batch.dstReadList; draw != nullptr;
                         draw = draw->nextDstRead())
                    {
                        assert(draw->blendMode() != BlendMode::srcOver);
                        glutils::BlitFramebuffer(draw->pixelBounds(),
                                                 renderTarget->height());
                    }
                    renderTarget->bindMSAAFramebuffer(this,
                                                      desc.msaaSampleCount);
                }
                m_state->disableBlending(); // Blend in the shader instead.
            }

            // Set up the next clipRect.
            bool needsClipPlanes =
                (shaderFeatures & gpu::ShaderFeatures::ENABLE_CLIP_RECT);
            if (needsClipPlanes != clipPlanesEnabled)
            {
                auto toggleEnableOrDisable =
                    needsClipPlanes ? glEnable : glDisable;
                toggleEnableOrDisable(GL_CLIP_DISTANCE0_EXT);
                toggleEnableOrDisable(GL_CLIP_DISTANCE1_EXT);
                toggleEnableOrDisable(GL_CLIP_DISTANCE2_EXT);
                toggleEnableOrDisable(GL_CLIP_DISTANCE3_EXT);
                clipPlanesEnabled = needsClipPlanes;
            }
        }

        switch (gpu::DrawType drawType = batch.drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::midpointFanCenterAAPatches:
            case DrawType::outerCurvePatches:
            {
                // Draw PLS patches that connect the tessellation vertices.
                m_state->bindVAO(m_drawVAO);
                DrawIndexedInstancedHelper drawHelper(
                    drawProgram.baseInstanceUniformLocation(),
                    GL_TRIANGLES,
                    batch.elementCount,
                    batch.baseElement);

                if (desc.interlockMode != gpu::InterlockMode::msaa)
                {
                    m_plsImpl->ensureRasterOrderingEnabled(
                        this,
                        desc,
                        desc.interlockMode ==
                            gpu::InterlockMode::rasterOrdering);
                    drawHelper.setIndexRange(gpu::PatchIndexCount(drawType),
                                             gpu::PatchBaseIndex(drawType));
                    m_state->setCullFace(GL_BACK);
                    drawHelper.draw(m_capabilities);
                    break;
                }

                // MSAA path draws require different stencil settings, depending
                // on their drawContents.
                bool hasActiveClip =
                    ((batch.drawContents & gpu::DrawContents::activeClip));
                bool isClipUpdate =
                    ((batch.drawContents & gpu::DrawContents::clipUpdate));
                bool isNestedClipUpdate =
                    (batch.drawContents & gpu::kNestedClipUpdateMask) ==
                    gpu::kNestedClipUpdateMask;
                bool isEvenOddFill =
                    (batch.drawContents & gpu::DrawContents::evenOddFill);
                bool isStroke =
                    (batch.drawContents & gpu::DrawContents::stroke);
                if (isStroke)
                {
                    // MSAA strokes only use the "border" section of the patch.
                    // (The depth test prevents double hits.)
                    assert(drawType == gpu::DrawType::midpointFanPatches);
                    drawHelper.setIndexRange(
                        gpu::kMidpointFanPatchBorderIndexCount,
                        gpu::kMidpointFanPatchBaseIndex);
                    m_state->setWriteMasks(true, true, 0xff);
                    m_state->setCullFace(GL_BACK);
                    drawHelper.drawWithStencilSettings(
                        hasActiveClip ? GL_EQUAL : GL_ALWAYS,
                        0x80,
                        0xff,
                        GL_KEEP,
                        GL_KEEP,
                        GL_KEEP,
                        m_capabilities);
                    break;
                }

                // MSAA fills only use the "fan" section of the patch (they
                // don't need AA borders).
                drawHelper.setIndexRange(gpu::PatchFanIndexCount(drawType),
                                         gpu::PatchFanBaseIndex(drawType));

                // "nonZero" fill rules (that aren't nested clip updates) can be
                // optimized to render directly instead of using a "stencil then
                // cover" approach.
                if (!isEvenOddFill && !isNestedClipUpdate)
                {
                    // Count backward triangle hits (negative coverage) in the
                    // stencil buffer.
                    m_state->setWriteMasks(false, false, 0x7f);
                    m_state->setCullFace(GL_FRONT);
                    drawHelper.drawWithStencilSettings(
                        hasActiveClip ? GL_LEQUAL : GL_ALWAYS,
                        0x80,
                        0xff,
                        GL_KEEP,
                        GL_KEEP,
                        GL_INCR_WRAP,
                        m_capabilities);

                    // (Configure both stencil faces before issuing the next
                    // draws, so GL can give them the same internal pipeline
                    // under the hood.)
                    GLuint stencilReadMask = hasActiveClip ? 0xff : 0x7f;
                    glStencilFuncSeparate(GL_FRONT,
                                          GL_EQUAL,
                                          0x80,
                                          stencilReadMask);
                    glStencilOpSeparate(
                        GL_FRONT,
                        GL_DECR, // Don't wrap; 0 must stay 0 outside the clip.
                        GL_KEEP,
                        isClipUpdate ? GL_REPLACE : GL_KEEP);
                    glStencilFuncSeparate(GL_BACK,
                                          GL_LESS,
                                          0x80,
                                          stencilReadMask);
                    glStencilOpSeparate(GL_BACK,
                                        GL_KEEP,
                                        GL_KEEP,
                                        isClipUpdate ? GL_REPLACE : GL_ZERO);
                    m_state->setWriteMasks(!isClipUpdate,
                                           !isClipUpdate,
                                           isClipUpdate ? 0xff : 0x7f);

                    // Draw forward triangles, clipped by the backward triangle
                    // counts. (The depth test prevents double hits.)
                    m_state->setCullFace(GL_BACK);
                    drawHelper.draw(m_capabilities);

                    // Clean up backward triangles in the stencil buffer, (also
                    // filling negative winding numbers for nonZero fill).
                    m_state->setCullFace(GL_FRONT);
                    if (batch.drawContents & gpu::DrawContents::clockwiseFill)
                    {
                        // For clockwise fill, disable color & clip-bit writes
                        // when cleaning up backward triangles. This mode only
                        // fills in forward triangles.
                        m_state->setWriteMasks(false, false, 0x7f);
                    }
                    drawHelper.draw(m_capabilities);
                    break;
                }

                // Fall back on stencil-then-cover.
                glStencilFunc(hasActiveClip ? GL_LEQUAL : GL_ALWAYS,
                              0x80,
                              0xff);
                // Decrement front-facing triangles so the MSB is set when
                // clockwise.
                glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
                glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
                m_state->setWriteMasks(false,
                                       false,
                                       isEvenOddFill ? 0x1 : 0x7f);
                m_state->setCullFace(GL_NONE);
                drawHelper.draw(m_capabilities); // Stencil the path!

                // Nested clip updates do the "cover" operation during
                // DrawType::stencilClipReset.
                if (!isNestedClipUpdate)
                {
                    assert(isEvenOddFill);
                    m_state->setWriteMasks(!isClipUpdate,
                                           !isClipUpdate,
                                           isClipUpdate ? 0xff : 0x1);
                    drawHelper.drawWithStencilSettings(
                        GL_NOTEQUAL, // Cover the path!
                        0x80,
                        0x7f,
                        GL_KEEP,
                        GL_KEEP,
                        isClipUpdate ? GL_REPLACE : GL_ZERO,
                        m_capabilities);
                }
                break;
            }
            case gpu::DrawType::stencilClipReset:
            {
                assert(desc.interlockMode == gpu::InterlockMode::msaa);
                m_state->bindVAO(m_trianglesVAO);
                bool isNestedClipUpdate =
                    (batch.drawContents & gpu::kNestedClipUpdateMask) ==
                    gpu::kNestedClipUpdateMask;
                if (isNestedClipUpdate)
                {
                    // The nested clip just got stencilled and left in the
                    // stencil buffer. Intersect it with the existing clip.
                    // (Erasing regions of the existing clip that are outside
                    // the nested clip.)
                    glStencilFunc(
                        GL_LESS,
                        0x80,
                        (batch.drawContents & gpu::DrawContents::clockwiseFill)
                            // clockwise: (0x80 & 0xc0) < (stencilValue & 0xc0)
                            //   => "If clipbit is set and winding is negative"
                            //   => "If clipbit is set and winding is clockwise"
                            //      (because clockwise decrements)
                            //
                            ? 0xc0
                            // non-clockwise: 0x80 < stencilValue
                            //   => "If clipbit is set and winding is nonzero"
                            : 0xff);
                    glStencilOp(GL_ZERO, GL_KEEP, GL_REPLACE);
                }
                else
                {
                    // Clear the entire previous clip.
                    glStencilFunc(GL_NOTEQUAL, 0, 0xff);
                    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
                }
                m_state->setWriteMasks(false, false, 0xff);
                m_state->setCullFace(GL_BACK);
                glDrawArrays(GL_TRIANGLES,
                             batch.baseElement,
                             batch.elementCount);
                break;
            }
            case gpu::DrawType::interiorTriangulation:
            case gpu::DrawType::atlasBlit:
            {
                if (desc.interlockMode == gpu::InterlockMode::msaa)
                {
                    bool hasActiveClip =
                        ((batch.drawContents & gpu::DrawContents::activeClip));
                    glStencilFunc(hasActiveClip ? GL_EQUAL : GL_ALWAYS,
                                  0x80,
                                  0xff);
                    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
                    m_state->setWriteMasks(true, false, 0x0);
                }
                else if (desc.interlockMode ==
                         gpu::InterlockMode::rasterOrdering)
                {
                    // Disable raster ordering if we're drawing true interior
                    // triangles (not atlas coverage). We know the triangulation
                    // is large enough that it's faster to issue a barrier than
                    // to force raster ordering in the fragment shader.
                    const bool needRasterOrdering =
                        drawType == gpu::DrawType::atlasBlit;
                    m_plsImpl->ensureRasterOrderingEnabled(this,
                                                           desc,
                                                           needRasterOrdering);
                }
                m_state->bindVAO(m_trianglesVAO);
                m_state->setCullFace(GL_BACK);
                glDrawArrays(GL_TRIANGLES,
                             batch.baseElement,
                             batch.elementCount);
                if (desc.interlockMode == gpu::InterlockMode::rasterOrdering &&
                    drawType != gpu::DrawType::atlasBlit)
                {
                    // We turned off raster ordering even though we're in
                    // "rasterOrdering" mode because it improves performance and
                    // we know the interior triangles don't overlap. But now we
                    // have to insert a barrier before we draw anything else.
                    m_plsImpl->barrier(desc);
                }
                break;
            }
            case gpu::DrawType::imageRect:
            {
                assert(desc.interlockMode == gpu::InterlockMode::atomics);
                // m_imageRectVAO should have gotten lazily allocated by now.
                assert(m_imageRectVAO != 0);
                m_plsImpl->ensureRasterOrderingEnabled(this, desc, false);
                m_state->bindVAO(m_imageRectVAO);
                glBindBufferRange(GL_UNIFORM_BUFFER,
                                  IMAGE_DRAW_UNIFORM_BUFFER_IDX,
                                  gl_buffer_id(imageDrawUniformBufferRing()),
                                  batch.imageDrawDataOffset,
                                  sizeof(gpu::ImageDrawUniforms));
                m_state->setCullFace(GL_NONE);
                glDrawElements(GL_TRIANGLES,
                               std::size(gpu::kImageRectIndices),
                               GL_UNSIGNED_SHORT,
                               nullptr);
                break;
            }
            case gpu::DrawType::imageMesh:
            {
                LITE_RTTI_CAST_OR_BREAK(vertexBuffer,
                                        RenderBufferGLImpl*,
                                        batch.vertexBuffer);
                LITE_RTTI_CAST_OR_BREAK(uvBuffer,
                                        RenderBufferGLImpl*,
                                        batch.uvBuffer);
                LITE_RTTI_CAST_OR_BREAK(indexBuffer,
                                        RenderBufferGLImpl*,
                                        batch.indexBuffer);
                m_state->bindVAO(m_imageMeshVAO);
                m_state->bindBuffer(GL_ARRAY_BUFFER,
                                    vertexBuffer->frontBufferID());
                glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
                m_state->bindBuffer(GL_ARRAY_BUFFER, uvBuffer->frontBufferID());
                glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
                m_state->bindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                                    indexBuffer->frontBufferID());
                glBindBufferRange(GL_UNIFORM_BUFFER,
                                  IMAGE_DRAW_UNIFORM_BUFFER_IDX,
                                  gl_buffer_id(imageDrawUniformBufferRing()),
                                  batch.imageDrawDataOffset,
                                  sizeof(gpu::ImageDrawUniforms));
                if (desc.interlockMode != gpu::InterlockMode::msaa)
                {
                    // Try to enable raster ordering for image meshes in
                    // rasterOrdering and atomic mode both; we have no control
                    // over whether the internal geometry has self overlaps.
                    m_plsImpl->ensureRasterOrderingEnabled(
                        this,
                        desc,
                        m_platformFeatures.supportsRasterOrdering);
                }
                else
                {
                    bool hasActiveClip =
                        ((batch.drawContents & gpu::DrawContents::activeClip));
                    glStencilFunc(hasActiveClip ? GL_EQUAL : GL_ALWAYS,
                                  0x80,
                                  0xff);
                    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
                    m_state->setWriteMasks(true, false, 0x00);
                }
                m_state->setCullFace(GL_NONE);
                glDrawElements(GL_TRIANGLES,
                               batch.elementCount,
                               GL_UNSIGNED_SHORT,
                               reinterpret_cast<const void*>(batch.baseElement *
                                                             sizeof(uint16_t)));
                break;
            }
            case gpu::DrawType::atomicResolve:
            {
                assert(desc.interlockMode == gpu::InterlockMode::atomics);
                m_plsImpl->ensureRasterOrderingEnabled(this, desc, false);
                m_state->bindVAO(m_emptyVAO);
                m_plsImpl->setupAtomicResolve(this, desc);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                break;
            }
            case gpu::DrawType::atomicInitialize:
            {
                assert(desc.interlockMode == gpu::InterlockMode::atomics);
                RIVE_UNREACHABLE();
            }
        }
        if (desc.interlockMode != gpu::InterlockMode::msaa &&
            batch.needsBarrier)
        {
            assert(desc.interlockMode == gpu::InterlockMode::atomics);
            m_plsImpl->barrier(desc);
        }
    }

    if (desc.interlockMode != gpu::InterlockMode::msaa)
    {
        m_plsImpl->deactivatePixelLocalStorage(this, desc);
    }
    else
    {
        // Depth/stencil can be discarded.
        glInvalidateFramebuffer(GL_FRAMEBUFFER,
                                2,
                                msaaDepthStencilColor.data());
        if (msaaResolveAction ==
            RenderTargetGL::MSAAResolveAction::framebufferBlit)
        {
            renderTarget->bindDestinationFramebuffer(GL_DRAW_FRAMEBUFFER);
            glutils::BlitFramebuffer(desc.renderTargetUpdateBounds,
                                     renderTarget->height(),
                                     GL_COLOR_BUFFER_BIT);
            // Now that color is resolved elsewhere we can discard the MSAA
            // color buffer as well.
            glInvalidateFramebuffer(GL_READ_FRAMEBUFFER,
                                    1,
                                    msaaDepthStencilColor.data() + 2);
        }

        if ((desc.combinedShaderFeatures &
             gpu::ShaderFeatures::ENABLE_ADVANCED_BLEND) &&
            m_capabilities.KHR_blend_equation_advanced_coherent)
        {
            glDisable(GL_BLEND_ADVANCED_COHERENT_KHR);
        }
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);
        if (clipPlanesEnabled)
        {
            glDisable(GL_CLIP_DISTANCE0_EXT);
            glDisable(GL_CLIP_DISTANCE1_EXT);
            glDisable(GL_CLIP_DISTANCE2_EXT);
            glDisable(GL_CLIP_DISTANCE3_EXT);
        }
    }

#ifdef RIVE_DESKTOP_GL
    if (m_capabilities.ANGLE_polygon_mode && desc.wireframe)
    {
        glPolygonModeANGLE(GL_FRONT_AND_BACK, GL_FILL_ANGLE);
    }
#endif

    if (m_capabilities.isAdreno)
    {
        // Qualcomm experiences synchronization issues with multiple flushes per
        // frame if we don't call glFlush in between.
        glFlush();
    }
}

void RenderContextGLImpl::blitTextureToFramebufferAsDraw(
    GLuint textureID,
    const IAABB& bounds,
    uint32_t renderTargetHeight)
{
    if (m_blitAsDrawProgram == 0)
    {
        // Define "USE_TEXEL_FETCH_WITH_FRAG_COORD" so the shader uses
        // texelFetch() on the source texture instead of sampling it. This way
        // we don't have to configure the texture's sampling parameters, and
        // since textureID potentially refers to an external texture, it would
        // be very messy to try and change its sampling params.
        const char* blitDefines[] = {GLSL_USE_TEXEL_FETCH_WITH_FRAG_COORD};
        const char* blitSources[] = {glsl::constants,
                                     glsl::blit_texture_as_draw};
        m_blitAsDrawProgram = glutils::Program();
        m_blitAsDrawProgram.compileAndAttachShader(GL_VERTEX_SHADER,
                                                   blitDefines,
                                                   std::size(blitDefines),
                                                   blitSources,
                                                   std::size(blitSources),
                                                   m_capabilities);
        m_blitAsDrawProgram.compileAndAttachShader(GL_FRAGMENT_SHADER,
                                                   blitDefines,
                                                   std::size(blitDefines),
                                                   blitSources,
                                                   std::size(blitSources),
                                                   m_capabilities);
        m_blitAsDrawProgram.link();
        m_state->bindProgram(m_blitAsDrawProgram);
        glutils::Uniform1iByName(m_blitAsDrawProgram, GLSL_sourceTexture, 0);
    }

    m_state->bindProgram(m_blitAsDrawProgram);
    m_state->bindVAO(m_emptyVAO);
    m_state->setWriteMasks(true, true, 0xff);
    m_state->disableBlending();
    m_state->setCullFace(GL_NONE);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glEnable(GL_SCISSOR_TEST);
    glScissor(bounds.left,
              renderTargetHeight - bounds.bottom,
              bounds.width(),
              bounds.height());
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisable(GL_SCISSOR_TEST);
}

std::unique_ptr<RenderContext> RenderContextGLImpl::MakeContext(
    const ContextOptions& contextOptions)
{
    GLCapabilities capabilities{};

    const char* glVersionStr = (const char*)glGetString(GL_VERSION);
#ifdef RIVE_WEBGL
    capabilities.isGLES = true;
#else
    capabilities.isGLES = strstr(glVersionStr, "OpenGL ES") != NULL;
#endif
    if (capabilities.isGLES)
    {
#ifdef RIVE_ANDROID
        capabilities.isAndroidANGLE = strstr(glVersionStr, "ANGLE") != NULL;
#endif
#ifdef _MSC_VER
        sscanf_s(
#else
        sscanf(
#endif
            glVersionStr,
            "OpenGL ES %d.%d",
            &capabilities.contextVersionMajor,
            &capabilities.contextVersionMinor);
    }
    else
    {
#ifdef _MSC_VER
        sscanf_s(
#else
        sscanf(
#endif
            glVersionStr,
            "%d.%d",
            &capabilities.contextVersionMajor,
            &capabilities.contextVersionMinor);
    }
#ifdef RIVE_DESKTOP_GL
    assert(capabilities.contextVersionMajor == GLAD_GL_version_major);
    assert(capabilities.contextVersionMinor == GLAD_GL_version_minor);
    assert(capabilities.isGLES == static_cast<bool>(GLAD_GL_version_es));
#endif

    if (capabilities.isGLES)
    {
        if (!capabilities.isContextVersionAtLeast(3, 0))
        {
            fprintf(stderr,
                    "OpenGL ES %i.%i not supported. Minimum supported version "
                    "is 3.0.\n",
                    capabilities.contextVersionMajor,
                    capabilities.contextVersionMinor);
            return nullptr;
        }
    }
    else
    {
        if (!capabilities.isContextVersionAtLeast(4, 2))
        {
            fprintf(stderr,
                    "OpenGL %i.%i not supported. Minimum supported version is "
                    "4.2.\n",
                    capabilities.contextVersionMajor,
                    capabilities.contextVersionMinor);
            return nullptr;
        }
    }

    GLenum rendererToken = GL_RENDERER;
#ifdef RIVE_WEBGL
    if (emscripten_webgl_enable_extension(
            emscripten_webgl_get_current_context(),
            "WEBGL_debug_renderer_info"))
    {
        rendererToken = GL_UNMASKED_RENDERER_WEBGL;
    }
#endif
    const char* rendererString =
        reinterpret_cast<const char*>(glGetString(rendererToken));
    capabilities.isAdreno = strstr(rendererString, "Adreno");
    capabilities.isMali = strstr(rendererString, "Mali");
    capabilities.isPowerVR = strstr(rendererString, "PowerVR");
    if (capabilities.isMali || capabilities.isPowerVR)
    {
        // We have observed crashes on Mali-G71 when issuing instanced draws
        // with somewhere between 2^15 and 2^16 instances.
        //
        // Skia also reports crashes on PowerVR when drawing somewhere between
        // 2^14 and 2^15 instances.
        //
        // Limit the maximum number of instances we issue per-draw-call on these
        // devices to a safe value, far below the observed crash thresholds.
        capabilities.maxSupportedInstancesPerDrawCommand = 999;
    }
    else
    {
        capabilities.maxSupportedInstancesPerDrawCommand = ~0u;
    }

    // Our baseline feature set is GLES 3.0. Capabilities from newer context
    // versions are reported as extensions.
    if (capabilities.isGLES)
    {
        if (capabilities.isContextVersionAtLeast(3, 1))
        {
            capabilities.ARB_shader_storage_buffer_object = true;
        }
    }
    else
    {
        if (capabilities.isContextVersionAtLeast(4, 2))
        {
            capabilities.ARB_shader_image_load_store = true;
        }
        if (capabilities.isContextVersionAtLeast(4, 3))
        {
            capabilities.ARB_shader_storage_buffer_object = true;
        }
        capabilities.EXT_clip_cull_distance = true;
    }

#ifndef RIVE_WEBGL
    GLint extensionCount;
    glGetIntegerv(GL_NUM_EXTENSIONS, &extensionCount);
    for (int i = 0; i < extensionCount; ++i)
    {
        auto* ext =
            reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i));
        if (strcmp(ext, "GL_ANGLE_base_vertex_base_instance_shader_builtin") ==
            0)
        {
            capabilities.ANGLE_base_vertex_base_instance_shader_builtin = true;
        }
        else if (strcmp(ext, "GL_ANGLE_shader_pixel_local_storage") == 0)
        {
            capabilities.ANGLE_shader_pixel_local_storage = true;
        }
        else if (strcmp(ext, "GL_ANGLE_shader_pixel_local_storage_coherent") ==
                 0)
        {
            capabilities.ANGLE_shader_pixel_local_storage_coherent = true;
        }
        else if (strcmp(ext, "GL_ANGLE_provoking_vertex") == 0)
        {
            capabilities.ANGLE_provoking_vertex = true;
        }
        else if (strcmp(ext, "GL_ANGLE_polygon_mode") == 0)
        {
            capabilities.ANGLE_polygon_mode = true;
        }
        else if (strcmp(ext, "GL_ARM_shader_framebuffer_fetch") == 0)
        {
            capabilities.ARM_shader_framebuffer_fetch = true;
        }
        else if (strcmp(ext, "GL_ARB_fragment_shader_interlock") == 0)
        {
            capabilities.ARB_fragment_shader_interlock = true;
        }
        else if (strcmp(ext, "GL_ARB_shader_image_load_store") == 0)
        {
            capabilities.ARB_shader_image_load_store = true;
        }
        else if (strcmp(ext, "GL_ARB_shader_storage_buffer_object") == 0)
        {
            capabilities.ARB_shader_storage_buffer_object = true;
        }
        else if (strcmp(ext, "GL_KHR_blend_equation_advanced") == 0)
        {
            capabilities.KHR_blend_equation_advanced = true;
        }
        else if (strcmp(ext, "GL_KHR_blend_equation_advanced_coherent") == 0)
        {
            capabilities.KHR_blend_equation_advanced_coherent = true;
        }
        else if (strcmp(ext, "GL_EXT_base_instance") == 0)
        {
            capabilities.EXT_base_instance = true;
        }
        // Don't use EXT_clip_cull_distance if we're on ANGLE. Galaxy S22
        // (OpenGL Samsung Electronics Co., Ltd.;
        // ANGLE (Samsung Xclipse 920) on Vulkan 1.1.179;
        // OpenGL ES 3.2 ANGLE git hash: c7c78c41d520) advertises support for
        // this extension but then doesn't support gl_ClipDistance in the
        // shader. Only use clip planes on ANGLE if ANGLE_clip_cull_distance is
        // supported.
        else if (!capabilities.isAndroidANGLE &&
                 strcmp(ext, "GL_EXT_clip_cull_distance") == 0)
        {
            capabilities.EXT_clip_cull_distance = true;
        }
        else if (strcmp(ext, "GL_EXT_multisampled_render_to_texture") == 0)
        {
            capabilities.EXT_multisampled_render_to_texture = true;
        }
        else if (strcmp(ext, "GL_ANGLE_clip_cull_distance") == 0)
        {
            capabilities.EXT_clip_cull_distance = true;
        }
        else if (strcmp(ext, "GL_INTEL_fragment_shader_ordering") == 0)
        {
            capabilities.INTEL_fragment_shader_ordering = true;
        }
        else if (strcmp(ext, "GL_EXT_color_buffer_half_float") == 0)
        {
            capabilities.EXT_color_buffer_half_float = true;
        }
        else if (strcmp(ext, "GL_EXT_float_blend") == 0)
        {
            capabilities.EXT_float_blend = true;
        }
        else if (strcmp(ext, "GL_ARB_color_buffer_float") == 0)
        {
            capabilities.EXT_color_buffer_half_float = true;
            capabilities.EXT_float_blend = true;
        }
        else if (strcmp(ext, "GL_EXT_shader_framebuffer_fetch") == 0)
        {
            capabilities.EXT_shader_framebuffer_fetch = true;
        }
        else if (strcmp(ext, "GL_EXT_shader_pixel_local_storage") == 0)
        {
            capabilities.EXT_shader_pixel_local_storage = true;
        }
        else if (strcmp(ext, "GL_QCOM_shader_framebuffer_fetch_noncoherent") ==
                 0)
        {
            capabilities.QCOM_shader_framebuffer_fetch_noncoherent = true;
        }
    }
#else  // !RIVE_WEBGL -> RIVE_WEBGL
    if (webgl_enable_WEBGL_shader_pixel_local_storage_coherent())
    {
        capabilities.ANGLE_shader_pixel_local_storage = true;
        capabilities.ANGLE_shader_pixel_local_storage_coherent = true;
    }
    if (webgl_enable_WEBGL_provoking_vertex())
    {
        capabilities.ANGLE_provoking_vertex = true;
    }
    if (emscripten_webgl_enable_extension(
            emscripten_webgl_get_current_context(),
            "WEBGL_clip_cull_distance"))
    {
        capabilities.EXT_clip_cull_distance = true;
    }
    if (emscripten_webgl_enable_extension(
            emscripten_webgl_get_current_context(),
            "EXT_color_buffer_half_float"))
    {
        capabilities.EXT_color_buffer_half_float = true;
    }
    if (emscripten_webgl_enable_extension(
            emscripten_webgl_get_current_context(),
            "EXT_color_buffer_float") &&
        emscripten_webgl_enable_extension(
            emscripten_webgl_get_current_context(),
            "EXT_float_blend"))
    {
        capabilities.EXT_float_blend = true;
    }
#endif // RIVE_WEBGL

#ifdef RIVE_DESKTOP_GL
    if (GLAD_GL_ANGLE_base_vertex_base_instance_shader_builtin)
    {
        capabilities.ANGLE_base_vertex_base_instance_shader_builtin = true;
    }
    if (GLAD_GL_ANGLE_polygon_mode)
    {
        capabilities.ANGLE_polygon_mode = true;
    }
    if (GLAD_GL_EXT_base_instance)
    {
        capabilities.EXT_base_instance = true;
    }
#endif

    if (capabilities.ARB_shader_storage_buffer_object)
    {
        // We need four storage buffers in the vertex shader. Disable the
        // extension if this isn't supported.
        int maxVertexShaderStorageBlocks;
        glGetIntegerv(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS,
                      &maxVertexShaderStorageBlocks);
        if (maxVertexShaderStorageBlocks < gpu::kMaxStorageBuffers)
        {
            capabilities.ARB_shader_storage_buffer_object = false;
        }
    }

    if (capabilities.ANGLE_shader_pixel_local_storage ||
        capabilities.ANGLE_shader_pixel_local_storage_coherent)
    {
        // ANGLE_shader_pixel_local_storage enum values had a breaking change in
        // early 2025. Disable the extension if we can't verify that we're
        // running on the latest spec.
        if (!glutils::validate_pixel_local_storage_angle())
        {
            fprintf(stderr,
                    "WARNING: detected an old version of "
                    "ANGLE_shader_pixel_local_storage. Disabling the "
                    "extension. Please update your drivers.\n");
            capabilities.ANGLE_shader_pixel_local_storage =
                capabilities.ANGLE_shader_pixel_local_storage_coherent = false;
        }
    }

    if (contextOptions.disableFragmentShaderInterlock)
    {
        // Disable the extensions we don't want to use internally.
        capabilities.ARB_fragment_shader_interlock = false;
        capabilities.INTEL_fragment_shader_ordering = false;
    }

    if (strstr(rendererString, "Metal") != nullptr ||
        strstr(rendererString, "Direct3D") != nullptr)
    {
        // Disable ANGLE_base_vertex_base_instance_shader_builtin on ANGLE/D3D
        // and ANGLE/Metal.
        // The extension is polyfilled on D3D anyway, and on Metal it crashes.
        capabilities.ANGLE_base_vertex_base_instance_shader_builtin = false;
    }

    if (strstr(rendererString, "Direct3D") != nullptr)
    {
        // Our use of EXT_multisampled_render_to_texture causes a segfault in
        // the Microsoft WARP (software) renderer. Just don't use this extension
        // on D3D since it's polyfilled anyway.
        capabilities.EXT_multisampled_render_to_texture = false;
    }
#ifdef RIVE_ANDROID
    // Android doesn't load extension functions for us.
    LoadGLESExtensions(capabilities);
#endif

    if (!contextOptions.disablePixelLocalStorage)
    {
#ifdef RIVE_ANDROID
        if (capabilities.EXT_shader_pixel_local_storage &&
            (capabilities.ARM_shader_framebuffer_fetch ||
             capabilities.EXT_shader_framebuffer_fetch))
        {
            return MakeContext(rendererString,
                               capabilities,
                               MakePLSImplEXTNative(capabilities));
        }
#else
        if (capabilities.ANGLE_shader_pixel_local_storage_coherent)
        {
            // EXT_shader_framebuffer_fetch is costly on Qualcomm, with or
            // without the "noncoherent" extension. Use MSAA on Adreno.
            if (!capabilities.isAdreno)
            {
                return MakeContext(rendererString,
                                   capabilities,
                                   MakePLSImplWebGL());
            }
        }
#endif

#ifdef RIVE_DESKTOP_GL
        if (capabilities.ARB_shader_image_load_store)
        {
            return MakeContext(rendererString,
                               capabilities,
                               MakePLSImplRWTexture());
        }
#endif
    }

    return MakeContext(rendererString, capabilities, nullptr);
}

std::unique_ptr<RenderContext> RenderContextGLImpl::MakeContext(
    const char* rendererString,
    GLCapabilities capabilities,
    std::unique_ptr<PixelLocalStorageImpl> plsImpl)
{
    auto renderContextImpl = std::unique_ptr<RenderContextGLImpl>(
        new RenderContextGLImpl(rendererString,
                                capabilities,
                                std::move(plsImpl)));
    return std::make_unique<RenderContext>(std::move(renderContextImpl));
}
} // namespace rive::gpu
