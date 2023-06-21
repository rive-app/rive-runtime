/*
 * Copyright 2022 Rive
 */

#include "rive/pls/gl/pls_render_context_gl.hpp"

#include "buffer_ring_gl.hpp"
#include "gl_utils.hpp"
#include "pls_path.hpp"
#include "pls_paint.hpp"
#include <sstream>

#include "../out/obj/generated/advanced_blend.glsl.hpp"
#include "../out/obj/generated/color_ramp.glsl.hpp"
#include "../out/obj/generated/draw.glsl.hpp"
#include "../out/obj/generated/math.glsl.hpp"
#include "../out/obj/generated/tessellate.glsl.hpp"

// Offset all texture indices by 1 so we, and others who share our GL context, can use GL_TEXTURE0
// as a scratch texture index.
constexpr static int kGLTexIdxOffset = 1;

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

PLSRenderContextGL::PLSRenderContextGL(const PlatformFeatures& platformFeatures,
                                       const GLExtensions& extensions,
                                       std::unique_ptr<PLSImpl> plsImpl) :
    PLSRenderContext(platformFeatures), m_extensions(extensions), m_plsImpl(std::move(plsImpl))

{
    m_shaderVersionString[kShaderVersionStringBuffSize - 1] = '\0';
    strncpy(m_shaderVersionString, "#version 300 es\n", kShaderVersionStringBuffSize - 1);
#ifdef RIVE_DESKTOP_GL
    if (!GLAD_GL_version_es && GLAD_IS_GL_VERSION_AT_LEAST(4, 0))
    {
        snprintf(m_shaderVersionString,
                 kShaderVersionStringBuffSize,
                 "#version %d%d0\n",
                 GLAD_GL_version_major,
                 GLAD_GL_version_minor);
        m_supportsBaseInstanceInShader = GLAD_IS_GL_VERSION_AT_LEAST(4, 6);
    }
#endif
    assert(!m_supportsBaseInstanceInShader || m_extensions.EXT_base_instance);

    m_colorRampProgram = glCreateProgram();
    glutils::CompileAndAttachShader(m_colorRampProgram,
                                    GL_VERTEX_SHADER,
                                    glsl::color_ramp,
                                    m_shaderVersionString);
    glutils::CompileAndAttachShader(m_colorRampProgram,
                                    GL_FRAGMENT_SHADER,
                                    glsl::color_ramp,
                                    m_shaderVersionString);
    glutils::LinkProgram(m_colorRampProgram);
    glUniformBlockBinding(m_colorRampProgram,
                          glGetUniformBlockIndex(m_colorRampProgram, GLSL_Uniforms),
                          0);

    glGenVertexArrays(1, &m_colorRampVAO);
    glBindVertexArray(m_colorRampVAO);
    glEnableVertexAttribArray(0);
    glVertexAttribDivisor(0, 1);

    glGenFramebuffers(1, &m_colorRampFBO);

    m_tessellateProgram = glCreateProgram();
    const char* tessellateSources[] = {glsl::math, glsl::tessellate};
    glutils::CompileAndAttachShader(m_tessellateProgram,
                                    GL_VERTEX_SHADER,
                                    nullptr,
                                    0,
                                    tessellateSources,
                                    2,
                                    m_shaderVersionString);
    glutils::CompileAndAttachShader(m_tessellateProgram,
                                    GL_FRAGMENT_SHADER,
                                    nullptr,
                                    0,
                                    tessellateSources,
                                    2,
                                    m_shaderVersionString);
    glutils::LinkProgram(m_tessellateProgram);
    glUniformBlockBinding(m_tessellateProgram,
                          glGetUniformBlockIndex(m_tessellateProgram, GLSL_Uniforms),
                          0);

    glGenVertexArrays(1, &m_tessellateVAO);
    glBindVertexArray(m_tessellateVAO);
    for (int i = 0; i < 4; ++i)
    {
        glEnableVertexAttribArray(i);
        glVertexAttribDivisor(i, 1);
    }

    glGenFramebuffers(1, &m_tessellateFBO);

    glGenVertexArrays(1, &m_drawVAO);
    glBindVertexArray(m_drawVAO);

    WedgeVertex wedgeVertices[kOuterStrokeWedgeVertexCount];
    uint16_t wedgeIndices[kOuterStrokeWedgeIndexCount];
    GenerateWedgeTriangles(wedgeVertices, wedgeIndices, WedgeType::outerStroke);

    glGenBuffers(1, &m_pathWedgeVertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_pathWedgeVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(wedgeVertices), wedgeVertices, GL_STATIC_DRAW);

    glGenBuffers(1, &m_pathWedgeIndexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_pathWedgeIndexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(wedgeIndices), wedgeIndices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, nullptr);

    glVertexAttribDivisor(3, 1);

    glFrontFace(GL_CW);

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

PLSRenderContextGL::~PLSRenderContextGL()
{
    glDeleteProgram(m_colorRampProgram);
    glDeleteVertexArrays(1, &m_colorRampVAO);
    glDeleteFramebuffers(1, &m_colorRampFBO);

    glDeleteProgram(m_tessellateProgram);
    glDeleteVertexArrays(1, &m_tessellateVAO);
    glDeleteTextures(1, &m_tessVertexTexture);
    glDeleteFramebuffers(1, &m_tessellateFBO);

    glDeleteVertexArrays(1, &m_drawVAO);
    glDeleteBuffers(1, &m_pathWedgeVertexBuffer);
    glDeleteBuffers(1, &m_pathWedgeIndexBuffer);
}

std::unique_ptr<BufferRingImpl> PLSRenderContextGL::makeVertexBufferRing(size_t capacity,
                                                                         size_t itemSizeInBytes)
{
    return std::make_unique<BufferGL>(GL_ARRAY_BUFFER, capacity, itemSizeInBytes);
}

std::unique_ptr<TexelBufferRing> PLSRenderContextGL::makeTexelBufferRing(
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
                                           GL_TEXTURE0 + kGLTexIdxOffset + textureIdx,
                                           filter);
}

