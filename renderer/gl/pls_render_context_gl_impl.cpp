/*
 * Copyright 2022 Rive
 */

#include "rive/pls/gl/pls_render_context_gl_impl.hpp"

#include "gl_utils.hpp"
#include "rive/pls/gl/pls_render_buffer_gl_impl.hpp"
#include "rive/pls/gl/pls_render_target_gl.hpp"
#include "rive/pls/pls_image.hpp"
#include "shaders/constants.glsl"

#include "shaders/out/generated/advanced_blend.glsl.hpp"
#include "shaders/out/generated/color_ramp.glsl.hpp"
#include "shaders/out/generated/constants.glsl.hpp"
#include "shaders/out/generated/common.glsl.hpp"
#include "shaders/out/generated/draw_path_common.glsl.hpp"
#include "shaders/out/generated/draw_path.glsl.hpp"
#include "shaders/out/generated/draw_image_mesh.glsl.hpp"
#include "shaders/out/generated/tessellate.glsl.hpp"

#if defined(RIVE_GLES) || defined(RIVE_WEBGL)
// In an effort to save space on Android, and since GLES doesn't usually need atomic mode, don't
// include the atomic sources.
namespace rive::pls::glsl
{
const char atomic_draw[] = "";
}
#else
#include "shaders/out/generated/atomic_draw.glsl.hpp"
#define ENABLE_PLS_EXPERIMENTAL_ATOMICS
#endif

// Offset all PLS texture indices by 1 so we, and others who share our GL context, can use
// GL_TEXTURE0 as a scratch texture index.
constexpr static int kPLSTexIdxOffset = 1;

namespace rive::pls
{
PLSRenderContextGLImpl::PLSRenderContextGLImpl(const char* rendererString,
                                               GLCapabilities capabilities,
                                               std::unique_ptr<PLSImpl> plsImpl) :
    m_capabilities(capabilities),
    m_plsImpl(std::move(plsImpl)),
    m_state(make_rcp<GLState>(m_capabilities))

{
    if (strstr(rendererString, "Apple") && strstr(rendererString, "Metal"))
    {
        // In Metal, non-flat varyings preserve their exact value if all vertices in the triangle
        // emit the same value, and we also see a small (5-10%) improvement from not using flat
        // varyings.
        m_platformFeatures.avoidFlatVaryings = true;
    }
    m_platformFeatures.fragCoordBottomUp = true;
    if (m_capabilities.ARB_bindless_texture)
    {
        m_platformFeatures.supportsBindlessTextures = true;
    }
    m_platformFeatures.supportsRasterOrdering = m_plsImpl->supportsRasterOrdering(m_capabilities);

    std::vector<const char*> generalDefines;
    if (!m_capabilities.ARB_shader_storage_buffer_object)
    {
        generalDefines.push_back(GLSL_DISABLE_SHADER_STORAGE_BUFFERS);
    }

    m_colorRampProgram = glCreateProgram();
    const char* colorRampSources[] = {glsl::constants, glsl::common, glsl::color_ramp};
    glutils::CompileAndAttachShader(m_colorRampProgram,
                                    GL_VERTEX_SHADER,
                                    generalDefines.data(),
                                    generalDefines.size(),
                                    colorRampSources,
                                    std::size(colorRampSources),
                                    m_capabilities);
    glutils::CompileAndAttachShader(m_colorRampProgram,
                                    GL_FRAGMENT_SHADER,
                                    generalDefines.data(),
                                    generalDefines.size(),
                                    colorRampSources,
                                    std::size(colorRampSources),
                                    m_capabilities);
    glutils::LinkProgram(m_colorRampProgram);
    glUniformBlockBinding(m_colorRampProgram,
                          glGetUniformBlockIndex(m_colorRampProgram, GLSL_FlushUniforms),
                          FLUSH_UNIFORM_BUFFER_IDX);

    glGenVertexArrays(1, &m_colorRampVAO);
    m_state->bindVAO(m_colorRampVAO);
    glEnableVertexAttribArray(0);
    glVertexAttribDivisor(0, 1);

    glGenFramebuffers(1, &m_colorRampFBO);

    m_tessellateProgram = glCreateProgram();
    const char* tessellateSources[] = {glsl::constants, glsl::common, glsl::tessellate};
    glutils::CompileAndAttachShader(m_tessellateProgram,
                                    GL_VERTEX_SHADER,
                                    generalDefines.data(),
                                    generalDefines.size(),
                                    tessellateSources,
                                    std::size(tessellateSources),
                                    m_capabilities);
    glutils::CompileAndAttachShader(m_tessellateProgram,
                                    GL_FRAGMENT_SHADER,
                                    generalDefines.data(),
                                    generalDefines.size(),
                                    tessellateSources,
                                    std::size(tessellateSources),
                                    m_capabilities);
    glutils::LinkProgram(m_tessellateProgram);
    m_state->bindProgram(m_tessellateProgram);
    glUniformBlockBinding(m_tessellateProgram,
                          glGetUniformBlockIndex(m_tessellateProgram, GLSL_FlushUniforms),
                          FLUSH_UNIFORM_BUFFER_IDX);
    if (!m_capabilities.ARB_shader_storage_buffer_object)
    {
        // Our GL driver doesn't support storage buffers. We polyfill these buffers as textures.
        glUniform1i(glGetUniformLocation(m_tessellateProgram, GLSL_pathBuffer),
                    kPLSTexIdxOffset + PATH_BUFFER_IDX);
        glUniform1i(glGetUniformLocation(m_tessellateProgram, GLSL_contourBuffer),
                    kPLSTexIdxOffset + CONTOUR_BUFFER_IDX);
    }

    glGenVertexArrays(1, &m_tessellateVAO);
    m_state->bindVAO(m_tessellateVAO);
    for (int i = 0; i < 4; ++i)
    {
        glEnableVertexAttribArray(i);
        // Draw two instances per TessVertexSpan: one normal and one optional reflection.
        glVertexAttribDivisor(i, 1);
    }

    glGenBuffers(1, &m_tessSpanIndexBuffer);
    m_state->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_tessSpanIndexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof(pls::kTessSpanIndices),
                 pls::kTessSpanIndices,
                 GL_STATIC_DRAW);

    glGenFramebuffers(1, &m_tessellateFBO);

    glGenVertexArrays(1, &m_drawVAO);
    m_state->bindVAO(m_drawVAO);

