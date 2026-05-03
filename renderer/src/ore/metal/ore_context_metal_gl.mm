/*
 * Copyright 2025 Rive
 */

// Combined Metal + GL Context implementation for macOS.
// Compiled when both ORE_BACKEND_METAL and ORE_BACKEND_GL are active.
// Provides all Context method definitions with runtime dispatch via
// Context::BackendType so Metal and GL contexts can coexist in the same binary.

#if defined(ORE_BACKEND_METAL) && defined(ORE_BACKEND_GL)

// Pull in Metal static helpers.
// The !defined(ORE_BACKEND_GL) guard in ore_context_metal.mm excludes
// Context method bodies, so we get only the helper functions.
#include "ore_context_metal.mm"

// Pull in GL static helpers (oreFormatToGLInternal, oreFilterToGL, etc.).
// The !defined(ORE_BACKEND_METAL) guard in ore_context_gl.cpp excludes
// Context method bodies, so we get only the helper functions.
#include "../gl/ore_context_gl.cpp"

// GL render-target header for wrapCanvasTexture (GL path).
#include "rive/renderer/gl/render_target_gl.hpp"

#include <string>

namespace rive::ore
{

// ============================================================================
// Context lifecycle (Metal + GL dispatch)
// ============================================================================

Context::Context() {}

Context::~Context()
{
    if (m_backendType == BackendType::GL)
        return; // GL Context holds no owned GPU resources.
    m_mtlCommandBuffer = nil;
    m_mtlQueue = nil;
    m_mtlDevice = nil;
}

Context::Context(Context&& other) noexcept :
    m_features(other.m_features),
    m_backendType(other.m_backendType),
    m_savedState(other.m_savedState),
    m_mtlDevice(other.m_mtlDevice),
    m_mtlQueue(other.m_mtlQueue),
    m_mtlCommandBuffer(other.m_mtlCommandBuffer)
{
    other.m_mtlDevice = nil;
    other.m_mtlQueue = nil;
    other.m_mtlCommandBuffer = nil;
}

Context& Context::operator=(Context&& other) noexcept
{
    if (this != &other)
    {
        m_mtlCommandBuffer = nil;
        m_mtlQueue = nil;
        m_mtlDevice = nil;

        m_features = other.m_features;
        m_backendType = other.m_backendType;
        m_savedState = other.m_savedState;
        m_mtlDevice = other.m_mtlDevice;
        m_mtlQueue = other.m_mtlQueue;
        m_mtlCommandBuffer = other.m_mtlCommandBuffer;
        other.m_mtlDevice = nil;
        other.m_mtlQueue = nil;
        other.m_mtlCommandBuffer = nil;
    }
    return *this;
}

// ============================================================================
// createMetal / createGL
// ============================================================================

Context Context::createMetal(id<MTLDevice> device, id<MTLCommandQueue> queue)
{
    Context ctx;
    ctx.m_mtlDevice = device;
    ctx.m_mtlQueue = queue;
    ctx.mtlPopulateFeatures(device);
    return ctx;
}

Context Context::createGL()
{
    Context ctx;
    ctx.m_backendType = BackendType::GL;

    Features& f = ctx.m_features;
    f.colorBufferFloat = false;
    f.perTargetBlend = false;
    f.perTargetWriteMask = false;
    f.textureViewSampling = false;
    f.drawBaseInstance = false;
    f.depthBiasClamp = false;
    f.anisotropicFiltering = false;
    f.texture3D = true;
    f.textureArrays = true;
    f.computeShaders = false;
    f.storageBuffers = false;
    f.bc = false;
    f.etc2 = true;
    f.astc = false;

    GLint maxTexSize = 0, maxCubeSize = 0, max3DSize = 0, maxUBOSize = 0;
    GLint maxDrawBuffers = 0, maxVertexAttribs = 0, maxTexUnits = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
    glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &maxCubeSize);
    glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &max3DSize);
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUBOSize);
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxTexUnits);

    f.maxTextureSize2D = maxTexSize;
    f.maxTextureSizeCube = maxCubeSize;
    f.maxTextureSize3D = max3DSize;
    f.maxUniformBufferSize = maxUBOSize;
    f.maxColorAttachments = std::min(static_cast<uint32_t>(maxDrawBuffers), 4u);
    f.maxVertexAttributes = maxVertexAttribs;
    f.maxSamplers = maxTexUnits;

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

// ============================================================================
// beginFrame / endFrame
// ============================================================================

