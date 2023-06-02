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
PLSRenderContextGL::PLSRenderContextGL(const PlatformFeatures& platformFeatures) :
    PLSRenderContext(platformFeatures)
{
    m_colorRampProgram = glCreateProgram();
    glutils::CompileAndAttachShader(m_colorRampProgram, GL_VERTEX_SHADER, glsl::color_ramp);
    glutils::CompileAndAttachShader(m_colorRampProgram, GL_FRAGMENT_SHADER, glsl::color_ramp);
    glutils::LinkProgram(m_colorRampProgram);
    glUniformBlockBinding(m_colorRampProgram,
                          glGetUniformBlockIndex(m_colorRampProgram, GLSL_Uniforms),
                          kUniformBlockIdx);

    glGenVertexArrays(1, &m_colorRampVAO);
    glBindVertexArray(m_colorRampVAO);
    glEnableVertexAttribArray(0);
    glVertexAttribDivisor(0, 1);

    glGenFramebuffers(1, &m_colorRampFBO);

    m_tessellateProgram = glCreateProgram();
    const char* tessellateSources[] = {glsl::math, glsl::tessellate};
    glutils::CompileAndAttachShader(m_tessellateProgram, GL_VERTEX_SHADER, tessellateSources, 2);
    glutils::CompileAndAttachShader(m_tessellateProgram, GL_FRAGMENT_SHADER, tessellateSources, 2);
    glutils::LinkProgram(m_tessellateProgram);
    glUniformBlockBinding(m_tessellateProgram,
                          glGetUniformBlockIndex(m_tessellateProgram, GLSL_Uniforms),
                          kUniformBlockIdx);

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

std::unique_ptr<UniformBufferRing> PLSRenderContextGL::makeUniformBufferRing(size_t sizeInBytes)
{
    return std::make_unique<UniformBufferGL>(sizeInBytes);
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

std::unique_ptr<BufferRingImpl> PLSRenderContextGL::makeVertexBufferRing(size_t capacity,
                                                                         size_t itemSizeInBytes)
{
    return std::make_unique<VertexBufferGL>(capacity, itemSizeInBytes);
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
               const PlatformFeatures& platformFeatures)
    {
        auto sourceType =
            shaderType == GL_VERTEX_SHADER ? SourceType::vertexOnly : SourceType::wholeProgram;

        std::ostringstream defines;
        defines << "#define " << GLSL_OPTIONALLY_FLAT
                << (platformFeatures.avoidFlatVaryings ? "\n" : " flat\n");
        shaderFeatures.generatePreprocessorDefines(sourceType, [&defines](const char* macro) {
            defines << "#define " << macro << "\n";
        });

        std::string definesStr = defines.str();
        std::vector<const char*> sources{definesStr.c_str()};
        sources.push_back(glsl::math);
        if (sourceType != SourceType::vertexOnly)
        {
            if (shaderFeatures.programFeatures.blendTier > BlendTier::srcOver)
            {
                sources.push_back(glsl::advanced_blend);
            }
        }
        sources.push_back(glsl::draw);

        m_id = glutils::CompileShader(shaderType, sources.data(), sources.size());
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
    const DrawShader& vertexShader =
        context->m_vertexShaders
            .try_emplace(shaderFeatures.uniqueKey(SourceType::vertexOnly),
                         GL_VERTEX_SHADER,
                         shaderFeatures,
                         context->m_platformFeatures)
            .first->second;
    glAttachShader(m_id, vertexShader.id());

    // Every fragment shader is unique.
    DrawShader fragmentShader(GL_FRAGMENT_SHADER, shaderFeatures, context->m_platformFeatures);
    glAttachShader(m_id, fragmentShader.id());

    glutils::LinkProgram(m_id);

    glUseProgram(m_id);
    glUniformBlockBinding(m_id, glGetUniformBlockIndex(m_id, GLSL_Uniforms), kUniformBlockIdx);
    glUniform1i(glGetUniformLocation(m_id, GLSL_tessVertexTexture),
                kGLTexIdxOffset + kTessVertexTextureIdx);
    glUniform1i(glGetUniformLocation(m_id, GLSL_pathTexture), kGLTexIdxOffset + kPathTextureIdx);
    glUniform1i(glGetUniformLocation(m_id, GLSL_contourTexture),
                kGLTexIdxOffset + kContourTextureIdx);
    glUniform1i(glGetUniformLocation(m_id, GLSL_gradTexture), kGLTexIdxOffset + kGradTextureIdx);
}

PLSRenderContextGL::DrawProgram::~DrawProgram() { glDeleteProgram(m_id); }

void PLSRenderContextGL::DrawProgram::bind() const { glUseProgram(m_id); }

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
        static_cast<const VertexBufferGL*>(gradSpanBufferRing())->bindCurrentBuffer();
        glBindVertexArray(m_colorRampVAO);
        glVertexAttribIPointer(0, 4, GL_UNSIGNED_INT, sizeof(GradientSpan), nullptr);
        glViewport(0, gradTextureRowsForSimpleRamps(), kGradTextureWidth, gradSpansHeight);
        glBindFramebuffer(GL_FRAMEBUFFER, m_colorRampFBO);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            static_cast<const TexelBufferGL*>(gradTexelBufferRing())->submittedTextureID(),
            0);
        static_cast<const UniformBufferGL*>(gradUniformBufferRing())
            ->bindToUniformBlock(kUniformBlockIdx);
        glUseProgram(m_colorRampProgram);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, gradSpanCount);
    }

    // Tessellate all curves into vertices in the tessellation texture.
    if (tessVertexSpanCount > 0)
    {
        static_cast<const VertexBufferGL*>(tessSpanBufferRing())->bindCurrentBuffer();
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
        static_cast<const UniformBufferGL*>(tessellateUniformBufferRing())
            ->bindToUniformBlock(kUniformBlockIdx);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, tessVertexSpanCount);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, renderTarget()->drawFramebufferID());
    glViewport(0, 0, renderTarget()->width(), renderTarget()->height());

    // Compile the draw program before activating pixel local storage.
    // Cache specific compilations of draw.glsl by ShaderFeatures.
    const DrawProgram& drawProgram =
        m_drawPrograms
            .try_emplace(shaderFeatures.uniqueKey(SourceType::wholeProgram), this, shaderFeatures)
            .first->second;

    if (frameDescriptor().wireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(2);
    }

    activatePixelLocalStorage(loadAction, shaderFeatures, drawProgram);

    if (wedgeInstanceCount > 0)
    {
        // Draw wedges connecting all tessellated vertices.
        glDrawElementsInstanced(GL_TRIANGLES,
                                kOuterStrokeWedgeIndexCount,
                                GL_UNSIGNED_SHORT,
                                nullptr,
                                wedgeInstanceCount);
    }

    deactivatePixelLocalStorage(shaderFeatures);

    if (frameDescriptor().wireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}
} // namespace rive::pls
