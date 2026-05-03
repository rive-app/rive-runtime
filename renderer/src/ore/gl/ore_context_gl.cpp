/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/gl/load_gles_extensions.hpp"
#include "rive/renderer/gl/render_target_gl.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_context.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/rive_types.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#include <cstdio>
#endif

namespace rive::ore
{

// ============================================================================
// Enum → GL conversion helpers (used by factory methods)
// ============================================================================

static GLenum oreFormatToGLInternal(TextureFormat fmt)
{
    switch (fmt)
    {
        case TextureFormat::r8unorm:
            return GL_R8;
        case TextureFormat::rg8unorm:
            return GL_RG8;
        case TextureFormat::rgba8unorm:
            return GL_RGBA8;
        case TextureFormat::rgba8snorm:
            return GL_RGBA8_SNORM;
        case TextureFormat::bgra8unorm:
            return GL_RGBA8;
        case TextureFormat::rgba16float:
            return GL_RGBA16F;
        case TextureFormat::rg16float:
            return GL_RG16F;
        case TextureFormat::r16float:
            return GL_R16F;
        case TextureFormat::rgba32float:
            return GL_RGBA32F;
        case TextureFormat::rg32float:
            return GL_RG32F;
        case TextureFormat::r32float:
            return GL_R32F;
        case TextureFormat::rgb10a2unorm:
            return GL_RGB10_A2;
        case TextureFormat::r11g11b10float:
            return GL_R11F_G11F_B10F;
        case TextureFormat::depth16unorm:
            return GL_DEPTH_COMPONENT16;
        case TextureFormat::depth24plusStencil8:
            return GL_DEPTH24_STENCIL8;
        case TextureFormat::depth32float:
            return GL_DEPTH_COMPONENT32F;
        case TextureFormat::depth32floatStencil8:
            return GL_DEPTH32F_STENCIL8;
        case TextureFormat::etc2rgb8:
            return GL_COMPRESSED_RGB8_ETC2;
        case TextureFormat::etc2rgba8:
            return GL_COMPRESSED_RGBA8_ETC2_EAC;
#ifdef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
        case TextureFormat::bc1unorm:
            return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
        case TextureFormat::bc3unorm:
            return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
#else
        case TextureFormat::bc1unorm:
        case TextureFormat::bc3unorm:
            RIVE_UNREACHABLE();
#endif
#ifdef GL_COMPRESSED_RGBA_BPTC_UNORM
        case TextureFormat::bc7unorm:
            return GL_COMPRESSED_RGBA_BPTC_UNORM;
#else
        case TextureFormat::bc7unorm:
            RIVE_UNREACHABLE();
#endif
#ifdef GL_COMPRESSED_RGBA_ASTC_4x4_KHR
        case TextureFormat::astc4x4:
            return GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
        case TextureFormat::astc6x6:
            return GL_COMPRESSED_RGBA_ASTC_6x6_KHR;
        case TextureFormat::astc8x8:
            return GL_COMPRESSED_RGBA_ASTC_8x8_KHR;
#else
        case TextureFormat::astc4x4:
        case TextureFormat::astc6x6:
        case TextureFormat::astc8x8:
            RIVE_UNREACHABLE();
#endif
    }
    RIVE_UNREACHABLE();
}

static bool isDepthFormat(TextureFormat fmt)
{
    return fmt == TextureFormat::depth16unorm ||
           fmt == TextureFormat::depth24plusStencil8 ||
           fmt == TextureFormat::depth32float ||
           fmt == TextureFormat::depth32floatStencil8;
}

static GLenum oreTextureTypeToGLTarget(TextureType type)
{
    switch (type)
    {
        case TextureType::texture2D:
            return GL_TEXTURE_2D;
        case TextureType::cube:
            return GL_TEXTURE_CUBE_MAP;
        case TextureType::texture3D:
            return GL_TEXTURE_3D;
        case TextureType::array2D:
            return GL_TEXTURE_2D_ARRAY;
    }
    RIVE_UNREACHABLE();
}

static GLenum oreFilterToGL(Filter f)
{
    return (f == Filter::linear) ? GL_LINEAR : GL_NEAREST;
}

static GLenum oreMipmapFilterToGL(Filter min, Filter mip)
{
    if (min == Filter::linear)
    {
        return (mip == Filter::linear) ? GL_LINEAR_MIPMAP_LINEAR
                                       : GL_LINEAR_MIPMAP_NEAREST;
    }
    return (mip == Filter::linear) ? GL_NEAREST_MIPMAP_LINEAR
                                   : GL_NEAREST_MIPMAP_NEAREST;
}

static GLenum oreWrapToGL(WrapMode mode)
{
    switch (mode)
    {
        case WrapMode::repeat:
            return GL_REPEAT;
        case WrapMode::mirrorRepeat:
            return GL_MIRRORED_REPEAT;
        case WrapMode::clampToEdge:
            return GL_CLAMP_TO_EDGE;
    }
    RIVE_UNREACHABLE();
}

static GLenum oreCompareFunctionToGL(CompareFunction fn)
{
    switch (fn)
    {
        case CompareFunction::none:
        case CompareFunction::never:
            return GL_NEVER;
        case CompareFunction::less:
            return GL_LESS;
        case CompareFunction::equal:
            return GL_EQUAL;
        case CompareFunction::lessEqual:
            return GL_LEQUAL;
        case CompareFunction::greater:
            return GL_GREATER;
        case CompareFunction::notEqual:
            return GL_NOTEQUAL;
        case CompareFunction::greaterEqual:
            return GL_GEQUAL;
        case CompareFunction::always:
            return GL_ALWAYS;
    }
    RIVE_UNREACHABLE();
}

// ============================================================================
// Context lifecycle
// ============================================================================

// When both Metal and GL are compiled into the same binary (macOS), the
// ore_context_metal_gl.mm file provides all Context method definitions with
// runtime dispatch. Only compile GL Context definitions when Metal is absent.
#if defined(ORE_BACKEND_GL) && !defined(ORE_BACKEND_METAL) &&                  \
    !defined(ORE_BACKEND_VK)

Context::Context() {}

Context::~Context() {}

Context::Context(Context&& other) noexcept :
    m_features(other.m_features), m_savedState(other.m_savedState)
{}

Context& Context::operator=(Context&& other) noexcept
{
    if (this != &other)
    {
        m_features = other.m_features;
        m_savedState = other.m_savedState;
    }
    return *this;
}

Context Context::createGL()
{
    Context ctx;

    Features& f = ctx.m_features;

    // GLES3 / WebGL2 baseline capabilities.
    f.colorBufferFloat = false; // Requires EXT_color_buffer_float.
    f.perTargetBlend = false;   // GLES3: global blend only.
    f.perTargetWriteMask = false;
    f.textureViewSampling = false; // No glTextureView on GLES3.
    f.drawBaseInstance = false;
    f.depthBiasClamp = false; // GLES3 glPolygonOffset has no clamp param.
    f.anisotropicFiltering = false;
    f.texture3D = true;
    f.textureArrays = true;
    f.computeShaders = false;
    f.storageBuffers = false;
    f.bc = false;
    f.etc2 = true; // GLES3 mandates ETC2.
    f.astc = false;

    // Query limits.
    GLint maxTexSize = 0, maxCubeSize = 0, max3DSize = 0, maxUBOSize = 0;
    GLint maxDrawBuffers = 0, maxVertexAttribs = 0, maxTexUnits = 0;
    GLint maxSamples = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
    glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &maxCubeSize);
    glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &max3DSize);
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUBOSize);
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxTexUnits);
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);

    f.maxTextureSize2D = maxTexSize;
    f.maxTextureSizeCube = maxCubeSize;
    f.maxTextureSize3D = max3DSize;
    f.maxUniformBufferSize = maxUBOSize;
    f.maxColorAttachments = std::min(static_cast<uint32_t>(maxDrawBuffers), 4u);
    f.maxVertexAttributes = maxVertexAttribs;
    f.maxSamplers = maxTexUnits;
    f.maxSamples = std::max(maxSamples, 1);

    // Check extensions.
    GLint numExtensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
    for (GLint i = 0; i < numExtensions; ++i)
    {
        const char* ext =
            reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i));
        if (!ext)
            continue;
        if (strcmp(ext, "GL_EXT_color_buffer_float") == 0)
            f.colorBufferFloat = true;
        else if (strcmp(ext, "GL_EXT_texture_filter_anisotropic") == 0)
            f.anisotropicFiltering = true;
        else if (strcmp(ext, "GL_KHR_texture_compression_astc_ldr") == 0)
            f.astc = true;
