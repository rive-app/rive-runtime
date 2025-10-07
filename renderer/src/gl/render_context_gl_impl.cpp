/*
 * Copyright 2022 Rive
 */

#include "rive/renderer/gl/render_context_gl_impl.hpp"

#include "rive/renderer/gl/render_buffer_gl_impl.hpp"
#include "rive/renderer/gl/render_target_gl.hpp"
#include "rive/renderer/draw.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/texture.hpp"
#include "rive/profiler/profiler_macros.h"
#include "shaders/constants.glsl"
#include "instance_chunker.hpp"

#include "generated/shaders/advanced_blend.glsl.hpp"
#include "generated/shaders/color_ramp.glsl.hpp"
#include "generated/shaders/constants.glsl.hpp"
#include "generated/shaders/common.glsl.hpp"
#include "generated/shaders/draw_path_common.glsl.hpp"
#include "generated/shaders/draw_path.vert.hpp"
#include "generated/shaders/draw_raster_order_path.frag.hpp"
#include "generated/shaders/draw_msaa_path.frag.hpp"
#include "generated/shaders/draw_image_mesh.vert.hpp"
#include "generated/shaders/draw_raster_order_image_mesh.frag.hpp"
#include "generated/shaders/draw_msaa_image_mesh.frag.hpp"
#include "generated/shaders/bezier_utils.glsl.hpp"
#include "generated/shaders/tessellate.glsl.hpp"
#include "generated/shaders/render_atlas.glsl.hpp"
#include "generated/shaders/blit_texture_as_draw.glsl.hpp"
#include "generated/shaders/stencil_draw.glsl.hpp"

#ifdef RIVE_ANDROID
#include "generated/shaders/resolve_atlas.glsl.hpp"
#endif

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
static bool is_tessellation_draw(gpu::DrawType drawType)
{
    switch (drawType)
    {
        case gpu::DrawType::midpointFanPatches:
        case gpu::DrawType::midpointFanCenterAAPatches:
        case gpu::DrawType::outerCurvePatches:
        case gpu::DrawType::msaaStrokes:
        case gpu::DrawType::msaaMidpointFanBorrowedCoverage:
        case gpu::DrawType::msaaMidpointFans:
        case gpu::DrawType::msaaMidpointFanStencilReset:
        case gpu::DrawType::msaaMidpointFanPathsStencil:
        case gpu::DrawType::msaaMidpointFanPathsCover:
        case gpu::DrawType::msaaOuterCubics:
            return true;
        case gpu::DrawType::imageRect:
        case gpu::DrawType::imageMesh:
        case gpu::DrawType::interiorTriangulation:
        case gpu::DrawType::atlasBlit:
        case gpu::DrawType::msaaStencilClipReset:
        case gpu::DrawType::renderPassInitialize:
        case gpu::DrawType::renderPassResolve:
            return false;
    }
    RIVE_UNREACHABLE();
}

// Returns atlasDesiredType, or the next supported AtlasType down the list if it
// is not supported.
static RenderContextGLImpl::AtlasType select_atlas_type(
    const GLCapabilities& capabilities,
    RenderContextGLImpl::AtlasType atlasDesiredType =
        RenderContextGLImpl::AtlasType::r32f)
{
    switch (atlasDesiredType)
    {
        using AtlasType = RenderContextGLImpl::AtlasType;
        case AtlasType::r32f:
            if (capabilities.EXT_color_buffer_float &&
                capabilities.EXT_float_blend)
            {
                // fp32 is ideal for the atlas. When there's a lot of overlap,
                // fp16 can run out of precision.
                return AtlasType::r32f;
            }
            [[fallthrough]];
        case AtlasType::r16f:
            if (capabilities.EXT_color_buffer_half_float)
            {
                return AtlasType::r16f;
            }
            [[fallthrough]];
        case AtlasType::r32uiFramebufferFetch:
            if (capabilities.EXT_shader_framebuffer_fetch)
            {
                return AtlasType::r32uiFramebufferFetch;
            }
            [[fallthrough]];
        case AtlasType::r32uiPixelLocalStorage:
#ifdef RIVE_ANDROID
            if (capabilities.EXT_shader_pixel_local_storage)
            {
                return AtlasType::r32uiPixelLocalStorage;
            }
#else
            if (capabilities.ANGLE_shader_pixel_local_storage_coherent)
            {
                return AtlasType::r32uiPixelLocalStorage;
            }
#endif
            [[fallthrough]];
        case AtlasType::r32iAtomicTexture:
#ifndef RIVE_WEBGL
            if (capabilities.ARB_shader_image_load_store ||
                capabilities.OES_shader_image_atomic)
            {
                return AtlasType::r32iAtomicTexture;
            }
#endif
            [[fallthrough]];
        case AtlasType::rgba8:
            return AtlasType::rgba8;
    }
    RIVE_UNREACHABLE();
}

RenderContextGLImpl::RenderContextGLImpl(
    const char* rendererString,
    GLCapabilities capabilities,
    std::unique_ptr<PixelLocalStorageImpl> plsImpl,
    ShaderCompilationMode shaderCompilationMode) :
    m_capabilities(capabilities),
    m_plsImpl(std::move(plsImpl)),
    m_atlasType(select_atlas_type(m_capabilities)),
    m_pipelineManager(shaderCompilationMode, this),
    m_state(make_rcp<GLState>(m_capabilities))

