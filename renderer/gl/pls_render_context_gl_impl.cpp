/*
 * Copyright 2022 Rive
 */

#include "rive/pls/gl/pls_render_context_gl_impl.hpp"

#include "buffer_ring_gl.hpp"
#include "gl_utils.hpp"
#include "pls_path.hpp"
#include "pls_paint.hpp"
#include "rive/pls/pls_image.hpp"
#include <sstream>

#include "../out/obj/generated/advanced_blend.glsl.hpp"
#include "../out/obj/generated/color_ramp.glsl.hpp"
#include "../out/obj/generated/common.glsl.hpp"
#include "../out/obj/generated/draw.glsl.hpp"
#include "../out/obj/generated/tessellate.glsl.hpp"

// Offset all PLS texture indices by 1 so we, and others who share our GL context, can use
// GL_TEXTURE0 as a scratch texture index.
constexpr static int kPLSTexIdxOffset = 1;

namespace rive::pls
{
#ifdef RIVE_WASM
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
    const char* colorRampSources[] = {glsl::common, glsl::color_ramp};
    glutils::CompileAndAttachShader(m_colorRampProgram,
                                    GL_VERTEX_SHADER,
                                    nullptr,
                                    0,
                                    colorRampSources,
                                    2,
                                    m_extensions,
                                    m_shaderVersionString);
    glutils::CompileAndAttachShader(m_colorRampProgram,
                                    GL_FRAGMENT_SHADER,
                                    nullptr,
                                    0,
                                    colorRampSources,
                                    2,
                                    m_extensions,
                                    m_shaderVersionString);
    glutils::LinkProgram(m_colorRampProgram);
    glUniformBlockBinding(m_colorRampProgram,
                          glGetUniformBlockIndex(m_colorRampProgram, GLSL_Uniforms),
                          0);

    glGenVertexArrays(1, &m_colorRampVAO);
    bindVAO(m_colorRampVAO);
    glEnableVertexAttribArray(0);
    glVertexAttribDivisor(0, 1);

    glGenFramebuffers(1, &m_colorRampFBO);

    m_tessellateProgram = glCreateProgram();
    const char* tessellateSources[] = {glsl::common, glsl::tessellate};
    glutils::CompileAndAttachShader(m_tessellateProgram,
                                    GL_VERTEX_SHADER,
                                    nullptr,
                                    0,
                                    tessellateSources,
                                    2,
                                    m_extensions,
                                    m_shaderVersionString);
    glutils::CompileAndAttachShader(m_tessellateProgram,
                                    GL_FRAGMENT_SHADER,
                                    nullptr,
                                    0,
                                    tessellateSources,
                                    2,
                                    m_extensions,
                                    m_shaderVersionString);
    glutils::LinkProgram(m_tessellateProgram);
    bindProgram(m_tessellateProgram);
    glUniformBlockBinding(m_tessellateProgram,
                          glGetUniformBlockIndex(m_tessellateProgram, GLSL_Uniforms),
                          0);
    glUniform1i(glGetUniformLocation(m_tessellateProgram, GLSL_pathTexture),
                kPLSTexIdxOffset + kPathTextureIdx);
    glUniform1i(glGetUniformLocation(m_tessellateProgram, GLSL_contourTexture),
                kPLSTexIdxOffset + kContourTextureIdx);

    glGenVertexArrays(1, &m_tessellateVAO);
    bindVAO(m_tessellateVAO);
    for (int i = 0; i < 4; ++i)
    {
        glEnableVertexAttribArray(i);
        // Draw two instances per TessVertexSpan: one normal and one optional reflection.
        glVertexAttribDivisor(i, 2);
    }

    glGenFramebuffers(1, &m_tessellateFBO);

    glGenVertexArrays(1, &m_drawVAO);
    bindVAO(m_drawVAO);

    PatchVertex patchVertices[kPatchVertexBufferCount];
    uint16_t patchIndices[kPatchIndexBufferCount];
    GeneratePatchBufferData(patchVertices, patchIndices);

    glGenBuffers(1, &m_patchVerticesBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_patchVerticesBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(patchVertices), patchVertices, GL_STATIC_DRAW);