std::unique_ptr<BufferRingImpl> PLSRenderContextGL::makeUniformBufferRing(size_t capacity,
                                                                          size_t sizeInBytes)
{
    return std::make_unique<BufferGL>(GL_UNIFORM_BUFFER, capacity, sizeInBytes);
}

void PLSRenderContextGL::allocateTessellationTexture(size_t height)
{
    glDeleteTextures(1, &m_tessVertexTexture);

    glGenTextures(1, &m_tessVertexTexture);
    glActiveTexture(GL_TEXTURE0 + kGLTexIdxOffset + kTessVertexTextureIdx);
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
class PLSRenderContextGL::DrawShader
{
public:
    DrawShader(const DrawShader&) = delete;
    DrawShader& operator=(const DrawShader&) = delete;

    DrawShader(PLSRenderContextGL* context, GLenum shaderType, const ShaderFeatures& shaderFeatures)
    {
        auto sourceType =
            shaderType == GL_VERTEX_SHADER ? SourceType::vertexOnly : SourceType::wholeProgram;

        std::vector<const char*> defines;
        defines.push_back(context->m_plsImpl->shaderDefineName());
        uint64_t shaderFeatureDefines = shaderFeatures.getPreprocessorDefines(sourceType);
        if (shaderFeatureDefines & ShaderFeatures::PreprocessorDefines::ENABLE_ADVANCED_BLEND)
        {
            defines.push_back(GLSL_ENABLE_ADVANCED_BLEND);
        }
        if (shaderFeatureDefines & ShaderFeatures::PreprocessorDefines::ENABLE_PATH_CLIPPING)
        {
            defines.push_back(GLSL_ENABLE_PATH_CLIPPING);
        }
        if (shaderFeatureDefines & ShaderFeatures::PreprocessorDefines::ENABLE_EVEN_ODD)
        {
            defines.push_back(GLSL_ENABLE_EVEN_ODD);
        }
        if (shaderFeatureDefines & ShaderFeatures::PreprocessorDefines::ENABLE_HSL_BLEND_MODES)
        {
            defines.push_back(GLSL_ENABLE_HSL_BLEND_MODES);
        }
        if (shaderType == GL_VERTEX_SHADER && !context->m_supportsBaseInstanceInShader)
        {
            defines.push_back(GLSL_BASE_INSTANCE_POLYFILL);
        }

        std::vector<const char*> sources;
        sources.push_back(glsl::math);
        if (sourceType != SourceType::vertexOnly)
        {
            if (shaderFeatures.programFeatures.blendTier > BlendTier::srcOver)
            {
                sources.push_back(glsl::advanced_blend);
            }
        }
        if (context->m_platformFeatures.avoidFlatVaryings)
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
                                      context->m_shaderVersionString);
    }

    ~DrawShader() { glDeleteShader(m_id); }

    GLuint id() const { return m_id; }

private:
    GLuint m_id;
};