{
    if (m_capabilities.isANGLESystemDriver &&
        capabilities.KHR_blend_equation_advanced)
    {
        // Some ANGLE devices report support for this extension but render
        //  incorrectly with it, so we'll need to run a quick test to validate
        //  that we get the proper color out of doing advance blending before
        //  rendering with it.
        m_testForAdvancedBlendError = true;
    }

    if (m_plsImpl != nullptr)
    {
        m_platformFeatures.supportsRasterOrdering =
            m_plsImpl->supportsRasterOrdering(m_capabilities);
        m_platformFeatures.supportsFragmentShaderAtomics =
            m_plsImpl->supportsFragmentShaderAtomics(m_capabilities);
    }
    if (m_capabilities.KHR_blend_equation_advanced ||
        m_capabilities.KHR_blend_equation_advanced_coherent)
    {
        m_platformFeatures.supportsBlendAdvancedKHR = true;
    }
    if (m_capabilities.KHR_blend_equation_advanced_coherent)
    {
        m_platformFeatures.supportsBlendAdvancedCoherentKHR = true;
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

void RenderContextGLImpl::buildAtlasRenderPipelines()
{
    std::vector<const char*> defines;
    defines.push_back(GLSL_DRAW_PATH);
    defines.push_back(GLSL_ENABLE_FEATHER);
    defines.push_back(GLSL_ENABLE_INSTANCE_INDEX);
    if (!m_capabilities.ARB_shader_storage_buffer_object)
    {
        defines.push_back(GLSL_DISABLE_SHADER_STORAGE_BUFFERS);
    }
    m_atlasFillPipelineState = gpu::ATLAS_FILL_PIPELINE_STATE;
    m_atlasStrokePipelineState = gpu::ATLAS_STROKE_PIPELINE_STATE;
    switch (m_atlasType)
    {
        case AtlasType::r32f:
        case AtlasType::r16f:
            break;
        case AtlasType::r32uiFramebufferFetch:
            defines.push_back(GLSL_ATLAS_RENDER_TARGET_R32UI_FRAMEBUFFER_FETCH);
            m_atlasFillPipelineState.blendEquation = gpu::BlendEquation::none;
            m_atlasStrokePipelineState.blendEquation = gpu::BlendEquation::none;
            break;
        case AtlasType::r32uiPixelLocalStorage:
#ifdef RIVE_ANDROID
            defines.push_back(GLSL_ATLAS_RENDER_TARGET_R32UI_PLS_EXT);
#else
            defines.push_back(GLSL_ATLAS_RENDER_TARGET_R32UI_PLS_ANGLE);
#endif
            m_atlasFillPipelineState.blendEquation = gpu::BlendEquation::none;
            m_atlasStrokePipelineState.blendEquation = gpu::BlendEquation::none;
            break;
        case AtlasType::r32iAtomicTexture:
#ifndef RIVE_WEBGL
            defines.push_back(GLSL_ATLAS_RENDER_TARGET_R32I_ATOMIC_TEXTURE);
            m_atlasFillPipelineState.colorWriteEnabled = false;
            m_atlasFillPipelineState.blendEquation = gpu::BlendEquation::none;
            m_atlasStrokePipelineState.colorWriteEnabled = false;
            m_atlasStrokePipelineState.blendEquation = gpu::BlendEquation::none;
#endif
            break;
        case AtlasType::rgba8:
            defines.push_back(GLSL_ATLAS_RENDER_TARGET_RGBA8_UNORM);
            break;
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

#ifdef RIVE_ANDROID
    if (m_atlasType == AtlasType::r32uiPixelLocalStorage)
    {
        // Build the pipelines for clearing and resolving
        // EXT_shader_pixel_local_storage.
        m_atlasResolveVertexShader.compile(GL_VERTEX_SHADER,
                                           glsl::resolve_atlas,
                                           m_capabilities);

        const char* atlasClearDefines[] = {
            GLSL_ATLAS_RENDER_TARGET_R32UI_PLS_EXT,
            GLSL_CLEAR_COVERAGE};
        const char* atlasClearSources[] = {glsl::resolve_atlas};
        m_atlasClearProgram = glutils::Program();
        glAttachShader(m_atlasClearProgram, m_atlasResolveVertexShader);
        m_atlasClearProgram.compileAndAttachShader(GL_FRAGMENT_SHADER,
                                                   atlasClearDefines,
                                                   std::size(atlasClearDefines),
                                                   atlasClearSources,
                                                   std::size(atlasClearSources),
                                                   m_capabilities);
        m_atlasClearProgram.link();

        const char* atlasResolveDefines[] = {
            GLSL_ATLAS_RENDER_TARGET_R32UI_PLS_EXT,
        };
        const char* atlasResolveSources[] = {glsl::resolve_atlas};
        m_atlasResolveProgram = glutils::Program();
        glAttachShader(m_atlasResolveProgram, m_atlasResolveVertexShader);
        m_atlasResolveProgram.compileAndAttachShader(
            GL_FRAGMENT_SHADER,
            atlasResolveDefines,
            std::size(atlasResolveDefines),
            atlasResolveSources,
            std::size(atlasResolveSources),
            m_capabilities);
        m_atlasResolveProgram.link();
    }
#endif
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
        Texture(width, height), m_texture(glutils::Texture::Adopt(textureID))
    {}

    operator GLuint() const { return m_texture; }

private:
    glutils::Texture m_texture;
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
    if (imageDataRGBAPremul != nullptr)
    {
        glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        0,
                        0,
                        width,
                        height,
                        GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        imageDataRGBAPremul);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
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

    ~BufferRingGLImpl() { m_state->deleteBuffer(m_bufferID); }

    GLuint bufferID() const { return m_bufferID; }

protected:
    BufferRingGLImpl(GLenum target,
                     size_t capacityInBytes,
                     rcp<GLState> state) :
        BufferRing(capacityInBytes), m_target(target), m_state(std::move(state))
    {
        glGenBuffers(1, &m_bufferID);
        m_state->bindBuffer(m_target, m_bufferID);
        glBufferData(m_target, capacityInBytes, nullptr, GL_DYNAMIC_DRAW);
    }

    void* onMapBuffer(int bufferIdx, size_t mapSizeInBytes) override
    {
#ifndef RIVE_WEBGL
        // WebGL doesn't support buffer mapping. Don't use it on ANGLE either
        // since we don't trust ANGLE with features that haven't been validated
        // by WebGL.
        if (!m_state->capabilities().isANGLESystemDriver)
        {
            m_state->bindBuffer(m_target, m_bufferID);
            return glMapBufferRange(m_target,
                                    0,
                                    mapSizeInBytes,
                                    GL_MAP_WRITE_BIT |
                                        GL_MAP_INVALIDATE_BUFFER_BIT);
        }
        else
#endif
        {
            return shadowBuffer();
        }
    }

    void onUnmapAndSubmitBuffer(int bufferIdx, size_t mapSizeInBytes) override
    {
        m_state->bindBuffer(m_target, m_bufferID);
#ifndef RIVE_WEBGL
        // WebGL doesn't support buffer mapping. Don't use it on ANGLE either
        // since we don't trust ANGLE with features that haven't been validated
        // by WebGL.
        if (!m_state->capabilities().isANGLESystemDriver)
        {
            glUnmapBuffer(m_target);
        }
        else
#endif
        {
            glBufferSubData(m_target, 0, mapSizeInBytes, shadowBuffer());
        }
    }

    const GLenum m_target;
    GLuint m_bufferID;
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
                          bufferID(),
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
        glGenTextures(1, &m_textureID);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_textureID);
        glTexStorage2D(GL_TEXTURE_2D, 1, internalformat, width, height);
        glutils::SetTexture2DSamplingParams(GL_NEAREST, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    ~TexelBufferRingWebGL() { glDeleteTextures(1, &m_textureID); }

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
        glBindTexture(GL_TEXTURE_2D, m_textureID);
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
    GLuint m_textureID;
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
    }
    else
    {
        glGenTextures(1, &m_gradientTexture);
        glActiveTexture(GL_TEXTURE0 + GRAD_TEXTURE_IDX);
        glBindTexture(GL_TEXTURE_2D, m_gradientTexture);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
        glutils::SetTexture2DSamplingParams(GL_LINEAR, GL_LINEAR);
    }

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
    }
    else
    {
        glGenTextures(1, &m_tessVertexTexture);
        glActiveTexture(GL_TEXTURE0 + TESS_VERTEX_TEXTURE_IDX);
        glBindTexture(GL_TEXTURE_2D, m_tessVertexTexture);
        glTexStorage2D(GL_TEXTURE_2D,
                       1,
                       m_capabilities.needsFloatingPointTessellationTexture
                           ? GL_RGBA32F
                           : GL_RGBA32UI,
                       width,
                       height);
        glutils::SetTexture2DSamplingParams(GL_NEAREST, GL_NEAREST);
    }

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

static GLenum atlas_gl_format(RenderContextGLImpl::AtlasType atlasType,
                              const GLCapabilities& capabilities)
{
    switch (atlasType)
    {
        using AtlasType = RenderContextGLImpl::AtlasType;
        case AtlasType::r32f:
            return GL_R32F;
        case AtlasType::r16f:
            return GL_R16F;
        case AtlasType::r32uiFramebufferFetch:
        case AtlasType::r32uiPixelLocalStorage:
            return GL_R32UI;
        case AtlasType::r32iAtomicTexture:
            return GL_R32I;
        case AtlasType::rgba8:
            return GL_RGBA8;
    }
    RIVE_UNREACHABLE();
}

void RenderContextGLImpl::resizeAtlasTexture(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
    {
        m_atlasTexture = glutils::Texture::Zero();
    }
    else
    {
        m_atlasTexture = glutils::Texture();
        glActiveTexture(GL_TEXTURE0 + ATLAS_TEXTURE_IDX);
        glBindTexture(GL_TEXTURE_2D, m_atlasTexture);
        glTexStorage2D(GL_TEXTURE_2D,
                       1,
                       atlas_gl_format(m_atlasType, m_capabilities),
                       width,
                       height);
        glutils::SetTexture2DSamplingParams(GL_NEAREST, GL_NEAREST);

        if (m_atlasVertexShader == 0)
        {
            // Don't compile the atlas programs until we get an indication that
            // they will be used.
            // FIXME: Do this in parallel at startup!!
            buildAtlasRenderPipelines();
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_atlasFBO);
#ifndef RIVE_ANDROID
    if (m_atlasType == AtlasType::r32uiPixelLocalStorage)
    {
        glFramebufferTexturePixelLocalStorageANGLE(0, m_atlasTexture, 0, 0);
    }
    else
#endif
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D,
                               m_atlasTexture,
                               0);
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
                renderContextImpl->m_capabilities.KHR_blend_equation_advanced)
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
        case gpu::DrawType::msaaStrokes:
        case gpu::DrawType::msaaMidpointFanBorrowedCoverage:
        case gpu::DrawType::msaaMidpointFans:
        case gpu::DrawType::msaaMidpointFanStencilReset:
        case gpu::DrawType::msaaMidpointFanPathsStencil:
        case gpu::DrawType::msaaMidpointFanPathsCover:
        case gpu::DrawType::msaaOuterCubics:
            if (shaderType == GL_VERTEX_SHADER)
            {
                defines.push_back(GLSL_ENABLE_INSTANCE_INDEX);
            }
            defines.push_back(GLSL_DRAW_PATH);
            sources.push_back(gpu::glsl::draw_path_common);
            if (interlockMode == gpu::InterlockMode::atomics)
            {
                sources.push_back(gpu::glsl::atomic_draw);
            }
            else
            {

                sources.push_back(gpu::glsl::draw_path_vert);
                sources.push_back(interlockMode == gpu::InterlockMode::msaa
                                      ? gpu::glsl::draw_msaa_path_frag
                                      : gpu::glsl::draw_raster_order_path_frag);
            }
            break;
        case gpu::DrawType::msaaStencilClipReset:
            assert(interlockMode == gpu::InterlockMode::msaa);
            sources.push_back(gpu::glsl::stencil_draw);
            break;
        case gpu::DrawType::atlasBlit:
            defines.push_back(GLSL_ATLAS_BLIT);
            switch (renderContextImpl->m_atlasType)
            {
                case AtlasType::r32f:
                case AtlasType::r16f:
                    break;
                case AtlasType::r32uiFramebufferFetch:
                case AtlasType::r32uiPixelLocalStorage:
                    defines.push_back(GLSL_ATLAS_TEXTURE_R32UI_FLOAT_BITS);
                    break;
                case AtlasType::r32iAtomicTexture:
                    defines.push_back(GLSL_ATLAS_TEXTURE_R32I_FIXED_POINT);
                    break;
                case AtlasType::rgba8:
                    defines.push_back(GLSL_ATLAS_TEXTURE_RGBA8_UNORM);
                    break;
            }
            [[fallthrough]];
        case gpu::DrawType::interiorTriangulation:
            defines.push_back(GLSL_DRAW_INTERIOR_TRIANGLES);
            sources.push_back(gpu::glsl::draw_path_common);
            if (interlockMode == gpu::InterlockMode::atomics)
            {
                sources.push_back(gpu::glsl::atomic_draw);
            }
            else
            {

                sources.push_back(gpu::glsl::draw_path_vert);
                sources.push_back(interlockMode == gpu::InterlockMode::msaa
                                      ? gpu::glsl::draw_msaa_path_frag
                                      : gpu::glsl::draw_raster_order_path_frag);
            }
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
                sources.push_back(gpu::glsl::draw_image_mesh_vert);
                sources.push_back(
                    interlockMode == gpu::InterlockMode::msaa
                        ? gpu::glsl::draw_msaa_image_mesh_frag
                        : gpu::glsl::draw_raster_order_image_mesh_frag);
            }
            break;
        case gpu::DrawType::renderPassResolve:
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
        case gpu::DrawType::renderPassInitialize:
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
                                  renderContextImpl->m_capabilities,
                                  glutils::DebugPrintErrorAndAbort::no);
}