    glGenBuffers(1, &m_patchIndicesBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_patchIndicesBuffer);
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
    bindVAO(m_interiorTrianglesVAO);
    glEnableVertexAttribArray(0);

    glFrontFace(GL_CW);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);

    // ANGLE_shader_pixel_local_storage doesn't allow dither.
    glDisable(GL_DITHER);

    // D3D and Metal both have a provoking vertex convention of "first" for flat varyings, and it's
    // very costly for ANGLE to implement the OpenGL convention of "last" on these backends. To
    // workaround this, ANGLE provides the ANGLE_provoking_vertex extension. When this extension is
    // present, we can just set the provoking vertex to "first" and trust that it will be fast.
#ifdef RIVE_WASM
    set_provoking_vertex_webgl(GL_FIRST_VERTEX_CONVENTION_WEBGL);
#elif defined(RIVE_DESKTOP_GL)
    if (m_extensions.ANGLE_provoking_vertex)
    {
        glProvokingVertexANGLE(GL_FIRST_VERTEX_CONVENTION_ANGLE);
    }
#endif
}

PLSRenderContextGLImpl::~PLSRenderContextGLImpl()
{
    glDeleteProgram(m_colorRampProgram);
    glDeleteVertexArrays(1, &m_colorRampVAO);
    glDeleteFramebuffers(1, &m_colorRampFBO);
    glDeleteTextures(1, &m_gradientTexture);

    glDeleteProgram(m_tessellateProgram);
    glDeleteVertexArrays(1, &m_tessellateVAO);
    glDeleteFramebuffers(1, &m_tessellateFBO);
    glDeleteTextures(1, &m_tessVertexTexture);

    glDeleteVertexArrays(1, &m_drawVAO);
    glDeleteBuffers(1, &m_patchVerticesBuffer);
    glDeleteBuffers(1, &m_patchIndicesBuffer);
}

class PLSTextureGLImpl : public PLSTexture
{
public:
    PLSTextureGLImpl(uint32_t width,
                     uint32_t height,
                     uint32_t mipLevelCount,
                     const uint8_t imageDataRGBA[]) :
        PLSTexture(width, height)
    {
        glGenTextures(1, &m_id);
        glActiveTexture(GL_TEXTURE0 + kPLSTexIdxOffset + kImageTextureIdx);
        glBindTexture(GL_TEXTURE_2D, m_id);
        glTexStorage2D(GL_TEXTURE_2D, mipLevelCount, GL_RGBA8, width, height);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
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
    GLuint m_id = 0;
};

rcp<PLSTexture> PLSRenderContextGLImpl::makeImageTexture(uint32_t width,
                                                         uint32_t height,
                                                         uint32_t mipLevelCount,
                                                         const uint8_t imageDataRGBA[])
{
    return make_rcp<PLSTextureGLImpl>(width, height, mipLevelCount, imageDataRGBA);
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
                                           filter);
}

std::unique_ptr<BufferRing> PLSRenderContextGLImpl::makeVertexBufferRing(size_t capacity,
                                                                         size_t itemSizeInBytes)
{
    return std::make_unique<BufferGL>(GL_ARRAY_BUFFER, capacity, itemSizeInBytes);
}

std::unique_ptr<BufferRing> PLSRenderContextGLImpl::makePixelUnpackBufferRing(
    size_t capacity,
    size_t itemSizeInBytes)
{
    return std::make_unique<BufferGL>(GL_PIXEL_UNPACK_BUFFER, capacity, itemSizeInBytes);
}

std::unique_ptr<BufferRing> PLSRenderContextGLImpl::makeUniformBufferRing(size_t sizeInBytes)
{
    return std::make_unique<BufferGL>(GL_UNIFORM_BUFFER, 1, sizeInBytes);
}