    PatchVertex patchVertices[kPatchVertexBufferCount];
    uint16_t patchIndices[kPatchIndexBufferCount];
    GeneratePatchBufferData(patchVertices, patchIndices);

    glGenBuffers(1, &m_patchVerticesBuffer);
    m_state->bindBuffer(GL_ARRAY_BUFFER, m_patchVerticesBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(patchVertices), patchVertices, GL_STATIC_DRAW);

    glGenBuffers(1, &m_patchIndicesBuffer);
    m_state->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_patchIndicesBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(patchIndices), patchIndices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(PatchVertex), nullptr);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,
                          4,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(PatchVertex),
                          reinterpret_cast<const void*>(sizeof(float) * 4));

    glGenVertexArrays(1, &m_interiorTrianglesVAO);
    m_state->bindVAO(m_interiorTrianglesVAO);
    glEnableVertexAttribArray(0);

    if (!m_capabilities.ARB_bindless_texture)
    {
        // We only have to draw imageRects when in atomic mode and bindless textures are not
        // supported.
        glGenVertexArrays(1, &m_imageRectVAO);
        m_state->bindVAO(m_imageRectVAO);

        glGenBuffers(1, &m_imageRectVertexBuffer);
        m_state->bindBuffer(GL_ARRAY_BUFFER, m_imageRectVertexBuffer);
        glBufferData(GL_ARRAY_BUFFER,
                     sizeof(pls::kImageRectVertices),
                     pls::kImageRectVertices,
                     GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(pls::ImageRectVertex), nullptr);

        glGenBuffers(1, &m_imageRectIndexBuffer);
        m_state->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_imageRectIndexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     sizeof(pls::kImageRectIndices),
                     pls::kImageRectIndices,
                     GL_STATIC_DRAW);
    }

    glGenVertexArrays(1, &m_plsResolveVAO);

    glGenVertexArrays(1, &m_imageMeshVAO);
    m_state->bindVAO(m_imageMeshVAO);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    m_plsImpl->init(m_state);
}

PLSRenderContextGLImpl::~PLSRenderContextGLImpl()
{
    m_state->deleteProgram(m_colorRampProgram);
    m_state->deleteVAO(m_colorRampVAO);
    glDeleteFramebuffers(1, &m_colorRampFBO);
    glDeleteTextures(1, &m_gradientTexture);

    m_state->deleteProgram(m_tessellateProgram);
    m_state->deleteVAO(m_tessellateVAO);
    m_state->deleteBuffer(m_tessSpanIndexBuffer);
    glDeleteFramebuffers(1, &m_tessellateFBO);
    glDeleteTextures(1, &m_tessVertexTexture);

    m_state->deleteVAO(m_drawVAO);
    m_state->deleteBuffer(m_patchVerticesBuffer);
    m_state->deleteBuffer(m_patchIndicesBuffer);

    m_state->deleteVAO(m_interiorTrianglesVAO);

    m_state->deleteVAO(m_imageRectVAO);
    m_state->deleteBuffer(m_imageRectVertexBuffer);
    m_state->deleteBuffer(m_imageRectIndexBuffer);

    m_state->deleteVAO(m_imageMeshVAO);
    m_state->deleteVAO(m_plsResolveVAO);
}

void PLSRenderContextGLImpl::invalidateGLState()
{
    glActiveTexture(GL_TEXTURE0 + kPLSTexIdxOffset + TESS_VERTEX_TEXTURE_IDX);
    glBindTexture(GL_TEXTURE_2D, m_tessVertexTexture);

    glActiveTexture(GL_TEXTURE0 + kPLSTexIdxOffset + GRAD_TEXTURE_IDX);
    glBindTexture(GL_TEXTURE_2D, m_gradientTexture);

    m_state->invalidate(m_capabilities);
}

