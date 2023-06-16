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

// ES doesn't have wireframe support. Use a hacked ANGLE driver that repurposes glPolygonOffset as a
// back door to turn on wireframe in the backend.
#define glPolygonMode(face, mode) glPolygonOffset(float(face), float(mode))
#define GL_POINT 0x1B00
#define GL_LINE 0x1B01
#define GL_FILL 0x1B02

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
                                       const Extensions& extensions,
                                       std::unique_ptr<PLSImpl> plsImpl) :
    PLSRenderContext(platformFeatures), m_extensions(extensions), m_plsImpl(std::move(plsImpl))

{
    m_colorRampProgram = glCreateProgram();
    glutils::CompileAndAttachShader(m_colorRampProgram, GL_VERTEX_SHADER, glsl::color_ramp);
    glutils::CompileAndAttachShader(m_colorRampProgram, GL_FRAGMENT_SHADER, glsl::color_ramp);
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
                                    2);
    glutils::CompileAndAttachShader(m_tessellateProgram,
                                    GL_FRAGMENT_SHADER,
                                    nullptr,
                                    0,
                                    tessellateSources,
                                    2);
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

    DrawShader(GLenum shaderType,
               const ShaderFeatures& shaderFeatures,
               const PlatformFeatures& platformFeatures,
               const PLSImpl* plsImpl)
    {
        auto sourceType =
            shaderType == GL_VERTEX_SHADER ? SourceType::vertexOnly : SourceType::wholeProgram;

        std::vector<const char*> defines;
        defines.push_back(plsImpl->shaderDefineName());
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

        std::vector<const char*> sources;
        sources.push_back(glsl::math);
        if (sourceType != SourceType::vertexOnly)
        {
            if (shaderFeatures.programFeatures.blendTier > BlendTier::srcOver)
            {
                sources.push_back(glsl::advanced_blend);
            }
        }
        if (platformFeatures.avoidFlatVaryings)
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
                                      plsImpl->shaderVersionOverrideString());
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
    const DrawShader& vertexShader = context->m_vertexShaders
                                         .try_emplace(vertexShaderKey,
                                                      GL_VERTEX_SHADER,
                                                      shaderFeatures,
                                                      context->m_platformFeatures,
                                                      context->m_plsImpl.get())
                                         .first->second;
    glAttachShader(m_id, vertexShader.id());

    // Every fragment shader is unique.
    DrawShader fragmentShader(GL_FRAGMENT_SHADER,
                              shaderFeatures,
                              context->m_platformFeatures,
                              context->m_plsImpl.get());
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

void PLSRenderContextGL::bindDrawProgram(const DrawProgram& drawProgram)
{
    glBindVertexArray(m_drawVAO);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, gl_buffer_id(drawUniforms()));
    glUseProgram(drawProgram.id());
}

void PLSRenderContextGL::onFlush(FlushType flushType,
                                 LoadAction loadAction,
                                 size_t gradSpanCount,
                                 size_t gradSpansHeight,
                                 size_t tessVertexSpanCount,
                                 size_t tessDataHeight,
                                 size_t wedgeInstanceCount,
                                 const ShaderFeatures& shaderFeatures)
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

    glViewport(0, 0, renderTarget()->width(), renderTarget()->height());

    // Compile the draw program before activating pixel local storage.
    // Cache specific compilations of draw.glsl by ShaderFeatures.
    uint64_t fragmentShaderKey = shaderFeatures.getPreprocessorDefines(SourceType::wholeProgram);
    const DrawProgram& drawProgram =
        m_drawPrograms.try_emplace(fragmentShaderKey, this, shaderFeatures).first->second;

    if (frameDescriptor().wireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(2);
    }

    m_plsImpl->activatePixelLocalStorage(this,
                                         renderTarget(),
                                         loadAction,
                                         shaderFeatures,
                                         drawProgram);

    if (wedgeInstanceCount > 0)
    {
        // Draw wedges connecting all tessellated vertices.
        glDrawElementsInstanced(GL_TRIANGLES,
                                kOuterStrokeWedgeIndexCount,
                                GL_UNSIGNED_SHORT,
                                nullptr,
                                wedgeInstanceCount);
    }

    m_plsImpl->deactivatePixelLocalStorage(shaderFeatures);

    if (frameDescriptor().wireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}

std::unique_ptr<PLSRenderContextGL> PLSRenderContextGL::Make()
{
    Extensions extensions;
    int extensionCount;
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
        else if (strcmp(ext, "GL_ARM_shader_framebuffer_fetch") == 0)
        {
            extensions.ARM_shader_framebuffer_fetch = true;
        }
        else if (strcmp(ext, "GL_ARB_fragment_shader_interlock") == 0)
        {
            extensions.ARB_fragment_shader_interlock = true;
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
