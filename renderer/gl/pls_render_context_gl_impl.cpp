/*
 * Copyright 2022 Rive
 */

#include "rive/pls/gl/pls_render_context_gl_impl.hpp"

#include "gl_utils.hpp"
#include "rive/pls/gl/pls_render_buffer_gl_impl.hpp"
#include "rive/pls/pls_image.hpp"
#include "shaders/constants.glsl"

#include "../out/obj/generated/advanced_blend.glsl.hpp"
#include "../out/obj/generated/color_ramp.glsl.hpp"
#include "../out/obj/generated/constants.glsl.hpp"
#include "../out/obj/generated/common.glsl.hpp"
#include "../out/obj/generated/draw_path_common.glsl.hpp"
#include "../out/obj/generated/draw_path.glsl.hpp"
#include "../out/obj/generated/draw_image_mesh.glsl.hpp"
#include "../out/obj/generated/tessellate.glsl.hpp"

#ifdef RIVE_GLES
// In an effort to save space on Android, and since GLES doesn't have storage buffers, don't include
// the atomic sources.
namespace rive::pls::glsl
{
const char atomic_draw[] = "";
}
#else
#include "../out/obj/generated/atomic_draw.glsl.hpp"
#define ENABLE_PLS_EXPERIMENTAL_ATOMICS
#endif

// Offset all PLS texture indices by 1 so we, and others who share our GL context, can use
// GL_TEXTURE0 as a scratch texture index.
constexpr static int kPLSTexIdxOffset = 1;

namespace rive::pls
{
PLSRenderContextGLImpl::PLSRenderContextGLImpl(const char* rendererString,
                                               GLExtensions extensions,
                                               std::unique_ptr<PLSImpl> plsImpl) :
    m_extensions(extensions),
    m_plsImpl(std::move(plsImpl)),
    m_state(make_rcp<GLState>(m_extensions))

{
    if (strstr(rendererString, "Apple") && strstr(rendererString, "Metal"))
    {
        // In Metal, non-flat varyings preserve their exact value if all vertices in the triangle
        // emit the same value, and we also see a small (5-10%) improvement from not using flat
        // varyings.
        m_platformFeatures.avoidFlatVaryings = true;
    }
    m_platformFeatures.fragCoordBottomUp = true;
    if (m_extensions.ARB_bindless_texture)
    {
        m_platformFeatures.supportsBindlessTextures = true;
    }

    m_shaderVersionString[kShaderVersionStringBuffSize - 1] = '\0';
#ifdef _WIN32
    strcpy_s(m_shaderVersionString, kShaderVersionStringBuffSize, "#version 300 es\n");
#else
    strncpy(m_shaderVersionString, "#version 300 es\n", kShaderVersionStringBuffSize - 1);
#endif
#ifdef RIVE_DESKTOP_GL
    if (!GLAD_GL_version_es && GLAD_IS_GL_VERSION_AT_LEAST(4, 0))
    {
        snprintf(m_shaderVersionString,
                 kShaderVersionStringBuffSize,
                 "#version %d%d0\n",
                 GLAD_GL_version_major,
                 GLAD_GL_version_minor);
    }
#endif

    m_colorRampProgram = glCreateProgram();
    const char* colorRampSources[] = {glsl::constants, glsl::common, glsl::color_ramp};
    glutils::CompileAndAttachShader(m_colorRampProgram,
                                    GL_VERTEX_SHADER,
                                    nullptr,
                                    0,
                                    colorRampSources,
                                    std::size(colorRampSources),
                                    m_shaderVersionString);
    glutils::CompileAndAttachShader(m_colorRampProgram,
                                    GL_FRAGMENT_SHADER,
                                    nullptr,
                                    0,
                                    colorRampSources,
                                    std::size(colorRampSources),
                                    m_shaderVersionString);
    glutils::LinkProgram(m_colorRampProgram);
    glUniformBlockBinding(m_colorRampProgram,
                          glGetUniformBlockIndex(m_colorRampProgram, GLSL_Uniforms),
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
                                    nullptr,
                                    0,
                                    tessellateSources,
                                    std::size(tessellateSources),
                                    m_shaderVersionString);
    glutils::CompileAndAttachShader(m_tessellateProgram,
                                    GL_FRAGMENT_SHADER,
                                    nullptr,
                                    0,
                                    tessellateSources,
                                    std::size(tessellateSources),
                                    m_shaderVersionString);
    glutils::LinkProgram(m_tessellateProgram);
    m_state->bindProgram(m_tessellateProgram);
    glUniformBlockBinding(m_tessellateProgram,
                          glGetUniformBlockIndex(m_tessellateProgram, GLSL_Uniforms),
                          FLUSH_UNIFORM_BUFFER_IDX);
    glUniform1i(glGetUniformLocation(m_tessellateProgram, GLSL_pathTexture),
                kPLSTexIdxOffset + PATH_TEXTURE_IDX);
    glUniform1i(glGetUniformLocation(m_tessellateProgram, GLSL_contourTexture),
                kPLSTexIdxOffset + CONTOUR_TEXTURE_IDX);

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
                          reinterpret_cast<void*>(sizeof(float) * 4));