void PLSRenderContextGLImpl::unbindGLInternalResources()
{
    m_state->bindVAO(0);
    m_state->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    m_state->bindBuffer(GL_ARRAY_BUFFER, 0);
    m_state->bindBuffer(GL_UNIFORM_BUFFER, 0);
    m_state->bindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    for (int i = 0; i <= CONTOUR_BUFFER_IDX; ++i)
    {
        glActiveTexture(GL_TEXTURE0 + kPLSTexIdxOffset + i);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

rcp<RenderBuffer> PLSRenderContextGLImpl::makeRenderBuffer(RenderBufferType type,
                                                           RenderBufferFlags flags,
                                                           size_t sizeInBytes)
{
    return make_rcp<PLSRenderBufferGLImpl>(type, flags, sizeInBytes, m_state);
}

class PLSTextureGLImpl : public PLSTexture
{
public:
    PLSTextureGLImpl(uint32_t width,
                     uint32_t height,
                     uint32_t mipLevelCount,
                     const uint8_t imageDataRGBA[],
                     const GLCapabilities& capabilities,
                     rcp<GLState> state) :
        PLSTexture(width, height), m_state(std::move(state))
    {
        glGenTextures(1, &m_id);
        glActiveTexture(GL_TEXTURE0 + kPLSTexIdxOffset + IMAGE_TEXTURE_IDX);
        glBindTexture(GL_TEXTURE_2D, m_id);
        glTexStorage2D(GL_TEXTURE_2D, mipLevelCount, GL_RGBA8, width, height);
        m_state->bindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        0,
                        0,
                        width,
                        height,
                        GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        imageDataRGBA);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenerateMipmap(GL_TEXTURE_2D);

#ifdef RIVE_DESKTOP_GL
        if (capabilities.ARB_bindless_texture)
        {
            m_bindlessTextureHandle = glGetTextureHandleARB(m_id);
            glMakeTextureHandleResidentARB(m_bindlessTextureHandle);
        }
#endif
    }

    ~PLSTextureGLImpl() override
    {
#ifdef RIVE_DESKTOP_GL
        if (m_bindlessTextureHandle != 0)
        {
            glMakeTextureHandleNonResidentARB(m_bindlessTextureHandle);
        }
#else
        assert(m_bindlessTextureHandle == 0);
#endif
    }

    GLuint id() const { return m_id; }

private:
    const rcp<GLState> m_state;
    GLuint m_id = 0;
};

rcp<PLSTexture> PLSRenderContextGLImpl::makeImageTexture(uint32_t width,
                                                         uint32_t height,
                                                         uint32_t mipLevelCount,
                                                         const uint8_t imageDataRGBA[])
{
    return make_rcp<PLSTextureGLImpl>(width,
                                      height,
                                      mipLevelCount,
                                      imageDataRGBA,
                                      m_capabilities,
                                      m_state);
}

// BufferRingImpl in GL on a given buffer target. In order to support WebGL2, we don't do hardware
// mapping.
class BufferRingGLImpl : public BufferRing
{
public:
    static std::unique_ptr<BufferRingGLImpl> Make(size_t capacityInBytes,
                                                  GLenum target,
                                                  rcp<GLState> state)
    {
        return capacityInBytes != 0
                   ? std::unique_ptr<BufferRingGLImpl>(
                         new BufferRingGLImpl(target, capacityInBytes, std::move(state)))
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
    BufferRingGLImpl(GLenum target, size_t capacityInBytes, rcp<GLState> state) :
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
                                GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT |
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
static GLenum storage_texture_internalformat(pls::StorageBufferStructure bufferStructure)
{
    switch (bufferStructure)
    {
        case pls::StorageBufferStructure::uint32x4:
            return GL_RGBA32UI;
        case pls::StorageBufferStructure::uint32x2:
            return GL_RG32UI;
        case pls::StorageBufferStructure::float32x4:
            return GL_RGBA32F;
    }
    RIVE_UNREACHABLE();
}

// GL format to use for a texture that polyfills a storage buffer.
static GLenum storage_texture_format(pls::StorageBufferStructure bufferStructure)
{
    switch (bufferStructure)
    {
        case pls::StorageBufferStructure::uint32x4:
            return GL_RGBA_INTEGER;
        case pls::StorageBufferStructure::uint32x2:
            return GL_RG_INTEGER;
        case pls::StorageBufferStructure::float32x4:
            return GL_RGBA;
    }
    RIVE_UNREACHABLE();
}

// GL type to use for a texture that polyfills a storage buffer.
static GLenum storage_texture_type(pls::StorageBufferStructure bufferStructure)
{
    switch (bufferStructure)
    {
        case pls::StorageBufferStructure::uint32x4:
            return GL_UNSIGNED_INT;
        case pls::StorageBufferStructure::uint32x2:
            return GL_UNSIGNED_INT;
        case pls::StorageBufferStructure::float32x4:
            return GL_FLOAT;
    }
    RIVE_UNREACHABLE();
}

class StorageBufferRingGLImpl : public BufferRingGLImpl
{
public:
    StorageBufferRingGLImpl(const GLCapabilities& capabilities,
                            size_t capacityInBytes,
                            pls::StorageBufferStructure bufferStructure,
                            rcp<GLState> state) :
        BufferRingGLImpl(
            // If we don't support storage buffers, instead make a pixel-unpack buffer that
            // will be used to copy data into the polyfill texture.
            capabilities.ARB_shader_storage_buffer_object ? GL_SHADER_STORAGE_BUFFER
                                                          : GL_PIXEL_UNPACK_BUFFER,
            capabilities.ARB_shader_storage_buffer_object
                ? capacityInBytes
                : pls::StorageTextureBufferSize(capacityInBytes, bufferStructure),
            std::move(state)),
        m_bufferStructure(bufferStructure)
    {
        if (!capabilities.ARB_shader_storage_buffer_object)
        {
            // Our GL driver doesn't support storage buffers. Allocate a polyfill texture.
            auto [width, height] = pls::StorageTextureSize(capacityInBytes, m_bufferStructure);
            glGenTextures(1, &m_polyfillTexture);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_polyfillTexture);
            glTexStorage2D(GL_TEXTURE_2D,
                           1,
                           storage_texture_internalformat(m_bufferStructure),
                           width,
                           height);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    ~StorageBufferRingGLImpl()
    {
        if (m_polyfillTexture != 0)
        {
            glDeleteTextures(1, &m_polyfillTexture);
        }
    }

    void bindToRenderContext(const GLCapabilities& capabilities,
                             uint32_t bindingIdx,
                             size_t bindingSizeInBytes,
                             size_t offsetSizeInBytes) const
    {
        if (capabilities.ARB_shader_storage_buffer_object)
        {
            assert(m_target == GL_SHADER_STORAGE_BUFFER);
            glBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                              bindingIdx,
                              submittedBufferID(),
                              offsetSizeInBytes,
                              bindingSizeInBytes);
        }
        else
        {
            // Our GL driver doesn't support storage buffers. Copy the buffer to a texture.
            assert(m_target == GL_PIXEL_UNPACK_BUFFER);
            auto [updateWidth, updateHeight] =
                pls::StorageTextureSize(bindingSizeInBytes, m_bufferStructure);
            m_state->bindBuffer(GL_PIXEL_UNPACK_BUFFER, submittedBufferID());
            glActiveTexture(GL_TEXTURE0 + kPLSTexIdxOffset + bindingIdx);
            glBindTexture(GL_TEXTURE_2D, m_polyfillTexture);
            glTexSubImage2D(GL_TEXTURE_2D,
                            0,
                            0,
                            0,
                            updateWidth,
                            updateHeight,
                            storage_texture_format(m_bufferStructure),
                            storage_texture_type(m_bufferStructure),
                            reinterpret_cast<const void*>(offsetSizeInBytes));
        }
    }

protected:
    const pls::StorageBufferStructure m_bufferStructure;
    GLuint m_polyfillTexture = 0;
};

std::unique_ptr<BufferRing> PLSRenderContextGLImpl::makeUniformBufferRing(size_t capacityInBytes)
{
    return BufferRingGLImpl::Make(capacityInBytes, GL_UNIFORM_BUFFER, m_state);
}

std::unique_ptr<BufferRing> PLSRenderContextGLImpl::makeStorageBufferRing(
    size_t capacityInBytes,
    pls::StorageBufferStructure bufferStructure)
{
    return capacityInBytes != 0 ? std::make_unique<StorageBufferRingGLImpl>(m_capabilities,
                                                                            capacityInBytes,
                                                                            bufferStructure,
                                                                            m_state)
                                : nullptr;
}

std::unique_ptr<BufferRing> PLSRenderContextGLImpl::makeVertexBufferRing(size_t capacityInBytes)
{
    return BufferRingGLImpl::Make(capacityInBytes, GL_ARRAY_BUFFER, m_state);
}

std::unique_ptr<BufferRing> PLSRenderContextGLImpl::makeTextureTransferBufferRing(
    size_t capacityInBytes)
{
    return BufferRingGLImpl::Make(capacityInBytes, GL_PIXEL_UNPACK_BUFFER, m_state);
}

void PLSRenderContextGLImpl::resizeGradientTexture(uint32_t width, uint32_t height)
{
    glDeleteTextures(1, &m_gradientTexture);
    if (width == 0 || height == 0)
    {
        m_gradientTexture = 0;
        return;
    }

    glGenTextures(1, &m_gradientTexture);
    glActiveTexture(GL_TEXTURE0 + kPLSTexIdxOffset + GRAD_TEXTURE_IDX);
    glBindTexture(GL_TEXTURE_2D, m_gradientTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, m_colorRampFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           m_gradientTexture,
                           0);
}

void PLSRenderContextGLImpl::resizeTessellationTexture(uint32_t width, uint32_t height)
{
    glDeleteTextures(1, &m_tessVertexTexture);
    if (width == 0 || height == 0)
    {
        m_tessVertexTexture = 0;
        return;
    }

    glGenTextures(1, &m_tessVertexTexture);
    glActiveTexture(GL_TEXTURE0 + kPLSTexIdxOffset + TESS_VERTEX_TEXTURE_IDX);
    glBindTexture(GL_TEXTURE_2D, m_tessVertexTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32UI, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, m_tessellateFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           m_tessVertexTexture,
                           0);
}

// Wraps a compiled GL shader of draw_path.glsl or draw_image_mesh.glsl, either vertex or fragment,
// with a specific set of features enabled via #define. The set of features to enable is dictated by
// ShaderFeatures.
class PLSRenderContextGLImpl::DrawShader
{
public:
    DrawShader(const DrawShader&) = delete;
    DrawShader& operator=(const DrawShader&) = delete;

    DrawShader(PLSRenderContextGLImpl* plsContextImpl,
               GLenum shaderType,
               DrawType drawType,
               ShaderFeatures shaderFeatures,
               pls::InterlockMode interlockMode,
               pls::ShaderMiscFlags shaderMiscFlags)
    {
#ifndef ENABLE_PLS_EXPERIMENTAL_ATOMICS
        if (interlockMode == pls::InterlockMode::atomics)
        {
            // Don't draw anything in atomic mode if support for it isn't compiled in.
            return;
        }
#endif

        std::vector<const char*> defines;
        defines.push_back(plsContextImpl->m_plsImpl->shaderDefineName());
        for (size_t i = 0; i < kShaderFeatureCount; ++i)
        {
            ShaderFeatures feature = static_cast<ShaderFeatures>(1 << i);
            if (shaderFeatures & feature)
            {
                assert((kVertexShaderFeaturesMask & feature) || shaderType == GL_FRAGMENT_SHADER);
                defines.push_back(GetShaderFeatureGLSLName(feature));
            }
        }

        std::vector<const char*> sources;
        sources.push_back(glsl::constants);
        sources.push_back(glsl::common);
        if (shaderType == GL_FRAGMENT_SHADER &&
            (shaderFeatures & ShaderFeatures::ENABLE_ADVANCED_BLEND))
        {
            sources.push_back(glsl::advanced_blend);
        }
        if (plsContextImpl->platformFeatures().avoidFlatVaryings)
        {
            sources.push_back("#define " GLSL_OPTIONALLY_FLAT "\n");
        }
        else
        {
            sources.push_back("#define " GLSL_OPTIONALLY_FLAT " flat\n");
        }
        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::outerCurvePatches:
                if (shaderType == GL_VERTEX_SHADER)
                {
                    defines.push_back(GLSL_ENABLE_INSTANCE_INDEX);
                    if (!plsContextImpl->m_capabilities
                             .ANGLE_base_vertex_base_instance_shader_builtin)
                    {
                        defines.push_back(GLSL_ENABLE_SPIRV_CROSS_BASE_INSTANCE);
                    }
                }
                defines.push_back(GLSL_DRAW_PATH);
                sources.push_back(pls::glsl::draw_path_common);
                sources.push_back(interlockMode == pls::InterlockMode::rasterOrdering
                                      ? pls::glsl::draw_path
                                      : pls::glsl::atomic_draw);
                break;
            case DrawType::interiorTriangulation:
                defines.push_back(GLSL_DRAW_INTERIOR_TRIANGLES);
                sources.push_back(pls::glsl::draw_path_common);
                sources.push_back(interlockMode == pls::InterlockMode::rasterOrdering
                                      ? pls::glsl::draw_path
                                      : pls::glsl::atomic_draw);
                break;
            case DrawType::imageRect:
                assert(interlockMode == pls::InterlockMode::atomics);
                defines.push_back(GLSL_DRAW_IMAGE);
                defines.push_back(GLSL_DRAW_IMAGE_RECT);
                sources.push_back(pls::glsl::atomic_draw);
                break;
            case DrawType::imageMesh:
                defines.push_back(GLSL_DRAW_IMAGE);
                defines.push_back(GLSL_DRAW_IMAGE_MESH);
                sources.push_back(interlockMode == pls::InterlockMode::rasterOrdering
                                      ? pls::glsl::draw_image_mesh
                                      : pls::glsl::atomic_draw);
                break;
            case DrawType::plsAtomicResolve:
                assert(interlockMode == pls::InterlockMode::atomics);
                defines.push_back(GLSL_RESOLVE_PLS);
                if (shaderMiscFlags & pls::ShaderMiscFlags::kCoalescedResolveAndTransfer)
                {
                    assert(shaderType == GL_FRAGMENT_SHADER);
                    defines.push_back(GLSL_COALESCED_PLS_RESOLVE_AND_TRANSFER);
                }
                sources.push_back(pls::glsl::atomic_draw);
                break;
        }
        if (plsContextImpl->m_capabilities.ARB_bindless_texture)
        {
            defines.push_back(GLSL_ENABLE_BINDLESS_TEXTURES);
        }
        if (!plsContextImpl->m_capabilities.ARB_shader_storage_buffer_object)
        {
            defines.push_back(GLSL_DISABLE_SHADER_STORAGE_BUFFERS);
        }

        m_id = glutils::CompileShader(shaderType,
                                      defines.data(),
                                      defines.size(),
                                      sources.data(),
                                      sources.size(),
                                      plsContextImpl->m_capabilities);
    }

    ~DrawShader() { glDeleteShader(m_id); }

    GLuint id() const { return m_id; }

private:
    GLuint m_id = 0;
};

PLSRenderContextGLImpl::DrawProgram::DrawProgram(PLSRenderContextGLImpl* plsContextImpl,
                                                 pls::DrawType drawType,
                                                 pls::ShaderFeatures shaderFeatures,
                                                 pls::InterlockMode interlockMode,
                                                 pls::ShaderMiscFlags fragmentShaderMiscFlags) :
    m_state(plsContextImpl->m_state)
{
    m_id = glCreateProgram();

    // Not every vertex shader is unique. Cache them by just the vertex features and reuse when
    // possible.
    ShaderFeatures vertexShaderFeatures = shaderFeatures & kVertexShaderFeaturesMask;
    uint32_t vertexShaderKey = pls::ShaderUniqueKey(drawType, vertexShaderFeatures, interlockMode);
    const DrawShader& vertexShader = plsContextImpl->m_vertexShaders
                                         .try_emplace(vertexShaderKey,
                                                      plsContextImpl,
                                                      GL_VERTEX_SHADER,
                                                      drawType,
                                                      vertexShaderFeatures,
                                                      interlockMode,
                                                      pls::ShaderMiscFlags::kNone)
                                         .first->second;
    glAttachShader(m_id, vertexShader.id());

    // Every fragment shader is unique.
    DrawShader fragmentShader(plsContextImpl,
                              GL_FRAGMENT_SHADER,
                              drawType,
                              shaderFeatures,
                              interlockMode,
                              fragmentShaderMiscFlags);
    glAttachShader(m_id, fragmentShader.id());

    glutils::LinkProgram(m_id);

    m_state->bindProgram(m_id);
    glUniformBlockBinding(m_id,
                          glGetUniformBlockIndex(m_id, GLSL_FlushUniforms),
                          FLUSH_UNIFORM_BUFFER_IDX);
    if (drawType == DrawType::imageRect || drawType == DrawType::imageMesh)
    {
        glUniformBlockBinding(m_id,
                              glGetUniformBlockIndex(m_id, GLSL_ImageDrawUniforms),
                              IMAGE_DRAW_UNIFORM_BUFFER_IDX);
    }
    glUniform1i(glGetUniformLocation(m_id, GLSL_tessVertexTexture),
                kPLSTexIdxOffset + TESS_VERTEX_TEXTURE_IDX);
    if (!plsContextImpl->m_capabilities.ARB_shader_storage_buffer_object)
    {
        // Our GL driver doesn't support storage buffers. We polyfill these buffers as textures.
        glUniform1i(glGetUniformLocation(m_id, GLSL_pathBuffer),
                    kPLSTexIdxOffset + PATH_BUFFER_IDX);
        glUniform1i(glGetUniformLocation(m_id, GLSL_paintBuffer),
                    kPLSTexIdxOffset + PAINT_BUFFER_IDX);
        glUniform1i(glGetUniformLocation(m_id, GLSL_paintAuxBuffer),
                    kPLSTexIdxOffset + PAINT_AUX_BUFFER_IDX);
        glUniform1i(glGetUniformLocation(m_id, GLSL_contourBuffer),
                    kPLSTexIdxOffset + CONTOUR_BUFFER_IDX);
    }
    glUniform1i(glGetUniformLocation(m_id, GLSL_gradTexture), kPLSTexIdxOffset + GRAD_TEXTURE_IDX);
    glUniform1i(glGetUniformLocation(m_id, GLSL_imageTexture),
                kPLSTexIdxOffset + IMAGE_TEXTURE_IDX);
    if (!plsContextImpl->m_capabilities.ANGLE_base_vertex_base_instance_shader_builtin)
    {
        // This uniform is specifically named "SPIRV_Cross_BaseInstance" for compatibility with
        // SPIRV-Cross sytems.
        m_spirvCrossBaseInstanceLocation = glGetUniformLocation(m_id, "SPIRV_Cross_BaseInstance");
    }
}

PLSRenderContextGLImpl::DrawProgram::~DrawProgram() { m_state->deleteProgram(m_id); }

static GLuint gl_buffer_id(const BufferRing* bufferRing)
{
    return static_cast<const BufferRingGLImpl*>(bufferRing)->submittedBufferID();
}

static void bind_storage_buffer(const GLCapabilities& capabilities,
                                const BufferRing* bufferRing,
                                uint32_t bindingIdx,
                                size_t bindingSizeInBytes,
                                size_t offsetSizeInBytes)
{
    assert(bufferRing != nullptr);
    assert(bindingSizeInBytes > 0);
    auto storageBufferRing = static_cast<const StorageBufferRingGLImpl*>(bufferRing);
    storageBufferRing->bindToRenderContext(capabilities,
                                           bindingIdx,
                                           bindingSizeInBytes,
                                           offsetSizeInBytes);
}

void PLSRenderContextGLImpl::PLSImpl::ensureRasterOrderingEnabled(
    PLSRenderContextGLImpl* plsContextImpl,
    bool enabled)
{
    assert(!enabled || supportsRasterOrdering(plsContextImpl->m_capabilities));
    auto rasterOrderState = enabled ? RasterOrderingState::enabled : RasterOrderingState::disabled;
    if (m_rasterOrderState != rasterOrderState)
    {
        onEnableRasterOrdering(enabled);
        m_rasterOrderState = rasterOrderState;
        // We only need a barrier when turning raster ordering OFF, because PLS already inserts the
        // necessary barriers after draws when it's disabled.
        if (m_rasterOrderState == RasterOrderingState::disabled)
        {
            onBarrier();
        }
    }
}

void PLSRenderContextGLImpl::flush(const FlushDescriptor& desc)
{
    auto renderTarget = static_cast<PLSRenderTargetGL*>(desc.renderTarget);

    // All programs use the same set of per-flush uniforms.
    glBindBufferRange(GL_UNIFORM_BUFFER,
                      FLUSH_UNIFORM_BUFFER_IDX,
                      gl_buffer_id(flushUniformBufferRing()),
                      desc.flushUniformDataOffsetInBytes,
                      sizeof(pls::FlushUniforms));

    // All programs use the same storage buffers.
    if (desc.pathCount > 0)
    {
        bind_storage_buffer(m_capabilities,
                            pathBufferRing(),
                            PATH_BUFFER_IDX,
                            desc.pathCount * sizeof(pls::PathData),
                            desc.firstPath * sizeof(pls::PathData));

        bind_storage_buffer(m_capabilities,
                            paintBufferRing(),
                            PAINT_BUFFER_IDX,
                            desc.pathCount * sizeof(pls::PaintData),
                            desc.firstPaint * sizeof(pls::PaintData));

        bind_storage_buffer(m_capabilities,
                            paintAuxBufferRing(),
                            PAINT_AUX_BUFFER_IDX,
                            desc.pathCount * sizeof(pls::PaintAuxData),
                            desc.firstPaintAux * sizeof(pls::PaintAuxData));
    }

    if (desc.contourCount > 0)
    {
        bind_storage_buffer(m_capabilities,
                            contourBufferRing(),
                            CONTOUR_BUFFER_IDX,
                            desc.contourCount * sizeof(pls::ContourData),
                            desc.firstContour * sizeof(pls::ContourData));
    }

    // Render the complex color ramps into the gradient texture.
    if (desc.complexGradSpanCount > 0)
    {
        m_state->bindBuffer(GL_ARRAY_BUFFER, gl_buffer_id(gradSpanBufferRing()));
        m_state->bindVAO(m_colorRampVAO);
        m_state->enableFaceCulling(true);
        glVertexAttribIPointer(
            0,
            4,
            GL_UNSIGNED_INT,
            0,
            reinterpret_cast<const void*>(desc.firstComplexGradSpan * sizeof(pls::GradientSpan)));
        glViewport(0, desc.complexGradRowsTop, kGradTextureWidth, desc.complexGradRowsHeight);
        glBindFramebuffer(GL_FRAMEBUFFER, m_colorRampFBO);
        m_state->bindProgram(m_colorRampProgram);
        GLenum colorAttachment0 = GL_COLOR_ATTACHMENT0;
        glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, &colorAttachment0);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, desc.complexGradSpanCount);
    }

    // Copy the simple color ramps to the gradient texture.
    if (desc.simpleGradTexelsHeight > 0)
    {
        m_state->bindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_buffer_id(simpleColorRampsBufferRing()));
        glActiveTexture(GL_TEXTURE0 + kPLSTexIdxOffset + GRAD_TEXTURE_IDX);
        glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        0,
                        0,
                        desc.simpleGradTexelsWidth,
                        desc.simpleGradTexelsHeight,
                        GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        reinterpret_cast<const void*>(desc.simpleGradDataOffsetInBytes));
    }

    // Tessellate all curves into vertices in the tessellation texture.
    if (desc.tessVertexSpanCount > 0)
    {
        m_state->bindBuffer(GL_ARRAY_BUFFER, gl_buffer_id(tessSpanBufferRing()));
        m_state->bindVAO(m_tessellateVAO);
        m_state->enableFaceCulling(true);
        size_t tessSpanOffsetInBytes = desc.firstTessVertexSpan * sizeof(pls::TessVertexSpan);
        for (uintptr_t i = 0; i < 3; ++i)
        {
            glVertexAttribPointer(i,
                                  4,
                                  GL_FLOAT,
                                  GL_FALSE,
                                  sizeof(TessVertexSpan),
                                  reinterpret_cast<const void*>(tessSpanOffsetInBytes + i * 4 * 4));
        }
        glVertexAttribIPointer(
            3,
            4,
            GL_UNSIGNED_INT,
            sizeof(TessVertexSpan),
            reinterpret_cast<const void*>(tessSpanOffsetInBytes + offsetof(TessVertexSpan, x0x1)));
        glViewport(0, 0, pls::kTessTextureWidth, desc.tessDataHeight);
        glBindFramebuffer(GL_FRAMEBUFFER, m_tessellateFBO);
        m_state->bindProgram(m_tessellateProgram);
        GLenum colorAttachment0 = GL_COLOR_ATTACHMENT0;
        glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, &colorAttachment0);
        glDrawElementsInstanced(GL_TRIANGLES,
                                std::size(pls::kTessSpanIndices),
                                GL_UNSIGNED_SHORT,
                                0,
                                desc.tessVertexSpanCount);
    }

    bool renderPassHasCoalescedResolveAndTransfer =
        desc.interlockMode == pls::InterlockMode::atomics && !desc.renderDirectToRasterPipeline &&
        m_plsImpl->supportsCoalescedPLSResolveAndTransfer(renderTarget);

    // Compile the draw programs before activating pixel local storage.
    // Cache specific compilations by DrawType and ShaderFeatures.
    // (ANGLE_shader_pixel_local_storage doesn't allow shader compilation while active.)
    for (const DrawBatch& batch : *desc.drawList)
    {
        auto shaderFeatures = desc.interlockMode == pls::InterlockMode::atomics
                                  ? desc.combinedShaderFeatures
                                  : batch.shaderFeatures;
        auto fragmentShaderMiscFlags = batch.drawType == pls::DrawType::plsAtomicResolve &&
                                               renderPassHasCoalescedResolveAndTransfer
                                           ? pls::ShaderMiscFlags::kCoalescedResolveAndTransfer
                                           : pls::ShaderMiscFlags::kNone;
        uint32_t fragmentShaderKey = pls::ShaderUniqueKey(batch.drawType,
                                                          shaderFeatures,
                                                          desc.interlockMode,
                                                          fragmentShaderMiscFlags);
        m_drawPrograms.try_emplace(fragmentShaderKey,
                                   this,
                                   batch.drawType,
                                   shaderFeatures,
                                   desc.interlockMode,
                                   fragmentShaderMiscFlags);
    }

    // Bind the currently-submitted buffer in the triangleBufferRing to its vertex array.
    if (desc.hasTriangleVertices)
    {
        m_state->bindVAO(m_interiorTrianglesVAO);
        m_state->bindBuffer(GL_ARRAY_BUFFER, gl_buffer_id(triangleBufferRing()));
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

    m_plsImpl->activatePixelLocalStorage(this, desc);

    // Execute the DrawList.
    for (const DrawBatch& batch : *desc.drawList)
    {
        if (batch.elementCount == 0)
        {
            continue;
        }

        auto shaderFeatures = desc.interlockMode == pls::InterlockMode::atomics
                                  ? desc.combinedShaderFeatures
                                  : batch.shaderFeatures;
        auto fragmentShaderMiscFlags = batch.drawType == pls::DrawType::plsAtomicResolve &&
                                               renderPassHasCoalescedResolveAndTransfer
                                           ? pls::ShaderMiscFlags::kCoalescedResolveAndTransfer
                                           : pls::ShaderMiscFlags::kNone;
        uint32_t fragmentShaderKey = pls::ShaderUniqueKey(batch.drawType,
                                                          shaderFeatures,
                                                          desc.interlockMode,
                                                          fragmentShaderMiscFlags);
        const DrawProgram& drawProgram = m_drawPrograms.find(fragmentShaderKey)->second;
        if (drawProgram.id() == 0)
        {
            fprintf(stderr, "WARNING: skipping draw due to missing GL program.\n");
            continue;
        }
        m_state->bindProgram(drawProgram.id());

        if (auto imageTextureGL = static_cast<const PLSTextureGLImpl*>(batch.imageTexture))
        {
            glActiveTexture(GL_TEXTURE0 + kPLSTexIdxOffset + IMAGE_TEXTURE_IDX);
            glBindTexture(GL_TEXTURE_2D, imageTextureGL->id());
        }

        switch (DrawType drawType = batch.drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::outerCurvePatches:
            {
                // Draw PLS patches that connect the tessellation vertices.
                m_plsImpl->ensureRasterOrderingEnabled(this,
                                                       desc.interlockMode ==
                                                           pls::InterlockMode::rasterOrdering);
                m_state->bindVAO(m_drawVAO);
                m_state->enableFaceCulling(true);
                uint32_t indexCount = PatchIndexCount(drawType);
                uint32_t baseIndex = PatchBaseIndex(drawType);
                const void* indexOffset =
                    reinterpret_cast<const void*>(baseIndex * sizeof(uint16_t));
#ifndef RIVE_WEBGL
                if (m_capabilities.ANGLE_base_vertex_base_instance_shader_builtin)
                {
                    glDrawElementsInstancedBaseInstanceEXT(GL_TRIANGLES,
                                                           indexCount,
                                                           GL_UNSIGNED_SHORT,
                                                           indexOffset,
                                                           batch.elementCount,
                                                           batch.baseElement);
                }
                else
#endif
                {
                    glUniform1i(drawProgram.spirvCrossBaseInstanceLocation(), batch.baseElement);
                    glDrawElementsInstanced(GL_TRIANGLES,
                                            indexCount,
                                            GL_UNSIGNED_SHORT,
                                            indexOffset,
                                            batch.elementCount);
                }
                break;
            }
            case DrawType::interiorTriangulation:
            {
                m_plsImpl->ensureRasterOrderingEnabled(this, false);
                m_state->bindVAO(m_interiorTrianglesVAO);
                m_state->enableFaceCulling(true);
                glDrawArrays(GL_TRIANGLES, batch.baseElement, batch.elementCount);
                break;
            }
            case DrawType::imageRect:
            {
                assert(!m_capabilities.ARB_bindless_texture);
                assert(m_imageRectVAO != 0); // Should have gotten lazily allocated by now.
                m_plsImpl->ensureRasterOrderingEnabled(this, false);
                m_state->bindVAO(m_imageRectVAO);
                glBindBufferRange(GL_UNIFORM_BUFFER,
                                  IMAGE_DRAW_UNIFORM_BUFFER_IDX,
                                  gl_buffer_id(imageDrawUniformBufferRing()),
                                  batch.imageDrawDataOffset,
                                  sizeof(pls::ImageDrawUniforms));
                m_state->enableFaceCulling(false);
                glDrawElements(GL_TRIANGLES,
                               std::size(pls::kImageRectIndices),
                               GL_UNSIGNED_SHORT,
                               nullptr);
                break;
            }
            case DrawType::imageMesh:
            {
                LITE_RTTI_CAST_OR_BREAK(vertexBuffer,
                                        const PLSRenderBufferGLImpl*,
                                        batch.vertexBuffer);
                LITE_RTTI_CAST_OR_BREAK(uvBuffer, const PLSRenderBufferGLImpl*, batch.uvBuffer);
                LITE_RTTI_CAST_OR_BREAK(indexBuffer,
                                        const PLSRenderBufferGLImpl*,
                                        batch.indexBuffer);
                // Enable raster ordering whenever we can for image meshes, even in atomic mode; we
                // have no control over whether the internal geometry has self overlaps.
                m_plsImpl->ensureRasterOrderingEnabled(this,
                                                       m_platformFeatures.supportsRasterOrdering);
                m_state->bindVAO(m_imageMeshVAO);
                m_state->bindBuffer(GL_ARRAY_BUFFER, vertexBuffer->submittedBufferID());
                glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
                m_state->bindBuffer(GL_ARRAY_BUFFER, uvBuffer->submittedBufferID());
                glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
                m_state->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->submittedBufferID());
                glBindBufferRange(GL_UNIFORM_BUFFER,
                                  IMAGE_DRAW_UNIFORM_BUFFER_IDX,
                                  gl_buffer_id(imageDrawUniformBufferRing()),
                                  batch.imageDrawDataOffset,
                                  sizeof(pls::ImageDrawUniforms));
                m_state->enableFaceCulling(false);
                glDrawElements(GL_TRIANGLES,
                               batch.elementCount,
                               GL_UNSIGNED_SHORT,
                               reinterpret_cast<const void*>(batch.baseElement * sizeof(uint16_t)));
                break;
            }
            case DrawType::plsAtomicResolve:
                m_plsImpl->ensureRasterOrderingEnabled(this, false);
                m_state->bindVAO(m_plsResolveVAO);
                if (renderPassHasCoalescedResolveAndTransfer)
                {
                    // Since setupCoalescedPLSResolveAndTransfer() binds a different framebuffer,
                    // this better be the final batch of the render pass.
                    assert(&batch == &desc.drawList->tail());
                    m_plsImpl->setupCoalescedPLSResolveAndTransfer(renderTarget);
                }
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                break;
        }
        if (batch.needsBarrier)
        {
            m_plsImpl->barrier();
        }
    }

    m_plsImpl->deactivatePixelLocalStorage(this, desc);