RenderContextGLImpl::DrawShader::DrawShader(DrawShader&& moveFrom) :
    m_id(std::exchange(moveFrom.m_id, 0))
{}

RenderContextGLImpl::DrawShader& RenderContextGLImpl::DrawShader ::operator=(
    DrawShader&& moveFrom)
{
    if (&moveFrom != this)
    {
        if (m_id != 0)
        {
            glDeleteShader(m_id);
        }

        m_id = std::exchange(moveFrom.m_id, 0);
    }

    return *this;
}

RenderContextGLImpl::DrawShader::~DrawShader()
{
    if (m_id != 0)
    {
        glDeleteShader(m_id);
    }
}

RenderContextGLImpl::DrawProgram::DrawProgram(
    RenderContextGLImpl* renderContextImpl,
    PipelineCreateType createType,
    gpu::DrawType drawType,
    gpu::ShaderFeatures shaderFeatures,
    gpu::InterlockMode interlockMode,
    gpu::ShaderMiscFlags shaderMiscFlags
#ifdef WITH_RIVE_TOOLS
    ,
    SynthesizedFailureType synthesizedFailureType
#endif
    ) :
    m_state(renderContextImpl->m_state)
{
#ifdef WITH_RIVE_TOOLS
    m_synthesizedFailureType = synthesizedFailureType;
    if (m_synthesizedFailureType == SynthesizedFailureType::shaderCompilation)
    {
        m_pipelineStatus = PipelineStatus::errored;
        return;
    }
#endif

    m_vertexShader =
        &renderContextImpl->m_pipelineManager.getVertexShaderSynchronous(
            drawType,
            shaderFeatures,
            interlockMode);

    m_fragmentShader =
        &renderContextImpl->m_pipelineManager.getFragmentShaderSynchronous(
            drawType,
            shaderFeatures,
            interlockMode,
            shaderMiscFlags);

    m_id = glCreateProgram();

    // In async mode, we do not need to wait for the shaders to finish compiling
    // before doing the link - the link is what we actually can poll to ensure
    // everything finishes. If the linking fails, that is where we can check the
    // compilation statuses and display compilation errors as needed.
    glAttachShader(m_id, m_vertexShader->id());
    glAttachShader(m_id, m_fragmentShader->id());
    glutils::LinkProgram(m_id, glutils::DebugPrintErrorAndAbort::no);

    std::ignore = advanceCreation(renderContextImpl,
                                  createType,
                                  drawType,
                                  shaderFeatures,
                                  interlockMode,
                                  shaderMiscFlags);
}