void PLSRenderContextGLImpl::resizeGradientTexture(size_t height)
{
    glDeleteTextures(1, &m_gradientTexture);

    glGenTextures(1, &m_gradientTexture);
    glActiveTexture(GL_TEXTURE0 + kPLSTexIdxOffset + kGradTextureIdx);
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
    glActiveTexture(GL_TEXTURE0 + kPLSTexIdxOffset + kTessVertexTextureIdx);
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

// Wraps a compiled GL shader of draw.glsl, either vertex or fragment, with a specific set of
// features enabled via #define. The set of features to enable is dictated by ShaderFeatures.
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
        if (drawType == DrawType::interiorTriangulation)
        {
            defines.push_back(GLSL_DRAW_INTERIOR_TRIANGLES);
        }
        for (size_t i = 0; i < kShaderFeatureCount; ++i)
        {
            if (shaderFeatures[i])
            {
                assert(kVertexShaderFeaturesMask[i] || shaderType == GL_FRAGMENT_SHADER);
                defines.push_back(kShaderFeatureGLSLNames[i]);
            }
        }

        std::vector<const char*> sources;
        sources.push_back(glsl::common);
        if (shaderType == GL_FRAGMENT_SHADER &&
            shaderFeatures[ShaderFeatureFlags::ENABLE_ADVANCED_BLEND])
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
        sources.push_back(glsl::draw);
        m_id = glutils::CompileShader(shaderType,
                                      defines.data(),
                                      defines.size(),
                                      sources.data(),
                                      sources.size(),
                                      plsContextImpl->m_extensions,
                                      plsContextImpl->m_shaderVersionString);
    }

    ~DrawShader() { glDeleteShader(m_id); }

    GLuint id() const { return m_id; }

private:
    GLuint m_id;
};

PLSRenderContextGLImpl::DrawProgram::DrawProgram(PLSRenderContextGLImpl* plsContextImpl,
                                                 DrawType drawType,
                                                 ShaderFeatures shaderFeatures)
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

    plsContextImpl->bindProgram(m_id);
    glUniformBlockBinding(m_id, glGetUniformBlockIndex(m_id, GLSL_Uniforms), 0);
    glUniform1i(glGetUniformLocation(m_id, GLSL_tessVertexTexture),
                kPLSTexIdxOffset + kTessVertexTextureIdx);
    glUniform1i(glGetUniformLocation(m_id, GLSL_pathTexture), kPLSTexIdxOffset + kPathTextureIdx);
    glUniform1i(glGetUniformLocation(m_id, GLSL_contourTexture),
                kPLSTexIdxOffset + kContourTextureIdx);
    glUniform1i(glGetUniformLocation(m_id, GLSL_gradTexture), kPLSTexIdxOffset + kGradTextureIdx);
    glUniform1i(glGetUniformLocation(m_id, GLSL_imageTexture), kPLSTexIdxOffset + kImageTextureIdx);
    if (!plsContextImpl->m_extensions.ANGLE_base_vertex_base_instance_shader_builtin)
    {
        m_baseInstancePolyfillLocation = glGetUniformLocation(m_id, GLSL_baseInstancePolyfill);
    }
}

PLSRenderContextGLImpl::DrawProgram::~DrawProgram() { glDeleteProgram(m_id); }

static GLuint gl_buffer_id(const BufferRing* bufferRing)
{
    return static_cast<const BufferGL*>(bufferRing)->submittedBufferID();
}