#ifdef RIVE_DESKTOP_GL
    if (m_capabilities.ANGLE_polygon_mode && desc.wireframe)
    {
        glPolygonModeANGLE(GL_FRONT_AND_BACK, GL_FILL_ANGLE);
    }
#endif
}

std::unique_ptr<PLSRenderContext> PLSRenderContextGLImpl::MakeContext(
    const ContextOptions& contextOptions)
{
    GLCapabilities capabilities{};

    const char* glVersionStr = (const char*)glGetString(GL_VERSION);
    capabilities.isGLES = strstr(glVersionStr, "OpenGL ES") != NULL;
    if (capabilities.isGLES)
    {
#ifdef _MSC_VER
        sscanf_s(glVersionStr,
                 "OpenGL ES %d.%d",
                 &capabilities.contextVersionMajor,
                 &capabilities.contextVersionMinor);
#else
        sscanf(glVersionStr,
               "OpenGL ES %d.%d",
               &capabilities.contextVersionMajor,
               &capabilities.contextVersionMinor);
#endif
    }
    else
    {
#ifdef _MSC_VER
        sscanf_s(glVersionStr,
                 "%d.%d",
                 &capabilities.contextVersionMajor,
                 &capabilities.contextVersionMinor);
#else
        sscanf(glVersionStr,
               "%d.%d",
               &capabilities.contextVersionMajor,
               &capabilities.contextVersionMinor);
#endif
    }
#ifdef RIVE_DESKTOP_GL
    assert(capabilities.contextVersionMajor == GLAD_GL_version_major);
    assert(capabilities.contextVersionMinor == GLAD_GL_version_minor);
    assert(capabilities.isGLES == static_cast<bool>(GLAD_GL_version_es));
#endif

    // Our baseline feature set is GLES 3.0. Capabilities from newer context versions are reported
    // as extensions.
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
    }

    GLint extensionCount;
    glGetIntegerv(GL_NUM_EXTENSIONS, &extensionCount);
    for (int i = 0; i < extensionCount; ++i)
    {
        auto* ext = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i));
        if (strcmp(ext, "GL_ANGLE_base_vertex_base_instance_shader_builtin") == 0)
        {
            capabilities.ANGLE_base_vertex_base_instance_shader_builtin = true;
        }
        if (strcmp(ext, "GL_ANGLE_shader_pixel_local_storage") == 0)
        {
            capabilities.ANGLE_shader_pixel_local_storage = true;
        }
        else if (strcmp(ext, "GL_ANGLE_shader_pixel_local_storage_coherent") == 0)
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
        else if (strcmp(ext, "GL_EXT_base_instance") == 0)
        {
            capabilities.EXT_base_instance = true;
        }
        else if (strcmp(ext, "GL_INTEL_fragment_shader_ordering") == 0)
        {
            capabilities.INTEL_fragment_shader_ordering = true;
        }
        else if (strcmp(ext, "GL_EXT_shader_framebuffer_fetch") == 0)
        {
            capabilities.EXT_shader_framebuffer_fetch = true;
        }
        else if (strcmp(ext, "GL_EXT_shader_pixel_local_storage") == 0)
        {
            capabilities.EXT_shader_pixel_local_storage = true;
        }
        else if (strcmp(ext, "GL_QCOM_shader_framebuffer_fetch_noncoherent") == 0)
        {
            capabilities.QCOM_shader_framebuffer_fetch_noncoherent = true;
        }
    }