bool RenderContextGLImpl::DrawProgram::advanceCreation(
    RenderContextGLImpl* renderContextImpl,
    PipelineCreateType createType,
    gpu::DrawType drawType,
    ShaderFeatures shaderFeatures,
    gpu::InterlockMode interlockMode,
    gpu::ShaderMiscFlags shaderMiscFlags)
{
    // This function should only be called if we're in the middle of creation.
    assert(m_pipelineStatus == PipelineStatus::notReady);

    if (createType == PipelineCreateType::async &&
        renderContextImpl->capabilities().KHR_parallel_shader_compile)
    {
        // Like above, this is async creation so verify the program is linked
        //  before continuing.

        GLint completed = 0;
        glGetProgramiv(m_id, GL_COMPLETION_STATUS_KHR, &completed);
        if (completed == 0)
        {
            return false;
        }
    }

#ifdef WITH_RIVE_TOOLS
    if (m_synthesizedFailureType == SynthesizedFailureType::pipelineCreation)
    {
        m_pipelineStatus = PipelineStatus::errored;
        return false;
    }
#endif

    {
        GLint successfullyLinked = 0;
        glGetProgramiv(m_id, GL_LINK_STATUS, &successfullyLinked);
        if (successfullyLinked == GL_FALSE)
        {
#ifdef DEBUG
            // The link failed, which might have been caused by compilation
            // failures, so output any compilation failure messages first.
            GLint compiledSuccessfully = 0;
            glGetShaderiv(m_vertexShader->id(),
                          GL_COMPILE_STATUS,
                          &compiledSuccessfully);
            if (compiledSuccessfully == GL_FALSE)
            {
                glutils::PrintShaderCompilationErrors(m_vertexShader->id());
            }

            glGetShaderiv(m_fragmentShader->id(),
                          GL_COMPILE_STATUS,
                          &compiledSuccessfully);
            if (compiledSuccessfully == GL_FALSE)
            {
                glutils::PrintShaderCompilationErrors(m_fragmentShader->id());
            }

            glutils::PrintLinkProgramErrors(m_id);
#endif
            m_pipelineStatus = PipelineStatus::errored;
            return false;
        }
    }

    m_state->bindProgram(m_id);
    glUniformBlockBinding(m_id,
                          glGetUniformBlockIndex(m_id, GLSL_FlushUniforms),
                          FLUSH_UNIFORM_BUFFER_IDX);

    const bool isImageDraw = gpu::DrawTypeIsImageDraw(drawType);
    const bool isTessellationDraw = is_tessellation_draw(drawType);
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
        !renderContextImpl->m_capabilities.KHR_blend_equation_advanced)
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

    // All done! This program is now usable by the renderer.
    m_pipelineStatus = PipelineStatus::ready;
    return true;
}

RenderContextGLImpl::DrawProgram::~DrawProgram()
{
    if (m_id != 0)
    {
        m_state->deleteProgram(m_id);
    }
}

static GLuint gl_buffer_id(const BufferRing* bufferRing)
{
    return static_cast<const BufferRingGLImpl*>(bufferRing)->bufferID();
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

void RenderContextGLImpl::preBeginFrame(RenderContext* ctx)
{
    if (!m_testForAdvancedBlendError)
    {
        return;
    }

    // We need to do a test to check whether or not KHR_blend_equation_advanced
    //  actually works as advertised. This is basically done by rendering a
    //  quad with a fancy blend mode to a tiny render target, reading it back,
    //  then checking how close to the true color we are.

    m_testForAdvancedBlendError = false;

    constexpr uint32_t RT_WIDTH = 4;
    constexpr uint32_t RT_HEIGHT = 4;
    constexpr ColorInt RT_CLEAR_COLOR = 0x8000ffff;

    RenderContext::FrameDescriptor fd = {
        .renderTargetWidth = RT_WIDTH,
        .renderTargetHeight = RT_HEIGHT,
        .clearColor = RT_CLEAR_COLOR,
    };

    glutils::Texture texture;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, RT_WIDTH, RT_HEIGHT);

    TextureRenderTargetGL rt{RT_WIDTH, RT_HEIGHT};
    rt.setTargetTexture(texture);

    ctx->beginFrame(fd);

    RiveRenderer renderer{ctx};

    constexpr ColorInt RT_QUAD_FILL_COLOR = 0x80404000;

    auto paint = ctx->makeRenderPaint();
    paint->style(RenderPaintStyle::fill);
    paint->color(RT_QUAD_FILL_COLOR);
    paint->blendMode(BlendMode::colorBurn);

    auto path = ctx->makeEmptyRenderPath();
    path->fillRule(FillRule::clockwise);
    path->moveTo(-1.0f, -1.0f);
    path->lineTo(float(RT_WIDTH + 1), -1.0f);
    path->lineTo(float(RT_WIDTH + 1), float(RT_HEIGHT + 1));
    path->lineTo(-1.0f, float(RT_HEIGHT + 1));

    renderer.drawPath(path.get(), paint.get());

    ctx->flush({.renderTarget = &rt});

    rt.bindDestinationFramebuffer(GL_READ_FRAMEBUFFER);

    uint8_t pixel[4];
    glReadPixels(1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);

    // Note that this color is *not* in the same channel order as the above
    //  colors.
    uint8_t EXPECTED_COLOR[] = {0x10, 0x90, 0x80, 0xc0};

    int maxRGBDiff = std::max({std::abs(int(pixel[0] - EXPECTED_COLOR[0])),
                               std::abs(int(pixel[1] - EXPECTED_COLOR[1])),
                               std::abs(int(pixel[2] - EXPECTED_COLOR[2]))});

    // Note that the RGB mismatch we are seeing that this is fixing is 96.
    constexpr int DIFF_TOLERANCE = 40;
    if (maxRGBDiff > DIFF_TOLERANCE)
    {
        // If the blending was out of tolerance then we need to disable this
        // feature.
        m_capabilities.KHR_blend_equation_advanced = false;
        m_platformFeatures.supportsBlendAdvancedKHR = false;

        // We also need to clear the shader caches because shaders get built
        //  differently based on whether KHR_blend_equation_advanced is set.
        //  Thankfully we should only have a couple that we just created for
        //  the test.
        m_pipelineManager.clearCache();
    }
}