#ifdef RIVE_DESKTOP_GL
        else if (strcmp(ext, "GL_EXT_texture_compression_s3tc") == 0)
            f.bc = true;
#endif
    }

    return ctx;
}

void Context::beginFrame()
{
    // Release deferred BindGroups from last frame. By beginFrame() the
    // caller has waited for the previous frame's GPU work to complete.
    m_deferredBindGroups.clear();

    // Save GL state that Ore will modify, so Rive's PLS renderer state is
    // preserved across an Ore render pass.
    glGetIntegerv(GL_CURRENT_PROGRAM, &m_savedState.program);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &m_savedState.arrayBuffer);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_savedState.framebuffer);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &m_savedState.vertexArray);
    // NOTE: GL_ELEMENT_ARRAY_BUFFER is intentionally NOT saved here because
    // it is part of VAO state — restoring the VAO implicitly restores the
    // element buffer that was associated with it.
}

void Context::waitForGPU() {} // GL is synchronous after glFinish/flush.

void Context::endFrame()
{
    // Restore saved state. Each `RenderPass::finish()` already restores
    // its own captured VAO in-place, so by the time we get here only the
    // program / array-buffer / framebuffer bindings need restoring —
    // and only if they still exist. Mac ANGLE (GL-ES-3 over Metal)
    // strictly validates non-zero object IDs via GL_KHR_debug and will
    // abort if the host renderer has deleted a program or buffer
    // between our `beginFrame` save and this restore. Guard each bind
    // with the matching `glIs*` probe; name == 0 always refers to the
    // default object (no validation error).
    if (m_savedState.program == 0 ||
        glIsProgram(m_savedState.program) == GL_TRUE)
    {
        glUseProgram(m_savedState.program);
    }
    if (m_savedState.vertexArray == 0 ||
        glIsVertexArray(m_savedState.vertexArray) == GL_TRUE)
    {
        glBindVertexArray(m_savedState.vertexArray);
    }
    if (m_savedState.arrayBuffer == 0 ||
        glIsBuffer(m_savedState.arrayBuffer) == GL_TRUE)
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_savedState.arrayBuffer);
    }
    // NOTE: GL_ELEMENT_ARRAY_BUFFER is intentionally NOT restored here.
    // It is VAO state — binding the saved VAO above already restored the
    // element buffer association. Explicitly binding it here would modify
    // the active VAO's element buffer, which can corrupt the host
    // renderer's VAO state if the saved value doesn't match.
    if (m_savedState.framebuffer == 0 ||
        glIsFramebuffer(m_savedState.framebuffer) == GL_TRUE)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_savedState.framebuffer);
    }
}

