#ifdef RIVE_WASM_MODULE
#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
// Module-side ore objects over rive_gpu_v1 handles, the same shape
// render_proxy.cpp gives the 2D surface. The binding files in lua_gpu.cpp
// compile in untouched and talk to this context; factories realize against
// imports namespace slice by namespace slice, the rest report through
// lastError like any backend construction failure.
#include "rive/renderer/ore/ore_types.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/render_target.hpp"
#include "rive/wasm/module_render.hpp"
#include "rive/renderer/ore/ore_context.hpp"
#include "rive/wasm/rive_bindings_v1.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace rive
{
namespace
{

class ModuleOreBuffer : public ore::Buffer
{
public:
    ModuleOreBuffer(uint32_t handle, uint32_t size, ore::BufferUsage usage) :
        ore::Buffer(size, usage), m_handle(handle)
    {}
    ~ModuleOreBuffer() override { rive_gpu_buffer_release(m_handle); }
    uint32_t handle() const { return m_handle; }
    void update(const void* data, uint32_t size, uint32_t offset) override
    {
        rive_gpu_buffer_update(m_handle,
                               offset,
                               static_cast<const uint8_t*>(data),
                               size);
    }

private:
    uint32_t m_handle;
};

class ModuleOreTexture : public ore::Texture
{
public:
    ModuleOreTexture(uint32_t handle, const ore::TextureDesc& desc) :
        ore::Texture(desc), m_handle(handle)
    {}
    // Handle 0 is the canvas metadata wrapper, which owns nothing host side.
    ~ModuleOreTexture() override
    {
        if (m_handle != 0)
        {
            rive_gpu_texture_release(m_handle);
        }
    }
    uint32_t handle() const { return m_handle; }
    void upload(const ore::TextureDataDesc& data) override
    {
        rive_gpu_texture_upload_v1 region;
        region.bytesPerRow = data.bytesPerRow;
        region.rowsPerImage = data.rowsPerImage;
        region.mipLevel = data.mipLevel;
        region.layer = data.layer;
        region.x = data.x;
        region.y = data.y;
        region.z = data.z;
        region.width = data.width;
        region.height = data.height;
        region.depth = data.depth;
        uint32_t byteCount =
            data.bytesPerRow * data.rowsPerImage * std::max(1u, data.depth);
        rive_gpu_texture_upload(m_handle,
                                &region,
                                sizeof(region),
                                static_cast<const uint8_t*>(data.data),
                                byteCount);
    }

private:
    uint32_t m_handle;
};

class ModuleOreSampler : public ore::Sampler
{
public:
    ModuleOreSampler(uint32_t handle) : m_handle(handle) {}
    ~ModuleOreSampler() override { rive_gpu_sampler_release(m_handle); }
    uint32_t handle() const { return m_handle; }

private:
    uint32_t m_handle;
};

class ModuleOreTextureView : public ore::TextureView
{
public:
    ModuleOreTextureView(uint32_t handle,
                         rcp<ore::Texture> texture,
                         const ore::TextureViewDesc& desc) :
        ore::TextureView(std::move(texture), desc), m_handle(handle)
    {}
    ~ModuleOreTextureView() override
    {
        rive_gpu_texture_view_release(m_handle);
    }
    uint32_t handle() const { return m_handle; }

private:
    uint32_t m_handle;
};

class ModuleOreShaderModule : public ore::ShaderModule
{
public:
    ModuleOreShaderModule(uint32_t handle, const ore::ShaderModuleDesc& desc) :
        m_handle(handle)
    {
        // Parses the binding-map sidecar into m_bindingMap so module-side
        // layout derivation walks the same data every backend does.
        applyBindingMapFromDesc(desc);
    }
    ~ModuleOreShaderModule() override
    {
        rive_gpu_shader_module_release(m_handle);
    }
    uint32_t handle() const { return m_handle; }

private:
    uint32_t m_handle;
};

class ModuleOreBindGroupLayout : public ore::BindGroupLayout
{
public:
    ModuleOreBindGroupLayout(uint32_t handle,
                             const ore::BindGroupLayoutDesc& desc) :
        m_handle(handle)
    {
        m_groupIndex = desc.groupIndex;
        m_entries.assign(desc.entries, desc.entries + desc.entryCount);
    }
    ~ModuleOreBindGroupLayout() override
    {
        rive_gpu_bind_group_layout_release(m_handle);
    }
    uint32_t handle() const { return m_handle; }

private:
    uint32_t m_handle;
};

class ModuleOreBindGroup : public ore::BindGroup
{
public:
    ModuleOreBindGroup(uint32_t handle, const ore::BindGroupDesc& desc) :
        m_handle(handle)
    {
        m_layoutRef = ref_rcp(desc.layout);
        for (uint32_t i = 0; i < desc.uboCount; i++)
        {
            m_retainedBuffers.push_back(ref_rcp(desc.ubos[i].buffer));
            if (m_layoutRef->hasDynamicOffset(desc.ubos[i].slot))
            {
                m_dynamicOffsetCount++;
            }
        }
        for (uint32_t i = 0; i < desc.textureCount; i++)
        {
            m_retainedViews.push_back(ref_rcp(desc.textures[i].view));
        }
        for (uint32_t i = 0; i < desc.samplerCount; i++)
        {
            m_retainedSamplers.push_back(ref_rcp(desc.samplers[i].sampler));
        }
    }
    ~ModuleOreBindGroup() override { rive_gpu_bind_group_release(m_handle); }
    uint32_t handle() const { return m_handle; }

private:
    uint32_t m_handle;
};

class ModuleOrePipeline : public ore::Pipeline
{
public:
    ModuleOrePipeline(uint32_t handle, const ore::PipelineDesc& desc) :
        ore::Pipeline(desc),
        m_handle(handle),
        m_vertexEntry(desc.vertexEntryPoint),
        m_fragmentEntry(desc.fragmentEntryPoint)
    {
        // The base copies the caller's entry point pointers; repoint them at
        // owned storage so desc() stays valid past construction.
        m_desc.vertexEntryPoint = m_vertexEntry.c_str();
        m_desc.fragmentEntryPoint = m_fragmentEntry.c_str();
    }
    ~ModuleOrePipeline() override { rive_gpu_pipeline_release(m_handle); }
    uint32_t handle() const { return m_handle; }

private:
    uint32_t m_handle;
    std::string m_vertexEntry;
    std::string m_fragmentEntry;
};

// Mirrors RenderPassRecording: the base validators run module side so
// lastError parity holds, then every command forwards over the wire.
class ModuleOreRenderPass : public ore::RenderPass
{
public:
    ModuleOreRenderPass(ore::Context* context,
                        uint32_t handle,
                        const ore::RenderPassDesc& desc) :
        ore::RenderPass(context), m_handle(handle)
    {
        populateAttachmentMetadata(desc);
    }
    ~ModuleOreRenderPass() override { rive_gpu_pass_release(m_handle); }
    uint32_t handle() const { return m_handle; }

    void setPipeline(ore::Pipeline* pipeline) override
    {
        if (!checkPipelineCompat(pipeline))
        {
            return;
        }
        rive_gpu_pass_set_pipeline(
            m_handle,
            static_cast<ModuleOrePipeline*>(pipeline)->handle());
    }
    void setVertexBuffer(uint32_t slot,
                         ore::Buffer* buffer,
                         uint32_t offset) override
    {
        rive_gpu_pass_set_vertex_buffer(
            m_handle,
            slot,
            static_cast<ModuleOreBuffer*>(buffer)->handle(),
            offset);
    }
    void setIndexBuffer(ore::Buffer* buffer,
                        ore::IndexFormat format,
                        uint32_t offset) override
    {
        rive_gpu_pass_set_index_buffer(
            m_handle,
            static_cast<ModuleOreBuffer*>(buffer)->handle(),
            (uint32_t)format,
            offset);
    }
    void setBindGroup(uint32_t groupIndex,
                      ore::BindGroup* bg,
                      const uint32_t* dynamicOffsets,
                      uint32_t dynamicOffsetCount) override
    {
        if (groupIndex < ore::kMaxBindGroups)
        {
            m_boundGroups[groupIndex] = ref_rcp(bg);
        }
        rive_gpu_pass_set_bind_group(
            m_handle,
            groupIndex,
            static_cast<ModuleOreBindGroup*>(bg)->handle(),
            dynamicOffsets,
            dynamicOffsetCount * (uint32_t)sizeof(uint32_t));
    }
    void setViewport(float x,
                     float y,
                     float width,
                     float height,
                     float minDepth,
                     float maxDepth) override
    {
        rive_gpu_pass_set_viewport(m_handle,
                                   x,
                                   y,
                                   width,
                                   height,
                                   minDepth,
                                   maxDepth);
    }
    void setScissorRect(uint32_t x,
                        uint32_t y,
                        uint32_t width,
                        uint32_t height) override
    {
        rive_gpu_pass_set_scissor(m_handle, x, y, width, height);
    }
    void setStencilReference(uint32_t ref) override
    {
        rive_gpu_pass_set_stencil_reference(m_handle, ref);
    }
    void setBlendColor(float r, float g, float b, float a) override
    {
        rive_gpu_pass_set_blend_color(m_handle, r, g, b, a);
    }
    void draw(uint32_t vertexCount,
              uint32_t instanceCount,
              uint32_t firstVertex,
              uint32_t firstInstance) override
    {
        rive_gpu_pass_draw(m_handle,
                           vertexCount,
                           instanceCount,
                           firstVertex,
                           firstInstance);
    }
    void drawIndexed(uint32_t indexCount,
                     uint32_t instanceCount,
                     uint32_t firstIndex,
                     int32_t baseVertex,
                     uint32_t firstInstance) override
    {
        rive_gpu_pass_draw_indexed(m_handle,
                                   indexCount,
                                   instanceCount,
                                   firstIndex,
                                   baseVertex,
                                   firstInstance);
    }
    void finish() override
    {
        if (m_finished)
        {
            return;
        }
        rive_gpu_pass_finish(m_handle);
        m_finished = true;
        for (uint32_t i = 0; i < ore::kMaxBindGroups; ++i)
        {
            m_boundGroups[i] = nullptr;
        }
    }

private:
    uint32_t m_handle;
};

class ModuleOreContext : public ore::Context
{
public:
    ModuleOreContext() : ore::Context(nullptr) {}

    rcp<ore::Buffer> makeBuffer(const ore::BufferDesc& desc) override
    {
        uint32_t handle =
            rive_gpu_buffer_new((uint32_t)desc.usage,
                                desc.size,
                                desc.immutable ? 1 : 0,
                                static_cast<const uint8_t*>(desc.data),
                                desc.data != nullptr ? desc.size : 0);
        if (handle == 0)
        {
            setLastError("host rejected buffer");
            return nullptr;
        }
        return make_rcp<ModuleOreBuffer>(handle, desc.size, desc.usage);
    }

    rcp<ore::Texture> makeTexture(const ore::TextureDesc& desc) override
    {
        rive_gpu_texture_desc_v1 pod;
        pod.width = desc.width;
        pod.height = desc.height;
        pod.depthOrArrayLayers = desc.depthOrArrayLayers;
        pod.format = (uint32_t)desc.format;
        pod.textureType = (uint32_t)desc.type;
        pod.renderTarget = desc.renderTarget ? 1 : 0;
        pod.numMipmaps = desc.numMipmaps;
        pod.sampleCount = desc.sampleCount;
        uint32_t handle = rive_gpu_texture_new(&pod, sizeof(pod));
        if (handle == 0)
        {
            setLastError("host rejected texture");
            return nullptr;
        }
        return make_rcp<ModuleOreTexture>(handle, desc);
    }

    rcp<ore::TextureView> makeTextureView(
        const ore::TextureViewDesc& desc) override
    {
        auto* texture = static_cast<ModuleOreTexture*>(desc.texture);
        if (texture == nullptr)
        {
            setLastError("texture view requires a texture");
            return nullptr;
        }
        rive_gpu_texture_view_desc_v1 pod;
        pod.dimension = (uint32_t)desc.dimension;
        pod.aspect = (uint32_t)desc.aspect;
        pod.baseMipLevel = desc.baseMipLevel;
        pod.mipCount = desc.mipCount;
        pod.baseLayer = desc.baseLayer;
        pod.layerCount = desc.layerCount;
        uint32_t handle =
            rive_gpu_texture_view_new(texture->handle(), &pod, sizeof(pod));
        if (handle == 0)
        {
            setLastError("host rejected texture view");
            return nullptr;
        }
        return make_rcp<ModuleOreTextureView>(handle,
                                              ref_rcp<ore::Texture>(texture),
                                              desc);
    }
    rcp<ore::Sampler> makeSampler(const ore::SamplerDesc& desc) override
    {
        rive_gpu_sampler_desc_v1 pod;
        pod.minFilter = (uint32_t)desc.minFilter;
        pod.magFilter = (uint32_t)desc.magFilter;
        pod.mipmapFilter = (uint32_t)desc.mipmapFilter;
        pod.wrapU = (uint32_t)desc.wrapU;
        pod.wrapV = (uint32_t)desc.wrapV;
        pod.wrapW = (uint32_t)desc.wrapW;
        pod.compare = (uint32_t)desc.compare;
        pod.minLod = desc.minLod;
        pod.maxLod = desc.maxLod;
        pod.maxAnisotropy = desc.maxAnisotropy;
        uint32_t handle = rive_gpu_sampler_new(&pod, sizeof(pod));
        if (handle == 0)
        {
            setLastError("host rejected sampler");
            return nullptr;
        }
        return make_rcp<ModuleOreSampler>(handle);
    }
    rcp<ore::ShaderModule> makeShaderModule(
        const ore::ShaderModuleDesc& desc) override
    {
        rive_gpu_shader_module_desc_v1 pod;
        pod.language = (uint32_t)desc.language;
        pod.stage = (uint32_t)desc.stage;
        pod.codeSize = desc.codeSize;
        pod.hlslSourceSize = desc.hlslSourceSize;
        pod.hlslEntryPointSize = desc.hlslEntryPoint != nullptr
                                     ? (uint32_t)strlen(desc.hlslEntryPoint)
                                     : 0;
        pod.bindingMapSize = desc.bindingMapSize;
        pod.glFixupSize = desc.glFixupSize;
        pod.shaderAssetId = desc.shaderAssetId;
        std::vector<uint8_t> blob;
        blob.reserve(pod.codeSize + pod.hlslSourceSize +
                     pod.hlslEntryPointSize + pod.bindingMapSize +
                     pod.glFixupSize);
        auto append = [&blob](const void* data, uint32_t size) {
            if (size != 0)
            {
                const uint8_t* bytes = static_cast<const uint8_t*>(data);
                blob.insert(blob.end(), bytes, bytes + size);
            }
        };
        append(desc.code, pod.codeSize);
        append(desc.hlslSource, pod.hlslSourceSize);
        append(desc.hlslEntryPoint, pod.hlslEntryPointSize);
        append(desc.bindingMapBytes, pod.bindingMapSize);
        append(desc.glFixupBytes, pod.glFixupSize);
        uint32_t handle = rive_gpu_shader_module_new(&pod,
                                                     sizeof(pod),
                                                     blob.data(),
                                                     (uint32_t)blob.size());
        if (handle == 0)
        {
            setLastError("host rejected shader module");
            return nullptr;
        }
        return make_rcp<ModuleOreShaderModule>(handle, desc);
    }
    rcp<ore::BindGroupLayout> makeBindGroupLayout(
        const ore::BindGroupLayoutDesc& desc) override
    {
        std::vector<rive_gpu_bind_group_layout_entry_v1> pods(desc.entryCount);
        for (uint32_t i = 0; i < desc.entryCount; i++)
        {
            auto& pod = pods[i];
            const auto& entry = desc.entries[i];
            pod.binding = entry.binding;
            pod.kind = (uint32_t)entry.kind;
            pod.visibility = entry.visibility.mask;
            pod.hasDynamicOffset = entry.hasDynamicOffset ? 1 : 0;
            pod.textureViewDim = (uint32_t)entry.textureViewDim;
            pod.textureSampleType = (uint32_t)entry.textureSampleType;
            pod.textureMultisampled = entry.textureMultisampled ? 1 : 0;
            pod.minBindingSize = entry.minBindingSize;
            pod.nativeSlotVS = entry.nativeSlotVS;
            pod.nativeSlotFS = entry.nativeSlotFS;
            pod.nativeSlotCS = entry.nativeSlotCS;
        }
        uint32_t handle = rive_gpu_bind_group_layout_new(
            desc.groupIndex,
            pods.data(),
            (uint32_t)(pods.size() * sizeof(pods[0])));
        if (handle == 0)
        {
            setLastError("host rejected bind group layout");
            return nullptr;
        }
        return make_rcp<ModuleOreBindGroupLayout>(handle, desc);
    }
    rcp<ore::Pipeline> makePipeline(const ore::PipelineDesc& desc,
                                    std::string* outError) override
    {
        rive_gpu_pipeline_desc_v1 pod = {};
        auto moduleHandle = [](ore::ShaderModule* module) -> uint32_t {
            return module != nullptr
                       ? static_cast<ModuleOreShaderModule*>(module)->handle()
                       : 0;
        };
        pod.vertexModule = moduleHandle(desc.vertexModule);
        pod.fragmentModule = moduleHandle(desc.fragmentModule);
        pod.vertexEntrySize = desc.vertexEntryPoint != nullptr
                                  ? (uint32_t)strlen(desc.vertexEntryPoint)
                                  : 0;
        pod.fragmentEntrySize = desc.fragmentEntryPoint != nullptr
                                    ? (uint32_t)strlen(desc.fragmentEntryPoint)
                                    : 0;
        pod.colorCount = desc.colorCount;
        pod.vertexBufferCount = desc.vertexBufferCount;
        uint32_t attributeCount = 0;
        for (uint32_t i = 0; i < desc.vertexBufferCount; i++)
        {
            attributeCount += desc.vertexBuffers[i].attributeCount;
        }
        pod.attributeCount = attributeCount;
        pod.bindGroupLayoutCount = desc.bindGroupLayoutCount;
        pod.topology = (uint32_t)desc.topology;
        pod.indexFormat = (uint32_t)desc.indexFormat;
        pod.cullMode = (uint32_t)desc.cullMode;
        pod.winding = (uint32_t)desc.winding;
        pod.depthFormat = (uint32_t)desc.depthStencil.format;
        pod.depthCompare = (uint32_t)desc.depthStencil.depthCompare;
        pod.depthWriteEnabled = desc.depthStencil.depthWriteEnabled ? 1 : 0;
        pod.depthBias = (uint32_t)desc.depthStencil.depthBias;
        pod.depthBiasSlopeScale = desc.depthStencil.depthBiasSlopeScale;
        pod.depthBiasClamp = desc.depthStencil.depthBiasClamp;
        pod.stencilFrontCompare = (uint32_t)desc.stencilFront.compare;
        pod.stencilFrontFailOp = (uint32_t)desc.stencilFront.failOp;
        pod.stencilFrontDepthFailOp = (uint32_t)desc.stencilFront.depthFailOp;
        pod.stencilFrontPassOp = (uint32_t)desc.stencilFront.passOp;
        pod.stencilBackCompare = (uint32_t)desc.stencilBack.compare;
        pod.stencilBackFailOp = (uint32_t)desc.stencilBack.failOp;
        pod.stencilBackDepthFailOp = (uint32_t)desc.stencilBack.depthFailOp;
        pod.stencilBackPassOp = (uint32_t)desc.stencilBack.passOp;
        pod.stencilReadMask = desc.stencilReadMask;
        pod.stencilWriteMask = desc.stencilWriteMask;
        pod.sampleCount = desc.sampleCount;

        std::vector<uint8_t> blob;
        auto append = [&blob](const void* data, size_t size) {
            if (size != 0)
            {
                const uint8_t* bytes = static_cast<const uint8_t*>(data);
                blob.insert(blob.end(), bytes, bytes + size);
            }
        };
        append(desc.vertexEntryPoint, pod.vertexEntrySize);
        append(desc.fragmentEntryPoint, pod.fragmentEntrySize);
        for (uint32_t i = 0; i < desc.colorCount; i++)
        {
            rive_gpu_color_target_v1 target;
            target.format = (uint32_t)desc.colorTargets[i].format;
            target.blendEnabled = desc.colorTargets[i].blendEnabled ? 1 : 0;
            target.srcColor = (uint32_t)desc.colorTargets[i].blend.srcColor;
            target.dstColor = (uint32_t)desc.colorTargets[i].blend.dstColor;
            target.colorOp = (uint32_t)desc.colorTargets[i].blend.colorOp;
            target.srcAlpha = (uint32_t)desc.colorTargets[i].blend.srcAlpha;
            target.dstAlpha = (uint32_t)desc.colorTargets[i].blend.dstAlpha;
            target.alphaOp = (uint32_t)desc.colorTargets[i].blend.alphaOp;
            target.writeMask = (uint32_t)desc.colorTargets[i].writeMask;
            append(&target, sizeof(target));
        }
        for (uint32_t i = 0; i < desc.vertexBufferCount; i++)
        {
            rive_gpu_vertex_buffer_layout_v1 layout;
            layout.stride = desc.vertexBuffers[i].stride;
            layout.stepMode = (uint32_t)desc.vertexBuffers[i].stepMode;
            layout.attributeCount = desc.vertexBuffers[i].attributeCount;
            append(&layout, sizeof(layout));
        }
        for (uint32_t i = 0; i < desc.vertexBufferCount; i++)
        {
            for (uint32_t a = 0; a < desc.vertexBuffers[i].attributeCount; a++)
            {
                rive_gpu_vertex_attribute_v1 attribute;
                attribute.format =
                    (uint32_t)desc.vertexBuffers[i].attributes[a].format;
                attribute.offset = desc.vertexBuffers[i].attributes[a].offset;
                attribute.shaderSlot =
                    desc.vertexBuffers[i].attributes[a].shaderSlot;
                append(&attribute, sizeof(attribute));
            }
        }
        for (uint32_t i = 0; i < desc.bindGroupLayoutCount; i++)
        {
            uint32_t handle = desc.bindGroupLayouts[i] != nullptr
                                  ? static_cast<ModuleOreBindGroupLayout*>(
                                        desc.bindGroupLayouts[i])
                                        ->handle()
                                  : 0;
            append(&handle, sizeof(handle));
        }

        uint32_t handle = rive_gpu_pipeline_new(&pod,
                                                sizeof(pod),
                                                blob.data(),
                                                (uint32_t)blob.size());
        if (handle == 0)
        {
            if (outError != nullptr)
            {
                *outError = "host rejected pipeline";
            }
            setLastError("host rejected pipeline");
            return nullptr;
        }
        return make_rcp<ModuleOrePipeline>(handle, desc);
    }
    rcp<ore::BindGroup> makeBindGroup(const ore::BindGroupDesc& desc) override
    {
        auto* layout = static_cast<ModuleOreBindGroupLayout*>(desc.layout);
        if (layout == nullptr)
        {
            setLastError("bind group requires a layout");
            return nullptr;
        }
        std::vector<rive_gpu_bind_group_ubo_v1> ubos(desc.uboCount);
        for (uint32_t i = 0; i < desc.uboCount; i++)
        {
            ubos[i].slot = desc.ubos[i].slot;
            ubos[i].buffer =
                static_cast<ModuleOreBuffer*>(desc.ubos[i].buffer)->handle();
            ubos[i].offset = desc.ubos[i].offset;
            ubos[i].size = desc.ubos[i].size;
        }
        std::vector<rive_gpu_bind_group_texture_v1> textures(desc.textureCount);
        for (uint32_t i = 0; i < desc.textureCount; i++)
        {
            textures[i].slot = desc.textures[i].slot;
            textures[i].view =
                static_cast<ModuleOreTextureView*>(desc.textures[i].view)
                    ->handle();
        }
        std::vector<rive_gpu_bind_group_sampler_v1> samplers(desc.samplerCount);
        for (uint32_t i = 0; i < desc.samplerCount; i++)
        {
            samplers[i].slot = desc.samplers[i].slot;
            samplers[i].sampler =
                static_cast<ModuleOreSampler*>(desc.samplers[i].sampler)
                    ->handle();
        }
        uint32_t handle = rive_gpu_bind_group_new(
            layout->handle(),
            ubos.data(),
            (uint32_t)(ubos.size() * sizeof(rive_gpu_bind_group_ubo_v1)),
            textures.data(),
            (uint32_t)(textures.size() *
                       sizeof(rive_gpu_bind_group_texture_v1)),
            samplers.data(),
            (uint32_t)(samplers.size() *
                       sizeof(rive_gpu_bind_group_sampler_v1)));
        if (handle == 0)
        {
            setLastError("host rejected bind group");
            return nullptr;
        }
        return make_rcp<ModuleOreBindGroup>(handle, desc);
    }
    std::unique_ptr<ore::RenderPass> beginRenderPass(
        const ore::RenderPassDesc& desc,
        std::string* outError) override
    {
        auto viewHandle = [](ore::TextureView* view) -> uint32_t {
            return view != nullptr
                       ? static_cast<ModuleOreTextureView*>(view)->handle()
                       : 0;
        };
        rive_gpu_pass_desc_v1 pod = {};
        pod.colorCount = desc.colorCount;
        pod.depthView = viewHandle(desc.depthStencil.view);
        pod.depthLoadOp = (uint32_t)desc.depthStencil.depthLoadOp;
        pod.depthStoreOp = (uint32_t)desc.depthStencil.depthStoreOp;
        pod.depthClearValue = desc.depthStencil.depthClearValue;
        pod.stencilLoadOp = (uint32_t)desc.depthStencil.stencilLoadOp;
        pod.stencilStoreOp = (uint32_t)desc.depthStencil.stencilStoreOp;
        pod.stencilClearValue = desc.depthStencil.stencilClearValue;
        rive_gpu_pass_color_attachment_v1 colors[4] = {};
        for (uint32_t i = 0; i < desc.colorCount && i < 4; i++)
        {
            colors[i].view = viewHandle(desc.colorAttachments[i].view);
            colors[i].resolveTarget =
                viewHandle(desc.colorAttachments[i].resolveTarget);
            colors[i].loadOp = (uint32_t)desc.colorAttachments[i].loadOp;
            colors[i].storeOp = (uint32_t)desc.colorAttachments[i].storeOp;
            colors[i].clearR = desc.colorAttachments[i].clearColor.r;
            colors[i].clearG = desc.colorAttachments[i].clearColor.g;
            colors[i].clearB = desc.colorAttachments[i].clearColor.b;
            colors[i].clearA = desc.colorAttachments[i].clearColor.a;
        }
        uint32_t handle =
            rive_gpu_pass_begin(&pod,
                                sizeof(pod),
                                colors,
                                desc.colorCount * (uint32_t)sizeof(colors[0]));
        if (handle == 0)
        {
            if (outError != nullptr)
            {
                *outError = "host rejected render pass";
            }
            setLastError("host rejected render pass");
            return nullptr;
        }
        return std::make_unique<ModuleOreRenderPass>(this, handle, desc);
    }
    rcp<ore::TextureView> wrapCanvasTexture(gpu::RenderCanvas*) override
    {
        return notPortedYet<ore::TextureView>("canvas wraps");
    }
    rcp<ore::TextureView> wrapRiveTexture(gpu::Texture*,
                                          uint32_t,
                                          uint32_t) override
    {
        return notPortedYet<ore::TextureView>("image wraps");
    }

    // Module images reach the host by handle; the host reruns the same
    // deferred-vs-canvas dispatch the Luau binding does.
    rcp<ore::TextureView> recordWrapCanvasImage(RenderImage* image,
                                                uint32_t width,
                                                uint32_t height) override
    {
        uint32_t handle =
            rive_gpu_image_view(wasmModuleImageHandle(image), width, height);
        if (handle == 0)
        {
            setLastError("host rejected image view");
            return nullptr;
        }
        ore::TextureDesc textureDesc;
        textureDesc.width = width;
        textureDesc.height = height;
        ore::TextureViewDesc viewDesc;
        auto texture = make_rcp<ModuleOreTexture>(0, textureDesc);
        viewDesc.texture = texture.get();
        return make_rcp<ModuleOreTextureView>(handle,
                                              std::move(texture),
                                              viewDesc);
    }

    void beginFrame(const FrameDescriptor&) override {}
    void endFrame() override {}
    void waitForGPU() override {}
    // Mirrors the host answer so RSTB entry selection matches the replay
    // backend.
    ore::ShaderTarget shaderTarget() const override
    {
        if (!m_shaderTargetKnown)
        {
            m_shaderTarget = (ore::ShaderTarget)rive_gpu_shader_target();
            m_shaderTargetKnown = true;
        }
        return m_shaderTarget;
    }
    // Same contract as a recording context with no replay device bound:
    // capability gates cannot be decided module side, so they let the call
    // through and the real backend stays the authority.
    bool featuresKnown() const override { return false; }
    // The host context this proxies is the deferred recorder; binding paths
    // that must not touch a driver gate on this.
    bool isRecording() const override { return true; }

private:
    template <typename T> rcp<T> notPortedYet(const char* what)
    {
        setLastError("%s are not available module side yet", what);
        return nullptr;
    }

    mutable ore::ShaderTarget m_shaderTarget = ore::ShaderTarget::wgsl;
    mutable bool m_shaderTargetKnown = false;
};

} // namespace

namespace
{
// Owns the host canvas handle: RenderCanvas has no virtual destructor, so
// release rides the target it retains instead.
class ModuleRenderTarget : public gpu::RenderTarget
{
public:
    ModuleRenderTarget(uint32_t canvasHandle, uint32_t width, uint32_t height) :
        gpu::RenderTarget(width, height), m_handle(canvasHandle)
    {}
    ~ModuleRenderTarget() override
    {
        if (m_handle != 0)
        {
            rive_gpu_canvas_release(m_handle);
        }
    }

    uint32_t handle() const { return m_handle; }
    // Transfers canvas handle ownership to a successor target on resize.
    uint32_t disownHandle()
    {
        uint32_t handle = m_handle;
        m_handle = 0;
        return handle;
    }

private:
    uint32_t m_handle;
};
} // namespace

WasmModuleCanvas wasmModuleWrapCanvas(uint32_t canvasHandle)
{
    uint32_t props[4] = {};
    uint32_t viewHandle = rive_gpu_canvas_color_view(canvasHandle, props, 4);
    if (viewHandle == 0)
    {
        return {};
    }
    ore::TextureDesc textureDesc;
    textureDesc.width = props[0];
    textureDesc.height = props[1];
    textureDesc.format = (ore::TextureFormat)props[2];
    textureDesc.sampleCount = props[3];
    textureDesc.renderTarget = true;
    ore::TextureViewDesc viewDesc;
    // Metadata-only texture wrapper so attachment validation sees the
    // canvas's real format and sample count.
    auto texture = make_rcp<ModuleOreTexture>(0, textureDesc);
    viewDesc.texture = texture.get();
    WasmModuleCanvas out;
    // Backing installs unbacked (null image texture, module semantics);
    // construction alone no longer carries the render target.
    out.canvas = make_rcp<gpu::RenderCanvas>(props[0], props[1]);
    out.canvas->setBacking(
        nullptr,
        make_rcp<ModuleRenderTarget>(canvasHandle, props[0], props[1]));
    out.colorView = make_rcp<ModuleOreTextureView>(viewHandle,
                                                   std::move(texture),
                                                   viewDesc);
    return out;
}

WasmModuleCanvas wasmModuleResizeCanvas(const rcp<gpu::RenderCanvas>& canvas,
                                        uint32_t width,
                                        uint32_t height)
{
    if (canvas == nullptr)
    {
        return {};
    }
    auto* target = static_cast<ModuleRenderTarget*>(canvas->renderTarget());
    uint32_t props[4] = {};
    uint32_t viewHandle =
        rive_gpu_canvas_resize(target->handle(), width, height, props, 4);
    if (viewHandle == 0)
    {
        return {};
    }
    ore::TextureDesc textureDesc;
    textureDesc.width = props[0];
    textureDesc.height = props[1];
    textureDesc.format = (ore::TextureFormat)props[2];
    textureDesc.sampleCount = props[3];
    textureDesc.renderTarget = true;
    ore::TextureViewDesc viewDesc;
    auto texture = make_rcp<ModuleOreTexture>(0, textureDesc);
    viewDesc.texture = texture.get();
    WasmModuleCanvas out;
    out.canvas = make_rcp<gpu::RenderCanvas>(props[0], props[1]);
    out.canvas->setBacking(nullptr,
                           make_rcp<ModuleRenderTarget>(target->disownHandle(),
                                                        props[0],
                                                        props[1]));
    out.colorView = make_rcp<ModuleOreTextureView>(viewHandle,
                                                   std::move(texture),
                                                   viewDesc);
    return out;
}

uint32_t wasmModuleCanvasImageHandle(const rcp<gpu::RenderCanvas>& canvas)
{
    if (canvas == nullptr)
    {
        return 0;
    }
    auto* target = static_cast<ModuleRenderTarget*>(canvas->renderTarget());
    return rive_gpu_canvas_image(target->handle());
}

ore::Context* wasmModuleOreContext()
{
    static ModuleOreContext context;
    return &context;
}
} // namespace rive

#endif
#endif