    glGenVertexArrays(1, &m_interiorTrianglesVAO);
    m_state->bindVAO(m_interiorTrianglesVAO);
    glEnableVertexAttribArray(0);

    glGenVertexArrays(1, &m_imageMeshVAO);
    m_state->bindVAO(m_imageMeshVAO);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    m_plsImpl->init(m_state);
}

PLSRenderContextGLImpl::~PLSRenderContextGLImpl()
{
    glDeleteTextures(1, &m_pathTexture);
    glDeleteTextures(1, &m_contourTexture);

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

    m_state->deleteBuffer(m_paintBuffer);
    m_state->deleteBuffer(m_paintMatrixBuffer);
    m_state->deleteBuffer(m_paintTranslateBuffer);
    m_state->deleteBuffer(m_clipRectMatrixBuffer);
    m_state->deleteBuffer(m_clipRectTranslateBuffer);
}

void PLSRenderContextGLImpl::resetGLState()
{
    m_state->reset(m_extensions);
    glActiveTexture(GL_TEXTURE0 + kPLSTexIdxOffset + TESS_VERTEX_TEXTURE_IDX);
    glBindTexture(GL_TEXTURE_2D, m_tessVertexTexture);

    glActiveTexture(GL_TEXTURE0 + kPLSTexIdxOffset + PATH_TEXTURE_IDX);
    glBindTexture(GL_TEXTURE_2D, m_pathTexture);

    glActiveTexture(GL_TEXTURE0 + kPLSTexIdxOffset + CONTOUR_TEXTURE_IDX);
    glBindTexture(GL_TEXTURE_2D, m_contourTexture);

    glActiveTexture(GL_TEXTURE0 + kPLSTexIdxOffset + GRAD_TEXTURE_IDX);
    glBindTexture(GL_TEXTURE_2D, m_gradientTexture);
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
    }

    ~PLSTextureGLImpl() override
    {
#ifdef RIVE_DESKTOP_GL
        if (m_bindlessHandle != 0)
        {
            glMakeTextureHandleNonResidentARB(m_bindlessHandle);
            m_bindlessHandle = 0;
        }
#endif
        assert(m_bindlessHandle == 0);
    }

    GLuint id() const { return m_id; }

    GLuint64 bindlessHandle(const GLExtensions& extensions) const
    {
#ifdef RIVE_DESKTOP_GL
        if (extensions.ARB_bindless_texture && m_bindlessHandle == 0)
        {
            m_bindlessHandle = glGetTextureHandleARB(m_id);
            glMakeTextureHandleResidentARB(m_bindlessHandle);
        }
#endif
        return m_bindlessHandle;
    }

private:
    const rcp<GLState> m_state;
    GLuint m_id = 0;
    mutable GLuint64 m_bindlessHandle = 0;
};

rcp<PLSTexture> PLSRenderContextGLImpl::makeImageTexture(uint32_t width,
                                                         uint32_t height,
                                                         uint32_t mipLevelCount,
                                                         const uint8_t imageDataRGBA[])
{
    return make_rcp<PLSTextureGLImpl>(width, height, mipLevelCount, imageDataRGBA, m_state);
}