// ============================================================================
// makeBuffer
// ============================================================================

rcp<Buffer> Context::makeBuffer(const BufferDesc& desc)
{
    auto buffer = rcp<Buffer>(new Buffer(desc.size, desc.usage));

    glGenBuffers(1, &buffer->m_glBuffer);

    switch (desc.usage)
    {
        case BufferUsage::vertex:
            buffer->m_glTarget = GL_ARRAY_BUFFER;
            break;
        case BufferUsage::index:
            buffer->m_glTarget = GL_ELEMENT_ARRAY_BUFFER;
            break;
        case BufferUsage::uniform:
            buffer->m_glTarget = GL_UNIFORM_BUFFER;
            break;
    }

    // WebGL forbids binding a buffer to GL_ELEMENT_ARRAY_BUFFER if it was ever
    // bound to a different target, so index buffers must use their native
    // target. GL_ELEMENT_ARRAY_BUFFER is VAO state, so we save/restore the
    // current binding to avoid clobbering the host renderer's VAO.
    GLenum glUsage = (desc.data != nullptr) ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW;
    if (desc.usage == BufferUsage::index)
    {
        GLint prevEBO;
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &prevEBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->m_glBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, desc.size, desc.data, glUsage);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prevEBO);
    }
    else
    {
        glBindBuffer(GL_COPY_WRITE_BUFFER, buffer->m_glBuffer);
        glBufferData(GL_COPY_WRITE_BUFFER, desc.size, desc.data, glUsage);
        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    }

    return buffer;
}

// ============================================================================
// makeTexture
// ============================================================================