void RenderContextGLImpl::flush(const FlushDescriptor& desc)
{
    assert(desc.interlockMode != gpu::InterlockMode::clockwiseAtomic);
    auto renderTarget = static_cast<RenderTargetGL*>(desc.renderTarget);

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

        m_state->bindProgram(m_colorRampProgram);
        glBindFramebuffer(GL_FRAMEBUFFER, m_colorRampFBO);
        glViewport(0, 0, kGradTextureWidth, desc.gradDataHeight);
        m_state->setPipelineState(gpu::COLOR_ONLY_PIPELINE_STATE);
        m_state->bindBuffer(GL_ARRAY_BUFFER,
                            gl_buffer_id(gradSpanBufferRing()));
        m_state->bindVAO(m_colorRampVAO);
        GLenum colorAttachment0 = GL_COLOR_ATTACHMENT0;
        glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, &colorAttachment0);
        for (auto [instanceCount, baseInstance] : InstanceChunker(
                 desc.gradSpanCount,
                 math::lossless_numeric_cast<uint32_t>(desc.firstGradSpan),
                 m_capabilities.maxSupportedInstancesPerDrawCommand))

        {
            glVertexAttribIPointer(
                0,
                4,
                GL_UNSIGNED_INT,
                0,
                reinterpret_cast<const void*>(baseInstance *
                                              sizeof(gpu::GradientSpan)));
            glDrawArraysInstanced(GL_TRIANGLE_STRIP,
                                  0,
                                  gpu::GRAD_SPAN_TRI_STRIP_VERTEX_COUNT,
                                  instanceCount);
        }
    }

    // Tessellate all curves into vertices in the tessellation texture.
    if (desc.tessVertexSpanCount > 0)
    {
        m_state->bindProgram(m_tessellateProgram);
        glBindFramebuffer(GL_FRAMEBUFFER, m_tessellateFBO);
        glViewport(0, 0, gpu::kTessTextureWidth, desc.tessDataHeight);
        m_state->setPipelineState(gpu::COLOR_ONLY_PIPELINE_STATE);
        m_state->bindBuffer(GL_ARRAY_BUFFER,
                            gl_buffer_id(tessSpanBufferRing()));
        m_state->bindVAO(m_tessellateVAO);
        GLenum colorAttachment0 = GL_COLOR_ATTACHMENT0;
        glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, &colorAttachment0);
        for (auto [instanceCount, baseInstance] : InstanceChunker(
                 desc.tessVertexSpanCount,
                 math::lossless_numeric_cast<uint32_t>(
                     desc.firstTessVertexSpan),
                 m_capabilities.maxSupportedInstancesPerDrawCommand))

        {
            size_t tessSpanOffsetInBytes =
                baseInstance * sizeof(gpu::TessVertexSpan);
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
            glDrawElementsInstanced(GL_TRIANGLES,
                                    std::size(gpu::kTessSpanIndices),
                                    GL_UNSIGNED_SHORT,
                                    0,
                                    instanceCount);
        }
    }

    // Render the atlas if we have any offscreen feathers.
    if ((desc.atlasFillBatchCount | desc.atlasStrokeBatchCount) != 0)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_atlasFBO);
        glViewport(0, 0, desc.atlasContentWidth, desc.atlasContentHeight);

        glEnable(GL_SCISSOR_TEST);
        glScissor(0, 0, desc.atlasContentWidth, desc.atlasContentHeight);

        // Invert the front face for atlas draws because GL is bottom up.
        glFrontFace(GL_CCW);

        // Finish setting up the atlas render pass and clear the atlas.
        m_state->setPipelineState(gpu::COLOR_ONLY_PIPELINE_STATE);
        switch (m_atlasType)
        {
            case AtlasType::r32f:
            case AtlasType::r16f:
            case AtlasType::rgba8:
            {
                constexpr GLfloat clearZero4f[4]{};
                glClearBufferfv(GL_COLOR, 0, clearZero4f);
                break;
            }
            case AtlasType::r32uiFramebufferFetch:
            {
                constexpr GLuint clearZero4ui[4]{};
                glClearBufferuiv(GL_COLOR, 0, clearZero4ui);
                break;
            }
            case AtlasType::r32uiPixelLocalStorage:
            {
#ifdef RIVE_ANDROID
                glEnable(GL_SHADER_PIXEL_LOCAL_STORAGE_EXT);
                // EXT_shader_pixel_local_storage doesn't support clearing.
                // Render the clear color.
                m_state->bindProgram(m_atlasClearProgram);
                m_state->bindVAO(m_atlasResolveVAO);
                m_state->setCullFace(GL_FRONT);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
#else
                glBeginPixelLocalStorageANGLE(
                    1,
                    std::array<GLenum, 1>{GL_LOAD_OP_ZERO_ANGLE}.data());
#endif
                break;
            }
            case AtlasType::r32iAtomicTexture:
            {
#ifndef RIVE_WEBGL
                constexpr GLint clearZero4i[4]{};
                glClearBufferiv(GL_COLOR, 0, clearZero4i);
                glBindImageTexture(0,
                                   m_atlasTexture,
                                   0,
                                   GL_FALSE,
                                   0,
                                   GL_READ_WRITE,
                                   GL_R32I);
                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
                                GL_TEXTURE_FETCH_BARRIER_BIT |
                                GL_FRAMEBUFFER_BARRIER_BIT);
#endif
                break;
            }
        }
        m_state->bindVAO(m_drawVAO);

        // Draw the atlas fills.
        if (desc.atlasFillBatchCount != 0)
        {
            m_state->setPipelineState(m_atlasFillPipelineState);
            m_state->bindProgram(m_atlasFillProgram);
            for (size_t i = 0; i < desc.atlasFillBatchCount; ++i)
            {
                const gpu::AtlasDrawBatch& fillBatch = desc.atlasFillBatches[i];
                glScissor(fillBatch.scissor.left,
                          fillBatch.scissor.top,
                          fillBatch.scissor.width(),
                          fillBatch.scissor.height());
                drawIndexedInstancedNoInstancedAttribs(
                    GL_TRIANGLES,
                    gpu::kMidpointFanCenterAAPatchIndexCount,
                    gpu::kMidpointFanCenterAAPatchBaseIndex,
                    fillBatch.patchCount,
                    fillBatch.basePatch,
                    m_atlasFillProgram.baseInstanceUniformLocation());
            }
        }

        // Draw the atlas strokes.
        if (desc.atlasStrokeBatchCount != 0)
        {
            m_state->setPipelineState(m_atlasStrokePipelineState);
            m_state->bindProgram(m_atlasStrokeProgram);
            for (size_t i = 0; i < desc.atlasStrokeBatchCount; ++i)
            {
                const gpu::AtlasDrawBatch& strokeBatch =
                    desc.atlasStrokeBatches[i];
                glScissor(strokeBatch.scissor.left,
                          strokeBatch.scissor.top,
                          strokeBatch.scissor.width(),
                          strokeBatch.scissor.height());
                drawIndexedInstancedNoInstancedAttribs(
                    GL_TRIANGLES,
                    gpu::kMidpointFanPatchBorderIndexCount,
                    gpu::kMidpointFanPatchBaseIndex,
                    strokeBatch.patchCount,
                    strokeBatch.basePatch,
                    m_atlasFillProgram.baseInstanceUniformLocation());
            }
        }

        // Finalize the atlas render pass if needed.
        switch (m_atlasType)
        {
            case AtlasType::r32f:
            case AtlasType::r16f:
            case AtlasType::rgba8:
            case AtlasType::r32uiFramebufferFetch:
                break;
            case AtlasType::r32uiPixelLocalStorage:
            {
#ifdef RIVE_ANDROID
                // EXT_shader_pixel_local_storage needs to be explicity resolved
                // with a draw.
                m_state->bindProgram(m_atlasResolveProgram);
                m_state->bindVAO(m_atlasResolveVAO);
                m_state->setCullFace(GL_FRONT);
                glScissor(0,
                          0,
                          desc.atlasContentWidth,
                          desc.atlasContentHeight);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                glDisable(GL_SHADER_PIXEL_LOCAL_STORAGE_EXT);
#else
                glEndPixelLocalStorageANGLE(
                    1,
                    std::array<GLenum, 1>{GL_STORE_OP_STORE_ANGLE}.data());
#endif
                break;
            }
            case AtlasType::r32iAtomicTexture:
#ifndef RIVE_WEBGL
                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
                                GL_TEXTURE_FETCH_BARRIER_BIT |
                                GL_FRAMEBUFFER_BARRIER_BIT);