#ifdef RIVE_DESKTOP_GL
    // We implement some ES capabilities with core Desktop GL in glad_custom.c.
    if (GLAD_GL_ARB_bindless_texture)
    {
        capabilities.ARB_bindless_texture = true;
    }
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

    // We need four storage buffers in the vertex shader. Disable the extension if this isn't
    // supported.
    if (capabilities.ARB_shader_storage_buffer_object)
    {
        int maxVertexShaderStorageBlocks;
        glGetIntegerv(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS, &maxVertexShaderStorageBlocks);
        if (maxVertexShaderStorageBlocks < pls::kMaxStorageBuffers)
        {
            capabilities.ARB_shader_storage_buffer_object = false;
        }
    }

    // Now disable the extensions we don't want to use internally.
    if (contextOptions.disableFragmentShaderInterlock)
    {
        capabilities.ARB_fragment_shader_interlock = false;
        capabilities.INTEL_fragment_shader_ordering = false;
    }

    // Disable ANGLE_base_vertex_base_instance_shader_builtin on ANGLE/D3D. This extension is
    // polyfilled on D3D anyway, and we need to test our fallback.
    GLenum rendererToken = GL_RENDERER;
#if defined(RIVE_WEBGL) && defined(GL_WEBGL_shader_pixel_local_storage)
    if (emscripten_webgl_enable_extension(emscripten_webgl_get_current_context(),
                                          "WEBGL_debug_renderer_info"))
    {
        rendererToken = GL_UNMASKED_RENDERER_WEBGL;
    }