PLSRenderContextGL::DrawProgram::DrawProgram(PLSRenderContextGL* context,
                                             const ShaderFeatures& shaderFeatures)
{
    m_id = glCreateProgram();

    // Not every vertex shader is unique. Cache them by just the vertex features and reuse when
    // possible.
    uint64_t vertexShaderKey = shaderFeatures.getPreprocessorDefines(SourceType::vertexOnly);
    const DrawShader& vertexShader =
        context->m_vertexShaders
            .try_emplace(vertexShaderKey, context, GL_VERTEX_SHADER, shaderFeatures)
            .first->second;
    glAttachShader(m_id, vertexShader.id());

    // Every fragment shader is unique.
    DrawShader fragmentShader(context, GL_FRAGMENT_SHADER, shaderFeatures);
    glAttachShader(m_id, fragmentShader.id());

    glutils::LinkProgram(m_id);

    glUseProgram(m_id);
    glUniformBlockBinding(m_id, glGetUniformBlockIndex(m_id, GLSL_Uniforms), 0);
    glUniform1i(glGetUniformLocation(m_id, GLSL_tessVertexTexture),
                kGLTexIdxOffset + kTessVertexTextureIdx);
    glUniform1i(glGetUniformLocation(m_id, GLSL_pathTexture), kGLTexIdxOffset + kPathTextureIdx);
    glUniform1i(glGetUniformLocation(m_id, GLSL_contourTexture),
                kGLTexIdxOffset + kContourTextureIdx);
    glUniform1i(glGetUniformLocation(m_id, GLSL_gradTexture), kGLTexIdxOffset + kGradTextureIdx);
    if (!context->m_supportsBaseInstanceInShader)
    {
        m_baseInstancePolyfillLocation = glGetUniformLocation(m_id, GLSL_baseInstancePolyfill);
    }
}

PLSRenderContextGL::DrawProgram::~DrawProgram() { glDeleteProgram(m_id); }

static GLuint gl_buffer_id(const BufferRingImpl* bufferRing)
{
    return static_cast<const BufferGL*>(bufferRing)->submittedBufferID();
}

static GLuint gl_texture_id(const TexelBufferRing* texelBufferRing)
{
    return static_cast<const TexelBufferGL*>(texelBufferRing)->submittedTextureID();
}

void PLSRenderContextGL::onFlush(FlushType flushType,
                                 LoadAction loadAction,
                                 size_t gradSpanCount,
                                 size_t gradSpansHeight,
                                 size_t tessVertexSpanCount,
                                 size_t tessDataHeight,
                                 bool needsClipBuffer)
{
    // Render the complex color ramps to the gradient texture.
    if (gradSpanCount > 0)
    {
        glBindBuffer(GL_ARRAY_BUFFER, gl_buffer_id(gradSpanBufferRing()));
        glBindVertexArray(m_colorRampVAO);
        glVertexAttribIPointer(0, 4, GL_UNSIGNED_INT, sizeof(GradientSpan), nullptr);
        glViewport(0, gradTextureRowsForSimpleRamps(), kGradTextureWidth, gradSpansHeight);
        glBindFramebuffer(GL_FRAMEBUFFER, m_colorRampFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D,
                               gl_texture_id(gradTexelBufferRing()),
                               0);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, gl_buffer_id(colorRampUniforms()));
        glUseProgram(m_colorRampProgram);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, gradSpanCount);
    }

    // Tessellate all curves into vertices in the tessellation texture.
    if (tessVertexSpanCount > 0)
    {
        glBindBuffer(GL_ARRAY_BUFFER, gl_buffer_id(tessSpanBufferRing()));
        glBindVertexArray(m_tessellateVAO);
        for (int i = 0; i < 3; ++i)
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
        glViewport(0, 0, kTessTextureWidth, tessDataHeight);
        glBindFramebuffer(GL_FRAMEBUFFER, m_tessellateFBO);
        glUseProgram(m_tessellateProgram);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, gl_buffer_id(tessellateUniforms()));
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, tessVertexSpanCount);
    }

    // Compile the draw programs before activating pixel local storage.
    // (ANGLE_shader_pixel_local_storage doesn't allow shader compilation while active.)
    size_t drawIdx = 0;
    auto drawPrograms = reinterpret_cast<const DrawProgram**>(
        m_perFlushAllocator.alloc(sizeof(void*) * m_drawListCount));
    for (DrawList* draw = m_drawList; draw; draw = draw->next, ++drawIdx)
    {
        // Compile the draw program before activating pixel local storage.
        // Cache specific compilations of draw.glsl by ShaderFeatures.
        const ShaderFeatures& shaderFeatures = draw->shaderFeatures;
        uint64_t fragmentShaderKey =
            shaderFeatures.getPreprocessorDefines(SourceType::wholeProgram);
        drawPrograms[drawIdx] =
            &m_drawPrograms.try_emplace(fragmentShaderKey, this, shaderFeatures).first->second;
    }
    assert(drawIdx == m_drawListCount);

    glViewport(0, 0, renderTarget()->width(), renderTarget()->height());