rcp<Texture> Context::makeTexture(const TextureDesc& desc)
{
    auto texture = rcp<Texture>(new Texture(desc));

    glGenTextures(1, &texture->m_glTexture);
    texture->m_glTarget = oreTextureTypeToGLTarget(desc.type);

    GLenum internalFormat = oreFormatToGLInternal(desc.format);

    // Force the active unit to GL_TEXTURE0 before a transient bind. The host
    // renderer (Rive PLS) reserves units 8..14 for its own textures and
    // assumes those bindings persist between flushes. If the caller entered
    // with one of those units active (common after a flush), a naked
    // glBindTexture() here would overwrite Rive's binding on that unit.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(texture->m_glTarget, texture->m_glTexture);

    switch (desc.type)
    {
        case TextureType::texture2D:
            if (desc.sampleCount > 1)
            {
                // MSAA render targets use renderbuffers — they can't be
                // sampled anyway, and renderbuffers work on all
                // GL/GLES3/WebGL2 (unlike GL_TEXTURE_2D_MULTISAMPLE).
                glDeleteTextures(1, &texture->m_glTexture);
                texture->m_glTexture = 0;
                texture->m_glTarget = 0;

                glGenRenderbuffers(1, &texture->m_glRenderbuffer);
                glBindRenderbuffer(GL_RENDERBUFFER, texture->m_glRenderbuffer);
                glRenderbufferStorageMultisample(GL_RENDERBUFFER,
                                                 desc.sampleCount,
                                                 internalFormat,
                                                 desc.width,
                                                 desc.height);
                glBindRenderbuffer(GL_RENDERBUFFER, 0);
            }
            else
            {
                glTexStorage2D(GL_TEXTURE_2D,
                               desc.numMipmaps,
                               internalFormat,
                               desc.width,
                               desc.height);
            }
            break;

        case TextureType::cube:
            glTexStorage2D(GL_TEXTURE_CUBE_MAP,
                           desc.numMipmaps,
                           internalFormat,
                           desc.width,
                           desc.height);
            break;

        case TextureType::texture3D:
            glTexStorage3D(GL_TEXTURE_3D,
                           desc.numMipmaps,
                           internalFormat,
                           desc.width,
                           desc.height,
                           desc.depthOrArrayLayers);
            break;

        case TextureType::array2D:
            glTexStorage3D(GL_TEXTURE_2D_ARRAY,
                           desc.numMipmaps,
                           internalFormat,
                           desc.width,
                           desc.height,
                           desc.depthOrArrayLayers);
            break;
    }

    // Renderbuffer-backed MSAA textures don't have sampler parameters.
    if (texture->m_glRenderbuffer == 0)
    {
        glTexParameteri(texture->m_glTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(texture->m_glTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(texture->m_glTarget,
                        GL_TEXTURE_WRAP_S,
                        GL_CLAMP_TO_EDGE);
        glTexParameteri(texture->m_glTarget,
                        GL_TEXTURE_WRAP_T,
                        GL_CLAMP_TO_EDGE);
        glBindTexture(texture->m_glTarget, 0);
    }

    return texture;
}

// ============================================================================
// makeTextureView
// ============================================================================

rcp<TextureView> Context::makeTextureView(const TextureViewDesc& desc)
{
    Texture* tex = desc.texture;
    if (!tex)
        return nullptr;

    auto view = rcp<TextureView>(new TextureView(ref_rcp(tex), desc));

    // GLES3 doesn't have glTextureView. We store m_glTextureView = 0
    // and use the base texture at bind time. FBO attachment targeting
    // specific mip/layer is done via glFramebufferTextureLayer.
    view->m_glTextureView = 0;

    return view;
}

// ============================================================================
// makeSampler
// ============================================================================

rcp<Sampler> Context::makeSampler(const SamplerDesc& desc)
{
    auto sampler = rcp<Sampler>(new Sampler());

    glGenSamplers(1, &sampler->m_glSampler);
    GLuint s = sampler->m_glSampler;

    // Min filter includes mipmap mode.
    if (desc.maxLod > 0.0f)
    {
        glSamplerParameteri(
            s,
            GL_TEXTURE_MIN_FILTER,
            oreMipmapFilterToGL(desc.minFilter, desc.mipmapFilter));
    }
    else
    {
        glSamplerParameteri(s,
                            GL_TEXTURE_MIN_FILTER,
                            oreFilterToGL(desc.minFilter));
    }
    glSamplerParameteri(s,
                        GL_TEXTURE_MAG_FILTER,
                        oreFilterToGL(desc.magFilter));

    glSamplerParameteri(s, GL_TEXTURE_WRAP_S, oreWrapToGL(desc.wrapU));
    glSamplerParameteri(s, GL_TEXTURE_WRAP_T, oreWrapToGL(desc.wrapV));
    glSamplerParameteri(s, GL_TEXTURE_WRAP_R, oreWrapToGL(desc.wrapW));

    glSamplerParameterf(s, GL_TEXTURE_MIN_LOD, desc.minLod);
    glSamplerParameterf(s, GL_TEXTURE_MAX_LOD, desc.maxLod);

    if (desc.compare != CompareFunction::none)
    {
        glSamplerParameteri(s,
                            GL_TEXTURE_COMPARE_MODE,
                            GL_COMPARE_REF_TO_TEXTURE);
        glSamplerParameteri(s,
                            GL_TEXTURE_COMPARE_FUNC,
                            oreCompareFunctionToGL(desc.compare));
    }

    return sampler;
}

// ============================================================================
// makeShaderModule
// ============================================================================

rcp<ShaderModule> Context::makeShaderModule(const ShaderModuleDesc& desc)
{
    auto module = rcp<ShaderModule>(new ShaderModule());

    const char* source = static_cast<const char*>(desc.code);

    bool isVertex;
    if (desc.stage != ShaderStage::autoDetect)
    {
        isVertex = (desc.stage == ShaderStage::vertex);
    }
    else
    {
        // Legacy fallback: detect shader type from gl_Position presence.
        // Use bounded search — RSTB blobs are NOT null-terminated.
        const char* needle = "gl_Position";
        const size_t needleLen = 11; // strlen("gl_Position")
        isVertex = false;
        if (desc.codeSize >= needleLen)
        {
            const char* end = source + desc.codeSize - needleLen + 1;
            for (const char* p = source; p < end; ++p)
            {
                if (memcmp(p, needle, needleLen) == 0)
                {
                    isVertex = true;
                    break;
                }
            }
        }
    }
    module->m_glShaderType = isVertex ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;

    module->m_glShader = glCreateShader(module->m_glShaderType);

    GLint length = static_cast<GLint>(desc.codeSize);
    glShaderSource(module->m_glShader, 1, &source, &length);
    glCompileShader(module->m_glShader);

    GLint status = 0;
    glGetShaderiv(module->m_glShader, GL_COMPILE_STATUS, &status);
    if (!status)
    {
        char log[1024] = {};
        GLint logLen = 0;
        glGetShaderiv(module->m_glShader, GL_INFO_LOG_LENGTH, &logLen);
        if (logLen > 0)
        {
            glGetShaderInfoLog(module->m_glShader,
                               std::min(logLen, (GLint)sizeof(log)),
                               nullptr,
                               log);
        }
        setLastError("Ore GL shader compile error: %s", log);
        glDeleteShader(module->m_glShader);
        return nullptr;
    }

    module->applyBindingMapFromDesc(desc);
    return module;
}

#endif // close standalone-GL guard; helpers below are shared with VK+GL

// ============================================================================
// GL uniform fixup (shared by pure GL and VK+GL backends)
// ============================================================================

/// Fix up UBO block bindings and sampler texture unit assignments for a
/// linked GL program. Driven entirely by the RSTB-baked fixup tables
/// carried on each `ShaderModule` (sidecar targets 14 = VS, 15 = FS).
/// The runtime never parses the emitted GLSL names — the shader-side
/// allocator and the runtime-side `BindingMap::lookup` agree on a single
/// set of GL slots via the allocator's output.
///
/// Calls `glUniformBlockBinding` / `glUniform1i` by name: redundant
/// calls (a UBO declared in both VS and FS, which shows up in both
/// stages' fixup tables with the same slot) are harmless — the GL
/// driver overwrites with the same value.
static void oreGLFixupProgramBindings(GLuint program,
                                      const ShaderModule* vsModule,
                                      const ShaderModule* fsModule)
{
    glUseProgram(program);

    auto applyEntry = [&](const ShaderModule::GLFixupEntry& e) {
        if (e.kind == ShaderModule::GLFixupEntry::Kind::UBOBlock)
        {
            GLuint idx = glGetUniformBlockIndex(program, e.name.c_str());
            if (idx != GL_INVALID_INDEX)
                glUniformBlockBinding(program, idx, e.slot);
        }
        else // SamplerUniform
        {
            GLint loc = glGetUniformLocation(program, e.name.c_str());
            if (loc >= 0)
                glUniform1i(loc, e.slot);
        }
    };

    if (vsModule != nullptr)
    {
        for (const auto& e : vsModule->m_glFixup)
            applyEntry(e);
    }
    if (fsModule != nullptr && fsModule != vsModule)
    {
        for (const auto& e : fsModule->m_glFixup)
            applyEntry(e);
    }

    glUseProgram(0);
}

#if defined(ORE_BACKEND_GL) && !defined(ORE_BACKEND_METAL) &&                  \
    !defined(ORE_BACKEND_VK)

// ============================================================================
// makePipeline
// ============================================================================

rcp<Pipeline> Context::makePipeline(const PipelineDesc& desc,
                                    std::string* outError)
{
    auto pipeline = rcp<Pipeline>(new Pipeline(desc));

    // --- Validate user-supplied layouts against shader binding map ---
    {
        std::string err;
        if (!validateLayoutsAgainstBindingMap(pipeline->m_bindingMap,
                                              desc.bindGroupLayouts,
                                              desc.bindGroupLayoutCount,
                                              &err))
        {
            if (outError)
                *outError = err;
            else
                setLastError("makePipeline: %s", err.c_str());
            return nullptr;
        }
    }

    pipeline->m_glProgram = glCreateProgram();
    glAttachShader(pipeline->m_glProgram, desc.vertexModule->m_glShader);
    glAttachShader(pipeline->m_glProgram, desc.fragmentModule->m_glShader);

    // Bind vertex attribute locations before linking (matches shaderSlot).
    for (uint32_t bufIdx = 0; bufIdx < desc.vertexBufferCount; ++bufIdx)
    {
        const auto& layout = desc.vertexBuffers[bufIdx];
        for (uint32_t attrIdx = 0; attrIdx < layout.attributeCount; ++attrIdx)
        {
            const auto& attr = layout.attributes[attrIdx];
            char name[32];
            snprintf(name, sizeof(name), "a_attr%u", attr.shaderSlot);
            glBindAttribLocation(pipeline->m_glProgram, attr.shaderSlot, name);
        }
    }

    glLinkProgram(pipeline->m_glProgram);

    GLint status = 0;
    glGetProgramiv(pipeline->m_glProgram, GL_LINK_STATUS, &status);
    if (!status)
    {
        std::string linkLog = "GL program link failed";
        GLint logLen = 0;
        glGetProgramiv(pipeline->m_glProgram, GL_INFO_LOG_LENGTH, &logLen);
        if (logLen > 0)
        {
            char log[1024];
            glGetProgramInfoLog(pipeline->m_glProgram,
                                std::min(logLen, (GLint)sizeof(log)),
                                nullptr,
                                log);
            linkLog = log;
        }
        setLastError("Ore GL program link error: %s", linkLog.c_str());
        if (outError)
            *outError = std::move(linkLog);
        return nullptr;
    }

    oreGLFixupProgramBindings(pipeline->m_glProgram,
                              desc.vertexModule,
                              desc.fragmentModule);

    // (Removed: a `m_glState` cache that was populated here but never
    // consulted by `setPipeline`. Re-add when an actual delta-tracking
    // path lands.)

    return pipeline;
}

// ============================================================================
// makeBindGroup
// ============================================================================

rcp<BindGroup> Context::makeBindGroup(const BindGroupDesc& desc)
{
    if (desc.layout == nullptr)
    {
        setLastError("makeBindGroup: BindGroupDesc::layout is null");
        return nullptr;
    }
    BindGroupLayout* layout = desc.layout;
    const uint32_t groupIndex = layout->groupIndex();
    if (groupIndex >= kMaxBindGroups)
    {
        setLastError("makeBindGroup: layout->groupIndex %u out of range",
                     groupIndex);
        return nullptr;
    }
    auto bg = rcp<BindGroup>(new BindGroup());
    bg->m_context = this;
    bg->m_layoutRef = ref_rcp(layout);

    // Native GL slot resolution: layout entries are pre-resolved by the
    // GM/Lua helper (makeLayoutFromShader) using the shader's binding map.
    // The v1 GLSL allocator's global-counter-per-kind ensures slots are
    // unique across groups. Sampler entries carry the paired-texture's GL
    // unit (combined-sampler fold-in), matching the GL link-side fixup.
    auto nativeSlot =
        [&](uint32_t binding, BindingKind expected, uint32_t* outSlot) -> bool {
        const BindGroupLayoutEntry* le = layout->findEntry(binding);
        if (le == nullptr)
        {
            setLastError("makeBindGroup: (group=%u, binding=%u) not declared "
                         "in BindGroupLayout",
                         groupIndex,
                         binding);
            return false;
        }
        const bool kindOK = le->kind == expected ||
                            ((le->kind == BindingKind::sampler ||
                              le->kind == BindingKind::comparisonSampler) &&
                             (expected == BindingKind::sampler ||
                              expected == BindingKind::comparisonSampler));
        if (!kindOK)
        {
            setLastError("makeBindGroup: (group=%u, binding=%u) layout kind "
                         "mismatch",
                         groupIndex,
                         binding);
            return false;
        }
        uint32_t s =
            (le->nativeSlotVS != BindGroupLayoutEntry::kNativeSlotAbsent)
                ? le->nativeSlotVS
                : le->nativeSlotFS;
        if (s == BindGroupLayoutEntry::kNativeSlotAbsent)
        {
            setLastError("makeBindGroup: (group=%u, binding=%u) layout has "
                         "no resolved native slot — call "
                         "makeLayoutFromShader",
                         groupIndex,
                         binding);
            return false;
        }
        *outSlot = s;
        return true;
    };

    // UBO bindings.
    uint32_t nBufs = std::min(desc.uboCount, 8u);
    bg->m_glUBOs.reserve(nBufs);
    for (uint32_t i = 0; i < nBufs; ++i)
    {
        const auto& entry = desc.ubos[i];
        BindGroup::GLUBOBinding binding{};
        binding.buffer = entry.buffer->m_glBuffer;
        binding.offset = entry.offset;
        binding.size = entry.size != 0
                           ? entry.size
                           : static_cast<uint32_t>(entry.buffer->size());
        binding.binding = entry.slot;
        if (!nativeSlot(entry.slot, BindingKind::uniformBuffer, &binding.slot))
            continue;
        binding.hasDynamicOffset = layout->hasDynamicOffset(entry.slot);
        if (binding.hasDynamicOffset)
            bg->m_dynamicOffsetCount++;
        bg->m_glUBOs.push_back(binding);
        bg->m_retainedBuffers.push_back(ref_rcp(entry.buffer));
    }
    std::sort(
        bg->m_glUBOs.begin(),
        bg->m_glUBOs.end(),
        [](const BindGroup::GLUBOBinding& a, const BindGroup::GLUBOBinding& b) {
            return a.binding < b.binding;
        });

    uint32_t nTexs = std::min(desc.textureCount, 8u);
    bg->m_glTextures.reserve(nTexs);
    for (uint32_t i = 0; i < nTexs; ++i)
    {
        const auto& entry = desc.textures[i];
        BindGroup::GLTexBinding binding{};
        binding.texture = entry.view->m_glTextureView != 0
                              ? entry.view->m_glTextureView
                              : entry.view->texture()->m_glTexture;
        binding.target = entry.view->texture()->m_glTarget;
        if (!nativeSlot(entry.slot, BindingKind::sampledTexture, &binding.slot))
            continue;
        bg->m_glTextures.push_back(binding);
        bg->m_retainedViews.push_back(ref_rcp(entry.view));
    }

    uint32_t nSamps = std::min(desc.samplerCount, 8u);
    bg->m_glSamplers.reserve(nSamps);
    for (uint32_t i = 0; i < nSamps; ++i)
    {
        const auto& entry = desc.samplers[i];
        BindGroup::GLSamplerBinding binding{};
        binding.sampler = entry.sampler->m_glSampler;
        if (!nativeSlot(entry.slot, BindingKind::sampler, &binding.slot))
            continue;
        bg->m_glSamplers.push_back(binding);
        bg->m_retainedSamplers.push_back(ref_rcp(entry.sampler));
    }

    return bg;
}

// ============================================================================
// makeBindGroupLayout
// ============================================================================

rcp<BindGroupLayout> Context::makeBindGroupLayout(
    const BindGroupLayoutDesc& desc)
{
    if (desc.groupIndex >= kMaxBindGroups)
    {
        setLastError("makeBindGroupLayout: groupIndex %u out of range [0, %u)",
                     desc.groupIndex,
                     kMaxBindGroups);
        return nullptr;
    }
    auto layout = rcp<BindGroupLayout>(new BindGroupLayout());
    layout->m_context = this;
    layout->m_groupIndex = desc.groupIndex;
    layout->m_entries.reserve(desc.entryCount);
    for (uint32_t i = 0; i < desc.entryCount; ++i)
        layout->m_entries.push_back(desc.entries[i]);
    // GL has no native layout object — entries-only suffices.
    return layout;
}

// ============================================================================
// beginRenderPass
// ============================================================================

RenderPass Context::beginRenderPass(const RenderPassDesc& desc,
                                    std::string* outError)
{
    finishActiveRenderPass();

    RenderPass pass;
    pass.m_context = this;
    pass.populateAttachmentMetadata(desc);

    // Save the host renderer's VAO and FBO so we can restore them in finish().
    GLint prevVAO = 0, prevFBO = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVAO);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    pass.m_prevVAO = static_cast<unsigned int>(prevVAO);
    pass.m_prevFBO = static_cast<unsigned int>(prevFBO);

    // Create an FBO for this render pass.
    glGenFramebuffers(1, &pass.m_glFBO);
    pass.m_ownsFBO = true;
    glBindFramebuffer(GL_FRAMEBUFFER, pass.m_glFBO);

    // Attach color targets.
    GLenum drawBuffers[4] = {};
    for (uint32_t i = 0; i < desc.colorCount; ++i)
    {
        const auto& ca = desc.colorAttachments[i];
        if (ca.view)
        {
            Texture* tex = ca.view->texture();
            GLenum attachment = GL_COLOR_ATTACHMENT0 + i;

            if (tex->m_glRenderbuffer != 0)
            {
                glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                          attachment,
                                          GL_RENDERBUFFER,
                                          tex->m_glRenderbuffer);
            }
            else if (tex->type() == TextureType::cube)
            {
                glFramebufferTexture2D(GL_FRAMEBUFFER,
                                       attachment,
                                       GL_TEXTURE_CUBE_MAP_POSITIVE_X +
                                           ca.view->baseLayer(),
                                       tex->m_glTexture,
                                       ca.view->baseMipLevel());
            }
            else if (tex->type() == TextureType::array2D ||
                     tex->type() == TextureType::texture3D)
            {
                glFramebufferTextureLayer(GL_FRAMEBUFFER,
                                          attachment,
                                          tex->m_glTexture,
                                          ca.view->baseMipLevel(),
                                          ca.view->baseLayer());
            }
            else
            {
                glFramebufferTexture2D(GL_FRAMEBUFFER,
                                       attachment,
                                       GL_TEXTURE_2D,
                                       tex->m_glTexture,
                                       ca.view->baseMipLevel());
            }

            // Record resolve target for MSAA blit in finish().
            if (ca.resolveTarget)
            {
                auto& r = pass.m_glResolves[pass.m_glResolveCount++];
                r.colorIndex = i;
                r.resolveTex = ca.resolveTarget->texture()->m_glTexture;
                r.resolveTarget = ca.resolveTarget->texture()->m_glTarget;
                r.width = tex->width();
                r.height = tex->height();
            }

            drawBuffers[i] = attachment;
        }
    }

    if (desc.colorCount > 0)
    {
        glDrawBuffers(desc.colorCount, drawBuffers);
    }

    // Attach depth/stencil.
    if (desc.depthStencil.view)
    {
        Texture* depthTex = desc.depthStencil.view->texture();
        GLenum depthAttachment;

        TextureFormat depthFmt = depthTex->format();
        bool hasStencil = (depthFmt == TextureFormat::depth24plusStencil8 ||
                           depthFmt == TextureFormat::depth32floatStencil8);
        depthAttachment =
            hasStencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;

        if (depthTex->m_glRenderbuffer != 0)
        {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                      depthAttachment,
                                      GL_RENDERBUFFER,
                                      depthTex->m_glRenderbuffer);
        }
        else
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                                   depthAttachment,
                                   GL_TEXTURE_2D,
                                   depthTex->m_glTexture,
                                   desc.depthStencil.view->baseMipLevel());
        }
    }

    // Verify completeness (debug only).
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
               GL_FRAMEBUFFER_COMPLETE &&
           "Ore GL FBO incomplete");

    // Handle clear ops.
    for (uint32_t i = 0; i < desc.colorCount; ++i)
    {
        const auto& ca = desc.colorAttachments[i];
        if (ca.loadOp == LoadOp::clear)
        {
            GLfloat clearColor[4] = {ca.clearColor.r,
                                     ca.clearColor.g,
                                     ca.clearColor.b,
                                     ca.clearColor.a};
            glClearBufferfv(GL_COLOR, i, clearColor);
        }
    }

    if (desc.depthStencil.view)
    {
        TextureFormat depthFmt = desc.depthStencil.view->texture()->format();
        bool hasStencil = (depthFmt == TextureFormat::depth24plusStencil8 ||
                           depthFmt == TextureFormat::depth32floatStencil8);

        if (desc.depthStencil.depthLoadOp == LoadOp::clear && hasStencil &&
            desc.depthStencil.stencilLoadOp == LoadOp::clear)
        {
            glDepthMask(GL_TRUE);
            glStencilMask(0xFF);
            GLfloat depth = desc.depthStencil.depthClearValue;
            GLint stencil =
                static_cast<GLint>(desc.depthStencil.stencilClearValue);
            glClearBufferfi(GL_DEPTH_STENCIL, 0, depth, stencil);
        }
        else
        {
            if (desc.depthStencil.depthLoadOp == LoadOp::clear)
            {
                glDepthMask(GL_TRUE);
                GLfloat depth = desc.depthStencil.depthClearValue;
                glClearBufferfv(GL_DEPTH, 0, &depth);
            }
            if (hasStencil && desc.depthStencil.stencilLoadOp == LoadOp::clear)
            {
                glStencilMask(0xFF);
                GLint stencil =
                    static_cast<GLint>(desc.depthStencil.stencilClearValue);
                glClearBufferiv(GL_STENCIL, 0, &stencil);
            }
        }
    }

    // Create a dedicated VAO for this render pass so Ore's vertex attrib state
    // doesn't contaminate the host renderer's VAO (e.g., Rive's draw VAO).
    glGenVertexArrays(1, &pass.m_glVAO);
    pass.m_ownsVAO = true;
    glBindVertexArray(pass.m_glVAO);

    // Set a default viewport from the first color or depth attachment so
    // scripts that omit setViewport() still render correctly. Metal infers
    // this automatically; GL does not.
    uint32_t defaultW = 0, defaultH = 0;
    if (desc.colorCount > 0 && desc.colorAttachments[0].view)
    {
        defaultW = desc.colorAttachments[0].view->texture()->width();
        defaultH = desc.colorAttachments[0].view->texture()->height();
    }
    else if (desc.depthStencil.view)
    {
        defaultW = desc.depthStencil.view->texture()->width();
        defaultH = desc.depthStencil.view->texture()->height();
    }
    if (defaultW > 0 && defaultH > 0)
    {
        glViewport(0, 0, defaultW, defaultH);
        pass.m_viewportWidth = defaultW;
        pass.m_viewportHeight = defaultH;
    }

    return pass;
}