// BufferRingImpl in GL on a given buffer target. In order to support WebGL2, we don't do hardware
// mapping.
class BufferRingGLImpl : public BufferRingShadowImpl
{
public:
    BufferRingGLImpl(GLenum target, size_t capacity, size_t itemSizeInBytes, rcp<GLState> state) :
        BufferRingShadowImpl(capacity, itemSizeInBytes), m_target(target), m_state(std::move(state))
    {
        glGenBuffers(kBufferRingSize, m_ids);
        for (int i = 0; i < kBufferRingSize; ++i)
        {
            m_state->bindBuffer(m_target, m_ids[i]);
            glBufferData(m_target, capacity * itemSizeInBytes, nullptr, GL_DYNAMIC_DRAW);
        }
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
    void onUnmapAndSubmitBuffer(int bufferIdx, size_t bytesWritten) override
    {
        m_state->bindBuffer(m_target, m_ids[bufferIdx]);
        glBufferSubData(m_target, 0, bytesWritten, shadowBuffer());
    }

private:
    const GLenum m_target;
    GLuint m_ids[kBufferRingSize];
    const rcp<GLState> m_state;
};

std::unique_ptr<BufferRing> PLSRenderContextGLImpl::makeVertexBufferRing(size_t capacity,
                                                                         size_t itemSizeInBytes)
{
    return capacity != 0 ? std::make_unique<BufferRingGLImpl>(GL_ARRAY_BUFFER,
                                                              capacity,
                                                              itemSizeInBytes,
                                                              m_state)
                         : nullptr;
}

std::unique_ptr<BufferRing> PLSRenderContextGLImpl::makePixelUnpackBufferRing(
    size_t capacity,
    size_t itemSizeInBytes)
{
    return capacity != 0 ? std::make_unique<BufferRingGLImpl>(GL_PIXEL_UNPACK_BUFFER,
                                                              capacity,
                                                              itemSizeInBytes,
                                                              m_state)
                         : nullptr;
}

std::unique_ptr<BufferRing> PLSRenderContextGLImpl::makeUniformBufferRing(size_t capacity,
                                                                          size_t sizeInBytes)
{
    return capacity != 0 ? std::make_unique<BufferRingGLImpl>(GL_UNIFORM_BUFFER,
                                                              capacity,
                                                              sizeInBytes,
                                                              m_state)
                         : nullptr;
}

void PLSRenderContextGLImpl::resizePathTexture(uint32_t width, uint32_t height)
{
    glDeleteTextures(1, &m_pathTexture);
    if (width == 0 || height == 0)
    {
        m_pathTexture = 0;
        return;
    }
    glGenTextures(1, &m_pathTexture);
    glActiveTexture(GL_TEXTURE0 + kPLSTexIdxOffset + PATH_TEXTURE_IDX);
    glBindTexture(GL_TEXTURE_2D, m_pathTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32UI, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void PLSRenderContextGLImpl::resizeContourTexture(uint32_t width, uint32_t height)
{
    glDeleteTextures(1, &m_contourTexture);
    if (width == 0 || height == 0)
    {
        m_contourTexture = 0;
        return;
    }
    glGenTextures(1, &m_contourTexture);
    glActiveTexture(GL_TEXTURE0 + kPLSTexIdxOffset + CONTOUR_TEXTURE_IDX);
    glBindTexture(GL_TEXTURE_2D, m_contourTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32UI, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
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
               pls::InterlockMode interlockMode)
    {
#ifndef ENABLE_PLS_EXPERIMENTAL_ATOMICS
        if (interlockMode == pls::InterlockMode::experimentalAtomics)
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
                    if (!plsContextImpl->m_extensions
                             .ANGLE_base_vertex_base_instance_shader_builtin)
                    {
                        defines.push_back(GLSL_ENABLE_SPIRV_CROSS_BASE_INSTANCE);
                    }
                }
                defines.push_back(GLSL_DRAW_PATH);
                sources.push_back(pls::glsl::draw_path_common);
                sources.push_back(interlockMode == pls::InterlockMode::rasterOrdered
                                      ? pls::glsl::draw_path
                                      : pls::glsl::atomic_draw);
                break;
            case DrawType::interiorTriangulation:
                defines.push_back(GLSL_DRAW_INTERIOR_TRIANGLES);
                sources.push_back(pls::glsl::draw_path_common);
                sources.push_back(interlockMode == pls::InterlockMode::rasterOrdered
                                      ? pls::glsl::draw_path
                                      : pls::glsl::atomic_draw);
                break;
            case DrawType::imageRect:
                assert(interlockMode == pls::InterlockMode::experimentalAtomics);
                defines.push_back(GLSL_DRAW_IMAGE);
                defines.push_back(GLSL_DRAW_IMAGE_RECT);
                sources.push_back(pls::glsl::atomic_draw);
                break;
            case DrawType::imageMesh:
                defines.push_back(GLSL_DRAW_IMAGE);
                defines.push_back(GLSL_DRAW_IMAGE_MESH);
                sources.push_back(interlockMode == pls::InterlockMode::rasterOrdered
                                      ? pls::glsl::draw_image_mesh
                                      : pls::glsl::atomic_draw);
                break;
            case DrawType::plsAtomicResolve:
                assert(interlockMode == pls::InterlockMode::experimentalAtomics);
                defines.push_back(GLSL_RESOLVE_PLS);
                sources.push_back(pls::glsl::atomic_draw);
                break;
        }
        if (plsContextImpl->m_extensions.ARB_bindless_texture)
        {
            defines.push_back(GLSL_ENABLE_BINDLESS_TEXTURES);
        }

        m_id = glutils::CompileShader(shaderType,
                                      defines.data(),
                                      defines.size(),
                                      sources.data(),
                                      sources.size(),
                                      plsContextImpl->m_shaderVersionString);
    }

    ~DrawShader() { glDeleteShader(m_id); }

    GLuint id() const { return m_id; }

private:
    GLuint m_id = 0;
};

PLSRenderContextGLImpl::DrawProgram::DrawProgram(PLSRenderContextGLImpl* plsContextImpl,
                                                 DrawType drawType,
                                                 ShaderFeatures shaderFeatures,
                                                 pls::InterlockMode interlockMode) :
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
                                                      interlockMode)
                                         .first->second;
    glAttachShader(m_id, vertexShader.id());

    // Every fragment shader is unique.
    DrawShader fragmentShader(plsContextImpl,
                              GL_FRAGMENT_SHADER,
                              drawType,
                              shaderFeatures,
                              interlockMode);
    glAttachShader(m_id, fragmentShader.id());

    glutils::LinkProgram(m_id);

    m_state->bindProgram(m_id);
    glUniformBlockBinding(m_id,
                          glGetUniformBlockIndex(m_id, GLSL_Uniforms),
                          FLUSH_UNIFORM_BUFFER_IDX);
    if (drawType == DrawType::imageRect || drawType == DrawType::imageMesh)
    {
        glUniformBlockBinding(m_id,
                              glGetUniformBlockIndex(m_id, GLSL_ImageDrawUniforms),
                              IMAGE_DRAW_UNIFORM_BUFFER_IDX);
    }
    glUniform1i(glGetUniformLocation(m_id, GLSL_tessVertexTexture),
                kPLSTexIdxOffset + TESS_VERTEX_TEXTURE_IDX);
    glUniform1i(glGetUniformLocation(m_id, GLSL_pathTexture), kPLSTexIdxOffset + PATH_TEXTURE_IDX);
    glUniform1i(glGetUniformLocation(m_id, GLSL_contourTexture),
                kPLSTexIdxOffset + CONTOUR_TEXTURE_IDX);
    glUniform1i(glGetUniformLocation(m_id, GLSL_gradTexture), kPLSTexIdxOffset + GRAD_TEXTURE_IDX);
    glUniform1i(glGetUniformLocation(m_id, GLSL_imageTexture),
                kPLSTexIdxOffset + IMAGE_TEXTURE_IDX);
    if (!plsContextImpl->m_extensions.ANGLE_base_vertex_base_instance_shader_builtin)
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

