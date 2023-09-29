/*
 * Copyright 2022 Rive
 */

#include "rive/pls/gl/pls_render_context_gl_impl.hpp"

#include "buffer_ring_gl.hpp"
#include "gl_utils.hpp"
#include "pls_path.hpp"
#include "pls_paint.hpp"
#include "rive/pls/gl/pls_render_buffer_gl_impl.hpp"
#include "rive/pls/pls_image.hpp"
#include "shaders/constants.glsl"
#include <sstream>

#include "../out/obj/generated/advanced_blend.glsl.hpp"
#include "../out/obj/generated/color_ramp.glsl.hpp"
#include "../out/obj/generated/constants.glsl.hpp"
#include "../out/obj/generated/common.glsl.hpp"
#include "../out/obj/generated/draw_path.glsl.hpp"
#include "../out/obj/generated/draw_image_mesh.glsl.hpp"
#include "../out/obj/generated/tessellate.glsl.hpp"

// Offset all PLS texture indices by 1 so we, and others who share our GL context, can use
// GL_TEXTURE0 as a scratch texture index.
constexpr static int kPLSTexIdxOffset = 1;

namespace rive::pls
{
#ifdef RIVE_WEBGL
EM_JS(void, set_provoking_vertex_webgl, (GLenum convention), {
    const ext = Module["ctx"].getExtension("WEBGL_provoking_vertex");
    if (ext)
    {
        ext.provokingVertexWEBGL(convention);
    }
});
#endif

PLSRenderContextGLImpl::PLSRenderContextGLImpl(const char* rendererString,
                                               GLExtensions extensions,
                                               std::unique_ptr<PLSImpl> plsImpl) :
    m_extensions(extensions), m_plsImpl(std::move(plsImpl))

{
    if (strstr(rendererString, "Apple") && strstr(rendererString, "Metal"))
    {
        // In Metal, non-flat varyings preserve their exact value if all vertices in the triangle
        // emit the same value, and we also see a small (5-10%) improvement from not using flat
        // varyings.
        m_platformFeatures.avoidFlatVaryings = true;
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

    glGenBuffers(1, &m_flushUniformBuffer);
    m_state->bindBuffer(GL_UNIFORM_BUFFER, m_flushUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(pls::FlushUniforms), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, FLUSH_UNIFORM_BUFFER_IDX, m_flushUniformBuffer);

    glGenBuffers(1, &m_imageMeshUniformBuffer);
    m_state->bindBuffer(GL_UNIFORM_BUFFER, m_imageMeshUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(pls::ImageMeshUniforms), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, IMAGE_MESH_UNIFORM_BUFFER_IDX, m_imageMeshUniformBuffer);

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

    glFrontFace(GL_CW);
    glCullFace(GL_BACK);

    // ANGLE_shader_pixel_local_storage doesn't allow dither.
    glDisable(GL_DITHER);

    // D3D and Metal both have a provoking vertex convention of "first" for flat varyings, and it's
    // very costly for ANGLE to implement the OpenGL convention of "last" on these backends. To
    // workaround this, ANGLE provides the ANGLE_provoking_vertex extension. When this extension is
    // present, we can just set the provoking vertex to "first" and trust that it will be fast.
#ifdef RIVE_WEBGL
    set_provoking_vertex_webgl(GL_FIRST_VERTEX_CONVENTION_WEBGL);
#elif defined(RIVE_DESKTOP_GL)
    if (m_extensions.ANGLE_provoking_vertex)
    {
        glProvokingVertexANGLE(GL_FIRST_VERTEX_CONVENTION_ANGLE);
    }
#endif

    m_plsImpl->init(m_state);
}

PLSRenderContextGLImpl::~PLSRenderContextGLImpl()
{
    m_state->deleteBuffer(m_flushUniformBuffer);
    m_state->deleteBuffer(m_imageMeshUniformBuffer);

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

    m_state->deleteVAO(m_imageMeshVAO);
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

    ~PLSTextureGLImpl() override { glDeleteTextures(1, &m_id); }

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
    return make_rcp<PLSTextureGLImpl>(width, height, mipLevelCount, imageDataRGBA, m_state);
}

std::unique_ptr<TexelBufferRing> PLSRenderContextGLImpl::makeTexelBufferRing(
    TexelBufferRing::Format format,
    size_t widthInItems,
    size_t height,
    size_t texelsPerItem,
    int textureIdx,
    TexelBufferRing::Filter filter)
{
    return std::make_unique<TexelBufferGL>(format,
                                           widthInItems,
                                           height,
                                           texelsPerItem,
                                           GL_TEXTURE0 + kPLSTexIdxOffset + textureIdx,
                                           filter,
                                           m_state);
}

std::unique_ptr<BufferRing> PLSRenderContextGLImpl::makeVertexBufferRing(size_t capacity,
                                                                         size_t itemSizeInBytes)
{
    return std::make_unique<BufferGL>(GL_ARRAY_BUFFER, capacity, itemSizeInBytes, m_state);
}

std::unique_ptr<BufferRing> PLSRenderContextGLImpl::makePixelUnpackBufferRing(
    size_t capacity,
    size_t itemSizeInBytes)
{
    return std::make_unique<BufferGL>(GL_PIXEL_UNPACK_BUFFER, capacity, itemSizeInBytes, m_state);
}

std::unique_ptr<BufferRing> PLSRenderContextGLImpl::makeUniformBufferRing(size_t capacity,
                                                                          size_t sizeInBytes)
{
    // In GL we update uniform data inline with commands, rather than filling a buffer up front.
    return std::make_unique<HeapBufferRing>(capacity, sizeInBytes);
}

void PLSRenderContextGLImpl::resizeGradientTexture(size_t height)
{
    glDeleteTextures(1, &m_gradientTexture);

    glGenTextures(1, &m_gradientTexture);
    glActiveTexture(GL_TEXTURE0 + kPLSTexIdxOffset + GRAD_TEXTURE_IDX);
    glBindTexture(GL_TEXTURE_2D, m_gradientTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, kGradTextureWidth, height);
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

void PLSRenderContextGLImpl::resizeTessellationTexture(size_t height)
{
    glDeleteTextures(1, &m_tessVertexTexture);

    glGenTextures(1, &m_tessVertexTexture);
    glActiveTexture(GL_TEXTURE0 + kPLSTexIdxOffset + TESS_VERTEX_TEXTURE_IDX);
    glBindTexture(GL_TEXTURE_2D, m_tessVertexTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32UI, kTessTextureWidth, height);
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
               ShaderFeatures shaderFeatures)
    {
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
                sources.push_back(pls::glsl::draw_path);
                break;
            case DrawType::interiorTriangulation:
                defines.push_back(GLSL_DRAW_INTERIOR_TRIANGLES);
                sources.push_back(pls::glsl::draw_path);
                break;
            case DrawType::imageMesh:
                sources.push_back(pls::glsl::draw_image_mesh);
                break;
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
    GLuint m_id;
};

PLSRenderContextGLImpl::DrawProgram::DrawProgram(PLSRenderContextGLImpl* plsContextImpl,
                                                 DrawType drawType,
                                                 ShaderFeatures shaderFeatures) :
    m_state(plsContextImpl->m_state)
{
    m_id = glCreateProgram();

    // Not every vertex shader is unique. Cache them by just the vertex features and reuse when
    // possible.
    ShaderFeatures vertexShaderFeatures = shaderFeatures & kVertexShaderFeaturesMask;
    uint32_t vertexShaderKey = ShaderUniqueKey(drawType, vertexShaderFeatures);
    const DrawShader& vertexShader = plsContextImpl->m_vertexShaders
                                         .try_emplace(vertexShaderKey,
                                                      plsContextImpl,
                                                      GL_VERTEX_SHADER,
                                                      drawType,
                                                      vertexShaderFeatures)
                                         .first->second;
    glAttachShader(m_id, vertexShader.id());

    // Every fragment shader is unique.
    DrawShader fragmentShader(plsContextImpl, GL_FRAGMENT_SHADER, drawType, shaderFeatures);
    glAttachShader(m_id, fragmentShader.id());

    glutils::LinkProgram(m_id);

    m_state->bindProgram(m_id);
    glUniformBlockBinding(m_id,
                          glGetUniformBlockIndex(m_id, GLSL_Uniforms),
                          FLUSH_UNIFORM_BUFFER_IDX);
    if (drawType == DrawType::imageMesh)
    {
        glUniformBlockBinding(m_id,
                              glGetUniformBlockIndex(m_id, GLSL_MeshUniforms),
                              IMAGE_MESH_UNIFORM_BUFFER_IDX);
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
    return static_cast<const BufferGL*>(bufferRing)->submittedBufferID();
}

static const void* heap_buffer_contents(const BufferRing* bufferRing)
{
    return static_cast<const HeapBufferRing*>(bufferRing)->contents();
}

void PLSRenderContextGLImpl::flush(const PLSRenderContext::FlushDescriptor& desc)
{
    // All programs use the same set of per-flush uniforms.
    m_state->bindBuffer(GL_UNIFORM_BUFFER, m_flushUniformBuffer);
    glBufferSubData(GL_UNIFORM_BUFFER,
                    0,
                    sizeof(pls::FlushUniforms),
                    heap_buffer_contents(flushUniformBufferRing()));

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
        uint32_t fragmentShaderKey = ShaderUniqueKey(draw.drawType, draw.shaderFeatures);
        m_drawPrograms.try_emplace(fragmentShaderKey, this, draw.drawType, draw.shaderFeatures);
    }

    // Bind the currently-submitted buffer in the triangleBufferRing to its vertex array.
    if (desc.hasTriangleVertices)
    {
        m_state->bindVAO(m_interiorTrianglesVAO);
        m_state->bindBuffer(GL_ARRAY_BUFFER, gl_buffer_id(triangleBufferRing()));
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    }

    auto renderTarget = static_cast<const PLSRenderTargetGL*>(desc.renderTarget);
    glViewport(0, 0, renderTarget->width(), renderTarget->height());

#ifdef RIVE_DESKTOP_GL
    if (m_extensions.ANGLE_polygon_mode && desc.wireframe)
    {
        glPolygonModeANGLE(GL_FRONT_AND_BACK, GL_LINE_ANGLE);
        glLineWidth(2);
    }
#endif

    m_plsImpl->activatePixelLocalStorage(this, desc);

    auto imageMeshUniformData = static_cast<const pls::ImageMeshUniforms*>(
        heap_buffer_contents(imageMeshUniformBufferRing()));

    // Execute the DrawList.
    for (const Draw& draw : *desc.drawList)
    {
        if (draw.elementCount == 0)
        {
            continue;
        }

        uint32_t fragmentShaderKey = ShaderUniqueKey(draw.drawType, draw.shaderFeatures);
        const DrawProgram& drawProgram = m_drawPrograms.find(fragmentShaderKey)->second;
        m_state->bindProgram(drawProgram.id());

        if (auto imageTextureGL = static_cast<const PLSTextureGLImpl*>(draw.imageTextureRef))
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
                m_plsImpl->ensureRasterOrderingEnabled(true);
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
                m_plsImpl->barrier();
                break;
            }
            case DrawType::imageMesh:
            {
                auto vertexBuffer = static_cast<const PLSRenderBufferGLImpl*>(draw.vertexBufferRef);
                auto uvBuffer = static_cast<const PLSRenderBufferGLImpl*>(draw.uvBufferRef);
                auto indexBuffer = static_cast<const PLSRenderBufferGLImpl*>(draw.indexBufferRef);
                m_state->bindVAO(m_imageMeshVAO);
                m_state->bindBuffer(GL_ARRAY_BUFFER, vertexBuffer->submittedBufferID());
                glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
                m_state->bindBuffer(GL_ARRAY_BUFFER, uvBuffer->submittedBufferID());
                glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
                m_state->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->submittedBufferID());
                m_state->bindBuffer(GL_UNIFORM_BUFFER, m_imageMeshUniformBuffer);
                glBufferSubData(GL_UNIFORM_BUFFER,
                                0,
                                sizeof(pls::ImageMeshUniforms),
                                imageMeshUniformData++);
                m_state->enableFaceCulling(false);
                glDrawElements(GL_TRIANGLES,
                               draw.elementCount,
                               GL_UNSIGNED_SHORT,
                               reinterpret_cast<const void*>(draw.baseElement * sizeof(uint16_t)));
                break;
            }
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