void Context::beginFrame()
{
    if (m_backendType == BackendType::GL)
    {
        // GL is synchronous from the host's POV — `glDelete*` calls
        // wait for any in-flight reads themselves — so dropping
        // deferred BindGroups at frame start is safe.
        m_deferredBindGroups.clear();
        glGetIntegerv(GL_CURRENT_PROGRAM, &m_savedState.program);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &m_savedState.arrayBuffer);
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_savedState.framebuffer);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &m_savedState.vertexArray);
        // NOTE: GL_ELEMENT_ARRAY_BUFFER is VAO state — not saved/restored
        // separately to avoid corrupting the host renderer's VAO bindings.
        return;
    }
    // Metal: don't drop `m_deferredBindGroups` here. They're released
    // by the prior frame's `addCompletedHandler:` once the GPU is
    // actually done — see `endFrame` below.
    m_mtlCommandBuffer = [m_mtlQueue commandBuffer];
}

void Context::waitForGPU() {} // Metal+GL variant; synchronous.

void Context::endFrame()
{
    if (m_backendType == BackendType::GL)
    {
        // Guard each restore with the matching `glIs*` probe so Mac
        // ANGLE's strict validation doesn't fire if the host renderer
        // deleted the saved object between beginFrame and now. Name ==
        // 0 always refers to the default object (no validation error).
        if (m_savedState.program == 0 ||
            glIsProgram(m_savedState.program) == GL_TRUE)
        {
            glUseProgram(m_savedState.program);
        }
        // Restore VAO before ARRAY_BUFFER — rebinding the saved VAO
        // implicitly restores its element-array-buffer association, and
        // must happen before any subsequent array-buffer bind to avoid
        // clobbering the host renderer's VAO state.
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
        // NOTE: GL_ELEMENT_ARRAY_BUFFER is VAO state — not restored here.
        if (m_savedState.framebuffer == 0 ||
            glIsFramebuffer(m_savedState.framebuffer) == GL_TRUE)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, m_savedState.framebuffer);
        }
        return;
    }
    if (m_mtlCommandBuffer)
    {
        // Drain `m_deferredBindGroups` from the command buffer's
        // completion handler so the underlying `id<MTL...>` resources
        // outlive the GPU's reads. See sibling fix in
        // `ore_context_metal.mm`.
        if (!m_deferredBindGroups.empty())
        {
            __block std::vector<rcp<BindGroup>> deferred =
                std::move(m_deferredBindGroups);
            m_deferredBindGroups.clear();
            [m_mtlCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer>) {
              deferred.clear();
            }];
        }
        [m_mtlCommandBuffer commit];
        m_mtlCommandBuffer = nil;
    }
}

// ============================================================================
// makeBuffer
// ============================================================================

rcp<Buffer> Context::makeBuffer(const BufferDesc& desc)
{
    if (m_backendType == BackendType::GL)
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
        glBindBuffer(buffer->m_glTarget, buffer->m_glBuffer);
        GLenum glUsage =
            (desc.data != nullptr) ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW;
        glBufferData(buffer->m_glTarget, desc.size, desc.data, glUsage);
        glBindBuffer(buffer->m_glTarget, 0);
        return buffer;
    }

    return mtlMakeBuffer(desc);
}

// ============================================================================
// makeTexture
// ============================================================================