#endif
    const char* rendererString = reinterpret_cast<const char*>(glGetString(rendererToken));
    if (strstr(rendererString, "Direct3D"))
    {
        capabilities.ANGLE_base_vertex_base_instance_shader_builtin = false;
    }

#ifdef RIVE_GLES
    loadGLESExtensions(capabilities); // Android doesn't load extension functions for us.
    if (capabilities.EXT_shader_pixel_local_storage &&
        (capabilities.ARM_shader_framebuffer_fetch || capabilities.EXT_shader_framebuffer_fetch))
    {
        return MakeContext(rendererString, capabilities, MakePLSImplEXTNative(capabilities));
    }

    if (capabilities.EXT_shader_framebuffer_fetch)
    {
        return MakeContext(rendererString, capabilities, MakePLSImplFramebufferFetch(capabilities));
    }
#endif

#ifdef RIVE_DESKTOP_GL
    if (capabilities.ANGLE_shader_pixel_local_storage_coherent)
    {
        return MakeContext(rendererString, capabilities, MakePLSImplWebGL());
    }

    if (capabilities.ARB_shader_image_load_store)
    {
        return MakeContext(rendererString, capabilities, MakePLSImplRWTexture());
    }
#endif

#if defined(RIVE_WEBGL) && defined(GL_WEBGL_shader_pixel_local_storage)
    if (emscripten_webgl_enable_WEBGL_shader_pixel_local_storage(
            emscripten_webgl_get_current_context()) &&
        emscripten_webgl_shader_pixel_local_storage_is_coherent())
    {
        capabilities.ANGLE_shader_pixel_local_storage = true;
        capabilities.ANGLE_shader_pixel_local_storage_coherent = true;
        return MakeContext(rendererString, capabilities, MakePLSImplWebGL());
    }
#endif

    fprintf(stderr, "Pixel local storage is not supported.\n");
    return nullptr;
}

std::unique_ptr<PLSRenderContext> PLSRenderContextGLImpl::MakeContext(
    const char* rendererString,
    GLCapabilities capabilities,
    std::unique_ptr<PLSImpl> plsImpl)
{
    auto plsContextImpl = std::unique_ptr<PLSRenderContextGLImpl>(
        new PLSRenderContextGLImpl(rendererString, capabilities, std::move(plsImpl)));
    return std::make_unique<PLSRenderContext>(std::move(plsContextImpl));
}
} // namespace rive::pls