void PLSRenderContextGLImpl::flush(const FlushDescriptor& desc)
{
    auto renderTarget = static_cast<const PLSRenderTargetGL*>(desc.renderTarget);

    // All programs use the same set of per-flush uniforms.
    glBindBufferBase(GL_UNIFORM_BUFFER,
                     FLUSH_UNIFORM_BUFFER_IDX,
                     gl_buffer_id(flushUniformBufferRing()));

#ifdef ENABLE_PLS_EXPERIMENTAL_ATOMICS
    if (desc.interlockMode == pls::InterlockMode::experimentalAtomics)
    {
        if (m_paintBuffer == 0)
        {
            // This is the first flush with "atomic mode" enabled. Allocate the additional resources
            // for atomic mode.
            if (!m_extensions.ARB_bindless_texture)
            {
                // We only have to draw imageRects when in atomic mode and bindless textures are not
                // supported.
                assert(m_imageRectVAO == 0);
                glGenVertexArrays(1, &m_imageRectVAO);
                m_state->bindVAO(m_imageRectVAO);

                assert(m_imageRectVertexBuffer == 0);
                glGenBuffers(1, &m_imageRectVertexBuffer);
                m_state->bindBuffer(GL_ARRAY_BUFFER, m_imageRectVertexBuffer);
                glBufferData(GL_ARRAY_BUFFER,
                             sizeof(pls::kImageRectVertices),
                             pls::kImageRectVertices,
                             GL_STATIC_DRAW);

                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0,
                                      4,
                                      GL_FLOAT,
                                      GL_FALSE,
                                      sizeof(pls::ImageRectVertex),
                                      nullptr);

                assert(m_imageRectIndexBuffer == 0);
                glGenBuffers(1, &m_imageRectIndexBuffer);
                m_state->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_imageRectIndexBuffer);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                             sizeof(pls::kImageRectIndices),
                             pls::kImageRectIndices,
                             GL_STATIC_DRAW);
            }

            assert(m_plsResolveVAO == 0);
            glGenVertexArrays(1, &m_plsResolveVAO);

            assert(m_paintBuffer == 0);
            glGenBuffers(1, &m_paintBuffer);
            m_state->bindBuffer(GL_SHADER_STORAGE_BUFFER, m_paintBuffer);
            glBufferData(GL_SHADER_STORAGE_BUFFER,
                         sizeof(pls::ExperimentalAtomicModeData::m_paints),
                         nullptr,
                         GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, PAINT_STORAGE_BUFFER_IDX, m_paintBuffer);

            assert(m_paintMatrixBuffer == 0);
            glGenBuffers(1, &m_paintMatrixBuffer);
            m_state->bindBuffer(GL_SHADER_STORAGE_BUFFER, m_paintMatrixBuffer);
            glBufferData(GL_SHADER_STORAGE_BUFFER,
                         sizeof(pls::ExperimentalAtomicModeData::m_paintMatrices),
                         nullptr,
                         GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                             PAINT_MATRIX_STORAGE_BUFFER_IDX,
                             m_paintMatrixBuffer);

            assert(m_paintTranslateBuffer == 0);
            glGenBuffers(1, &m_paintTranslateBuffer);
            m_state->bindBuffer(GL_SHADER_STORAGE_BUFFER, m_paintTranslateBuffer);
            glBufferData(GL_SHADER_STORAGE_BUFFER,
                         sizeof(pls::ExperimentalAtomicModeData::m_paintTranslates),
                         nullptr,
                         GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                             PAINT_TRANSLATE_STORAGE_BUFFER_IDX,
                             m_paintTranslateBuffer);

            assert(m_clipRectMatrixBuffer == 0);
            glGenBuffers(1, &m_clipRectMatrixBuffer);
            m_state->bindBuffer(GL_SHADER_STORAGE_BUFFER, m_clipRectMatrixBuffer);
            glBufferData(GL_SHADER_STORAGE_BUFFER,
                         sizeof(pls::ExperimentalAtomicModeData::m_clipRectMatrices),
                         nullptr,
                         GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                             CLIPRECT_MATRIX_STORAGE_BUFFER_IDX,
                             m_clipRectMatrixBuffer);

            assert(m_clipRectTranslateBuffer == 0);
            glGenBuffers(1, &m_clipRectTranslateBuffer);
            m_state->bindBuffer(GL_SHADER_STORAGE_BUFFER, m_clipRectTranslateBuffer);
            glBufferData(GL_SHADER_STORAGE_BUFFER,
                         sizeof(pls::ExperimentalAtomicModeData::m_clipRectTranslates),
                         nullptr,
                         GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                             CLIPRECT_TRANSLATE_STORAGE_BUFFER_IDX,
                             m_clipRectTranslateBuffer);
        }

        assert(desc.experimentalAtomicModeData);
        pls::ExperimentalAtomicModeData& atomicModeData = *desc.experimentalAtomicModeData;
        if (m_extensions.ARB_bindless_texture)
        {
            // We support bindless textures, so write out image texture handles.
            for (uint32_t i = 1; i <= desc.pathCount; ++i)
            {
                if (auto imageTextureGL =
                        static_cast<const PLSTextureGLImpl*>(atomicModeData.m_imageTextures[i]))
                {
                    GLuint64 handle = imageTextureGL->bindlessHandle(m_extensions);
                    atomicModeData.m_paintTranslates[i].bindlessTextureHandle[0] = handle;
                    atomicModeData.m_paintTranslates[i].bindlessTextureHandle[1] = handle >> 32;
                }
            }
        }

        // Fill in the atomic mode buffers.
        size_t bufferCount = desc.pathCount + 1;
        m_state->bindBuffer(GL_SHADER_STORAGE_BUFFER, m_paintBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                        0,
                        bufferCount * sizeof(*atomicModeData.m_paints),
                        atomicModeData.m_paints);

        m_state->bindBuffer(GL_SHADER_STORAGE_BUFFER, m_paintMatrixBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                        0,
                        bufferCount * sizeof(*atomicModeData.m_paintMatrices),
                        atomicModeData.m_paintMatrices);

        m_state->bindBuffer(GL_SHADER_STORAGE_BUFFER, m_paintTranslateBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                        0,
                        bufferCount * sizeof(*atomicModeData.m_paintTranslates),
                        atomicModeData.m_paintTranslates);

        if (desc.combinedShaderFeatures & ShaderFeatures::ENABLE_CLIP_RECT)
        {
            m_state->bindBuffer(GL_SHADER_STORAGE_BUFFER, m_clipRectMatrixBuffer);
            glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                            0,
                            bufferCount * sizeof(*atomicModeData.m_clipRectMatrices),
                            atomicModeData.m_clipRectMatrices);

            m_state->bindBuffer(GL_SHADER_STORAGE_BUFFER, m_clipRectTranslateBuffer);
            glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                            0,
                            bufferCount * sizeof(*atomicModeData.m_clipRectTranslates),
                            atomicModeData.m_clipRectTranslates);
        }
    }