rcp<Texture> Context::makeTexture(const TextureDesc& desc)
{
    if (m_backendType == BackendType::GL)
    {
        auto texture = rcp<Texture>(new Texture(desc));
        glGenTextures(1, &texture->m_glTexture);
        texture->m_glTarget = oreTextureTypeToGLTarget(desc.type);
        GLenum internalFormat = oreFormatToGLInternal(desc.format);
        glBindTexture(texture->m_glTarget, texture->m_glTexture);
        switch (desc.type)
        {
            case TextureType::texture2D:
                glTexStorage2D(GL_TEXTURE_2D,
                               desc.numMipmaps,
                               internalFormat,
                               desc.width,
                               desc.height);
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
        glTexParameteri(texture->m_glTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(texture->m_glTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(
            texture->m_glTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(
            texture->m_glTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(texture->m_glTarget, 0);
        return texture;
    }

    return mtlMakeTexture(desc);
}

// ============================================================================
// makeTextureView
// ============================================================================

rcp<TextureView> Context::makeTextureView(const TextureViewDesc& desc)
{
    if (m_backendType == BackendType::GL)
    {
        Texture* tex = desc.texture;
        assert(tex != nullptr);
        auto view = rcp<TextureView>(new TextureView(ref_rcp(tex), desc));
        view->m_glTextureView = 0;
        return view;
    }

    return mtlMakeTextureView(desc);
}

// ============================================================================
// makeSampler
// ============================================================================

rcp<Sampler> Context::makeSampler(const SamplerDesc& desc)
{
    if (m_backendType == BackendType::GL)
    {
        auto sampler = rcp<Sampler>(new Sampler());
        glGenSamplers(1, &sampler->m_glSampler);
        GLuint s = sampler->m_glSampler;
        if (desc.maxLod > 0.0f)
        {
            glSamplerParameteri(
                s,
                GL_TEXTURE_MIN_FILTER,
                oreMipmapFilterToGL(desc.minFilter, desc.mipmapFilter));
        }
        else
        {
            glSamplerParameteri(
                s, GL_TEXTURE_MIN_FILTER, oreFilterToGL(desc.minFilter));
        }
        glSamplerParameteri(
            s, GL_TEXTURE_MAG_FILTER, oreFilterToGL(desc.magFilter));
        glSamplerParameteri(s, GL_TEXTURE_WRAP_S, oreWrapToGL(desc.wrapU));
        glSamplerParameteri(s, GL_TEXTURE_WRAP_T, oreWrapToGL(desc.wrapV));
        glSamplerParameteri(s, GL_TEXTURE_WRAP_R, oreWrapToGL(desc.wrapW));
        glSamplerParameterf(s, GL_TEXTURE_MIN_LOD, desc.minLod);
        glSamplerParameterf(s, GL_TEXTURE_MAX_LOD, desc.maxLod);
        if (desc.compare != CompareFunction::none)
        {
            glSamplerParameteri(
                s, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glSamplerParameteri(s,
                                GL_TEXTURE_COMPARE_FUNC,
                                oreCompareFunctionToGL(desc.compare));
        }
        return sampler;
    }

    return mtlMakeSampler(desc);
}

// ============================================================================
// makeShaderModule
// ============================================================================

rcp<ShaderModule> Context::makeShaderModule(const ShaderModuleDesc& desc)
{
    if (m_backendType == BackendType::GL)
    {
        auto module = rcp<ShaderModule>(new ShaderModule());
        const char* source = static_cast<const char*>(desc.code);
        bool isVertex = (strstr(source, "gl_Position") != nullptr);
        module->m_glShaderType =
            isVertex ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
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

    return mtlMakeShaderModule(desc);
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
    // Both Metal and GL backends have no native layout object — entries
    // alone suffice. Backends use the per-Entry nativeSlotVS/FS at
    // makeBindGroup time.
    return layout;
}

// ============================================================================
// makePipeline
// ============================================================================

rcp<Pipeline> Context::makePipeline(const PipelineDesc& desc,
                                    std::string* outError)
{
    if (m_backendType == BackendType::GL)
    {
        auto pipeline = rcp<Pipeline>(new Pipeline(desc));
        // Validate user-supplied layouts against shader binding map.
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
        pipeline->m_glProgram = glCreateProgram();
        glAttachShader(pipeline->m_glProgram, desc.vertexModule->m_glShader);
        glAttachShader(pipeline->m_glProgram, desc.fragmentModule->m_glShader);
        for (uint32_t bufIdx = 0; bufIdx < desc.vertexBufferCount; ++bufIdx)
        {
            const auto& layout = desc.vertexBuffers[bufIdx];
            for (uint32_t attrIdx = 0; attrIdx < layout.attributeCount;
                 ++attrIdx)
            {
                const auto& attr = layout.attributes[attrIdx];
                char name[32];
                snprintf(name, sizeof(name), "a_attr%u", attr.shaderSlot);
                glBindAttribLocation(
                    pipeline->m_glProgram, attr.shaderSlot, name);
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
        // Apply the RSTB-baked UBO/sampler fixup so the linked program's
        // samplers land on the allocator's chosen texture units (not the
        // default unit 0, which ANGLE rejects at draw-time as "two
        // different sampler types on the same unit").
        oreGLFixupProgramBindings(
            pipeline->m_glProgram, desc.vertexModule, desc.fragmentModule);
        // (Removed: a `m_glState` cache that was populated here but never
        // consulted by `setPipeline`. Mirror of the dead-cache cleanup
        // in ore_context_gl.cpp.)
        return pipeline;
    }

    return mtlMakePipeline(desc, outError);
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
    if (m_backendType == BackendType::GL)
    {
        auto bg = rcp<BindGroup>(new BindGroup());
        bg->m_context = this;
        bg->m_layoutRef = ref_rcp(layout);

        auto nativeSlot = [&](uint32_t binding,
                              BindingKind expected,
                              uint32_t* outSlot) -> bool {
            const BindGroupLayoutEntry* le = layout->findEntry(binding);
            if (le == nullptr)
            {
                setLastError("makeBindGroup: (group=%u, binding=%u) not "
                             "declared in BindGroupLayout",
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
                setLastError("makeBindGroup: (group=%u, binding=%u) layout "
                             "kind mismatch",
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
                setLastError("makeBindGroup: (group=%u, binding=%u) layout "
                             "has no resolved native slot — call "
                             "makeLayoutFromShader",
                             groupIndex,
                             binding);
                return false;
            }
            *outSlot = s;
            return true;
        };

        uint32_t nBufs = std::min(desc.uboCount, 8u);
        for (uint32_t i = 0; i < nBufs; i++)
        {
            const auto& u = desc.ubos[i];
            bg->m_retainedBuffers.push_back(ref_rcp(u.buffer));
            BindGroup::GLUBOBinding binding;
            binding.buffer = u.buffer->m_glBuffer;
            binding.offset = u.offset;
            binding.size = u.size ? u.size : u.buffer->size();
            binding.binding = u.slot;
            if (!nativeSlot(u.slot, BindingKind::uniformBuffer, &binding.slot))
                continue;
            binding.hasDynamicOffset = layout->hasDynamicOffset(u.slot);
            if (binding.hasDynamicOffset)
                bg->m_dynamicOffsetCount++;
            bg->m_glUBOs.push_back(binding);
        }
        std::sort(bg->m_glUBOs.begin(),
                  bg->m_glUBOs.end(),
                  [](const BindGroup::GLUBOBinding& a,
                     const BindGroup::GLUBOBinding& b) {
                      return a.binding < b.binding;
                  });

        uint32_t nTexs = std::min(desc.textureCount, 8u);
        for (uint32_t i = 0; i < nTexs; i++)
        {
            const auto& t = desc.textures[i];
            bg->m_retainedViews.push_back(ref_rcp(t.view));
            BindGroup::GLTexBinding binding;
            binding.texture = t.view->m_glTextureView != 0
                                  ? t.view->m_glTextureView
                                  : t.view->texture()->m_glTexture;
            binding.target = t.view->texture()->m_glTarget;
            if (!nativeSlot(t.slot, BindingKind::sampledTexture, &binding.slot))
                continue;
            bg->m_glTextures.push_back(binding);
        }

        uint32_t nSamps = std::min(desc.samplerCount, 8u);
        for (uint32_t i = 0; i < nSamps; i++)
        {
            const auto& s = desc.samplers[i];
            bg->m_retainedSamplers.push_back(ref_rcp(s.sampler));
            BindGroup::GLSamplerBinding binding;
            binding.sampler = s.sampler->m_glSampler;
            if (!nativeSlot(s.slot, BindingKind::sampler, &binding.slot))
                continue;
            bg->m_glSamplers.push_back(binding);
        }
        return bg;
    }

    return mtlMakeBindGroup(desc);
}

// ============================================================================
// beginRenderPass
// ============================================================================

RenderPass Context::beginRenderPass(const RenderPassDesc& desc,
                                    std::string* outError)
{
    finishActiveRenderPass();

    if (m_backendType == BackendType::GL)
    {
        RenderPass pass;
        pass.m_context = this;
        pass.populateAttachmentMetadata(desc);

        glGenFramebuffers(1, &pass.m_glFBO);
        pass.m_ownsFBO = true;
        glBindFramebuffer(GL_FRAMEBUFFER, pass.m_glFBO);

        GLenum drawBuffers[4] = {};
        for (uint32_t i = 0; i < desc.colorCount; ++i)
        {
            const auto& ca = desc.colorAttachments[i];
            if (ca.view)
            {
                Texture* tex = ca.view->texture();
                GLuint glTex = tex->m_glTexture;
                GLenum attachment = GL_COLOR_ATTACHMENT0 + i;

                if (tex->type() == TextureType::cube)
                {
                    glFramebufferTexture2D(GL_FRAMEBUFFER,
                                           attachment,
                                           GL_TEXTURE_CUBE_MAP_POSITIVE_X +
                                               ca.view->baseLayer(),
                                           glTex,
                                           ca.view->baseMipLevel());
                }
                else if (tex->type() == TextureType::array2D ||
                         tex->type() == TextureType::texture3D)
                {
                    glFramebufferTextureLayer(GL_FRAMEBUFFER,
                                              attachment,
                                              glTex,
                                              ca.view->baseMipLevel(),
                                              ca.view->baseLayer());
                }
                else
                {
                    glFramebufferTexture2D(GL_FRAMEBUFFER,
                                           attachment,
                                           GL_TEXTURE_2D,
                                           glTex,
                                           ca.view->baseMipLevel());
                }
                drawBuffers[i] = attachment;
            }
        }
        if (desc.colorCount > 0)
        {
            glDrawBuffers(desc.colorCount, drawBuffers);
        }

        if (desc.depthStencil.view)
        {
            Texture* depthTex = desc.depthStencil.view->texture();
            TextureFormat depthFmt = depthTex->format();
            bool hasStencil = (depthFmt == TextureFormat::depth24plusStencil8 ||
                               depthFmt == TextureFormat::depth32floatStencil8);
            GLenum depthAttachment =
                hasStencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                                   depthAttachment,
                                   GL_TEXTURE_2D,
                                   depthTex->m_glTexture,
                                   desc.depthStencil.view->baseMipLevel());
        }

        assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
                   GL_FRAMEBUFFER_COMPLETE &&
               "Ore GL FBO incomplete");

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
            TextureFormat depthFmt =
                desc.depthStencil.view->texture()->format();
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
                if (hasStencil &&
                    desc.depthStencil.stencilLoadOp == LoadOp::clear)
                {
                    glStencilMask(0xFF);
                    GLint stencil =
                        static_cast<GLint>(desc.depthStencil.stencilClearValue);
                    glClearBufferiv(GL_STENCIL, 0, &stencil);
                }
            }
        }

#ifdef RIVE_DESKTOP_GL
        glGenVertexArrays(1, &pass.m_glVAO);
        pass.m_ownsVAO = true;
        glBindVertexArray(pass.m_glVAO);
#endif

        return pass;
    }

    return mtlBeginRenderPass(desc, outError);
}

// ============================================================================
// wrapCanvasTexture
// ============================================================================

rcp<TextureView> Context::wrapCanvasTexture(gpu::RenderCanvas* canvas)
{
    assert(canvas != nullptr);

    if (m_backendType == BackendType::GL)
    {
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
        texture->m_glTexture = texID;
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

    return mtlWrapCanvasTexture(canvas);
}

rcp<TextureView> Context::wrapRiveTexture(gpu::Texture* gpuTex,
                                          uint32_t w,
                                          uint32_t h)
{
    if (!gpuTex)
        return nullptr;

    if (m_backendType == BackendType::GL)
    {
        GLuint texID = static_cast<GLuint>(
            reinterpret_cast<uintptr_t>(gpuTex->nativeHandle()));
        if (texID == 0)
            return nullptr;

        TextureDesc texDesc{};
        texDesc.width = w;
        texDesc.height = h;
        texDesc.format = TextureFormat::rgba8unorm;
        texDesc.type = TextureType::texture2D;
        texDesc.renderTarget = false;
        texDesc.numMipmaps = 1;
        texDesc.sampleCount = 1;

        auto texture = rcp<Texture>(new Texture(texDesc));
        texture->m_glTexture = texID;
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

    // Metal path
    id<MTLTexture> mtlTex = (__bridge id<MTLTexture>)gpuTex->nativeHandle();
    if (!mtlTex)
        return nullptr;

    TextureDesc texDesc{};
    texDesc.width = w;
    texDesc.height = h;
    texDesc.format = mtlFormatToOre(mtlTex.pixelFormat);
    texDesc.type = TextureType::texture2D;
    texDesc.renderTarget = false;
    texDesc.numMipmaps = 1;
    texDesc.sampleCount = 1;

    auto texture = rcp<Texture>(new Texture(texDesc));
    texture->m_mtlTexture = mtlTex;

    TextureViewDesc viewDesc{};
    viewDesc.texture = texture.get();
    viewDesc.dimension = TextureViewDimension::texture2D;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipCount = 1;
    viewDesc.baseLayer = 0;
    viewDesc.layerCount = 1;

    auto view = rcp<TextureView>(new TextureView(std::move(texture), viewDesc));
    view->m_mtlTextureView = mtlTex;
    return view;
}

} // namespace rive::ore

#endif // ORE_BACKEND_METAL && ORE_BACKEND_GL