void PLSRenderContextGLImpl::flush(const PLSRenderContext::FlushDescriptor& desc)
{
    // All programs use the same set of per-flush uniforms.
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, gl_buffer_id(uniformBufferRing()));

    // Render the complex color ramps into the gradient texture.
    if (desc.complexGradSpanCount > 0)
    {
        glBindBuffer(GL_ARRAY_BUFFER, gl_buffer_id(gradSpanBufferRing()));
        bindVAO(m_colorRampVAO);
        glVertexAttribIPointer(0, 4, GL_UNSIGNED_INT, 0, nullptr);
        glViewport(0, desc.complexGradRowsTop, kGradTextureWidth, desc.complexGradRowsHeight);
        glBindFramebuffer(GL_FRAMEBUFFER, m_colorRampFBO);
        bindProgram(m_colorRampProgram);
        GLenum colorAttachment0 = GL_COLOR_ATTACHMENT0;
        glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, &colorAttachment0);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, desc.complexGradSpanCount);
    }

    // Copy the simple color ramps to the gradient texture.
    if (desc.simpleGradTexelsHeight > 0)
    {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_buffer_id(simpleColorRampsBufferRing()));
        glActiveTexture(GL_TEXTURE0 + kPLSTexIdxOffset + kGradTextureIdx);
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
        glBindBuffer(GL_ARRAY_BUFFER, gl_buffer_id(tessSpanBufferRing()));
        bindVAO(m_tessellateVAO);
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
        bindProgram(m_tessellateProgram);
        GLenum colorAttachment0 = GL_COLOR_ATTACHMENT0;
        glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, &colorAttachment0);
        // Draw two instances per TessVertexSpan: one normal and one optional reflection.
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, desc.tessVertexSpanCount * 2);
    }

    // Compile the draw programs before activating pixel local storage.
    // (ANGLE_shader_pixel_local_storage doesn't allow shader compilation while active.)
    for (const Draw& draw : *desc.drawList)
    {
        // Compile the draw program before activating pixel local storage.
        // Cache specific compilations of draw.glsl by ShaderFeatures.
        uint32_t fragmentShaderKey = ShaderUniqueKey(draw.drawType, draw.shaderFeatures);
        m_drawPrograms.try_emplace(fragmentShaderKey, this, draw.drawType, draw.shaderFeatures);
    }

    // Bind the currently-submitted buffer in the triangleBufferRing to its vertex array.
    if (desc.hasTriangleVertices)
    {
        bindVAO(m_interiorTrianglesVAO);
        glBindBuffer(GL_ARRAY_BUFFER, gl_buffer_id(triangleBufferRing()));
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

    // Execute the DrawList.
    for (const Draw& draw : *desc.drawList)
    {
        if (draw.vertexOrInstanceCount == 0)
        {
            continue;
        }

        uint32_t fragmentShaderKey = ShaderUniqueKey(draw.drawType, draw.shaderFeatures);
        const DrawProgram& drawProgram = m_drawPrograms.find(fragmentShaderKey)->second;
        bindProgram(drawProgram.id());

        if (auto imageTextureGL = static_cast<const PLSTextureGLImpl*>(draw.imageTextureRef))
        {
            glActiveTexture(GL_TEXTURE0 + kPLSTexIdxOffset + kImageTextureIdx);
            glBindTexture(GL_TEXTURE_2D, imageTextureGL->id());
        }

        switch (DrawType drawType = draw.drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::outerCurvePatches:
            {
                // Draw PLS patches that connect the tessellation vertices.
                m_plsImpl->ensureRasterOrderingEnabled(true);
                bindVAO(m_drawVAO);
                uint32_t indexCount = PatchIndexCount(drawType);
                uint32_t baseIndex = PatchBaseIndex(drawType);
                void* indexOffset = reinterpret_cast<void*>(baseIndex * sizeof(uint16_t));
                if (m_extensions.ANGLE_base_vertex_base_instance_shader_builtin)
                {
                    glDrawElementsInstancedBaseInstanceEXT(GL_TRIANGLES,
                                                           indexCount,
                                                           GL_UNSIGNED_SHORT,
                                                           indexOffset,
                                                           draw.vertexOrInstanceCount,
                                                           draw.baseVertexOrInstance);
                }
                else
                {
                    glUniform1i(drawProgram.baseInstancePolyfillLocation(),
                                draw.baseVertexOrInstance);
                    glDrawElementsInstanced(GL_TRIANGLES,
                                            indexCount,
                                            GL_UNSIGNED_SHORT,
                                            indexOffset,
                                            draw.vertexOrInstanceCount);
                }
                break;
            }
            case DrawType::interiorTriangulation:
                // Draw generic triangles.
                m_plsImpl->ensureRasterOrderingEnabled(false);
                bindVAO(m_interiorTrianglesVAO);
                glDrawArrays(GL_TRIANGLES, draw.baseVertexOrInstance, draw.vertexOrInstanceCount);
                m_plsImpl->barrier();
                break;
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

void PLSRenderContextGLImpl::bindProgram(GLuint programID)
{
    if (programID != m_boundProgramID)
    {
        glUseProgram(programID);
        m_boundProgramID = programID;
    }
}

void PLSRenderContextGLImpl::bindVAO(GLuint vao)
{
    if (vao != m_boundVAO)
    {
        glBindVertexArray(vao);
        m_boundVAO = vao;
    }
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
#ifdef RIVE_WASM
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

#ifdef RIVE_WASM
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