#endif // ENABLE_PLS_EXPERIMENTAL_ATOMICS

    // Render the complex color ramps into the gradient texture.
    if (desc.complexGradSpanCount > 0)
    {
        m_state->bindBuffer(GL_ARRAY_BUFFER, gl_buffer_id(gradSpanBufferRing()));
        m_state->bindVAO(m_colorRampVAO);
        m_state->enableFaceCulling(true);
        glVertexAttribIPointer(0, 4, GL_UNSIGNED_INT, 0, nullptr);
        glViewport(0, desc.complexGradRowsTop, kGradTextureWidth, desc.complexGradRowsHeight);
        glBindFramebuffer(GL_FRAMEBUFFER, m_colorRampFBO);
        m_state->bindProgram(m_colorRampProgram);
        GLenum colorAttachment0 = GL_COLOR_ATTACHMENT0;
        glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, &colorAttachment0);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, desc.complexGradSpanCount);
    }

    // Copy the path data to the texture.
    if (desc.pathTexelsHeight > 0)
    {
        m_state->bindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_buffer_id(pathBufferRing()));
        glActiveTexture(GL_TEXTURE0 + kPLSTexIdxOffset + PATH_TEXTURE_IDX);
        glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        0,
                        0,
                        desc.pathTexelsWidth,
                        desc.pathTexelsHeight,
                        GL_RGBA_INTEGER,
                        GL_UNSIGNED_INT,
                        reinterpret_cast<void*>(desc.pathDataOffset));
    }

    // Copy the contour data to the texture.
    if (desc.contourTexelsHeight > 0)
    {
        m_state->bindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_buffer_id(contourBufferRing()));
        glActiveTexture(GL_TEXTURE0 + kPLSTexIdxOffset + CONTOUR_TEXTURE_IDX);
        glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        0,
                        0,
                        desc.contourTexelsWidth,
                        desc.contourTexelsHeight,
                        GL_RGBA_INTEGER,
                        GL_UNSIGNED_INT,
                        reinterpret_cast<void*>(desc.contourDataOffset));
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
                        nullptr);
    }

    // Tessellate all curves into vertices in the tessellation texture.
    if (desc.tessVertexSpanCount > 0)
    {
        m_state->bindBuffer(GL_ARRAY_BUFFER, gl_buffer_id(tessSpanBufferRing()));
        m_state->bindVAO(m_tessellateVAO);
        m_state->enableFaceCulling(true);
        for (uintptr_t i = 0; i < 3; ++i)
        {
            glVertexAttribPointer(i,
                                  4,
                                  GL_FLOAT,
                                  GL_FALSE,
                                  sizeof(TessVertexSpan),
                                  reinterpret_cast<const void*>(i * 4 * 4));
        }
        glVertexAttribIPointer(3,
                               4,
                               GL_UNSIGNED_INT,
                               sizeof(TessVertexSpan),
                               reinterpret_cast<const void*>(offsetof(TessVertexSpan, x0x1)));
        glViewport(0, 0, kTessTextureWidth, desc.tessDataHeight);
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

    // Compile the draw programs before activating pixel local storage.
    // (ANGLE_shader_pixel_local_storage doesn't allow shader compilation while active.)
    for (const Draw& draw : *desc.drawList)
    {
        // Compile the draw program before activating pixel local storage.
        // Cache specific compilations by DrawType and ShaderFeatures.
        auto shaderFeatures = desc.interlockMode == pls::InterlockMode::experimentalAtomics
                                  ? desc.combinedShaderFeatures
                                  : draw.shaderFeatures;
        uint32_t fragmentShaderKey =
            pls::ShaderUniqueKey(draw.drawType, shaderFeatures, desc.interlockMode);
        m_drawPrograms.try_emplace(fragmentShaderKey,
                                   this,
                                   draw.drawType,
                                   shaderFeatures,
                                   desc.interlockMode);
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
    if (m_extensions.ANGLE_polygon_mode && desc.wireframe)
    {
        glPolygonModeANGLE(GL_FRONT_AND_BACK, GL_LINE_ANGLE);
        glLineWidth(2);
    }
#endif

    m_plsImpl->activatePixelLocalStorage(this, desc);

    // Execute the DrawList.
    for (const Draw& draw : *desc.drawList)
    {
        if (draw.elementCount == 0)
        {
            continue;
        }

        auto shaderFeatures = desc.interlockMode == pls::InterlockMode::experimentalAtomics
                                  ? desc.combinedShaderFeatures
                                  : draw.shaderFeatures;
        uint32_t fragmentShaderKey =
            pls::ShaderUniqueKey(draw.drawType, shaderFeatures, desc.interlockMode);
        const DrawProgram& drawProgram = m_drawPrograms.find(fragmentShaderKey)->second;
        if (drawProgram.id() == 0)
        {
            continue;
        }
        m_state->bindProgram(drawProgram.id());

        if (auto imageTextureGL = static_cast<const PLSTextureGLImpl*>(draw.imageTexture))
        {
            glActiveTexture(GL_TEXTURE0 + kPLSTexIdxOffset + IMAGE_TEXTURE_IDX);
            glBindTexture(GL_TEXTURE_2D, imageTextureGL->id());
        }

        switch (DrawType drawType = draw.drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::outerCurvePatches:
            {
                // Draw PLS patches that connect the tessellation vertices.
                m_plsImpl->ensureRasterOrderingEnabled(desc.interlockMode ==
                                                       pls::InterlockMode::rasterOrdered);
                m_state->bindVAO(m_drawVAO);
                m_state->enableFaceCulling(true);
                uint32_t indexCount = PatchIndexCount(drawType);
                uint32_t baseIndex = PatchBaseIndex(drawType);
                void* indexOffset = reinterpret_cast<void*>(baseIndex * sizeof(uint16_t));
                if (m_extensions.ANGLE_base_vertex_base_instance_shader_builtin)
                {
                    glDrawElementsInstancedBaseInstanceEXT(GL_TRIANGLES,
                                                           indexCount,
                                                           GL_UNSIGNED_SHORT,
                                                           indexOffset,
                                                           draw.elementCount,
                                                           draw.baseElement);
                }
                else
                {
                    glUniform1i(drawProgram.spirvCrossBaseInstanceLocation(), draw.baseElement);
                    glDrawElementsInstanced(GL_TRIANGLES,
                                            indexCount,
                                            GL_UNSIGNED_SHORT,
                                            indexOffset,
                                            draw.elementCount);
                }
                break;
            }
            case DrawType::interiorTriangulation:
            {
                m_plsImpl->ensureRasterOrderingEnabled(false);
                m_state->bindVAO(m_interiorTrianglesVAO);
                m_state->enableFaceCulling(true);
                glDrawArrays(GL_TRIANGLES, draw.baseElement, draw.elementCount);
                // Atomic mode inserts barriers after every draw.
                if (desc.interlockMode == pls::InterlockMode::rasterOrdered)
                {
                    m_plsImpl->barrier();
                }
                break;
            }
            case DrawType::imageRect:
            {
                assert(!m_extensions.ARB_bindless_texture);
                assert(m_imageRectVAO != 0); // Should have gotten lazily allocated by now.
                m_plsImpl->ensureRasterOrderingEnabled(false);
                m_state->bindVAO(m_imageRectVAO);
                glBindBufferRange(GL_UNIFORM_BUFFER,
                                  IMAGE_DRAW_UNIFORM_BUFFER_IDX,
                                  gl_buffer_id(imageDrawUniformBufferRing()),
                                  draw.imageDrawDataOffset,
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
                m_plsImpl->ensureRasterOrderingEnabled(desc.interlockMode ==
                                                       pls::InterlockMode::rasterOrdered);
                LITE_RTTI_CAST_OR_BREAK(vertexBuffer,
                                        const PLSRenderBufferGLImpl*,
                                        draw.vertexBuffer);
                LITE_RTTI_CAST_OR_BREAK(uvBuffer, const PLSRenderBufferGLImpl*, draw.uvBuffer);
                LITE_RTTI_CAST_OR_BREAK(indexBuffer,
                                        const PLSRenderBufferGLImpl*,
                                        draw.indexBuffer);
                m_state->bindVAO(m_imageMeshVAO);
                m_state->bindBuffer(GL_ARRAY_BUFFER, vertexBuffer->submittedBufferID());
                glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
                m_state->bindBuffer(GL_ARRAY_BUFFER, uvBuffer->submittedBufferID());
                glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
                m_state->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->submittedBufferID());
                glBindBufferRange(GL_UNIFORM_BUFFER,
                                  IMAGE_DRAW_UNIFORM_BUFFER_IDX,
                                  gl_buffer_id(imageDrawUniformBufferRing()),
                                  draw.imageDrawDataOffset,
                                  sizeof(pls::ImageDrawUniforms));
                m_state->enableFaceCulling(false);
                glDrawElements(GL_TRIANGLES,
                               draw.elementCount,
                               GL_UNSIGNED_SHORT,
                               reinterpret_cast<const void*>(draw.baseElement * sizeof(uint16_t)));
                break;
            }
            case DrawType::plsAtomicResolve:
                m_plsImpl->ensureRasterOrderingEnabled(false);
                m_state->bindVAO(m_plsResolveVAO);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                break;
        }
        if (desc.interlockMode == pls::InterlockMode::experimentalAtomics)
        {
            m_plsImpl->barrier();
        }
    }

    m_plsImpl->deactivatePixelLocalStorage(this);

#ifdef RIVE_DESKTOP_GL
    if (m_extensions.ANGLE_polygon_mode && desc.wireframe)
    {
        glPolygonModeANGLE(GL_FRONT_AND_BACK, GL_FILL_ANGLE);
    }
#endif
}

std::unique_ptr<PLSRenderContext> PLSRenderContextGLImpl::MakeContext()
{
    GLExtensions extensions{};
    GLint extensionCount;
    glGetIntegerv(GL_NUM_EXTENSIONS, &extensionCount);
    for (int i = 0; i < extensionCount; ++i)
    {
        auto* ext = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i));
        if (strcmp(ext, "GL_ANGLE_base_vertex_base_instance_shader_builtin") == 0)
        {
            extensions.ANGLE_base_vertex_base_instance_shader_builtin = true;
        }
        if (strcmp(ext, "GL_ANGLE_shader_pixel_local_storage") == 0)
        {
            extensions.ANGLE_shader_pixel_local_storage = true;
        }
        else if (strcmp(ext, "GL_ANGLE_shader_pixel_local_storage_coherent") == 0)
        {
            extensions.ANGLE_shader_pixel_local_storage_coherent = true;
        }
        else if (strcmp(ext, "GL_ANGLE_provoking_vertex") == 0)
        {
            extensions.ANGLE_provoking_vertex = true;
        }
        else if (strcmp(ext, "GL_ANGLE_polygon_mode") == 0)
        {
            extensions.ANGLE_polygon_mode = true;
        }
        else if (strcmp(ext, "GL_ARM_shader_framebuffer_fetch") == 0)
        {
            extensions.ARM_shader_framebuffer_fetch = true;
        }
        else if (strcmp(ext, "GL_ARB_fragment_shader_interlock") == 0)
        {
            extensions.ARB_fragment_shader_interlock = true;
        }
        else if (strcmp(ext, "GL_EXT_base_instance") == 0)
        {
            extensions.EXT_base_instance = true;
        }
        else if (strcmp(ext, "GL_INTEL_fragment_shader_ordering") == 0)
        {
            extensions.INTEL_fragment_shader_ordering = true;
        }
        else if (strcmp(ext, "GL_EXT_shader_framebuffer_fetch") == 0)
        {
            extensions.EXT_shader_framebuffer_fetch = true;
        }
        else if (strcmp(ext, "GL_EXT_shader_pixel_local_storage") == 0)
        {
            extensions.EXT_shader_pixel_local_storage = true;
        }
        else if (strcmp(ext, "GL_QCOM_shader_framebuffer_fetch_noncoherent") == 0)
        {
            extensions.QCOM_shader_framebuffer_fetch_noncoherent = true;
        }
    }