// ============================================================================
// wrapCanvasTexture
// ============================================================================

rcp<TextureView> Context::wrapCanvasTexture(gpu::RenderCanvas* canvas)
{
    assert(canvas != nullptr);

    auto* glTarget =
        static_cast<gpu::TextureRenderTargetGL*>(canvas->renderTarget());
    GLuint texID = glTarget->externalTextureID();
    assert(texID != 0);

    TextureDesc texDesc{};
    texDesc.width = canvas->width();
    texDesc.height = canvas->height();
    texDesc.format = TextureFormat::rgba8unorm;
    texDesc.type = TextureType::texture2D;
    texDesc.renderTarget = true;
    texDesc.numMipmaps = 1;
    texDesc.sampleCount = 1;

    auto texture = rcp<Texture>(new Texture(texDesc));
    texture->m_glTexture = texID; // Borrow — RenderCanvas owns it.
    texture->m_glTarget = GL_TEXTURE_2D;
    texture->m_glOwnsTexture = false;

    TextureViewDesc viewDesc{};
    viewDesc.texture = texture.get();
    viewDesc.dimension = TextureViewDimension::texture2D;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipCount = 1;
    viewDesc.baseLayer = 0;
    viewDesc.layerCount = 1;

    return rcp<TextureView>(new TextureView(std::move(texture), viewDesc));
}