#ifdef RIVE_DESKTOP_GL
    if (m_extensions.ANGLE_polygon_mode && frameDescriptor().wireframe)
    {
        glPolygonModeANGLE(GL_FRONT_AND_BACK, GL_LINE_ANGLE);
        glLineWidth(2);
    }
#endif

    m_plsImpl->activatePixelLocalStorage(this, renderTarget(), loadAction, needsClipBuffer);

    // Issue all the draws.
    glBindVertexArray(m_drawVAO);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, gl_buffer_id(drawUniforms()));
    drawIdx = 0;
    for (DrawList* draw = m_drawList; draw; draw = draw->next, ++drawIdx)
    {
        // Draw wedges connecting all tessellated vertices.
        const DrawProgram* drawProgram = drawPrograms[drawIdx];
        glUseProgram(drawProgram->id());
        size_t wedgeInstanceCount = draw->vertexCount / kWedgeSize;
        assert(wedgeInstanceCount > 0);
        assert(wedgeInstanceCount * kWedgeSize == draw->vertexCount);
        size_t wedgeBaseInstance = draw->baseVertex / kWedgeSize;
        assert(wedgeBaseInstance * kWedgeSize == draw->baseVertex);
        if (m_supportsBaseInstanceInShader)
        {
            glDrawElementsInstancedBaseInstanceEXT(GL_TRIANGLES,
                                                   kOuterStrokeWedgeIndexCount,
                                                   GL_UNSIGNED_SHORT,
                                                   nullptr,
                                                   wedgeInstanceCount,
                                                   wedgeBaseInstance);
        }
        else
        {
            glUniform1i(drawProgram->baseInstancePolyfillLocation(), wedgeBaseInstance);
            glDrawElementsInstanced(GL_TRIANGLES,
                                    kOuterStrokeWedgeIndexCount,
                                    GL_UNSIGNED_SHORT,
                                    nullptr,
                                    wedgeInstanceCount);
        }
    }
    assert(drawIdx == m_drawListCount);

    m_plsImpl->deactivatePixelLocalStorage();

#ifdef RIVE_DESKTOP_GL
    if (m_extensions.ANGLE_polygon_mode && frameDescriptor().wireframe)
    {
        glPolygonModeANGLE(GL_FRONT_AND_BACK, GL_FILL_ANGLE);
    }
#endif
}

std::unique_ptr<PLSRenderContextGL> PLSRenderContextGL::Make()
{
    GLExtensions extensions;
    GLint extensionCount;
    glGetIntegerv(GL_NUM_EXTENSIONS, &extensionCount);
    for (int i = 0; i < extensionCount; ++i)
    {
        auto* ext = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i));
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
    if (GLAD_GL_ANGLE_polygon_mode)
    {
        extensions.ANGLE_polygon_mode = true;
    }
    if (GLAD_GL_EXT_base_instance)
    {
        extensions.EXT_base_instance = true;
    }
#endif

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

#ifdef RIVE_GLES
    loadGLESExtensions(extensions); // Android doesn't load extension functions for us.
    if (extensions.EXT_shader_pixel_local_storage &&
        (extensions.ARM_shader_framebuffer_fetch || extensions.EXT_shader_framebuffer_fetch))
    {
        return std::unique_ptr<PLSRenderContextGL>(
            new PLSRenderContextGL(platformFeatures, extensions, MakePLSImplEXTNative()));
    }

    if (extensions.EXT_shader_framebuffer_fetch)
    {
        return std::unique_ptr<PLSRenderContextGL>(
            new PLSRenderContextGL(platformFeatures, extensions, MakePLSImplFramebufferFetch()));
    }
#endif

#ifdef RIVE_DESKTOP_GL
    if (extensions.ANGLE_shader_pixel_local_storage_coherent)
    {
        return std::unique_ptr<PLSRenderContextGL>(
            new PLSRenderContextGL(platformFeatures, extensions, MakePLSImplWebGL()));
    }

    if (extensions.ARB_fragment_shader_interlock || extensions.INTEL_fragment_shader_ordering)
    {
        return std::unique_ptr<PLSRenderContextGL>(
            new PLSRenderContextGL(platformFeatures, extensions, MakePLSImplRWTexture()));
    }
#endif

#ifdef RIVE_WASM
    if (emscripten_webgl_enable_WEBGL_shader_pixel_local_storage(
            emscripten_webgl_get_current_context()) &&
        emscripten_webgl_shader_pixel_local_storage_is_coherent())
    {
        return std::unique_ptr<PLSRenderContextGL>(
            new PLSRenderContextGL(platformFeatures, extensions, MakePLSImplWebGL()));
    }
#endif

    fprintf(stderr, "Pixel local storage is not supported.\n");
    return nullptr;
}
} // namespace rive::pls