#ifdef RIVE_DESKTOP_GL
    // We implement some ES extensions with core Desktop GL in glad_custom.c.
    if (GLAD_GL_ARB_bindless_texture)
    {
        extensions.ARB_bindless_texture = true;
    }
    if (GLAD_GL_ANGLE_base_vertex_base_instance_shader_builtin)
    {
        extensions.ANGLE_base_vertex_base_instance_shader_builtin = true;
    }
    if (GLAD_GL_ANGLE_polygon_mode)
    {
        extensions.ANGLE_polygon_mode = true;
    }
    if (GLAD_GL_EXT_base_instance)
    {
        extensions.EXT_base_instance = true;
    }
#endif

    GLenum rendererToken = GL_RENDERER;
#ifdef RIVE_WEBGL
    if (emscripten_webgl_enable_extension(emscripten_webgl_get_current_context(),
                                          "WEBGL_debug_renderer_info"))
    {
        rendererToken = GL_UNMASKED_RENDERER_WEBGL;
    }
#endif
    const char* rendererString = reinterpret_cast<const char*>(glGetString(rendererToken));
    if (strstr(rendererString, "Direct3D"))
    {
        // This extension is polyfilled on D3D anyway. Just don't use it so we can make sure to test
        // our own fallback.
        extensions.ANGLE_base_vertex_base_instance_shader_builtin = false;
    }