rcp<TextureView> Context::wrapRiveTexture(gpu::Texture* gpuTex,
                                          uint32_t w,
                                          uint32_t h)
{
    if (!gpuTex)
        return nullptr;

    GLuint texID = static_cast<GLuint>(
        reinterpret_cast<uintptr_t>(gpuTex->nativeHandle()));
    if (texID == 0)
        return nullptr;

    TextureDesc texDesc{};
    texDesc.width = w;
    texDesc.height = h;
    texDesc.format = TextureFormat::rgba8unorm; // Images decode to RGBA8.
    texDesc.type = TextureType::texture2D;
    texDesc.renderTarget = false;
    texDesc.numMipmaps = 1;
    texDesc.sampleCount = 1;

    auto texture = rcp<Texture>(new Texture(texDesc));
    texture->m_glTexture = texID; // Borrow — caller owns via RenderImage.
    texture->m_glTarget = GL_TEXTURE_2D;
    texture->m_glOwnsTexture = false;

    TextureViewDesc viewDesc{};
    viewDesc.texture = texture.get();
    viewDesc.dimension = TextureViewDimension::texture2D;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipCount = 1;
    viewDesc.baseLayer = 0;
    viewDesc.layerCount = 1;

    return rcp<TextureView>(new TextureView(std::move(texture), viewDesc));
}

#endif // ORE_BACKEND_GL standalone

} // namespace rive::ore