#endif
                break;
        }

        glFrontFace(GL_CW);
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
        if (desc.interlockMode == gpu::InterlockMode::atomics)
        {
            m_plsImpl->ensureRasterOrderingEnabled(this, desc, false);
        }
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
        m_state->setWriteMasks(true, true, 0xff);
        glClear(buffersToClear);

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
        const gpu::DrawType drawType = batch.drawType;
        gpu::ShaderFeatures shaderFeatures =
            desc.interlockMode == gpu::InterlockMode::atomics
                ? desc.combinedShaderFeatures
                : batch.shaderFeatures;
        gpu::ShaderMiscFlags shaderMiscFlags = batch.shaderMiscFlags;
        if (m_plsImpl != nullptr)
        {
            shaderMiscFlags |= m_plsImpl->shaderMiscFlags(desc, drawType);
        }
        if (desc.interlockMode == gpu::InterlockMode::rasterOrdering &&
            (batch.drawContents & gpu::DrawContents::clockwiseFill))
        {
            shaderMiscFlags |= gpu::ShaderMiscFlags::clockwiseFill;
        }
        const DrawProgram* drawProgram = m_pipelineManager.tryGetPipeline(
            {
                .drawType = drawType,
                .shaderFeatures = shaderFeatures,
                .interlockMode = desc.interlockMode,
                .shaderMiscFlags = shaderMiscFlags,
#ifdef WITH_RIVE_TOOLS
                .synthesizedFailureType = desc.synthesizedFailureType,
#endif
            },
            m_platformFeatures);
        if (drawProgram == nullptr)
        {
            // There was an issue getting either the requested draw program or
            // its ubershader counterpart so we cannot draw anything.
            continue;
        }

        m_state->bindProgram(drawProgram->id());

        if (auto imageTextureGL =
                static_cast<const TextureGLImpl*>(batch.imageTexture))
        {
            glActiveTexture(GL_TEXTURE0 + IMAGE_TEXTURE_IDX);
            glBindTexture(GL_TEXTURE_2D, *imageTextureGL);
            glutils::SetTexture2DSamplingParams(batch.imageSampler);
        }

        gpu::PipelineState pipelineState;
        gpu::get_pipeline_state(batch,
                                desc,
                                m_platformFeatures,
                                &pipelineState);
        if (desc.interlockMode == gpu::InterlockMode::msaa)
        {
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
        else if (desc.interlockMode == gpu::InterlockMode::atomics)
        {
            if (!desc.atomicFixedFunctionColorOutput &&
                drawType != gpu::DrawType::renderPassResolve)
            {
                // When rendering to an offscreen texture in atomic mode, GL
                // leaves the target framebuffer bound the whole time, but
                // disables color writes until it's time to resolve.
                pipelineState.colorWriteEnabled = false;
            }
        }
        m_state->setPipelineState(pipelineState);

        if (batch.barriers &
            (BarrierFlags::plsAtomic | BarrierFlags::plsAtomicPreResolve))
        {
            assert(desc.interlockMode == gpu::InterlockMode::atomics);
            m_plsImpl->barrier(desc);
        }
        else if (batch.barriers & BarrierFlags::dstBlend)
        {
            assert(!m_capabilities.KHR_blend_equation_advanced_coherent);
            if (m_capabilities.KHR_blend_equation_advanced)
            {
                glBlendBarrierKHR();
            }
            else
            {
                // Read back the framebuffer where we need a dstColor for
                // blending.
                assert(desc.interlockMode == gpu::InterlockMode::msaa);
                assert(batch.dstReadList != nullptr);
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
                renderTarget->bindMSAAFramebuffer(this, desc.msaaSampleCount);
            }
        }

        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::midpointFanCenterAAPatches:
            case DrawType::outerCurvePatches:
            case DrawType::msaaStrokes:
            case DrawType::msaaMidpointFanBorrowedCoverage:
            case DrawType::msaaMidpointFans:
            case DrawType::msaaMidpointFanStencilReset:
            case DrawType::msaaMidpointFanPathsStencil:
            case DrawType::msaaMidpointFanPathsCover:
            case DrawType::msaaOuterCubics:
            {
                m_state->bindVAO(m_drawVAO);
                if (desc.interlockMode == gpu::InterlockMode::rasterOrdering)
                {
                    m_plsImpl->ensureRasterOrderingEnabled(this, desc, true);
                }
                drawIndexedInstancedNoInstancedAttribs(
                    GL_TRIANGLES,
                    gpu::PatchIndexCount(drawType),
                    gpu::PatchBaseIndex(drawType),
                    batch.elementCount,
                    batch.baseElement,
                    drawProgram->baseInstanceUniformLocation());
                break;
            }

            case gpu::DrawType::msaaStencilClipReset:
            {
                m_state->bindVAO(m_trianglesVAO);
                glDrawArrays(GL_TRIANGLES,
                             batch.baseElement,
                             batch.elementCount);
                break;
            }

            case gpu::DrawType::interiorTriangulation:
            case gpu::DrawType::atlasBlit:
            {
                m_state->bindVAO(m_trianglesVAO);
                if (desc.interlockMode == gpu::InterlockMode::rasterOrdering)
                {
                    // Disable raster ordering if we're drawing true interior
                    // triangles (not atlas coverage). We know the triangulation
                    // is large enough that it's faster to issue a barrier than
                    // to force raster ordering in the fragment shader.
                    m_plsImpl->ensureRasterOrderingEnabled(
                        this,
                        desc,
                        drawType != gpu::DrawType::interiorTriangulation);
                }
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
                // m_imageRectVAO should have gotten lazily allocated by now.
                assert(desc.interlockMode == gpu::InterlockMode::atomics);
                assert(m_plsImpl->rasterOrderingKnownDisabled());
                assert(m_imageRectVAO != 0);
                m_state->bindVAO(m_imageRectVAO);
                glBindBufferRange(GL_UNIFORM_BUFFER,
                                  IMAGE_DRAW_UNIFORM_BUFFER_IDX,
                                  gl_buffer_id(imageDrawUniformBufferRing()),
                                  batch.imageDrawDataOffset,
                                  sizeof(gpu::ImageDrawUniforms));
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
                m_state->bindBuffer(GL_ARRAY_BUFFER, vertexBuffer->bufferID());
                glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
                m_state->bindBuffer(GL_ARRAY_BUFFER, uvBuffer->bufferID());
                glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
                m_state->bindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                                    indexBuffer->bufferID());
                glBindBufferRange(GL_UNIFORM_BUFFER,
                                  IMAGE_DRAW_UNIFORM_BUFFER_IDX,
                                  gl_buffer_id(imageDrawUniformBufferRing()),
                                  batch.imageDrawDataOffset,
                                  sizeof(gpu::ImageDrawUniforms));
                if (desc.interlockMode == gpu::InterlockMode::rasterOrdering)
                {
                    m_plsImpl->ensureRasterOrderingEnabled(this, desc, true);
                }
                glDrawElements(GL_TRIANGLES,
                               batch.elementCount,
                               GL_UNSIGNED_SHORT,
                               reinterpret_cast<const void*>(batch.baseElement *
                                                             sizeof(uint16_t)));
                break;
            }

            case gpu::DrawType::renderPassResolve:
            {
                assert(desc.interlockMode == gpu::InterlockMode::atomics);
                assert(m_plsImpl->rasterOrderingKnownDisabled());
                m_state->bindVAO(m_emptyVAO);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                break;
            }

            case gpu::DrawType::renderPassInitialize:
            {
                RIVE_UNREACHABLE();
            }
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

    // Various Android vendors experience synchronization issues with multiple
    // flushes per frame if we don't call glFlush in between.
    glFlush();

#ifndef RIVE_WEBGL
    // ARM Mali-G78 also needs a memory barrier sometimes to ensure a resolve of
    // EXT_multisampled_render_to_texture. (Note that the spec says these
    // resolves should all be implicit and automatic.)
    //
    // ALSO NOTE: We only do this barrier on Mali because the barrier actually
    // introduces corruption on Xiaomi Redmi Note 8 (Qualcomm Adreno 610).
    if (m_capabilities.isMali && m_capabilities.isGLES &&
        m_capabilities.isContextVersionAtLeast(3, 1))
    {
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
    }
#endif
}

void RenderContextGLImpl::drawIndexedInstancedNoInstancedAttribs(
    GLenum primitiveTopology,
    uint32_t indexCount,
    uint32_t baseIndex,
    uint32_t instanceCount,
    uint32_t baseInstance,
    GLint baseInstanceUniformLocation)
{
    assert(m_capabilities.ANGLE_base_vertex_base_instance_shader_builtin ==
           (baseInstanceUniformLocation < 0));
    const void* indexOffset =
        reinterpret_cast<const void*>(baseIndex * sizeof(uint16_t));
    for (auto [chunkInstanceCount, chunkBaseInstance] :
         InstanceChunker(instanceCount,
                         baseInstance,
                         m_capabilities.maxSupportedInstancesPerDrawCommand))
    {
#ifndef RIVE_WEBGL
        if (m_capabilities.ANGLE_base_vertex_base_instance_shader_builtin)
        {
            glDrawElementsInstancedBaseInstanceEXT(primitiveTopology,
                                                   indexCount,
                                                   GL_UNSIGNED_SHORT,
                                                   indexOffset,
                                                   chunkInstanceCount,
                                                   chunkBaseInstance);
        }
        else
#endif
        {
            glUniform1i(baseInstanceUniformLocation, chunkBaseInstance);
            glDrawElementsInstanced(primitiveTopology,
                                    indexCount,
                                    GL_UNSIGNED_SHORT,
                                    indexOffset,
                                    chunkInstanceCount);
        }
    }
}