#ifdef RIVE_GLES
    loadGLESExtensions(extensions); // Android doesn't load extension functions for us.
    if (extensions.EXT_shader_pixel_local_storage &&
        (extensions.ARM_shader_framebuffer_fetch || extensions.EXT_shader_framebuffer_fetch))
    {
        return MakeContext(rendererString, extensions, MakePLSImplEXTNative(extensions));
    }

    if (extensions.EXT_shader_framebuffer_fetch)
    {
        return MakeContext(rendererString, extensions, MakePLSImplFramebufferFetch(extensions));
    }
#endif

#ifdef RIVE_DESKTOP_GL
    if (extensions.ANGLE_shader_pixel_local_storage_coherent)
    {
        return MakeContext(rendererString, extensions, MakePLSImplWebGL());
    }

    if (extensions.ARB_fragment_shader_interlock || extensions.INTEL_fragment_shader_ordering)
    {
        return MakeContext(rendererString, extensions, MakePLSImplRWTexture());
    }
#endif

#ifdef RIVE_WEBGL
    if (emscripten_webgl_enable_WEBGL_shader_pixel_local_storage(
            emscripten_webgl_get_current_context()) &&
        emscripten_webgl_shader_pixel_local_storage_is_coherent())
    {
        return MakeContext(rendererString, extensions, MakePLSImplWebGL());
    }
#endif

    fprintf(stderr, "Pixel local storage is not supported.\n");
    return nullptr;
}

std::unique_ptr<PLSRenderContext> PLSRenderContextGLImpl::MakeContext(
    const char* rendererString,
    GLExtensions extensions,
    std::unique_ptr<PLSImpl> plsImpl)
{
    auto plsContextImpl = std::unique_ptr<PLSRenderContextGLImpl>(
        new PLSRenderContextGLImpl(rendererString, extensions, std::move(plsImpl)));
    return std::make_unique<PLSRenderContext>(std::move(plsContextImpl));
}
} // namespace rive::pls