void RenderContextGLImpl::blitTextureToFramebufferAsDraw(
    GLuint textureID,
    const IAABB& bounds,
    uint32_t renderTargetHeight)
{
    if (m_blitAsDrawProgram == 0)
    {
        const char* blitSources[] = {glsl::constants,
                                     glsl::common,
                                     glsl::blit_texture_as_draw};
        m_blitAsDrawProgram = glutils::Program();
        m_blitAsDrawProgram.compileAndAttachShader(GL_VERTEX_SHADER,
                                                   nullptr,
                                                   0,
                                                   blitSources,
                                                   std::size(blitSources),
                                                   m_capabilities);
        m_blitAsDrawProgram.compileAndAttachShader(GL_FRAGMENT_SHADER,
                                                   nullptr,
                                                   0,
                                                   blitSources,
                                                   std::size(blitSources),
                                                   m_capabilities);
        m_blitAsDrawProgram.link();
        m_state->bindProgram(m_blitAsDrawProgram);
        glutils::Uniform1iByName(m_blitAsDrawProgram, GLSL_sourceTexture, 0);
    }

    m_state->setPipelineState(gpu::COLOR_ONLY_PIPELINE_STATE);
    m_state->bindProgram(m_blitAsDrawProgram);
    m_state->bindVAO(m_emptyVAO);
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

#ifdef WITH_RIVE_TOOLS
void RenderContextGLImpl::testingOnly_resetAtlasDesiredType(
    RenderContext* owningRenderContext,
    AtlasType atlasDesiredType)
{
    owningRenderContext->releaseResources();
    assert(m_atlasTexture == 0); // Should be cleared by releaseResources().
    m_atlasFBO = {};

    // Now release the atlas pipelines so they can be recompiled for the new
    // AtlasType.
    m_atlasVertexShader = {};
    m_atlasFillProgram = {};
    m_atlasStrokeProgram = {};
#ifdef RIVE_ANDROID
    m_atlasResolveVertexShader = {};
    m_atlasClearProgram = glutils::Program::Zero();
    m_atlasResolveProgram = glutils::Program::Zero();
#endif

    // ...And release all the DrawShaders in case any need to be recompiled for
    // sampling a different AtlasType.
    m_pipelineManager.clearCache();

    m_atlasType = select_atlas_type(m_capabilities, atlasDesiredType);
}
#endif

#ifdef _MSC_VER
#define SSCANF sscanf_s
#else
#define SSCANF sscanf
#endif

std::unique_ptr<RenderContext> RenderContextGLImpl::MakeContext(
    const ContextOptions& contextOptions)
{
    const char* glVersionStr = (const char*)glGetString(GL_VERSION);

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

    GLCapabilities capabilities{};
#ifdef RIVE_WEBGL
    capabilities.isGLES = true;
    // If GL_UNMASKED_RENDERER_WEBGL says "ANGLE", that means we are running on
    // an ANGLE system driver. e.g.:
    //
    //   WebGL (probably ANGLE) -> System OpenGL ES (also ANGLE) -> Vulkan
    //
    capabilities.isANGLESystemDriver =
        strstr(rendererString, "ANGLE") != nullptr;
#else
    capabilities.isGLES = strstr(glVersionStr, "OpenGL ES") != nullptr;
    capabilities.isANGLESystemDriver =
        strstr(glVersionStr, "ANGLE") != nullptr ||
        strstr(rendererString, "ANGLE") != nullptr;
#endif
    capabilities.isAdreno = strstr(rendererString, "Adreno");
    capabilities.isMali = strstr(rendererString, "Mali");
    capabilities.isPowerVR = strstr(rendererString, "PowerVR");

    if (!capabilities.isGLES)
    {
        SSCANF(glVersionStr,
               "%u.%u",
               &capabilities.contextVersionMajor,
               &capabilities.contextVersionMinor);
    }
    else if (capabilities.isPowerVR)
    {
        SSCANF(glVersionStr,
               "OpenGL ES %u.%u build %u.%u@",
               &capabilities.contextVersionMajor,
               &capabilities.contextVersionMinor,
               &capabilities.vendorDriverVersionMajor,
               &capabilities.vendorDriverVersionMinor);
    }
    else
    {
        SSCANF(glVersionStr,
               "OpenGL ES %u.%u",
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
        if (capabilities.isContextVersionAtLeast(3, 2))
        {
            capabilities.OES_shader_image_atomic = true;
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
        else if (strcmp(ext, "GL_OES_shader_image_atomic") == 0)
        {
            capabilities.OES_shader_image_atomic = true;
        }
        else if (strcmp(ext, "GL_KHR_blend_equation_advanced") == 0)
        {
            capabilities.KHR_blend_equation_advanced = true;
        }
        else if (strcmp(ext, "GL_KHR_blend_equation_advanced_coherent") == 0)
        {
            capabilities.KHR_blend_equation_advanced_coherent = true;
        }
        else if (strcmp(ext, "GL_KHR_parallel_shader_compile") == 0)
        {
            capabilities.KHR_parallel_shader_compile = true;
        }
        else if (strcmp(ext, "GL_EXT_base_instance") == 0)
        {
            capabilities.EXT_base_instance = true;
        }
        else if (strcmp(ext, "GL_EXT_clip_cull_distance") == 0 ||
                 strcmp(ext, "GL_ANGLE_clip_cull_distance") == 0)
        {
            capabilities.EXT_clip_cull_distance = true;
        }
        else if (strcmp(ext, "GL_EXT_multisampled_render_to_texture") == 0)
        {
            capabilities.EXT_multisampled_render_to_texture = true;
        }
        else if (strcmp(ext, "GL_INTEL_fragment_shader_ordering") == 0)
        {
            capabilities.INTEL_fragment_shader_ordering = true;
        }
        else if (strcmp(ext, "GL_EXT_color_buffer_half_float") == 0)
        {
            capabilities.EXT_color_buffer_half_float = true;
        }
        else if (strcmp(ext, "GL_EXT_color_buffer_float") == 0)
        {
            capabilities.EXT_color_buffer_float = true;
        }
        else if (strcmp(ext, "GL_EXT_float_blend") == 0)
        {
            capabilities.EXT_float_blend = true;
        }
        else if (strcmp(ext, "GL_ARB_color_buffer_float") == 0)
        {
            capabilities.EXT_color_buffer_half_float = true;
            capabilities.EXT_color_buffer_float = true;
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
        else if (strcmp(ext, "GL_EXT_shader_pixel_local_storage2") == 0)
        {
            capabilities.EXT_shader_pixel_local_storage2 = true;
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
            "EXT_color_buffer_float"))
    {
        capabilities.EXT_color_buffer_float = true;
    }
    if (emscripten_webgl_enable_extension(
            emscripten_webgl_get_current_context(),
            "EXT_float_blend"))
    {
        capabilities.EXT_float_blend = true;
    }
    if (emscripten_webgl_enable_extension(
            emscripten_webgl_get_current_context(),
            "KHR_parallel_shader_compile"))
    {
        capabilities.KHR_parallel_shader_compile = true;
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

    if (capabilities.OES_shader_image_atomic)
    {
        if (capabilities.isMali || capabilities.isPowerVR ||
            (capabilities.isAdreno &&
             strstr(rendererString, "Adreno (TM) 640") == nullptr))
        {
            // Don't use image atomics for feathering on Adreno, Mali, or
            // PowerVR. On Adreno (specifically 660 & 642L) and PowerVR they
            // sometimes just don't render, and on Mali they lead to a failure
            // that says:
            //
            //   Error:glDrawElementsInstanced::failed to allocate CPU memory
            //
            // NOTE: We allow Adreno 640 to use atomics because it works
            // reliably on our CI and gives us coverage of this codepath for ES.
            //
            // Realistically these vendors have better ways to render the
            // feather atlas that they will use in real lifeanyway, namely,
            // EXT_float_blend, EXT_color_buffer_half_float,
            // EXT_shader_framebuffer_fetch, and/or
            // EXT_shader_pixel_local_storage.
            //
            // It's possible we have some barriers wrong, but a fallback this
            // deep isn't a priority right now on GL.
            capabilities.OES_shader_image_atomic = false;
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

    if (capabilities.ANGLE_base_vertex_base_instance_shader_builtin)
    {
        if (strstr(rendererString, "Metal") != nullptr ||
            strstr(rendererString, "Direct3D") != nullptr)
        {
            // Disable ANGLE_base_vertex_base_instance_shader_builtin on
            // ANGLE/D3D and ANGLE/Metal. The extension is polyfilled on D3D
            // anyway, and on Metal it crashes.
            capabilities.ANGLE_base_vertex_base_instance_shader_builtin = false;
        }
    }

    if (capabilities.EXT_clip_cull_distance)
    {
        if (capabilities.isANGLESystemDriver)
        {
            // Don't use EXT_clip_cull_distance or ANGLE_clip_cull_distance if
            // our system GL driver is ANGLE. Various Galaxy devices using ANGLE
            // have bugs with these extensions.
            capabilities.EXT_clip_cull_distance = false;
        }
    }

    if (capabilities.EXT_multisampled_render_to_texture)
    {
        if (strstr(rendererString, "Direct3D") != nullptr)
        {
            // Our use of EXT_multisampled_render_to_texture causes a segfault
            // in the Microsoft WARP (software) renderer. Just don't use this
            // extension on D3D since it's polyfilled anyway.
            capabilities.EXT_multisampled_render_to_texture = false;
        }
        if (capabilities.isPowerVR &&
            !capabilities.isVendorDriverVersionAtLeast(1, 13))
        {
            // PowerVR Rogue GE8300, OpenGL ES 3.2 build 1.10@5187610 and
            // PowerVR Rogue GM9446; OpenGL ES 3.2 build 1.11@5425693 both have
            // similar artifacts when using EXT_multisampled_render_to_texture.
            // Block the extension before the earliest known good driver, which
            // is 1.13.
            capabilities.EXT_multisampled_render_to_texture = false;
        }
    }

    if (contextOptions.disableFragmentShaderInterlock)
    {
        // Disable the extensions we don't want to use internally.
        capabilities.ARB_fragment_shader_interlock = false;
        capabilities.INTEL_fragment_shader_ordering = false;
    }

#ifdef RIVE_ANDROID
    // On Android we need to explicitly load the extension functions. This will
    // additionally clear the capabilities flags for any extension that could
    // not load.
    LoadAndValidateGLESExtensions(&capabilities);
#endif

    if (strstr(rendererString, "ANGLE Metal Renderer") != nullptr &&
        capabilities.EXT_color_buffer_float)
    {
        capabilities.needsFloatingPointTessellationTexture = true;
    }
    else
    {
        capabilities.needsFloatingPointTessellationTexture = false;
    }

    if (capabilities.EXT_shader_pixel_local_storage2)
    {
        if (capabilities.isPowerVR &&
            !capabilities.isVendorDriverVersionAtLeast(1, 11))
        {
            // PowerVR Rogue GE8300, OpenGL ES 3.2 build 1.10@5187610 has severe
            // pixel local storage corruption issues with our renderer. Using
            // some of the EXT_shader_pixel_local_storage2 API is an apparent
            // workaround that comes with worse performance and other, less
            // severe visual artifacts.
            // Require this workaround before the earliest known good driver,
            // which is 1.11.
            capabilities.needsPixelLocalStorage2 = true;
        }
    }

    if (!contextOptions.disablePixelLocalStorage)
    {
#ifdef RIVE_ANDROID
        if (capabilities.EXT_shader_pixel_local_storage &&
            (capabilities.ARM_shader_framebuffer_fetch ||
             capabilities.EXT_shader_framebuffer_fetch))
        {
            // Favor MSAA over pixel local storage on PowerVR due to various
            // bugs in its driver, except on PowerVR pre-1.15, where MSAA
            // doesn't work.
            if (!capabilities.isPowerVR ||
                !capabilities.isVendorDriverVersionAtLeast(1, 15))
            {
                return MakeContext(rendererString,
                                   capabilities,
                                   MakePLSImplEXTNative(capabilities),
                                   contextOptions.shaderCompilationMode);
            }
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
                                   MakePLSImplWebGL(),
                                   contextOptions.shaderCompilationMode);
            }
        }
#endif

#ifdef RIVE_DESKTOP_GL
        if (capabilities.ARB_shader_image_load_store)
        {
            return MakeContext(rendererString,
                               capabilities,
                               MakePLSImplRWTexture(),
                               contextOptions.shaderCompilationMode);
        }
#endif
    }

    return MakeContext(rendererString,
                       capabilities,
                       nullptr,
                       contextOptions.shaderCompilationMode);
}

RenderContextGLImpl::GLPipelineManager::GLPipelineManager(
    ShaderCompilationMode mode,
    RenderContextGLImpl* context) :
    Super(mode), m_context(context)
{}

std::unique_ptr<RenderContextGLImpl::DrawProgram> RenderContextGLImpl::
    GLPipelineManager::createPipeline(PipelineCreateType createType,
                                      uint32_t, // unused key
                                      const PipelineProps& props)
{
    return std::make_unique<DrawProgram>(m_context,
                                         createType,
                                         props.drawType,
                                         props.shaderFeatures,
                                         props.interlockMode,
                                         props.shaderMiscFlags
#ifdef WITH_RIVE_TOOLS
                                         ,
                                         props.synthesizedFailureType
#endif
    );
}

PipelineStatus RenderContextGLImpl::GLPipelineManager::getPipelineStatus(
    const DrawProgram& state) const
{
    return state.status();
}

bool RenderContextGLImpl::GLPipelineManager::advanceCreation(
    DrawProgram& pipelineState,
    const PipelineProps& props)
{
    return pipelineState.advanceCreation(m_context,
                                         PipelineCreateType::async,
                                         props.drawType,
                                         props.shaderFeatures,
                                         props.interlockMode,
                                         props.shaderMiscFlags);
}

std::unique_ptr<RenderContextGLImpl::DrawShader> RenderContextGLImpl::
    GLPipelineManager::createVertexShader(DrawType drawType,
                                          ShaderFeatures shaderFeatures,
                                          InterlockMode interlockMode)
{
    return std::make_unique<DrawShader>(m_context,
                                        GL_VERTEX_SHADER,
                                        drawType,
                                        shaderFeatures,
                                        interlockMode,
                                        ShaderMiscFlags::none);
}

std::unique_ptr<RenderContextGLImpl::DrawShader> RenderContextGLImpl::
    GLPipelineManager::createFragmentShader(DrawType drawType,
                                            ShaderFeatures shaderFeatures,
                                            InterlockMode interlockMode,
                                            ShaderMiscFlags miscFlags)
{
    return std::make_unique<DrawShader>(m_context,
                                        GL_FRAGMENT_SHADER,
                                        drawType,
                                        shaderFeatures,
                                        interlockMode,
                                        miscFlags);
}

std::unique_ptr<RenderContext> RenderContextGLImpl::MakeContext(
    const char* rendererString,
    GLCapabilities capabilities,
    std::unique_ptr<PixelLocalStorageImpl> plsImpl,
    ShaderCompilationMode shaderCompilationMode)
{
    auto renderContextImpl = std::unique_ptr<RenderContextGLImpl>(
        new RenderContextGLImpl(rendererString,
                                capabilities,
                                std::move(plsImpl),
                                shaderCompilationMode));
    return std::make_unique<RenderContext>(std::move(renderContextImpl));
}
} // namespace rive::gpu
