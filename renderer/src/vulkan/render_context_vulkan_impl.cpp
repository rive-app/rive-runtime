/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"

#include "vulkan_shaders.hpp"
#include "rive/renderer/stack_vector.hpp"
#include "rive/renderer/texture.hpp"
#include "rive/renderer/rive_render_buffer.hpp"
#include "rive/renderer/vulkan/render_target_vulkan.hpp"
#include "shaders/constants.glsl"
#include "common_layouts.hpp"
#include "draw_pipeline_vulkan.hpp"
#include "draw_pipeline_layout_vulkan.hpp"
#include "draw_shader_vulkan.hpp"
#include "pipeline_manager_vulkan.hpp"
#include "render_pass_vulkan.hpp"

namespace rive::gpu
{

static VkBufferUsageFlagBits render_buffer_usage_flags(
    RenderBufferType renderBufferType)
{
    switch (renderBufferType)
    {
        case RenderBufferType::index:
            return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        case RenderBufferType::vertex:
            return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    RIVE_UNREACHABLE();
}

class RenderBufferVulkanImpl
    : public LITE_RTTI_OVERRIDE(RiveRenderBuffer, RenderBufferVulkanImpl)
{
public:
    RenderBufferVulkanImpl(rcp<VulkanContext> vk,
                           RenderBufferType renderBufferType,
                           RenderBufferFlags renderBufferFlags,
                           size_t sizeInBytes) :
        lite_rtti_override(renderBufferType, renderBufferFlags, sizeInBytes),
        m_bufferPool(make_rcp<vkutil::BufferPool>(
            std::move(vk),
            render_buffer_usage_flags(renderBufferType),
            sizeInBytes))
    {}

    vkutil::Buffer* currentBuffer() { return m_currentBuffer.get(); }

protected:
    void* onMap() override
    {
        m_bufferPool->recycle(std::move(m_currentBuffer));
        m_currentBuffer = m_bufferPool->acquire();
        return m_currentBuffer->contents();
    }

    void onUnmap() override { m_currentBuffer->flushContents(); }

private:
    rcp<vkutil::BufferPool> m_bufferPool;
    rcp<vkutil::Buffer> m_currentBuffer;
};

rcp<RenderBuffer> RenderContextVulkanImpl::makeRenderBuffer(
    RenderBufferType type,
    RenderBufferFlags flags,
    size_t sizeInBytes)
{
    return make_rcp<RenderBufferVulkanImpl>(m_vk, type, flags, sizeInBytes);
}

rcp<Texture> RenderContextVulkanImpl::makeImageTexture(
    uint32_t width,
    uint32_t height,
    uint32_t mipLevelCount,
    const uint8_t imageDataRGBAPremul[])
{
    auto texture = m_vk->makeTexture2D({
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = {width, height},
        .mipLevels = mipLevelCount,
    });
    texture->scheduleUpload(imageDataRGBAPremul, height * width * 4);
    return texture;
}

rcp<Texture> RenderContextVulkanImpl::makeImageTexture(
    uint32_t width, uint32_t height, uint32_t mipLevelCount,
    rcp<vkutil::Buffer> imageBufferRGBAPremul)
{
  assert(imageBufferRGBAPremul->info().size >= height * width * 4);
  auto texture = m_vk->makeTexture2D({
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .extent = {width, height},
      .mipLevels = mipLevelCount,
  });
  texture->scheduleUpload(imageBufferRGBAPremul);
  return texture;
}
  
// Renders color ramps to the gradient texture.
class RenderContextVulkanImpl::ColorRampPipeline
{
public:
    ColorRampPipeline(PipelineManagerVulkan* pipelineManager) :
        m_vk(ref_rcp(pipelineManager->vulkanContext()))
    {
        VkDescriptorSetLayout perFlushDescriptorSetLayout =
            pipelineManager->perFlushDescriptorSetLayout();
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &perFlushDescriptorSetLayout,
        };

        VK_CHECK(m_vk->CreatePipelineLayout(m_vk->device,
                                            &pipelineLayoutCreateInfo,
                                            nullptr,
                                            &m_pipelineLayout));

        VkShaderModuleCreateInfo shaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv::color_ramp_vert.size_bytes(),
            .pCode = spirv::color_ramp_vert.data(),
        };

        VkShaderModule vertexShader;
        VK_CHECK(m_vk->CreateShaderModule(m_vk->device,
                                          &shaderModuleCreateInfo,
                                          nullptr,
                                          &vertexShader));

        shaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv::color_ramp_frag.size_bytes(),
            .pCode = spirv::color_ramp_frag.data(),
        };

        VkShaderModule fragmentShader;
        VK_CHECK(m_vk->CreateShaderModule(m_vk->device,
                                          &shaderModuleCreateInfo,
                                          nullptr,
                                          &fragmentShader));

        VkPipelineShaderStageCreateInfo stages[] = {
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vertexShader,
                .pName = "main",
            },
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = fragmentShader,
                .pName = "main",
            },
        };

        VkVertexInputBindingDescription vertexInputBindingDescription = {
            .binding = 0,
            .stride = sizeof(gpu::GradientSpan),
            .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
        };

        VkVertexInputAttributeDescription vertexAttributeDescription = {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32A32_UINT,
        };

        VkPipelineVertexInputStateCreateInfo
            pipelineVertexInputStateCreateInfo = {
                .sType =
                    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &vertexInputBindingDescription,
                .vertexAttributeDescriptionCount = 1,
                .pVertexAttributeDescriptions = &vertexAttributeDescription,
            };

        VkAttachmentDescription attachment = {
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        VkRenderPassCreateInfo renderPassCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &attachment,
            .subpassCount = 1,
            .pSubpasses = &layout::SINGLE_ATTACHMENT_SUBPASS,
        };

        VK_CHECK(m_vk->CreateRenderPass(m_vk->device,
                                        &renderPassCreateInfo,
                                        nullptr,
                                        &m_renderPass));

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = stages,
            .pVertexInputState = &pipelineVertexInputStateCreateInfo,
            .pInputAssemblyState = &layout::INPUT_ASSEMBLY_TRIANGLE_STRIP,
            .pViewportState = &layout::SINGLE_VIEWPORT,
            .pRasterizationState = &layout::RASTER_STATE_CULL_BACK_CCW,
            .pMultisampleState = &layout::MSAA_DISABLED,
            .pColorBlendState = &layout::SINGLE_ATTACHMENT_BLEND_DISABLED,
            .pDynamicState = &layout::DYNAMIC_VIEWPORT_SCISSOR,
            .layout = m_pipelineLayout,
            .renderPass = m_renderPass,
        };

        VK_CHECK(m_vk->CreateGraphicsPipelines(m_vk->device,
                                               VK_NULL_HANDLE,
                                               1,
                                               &pipelineCreateInfo,
                                               nullptr,
                                               &m_renderPipeline));

        m_vk->DestroyShaderModule(m_vk->device, vertexShader, nullptr);
        m_vk->DestroyShaderModule(m_vk->device, fragmentShader, nullptr);
    }

    ~ColorRampPipeline()
    {
        m_vk->DestroyPipelineLayout(m_vk->device, m_pipelineLayout, nullptr);
        m_vk->DestroyRenderPass(m_vk->device, m_renderPass, nullptr);
        m_vk->DestroyPipeline(m_vk->device, m_renderPipeline, nullptr);
    }

    VkPipelineLayout pipelineLayout() const { return m_pipelineLayout; }
    VkRenderPass renderPass() const { return m_renderPass; }
    VkPipeline renderPipeline() const { return m_renderPipeline; }

private:
    rcp<VulkanContext> m_vk;
    VkPipelineLayout m_pipelineLayout;
    VkRenderPass m_renderPass;
    VkPipeline m_renderPipeline;
};

// Renders tessellated vertices to the tessellation texture.
class RenderContextVulkanImpl::TessellatePipeline
{
public:
    TessellatePipeline(PipelineManagerVulkan* pipelineManager) :
        m_vk(ref_rcp(pipelineManager->vulkanContext()))
    {
        VkDescriptorSetLayout pipelineDescriptorSetLayouts[] = {
            pipelineManager->perFlushDescriptorSetLayout(),
            pipelineManager->emptyDescriptorSetLayout(),
            pipelineManager->immutableSamplerDescriptorSetLayout(),
        };
        static_assert(PER_FLUSH_BINDINGS_SET == 0);
        static_assert(IMMUTABLE_SAMPLER_BINDINGS_SET == 2);

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = std::size(pipelineDescriptorSetLayouts),
            .pSetLayouts = pipelineDescriptorSetLayouts,
        };

        VK_CHECK(m_vk->CreatePipelineLayout(m_vk->device,
                                            &pipelineLayoutCreateInfo,
                                            nullptr,
                                            &m_pipelineLayout));

        VkShaderModuleCreateInfo shaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv::tessellate_vert.size_bytes(),
            .pCode = spirv::tessellate_vert.data(),
        };

        VkShaderModule vertexShader;
        VK_CHECK(m_vk->CreateShaderModule(m_vk->device,
                                          &shaderModuleCreateInfo,
                                          nullptr,
                                          &vertexShader));

        shaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv::tessellate_frag.size_bytes(),
            .pCode = spirv::tessellate_frag.data(),
        };

        VkShaderModule fragmentShader;
        VK_CHECK(m_vk->CreateShaderModule(m_vk->device,
                                          &shaderModuleCreateInfo,
                                          nullptr,
                                          &fragmentShader));

        VkPipelineShaderStageCreateInfo stages[] = {
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vertexShader,
                .pName = "main",
            },
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = fragmentShader,
                .pName = "main",
            },
        };

        VkVertexInputBindingDescription vertexInputBindingDescription = {
            .binding = 0,
            .stride = sizeof(gpu::TessVertexSpan),
            .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
        };

        VkVertexInputAttributeDescription vertexAttributeDescriptions[] = {
            {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = 0,
            },
            {
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = 4 * sizeof(float),
            },
            {
                .location = 2,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = 8 * sizeof(float),
            },
            {
                .location = 3,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32A32_UINT,
                .offset = 12 * sizeof(float),
            },
        };

        VkPipelineVertexInputStateCreateInfo
            pipelineVertexInputStateCreateInfo = {
                .sType =
                    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &vertexInputBindingDescription,
                .vertexAttributeDescriptionCount = 4,
                .pVertexAttributeDescriptions = vertexAttributeDescriptions,
            };

        VkAttachmentDescription attachment = {
            .format = VK_FORMAT_R32G32B32A32_UINT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        VkRenderPassCreateInfo renderPassCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &attachment,
            .subpassCount = 1,
            .pSubpasses = &layout::SINGLE_ATTACHMENT_SUBPASS,
        };

        VK_CHECK(m_vk->CreateRenderPass(m_vk->device,
                                        &renderPassCreateInfo,
                                        nullptr,
                                        &m_renderPass));

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = stages,
            .pVertexInputState = &pipelineVertexInputStateCreateInfo,
            .pInputAssemblyState = &layout::INPUT_ASSEMBLY_TRIANGLE_LIST,
            .pViewportState = &layout::SINGLE_VIEWPORT,
            .pRasterizationState = &layout::RASTER_STATE_CULL_BACK_CCW,
            .pMultisampleState = &layout::MSAA_DISABLED,
            .pColorBlendState = &layout::SINGLE_ATTACHMENT_BLEND_DISABLED,
            .pDynamicState = &layout::DYNAMIC_VIEWPORT_SCISSOR,
            .layout = m_pipelineLayout,
            .renderPass = m_renderPass,
        };

        VK_CHECK(m_vk->CreateGraphicsPipelines(m_vk->device,
                                               VK_NULL_HANDLE,
                                               1,
                                               &pipelineCreateInfo,
                                               nullptr,
                                               &m_renderPipeline));

        m_vk->DestroyShaderModule(m_vk->device, vertexShader, nullptr);
        m_vk->DestroyShaderModule(m_vk->device, fragmentShader, nullptr);
    }

    ~TessellatePipeline()
    {
        m_vk->DestroyPipelineLayout(m_vk->device, m_pipelineLayout, nullptr);
        m_vk->DestroyRenderPass(m_vk->device, m_renderPass, nullptr);
        m_vk->DestroyPipeline(m_vk->device, m_renderPipeline, nullptr);
    }

    VkPipelineLayout pipelineLayout() const { return m_pipelineLayout; }
    VkRenderPass renderPass() const { return m_renderPass; }
    VkPipeline renderPipeline() const { return m_renderPipeline; }

private:
    rcp<VulkanContext> m_vk;
    VkPipelineLayout m_pipelineLayout;
    VkRenderPass m_renderPass;
    VkPipeline m_renderPipeline;
};

// Renders feathers to the atlas.
class RenderContextVulkanImpl::AtlasPipeline
{
public:
    AtlasPipeline(PipelineManagerVulkan* pipelineManager) :
        m_vk(ref_rcp(pipelineManager->vulkanContext()))
    {
        VkDescriptorSetLayout pipelineDescriptorSetLayouts[] = {
            pipelineManager->perFlushDescriptorSetLayout(),
            pipelineManager->emptyDescriptorSetLayout(),
            pipelineManager->immutableSamplerDescriptorSetLayout(),
        };
        static_assert(PER_FLUSH_BINDINGS_SET == 0);
        static_assert(IMMUTABLE_SAMPLER_BINDINGS_SET == 2);

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = std::size(pipelineDescriptorSetLayouts),
            .pSetLayouts = pipelineDescriptorSetLayouts,
        };

        VK_CHECK(m_vk->CreatePipelineLayout(m_vk->device,
                                            &pipelineLayoutCreateInfo,
                                            nullptr,
                                            &m_pipelineLayout));

        VkShaderModuleCreateInfo shaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv::render_atlas_vert.size_bytes(),
            .pCode = spirv::render_atlas_vert.data(),
        };

        VkShaderModule vertexShader;
        VK_CHECK(m_vk->CreateShaderModule(m_vk->device,
                                          &shaderModuleCreateInfo,
                                          nullptr,
                                          &vertexShader));

        shaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv::render_atlas_fill_frag.size_bytes(),
            .pCode = spirv::render_atlas_fill_frag.data(),
        };

        VkShaderModule fragmentFillShader;
        VK_CHECK(m_vk->CreateShaderModule(m_vk->device,
                                          &shaderModuleCreateInfo,
                                          nullptr,
                                          &fragmentFillShader));

        shaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv::render_atlas_stroke_frag.size_bytes(),
            .pCode = spirv::render_atlas_stroke_frag.data(),
        };

        VkShaderModule fragmentStrokeShader;
        VK_CHECK(m_vk->CreateShaderModule(m_vk->device,
                                          &shaderModuleCreateInfo,
                                          VK_NULL_HANDLE,
                                          &fragmentStrokeShader));

        VkPipelineShaderStageCreateInfo stages[] = {
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vertexShader,
                .pName = "main",
            },
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                // Set for individual fill/stroke pipelines.
                .module = VK_NULL_HANDLE,
                .pName = "main",
            },
        };

        VkAttachmentDescription attachment = {
            .format = pipelineManager->atlasFormat(),
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        VkRenderPassCreateInfo renderPassCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &attachment,
            .subpassCount = 1,
            .pSubpasses = &layout::SINGLE_ATTACHMENT_SUBPASS,
        };
        VK_CHECK(m_vk->CreateRenderPass(m_vk->device,
                                        &renderPassCreateInfo,
                                        nullptr,
                                        &m_renderPass));

        VkPipelineColorBlendAttachmentState blendState =
            VkPipelineColorBlendAttachmentState{
                .blendEnable = VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT,
            };
        VkPipelineColorBlendStateCreateInfo blendStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1u,
            .pAttachments = &blendState,
        };

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = std::size(stages),
            .pStages = stages,
            .pVertexInputState = &layout::PATH_VERTEX_INPUT_STATE,
            .pInputAssemblyState = &layout::INPUT_ASSEMBLY_TRIANGLE_LIST,
            .pViewportState = &layout::SINGLE_VIEWPORT,
            .pRasterizationState = &layout::RASTER_STATE_CULL_BACK_CW,
            .pMultisampleState = &layout::MSAA_DISABLED,
            .pColorBlendState = &blendStateCreateInfo,
            .pDynamicState = &layout::DYNAMIC_VIEWPORT_SCISSOR,
            .layout = m_pipelineLayout,
            .renderPass = m_renderPass,
        };

        stages[1].module = fragmentFillShader;
        blendState.colorBlendOp = VK_BLEND_OP_ADD;
        VK_CHECK(m_vk->CreateGraphicsPipelines(m_vk->device,
                                               VK_NULL_HANDLE,
                                               1,
                                               &pipelineCreateInfo,
                                               nullptr,
                                               &m_fillPipeline));

        stages[1].module = fragmentStrokeShader;
        blendState.colorBlendOp = VK_BLEND_OP_MAX;
        VK_CHECK(m_vk->CreateGraphicsPipelines(m_vk->device,
                                               VK_NULL_HANDLE,
                                               1,
                                               &pipelineCreateInfo,
                                               nullptr,
                                               &m_strokePipeline));

        m_vk->DestroyShaderModule(m_vk->device, vertexShader, nullptr);
        m_vk->DestroyShaderModule(m_vk->device, fragmentFillShader, nullptr);
        m_vk->DestroyShaderModule(m_vk->device, fragmentStrokeShader, nullptr);
    }

    ~AtlasPipeline()
    {
        m_vk->DestroyPipelineLayout(m_vk->device, m_pipelineLayout, nullptr);
        m_vk->DestroyRenderPass(m_vk->device, m_renderPass, nullptr);
        m_vk->DestroyPipeline(m_vk->device, m_fillPipeline, nullptr);
        m_vk->DestroyPipeline(m_vk->device, m_strokePipeline, nullptr);
    }

    VkPipelineLayout pipelineLayout() const { return m_pipelineLayout; }
    VkRenderPass renderPass() const { return m_renderPass; }
    VkPipeline fillPipeline() const { return m_fillPipeline; }
    VkPipeline strokePipeline() const { return m_strokePipeline; }

private:
    rcp<VulkanContext> m_vk;
    VkPipelineLayout m_pipelineLayout;
    VkRenderPass m_renderPass;
    VkPipeline m_fillPipeline;
    VkPipeline m_strokePipeline;
};

RenderContextVulkanImpl::RenderContextVulkanImpl(
    rcp<VulkanContext> vk,
    const VkPhysicalDeviceProperties& physicalDeviceProps,
    const ContextOptions& contextOptions) :
    m_vk(std::move(vk)),
    m_flushUniformBufferPool(m_vk, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
    m_imageDrawUniformBufferPool(m_vk, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
    m_pathBufferPool(m_vk, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
    m_paintBufferPool(m_vk, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
    m_paintAuxBufferPool(m_vk, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
    m_contourBufferPool(m_vk, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
    m_gradSpanBufferPool(m_vk, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
    m_tessSpanBufferPool(m_vk, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
    m_triangleBufferPool(m_vk, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
    m_descriptorSetPoolPool(make_rcp<DescriptorSetPoolPool>(m_vk))
{
    m_platformFeatures.supportsRasterOrderingMode =
        !contextOptions.forceAtomicMode &&
        m_vk->features.rasterizationOrderColorAttachmentAccess;
#ifdef RIVE_ANDROID
    m_platformFeatures.supportsAtomicMode =
        m_vk->features.fragmentStoresAndAtomics &&
        // For now, disable gpu::InterlockMode::atomics on Android unless
        // explicitly requested. We will focus on stabilizing MSAA first, and
        // then roll this mode back in.
        contextOptions.forceAtomicMode;
#else
    m_platformFeatures.supportsAtomicMode =
        m_vk->features.fragmentStoresAndAtomics;
    m_platformFeatures.supportsClockwiseMode =
        m_vk->features.fragmentShaderPixelInterlock &&
        !contextOptions.forceAtomicMode &&
        // TODO: Before we can support rasterOrdering and clockwise at the same
        // time, we need to figure out barriers between transient PLS
        // attachments being bound as both input attachments and storage
        // textures. (Probably by using
        // VK_EXT_rasterization_order_attachment_access whenever possible in
        // clockwise mode.)
        !m_platformFeatures.supportsRasterOrderingMode;
#endif
    m_platformFeatures.supportsClockwiseAtomicMode =
        m_vk->features.fragmentStoresAndAtomics;
    m_platformFeatures.supportsClipPlanes =
        m_vk->features.shaderClipDistance &&
        // The Vulkan spec mandates that the minimum value for maxClipDistances
        // is 8, but we might as well make this >= 4 check to be more clear
        // about how we're using it.
        physicalDeviceProps.limits.maxClipDistances >= 4;
    m_platformFeatures.clipSpaceBottomUp = false;
    m_platformFeatures.framebufferBottomUp = false;
    // Vulkan can't load color from a different texture into the transient MSAA
    // texture. We need to draw the previous renderTarget contents into it
    // manually when LoadAction::preserveRenderTarget is specified.
    m_platformFeatures.msaaColorPreserveNeedsDraw = true;
    m_platformFeatures.maxCoverageBufferLength =
        std::min(physicalDeviceProps.limits.maxStorageBufferRange, 1u << 28) /
        sizeof(uint32_t);

    switch (physicalDeviceProps.vendorID)
    {
        case VULKAN_VENDOR_QUALCOMM:
            // Qualcomm advertises EXT_rasterization_order_attachment_access,
            // but it's slow. Use atomics instead on this platform.
            m_platformFeatures.supportsRasterOrderingMode = false;
            // Pixel4 struggles with fine-grained fp16 path IDs.
            m_platformFeatures.pathIDGranularity = 2;
            break;

        case VULKAN_VENDOR_ARM:
            // This is undocumented, but raster ordering always works on ARM
            // Mali GPUs if you define a subpass dependency, even without
            // EXT_rasterization_order_attachment_access.
            m_platformFeatures.supportsRasterOrderingMode = true;
            break;
    }
}

void RenderContextVulkanImpl::initGPUObjects(
    ShaderCompilationMode shaderCompilationMode,
    uint32_t vendorID)
{
    // Bound when there is not an image paint.
    constexpr static uint8_t black[] = {0, 0, 0, 1};
    m_nullImageTexture = m_vk->makeTexture2D({
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = {1, 1},
    });
    m_nullImageTexture->scheduleUpload(black, sizeof(black));

    m_pipelineManager = std::make_unique<PipelineManagerVulkan>(
        m_vk,
        shaderCompilationMode,
        vendorID,
        m_nullImageTexture->vkImageView());

    // The pipelines reference our vulkan objects. Delete them first.
    m_colorRampPipeline =
        std::make_unique<ColorRampPipeline>(m_pipelineManager.get());
    m_tessellatePipeline =
        std::make_unique<TessellatePipeline>(m_pipelineManager.get());
    m_atlasPipeline = std::make_unique<AtlasPipeline>(m_pipelineManager.get());

    // Determine usage flags for transient PLS backing textures.
    m_plsTransientUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                               VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    if (m_platformFeatures.supportsClockwiseMode)
    {
        // When we support the interlock, PLS backings can be storage textures.
        m_plsTransientUsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT |
                                    // For vkCmdClearColorImage.
                                    VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    else
    {
        // Otherwise, PLS backings are always transient input attachments.
        m_plsTransientUsageFlags |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    }

    // Emulate the feather texture1d array as a 2d texture until we add
    // texture1d support in Vulkan.
    uint16_t featherTextureData[gpu::GAUSSIAN_TABLE_SIZE *
                                FEATHER_TEXTURE_1D_ARRAY_LENGTH];
    memcpy(featherTextureData,
           gpu::g_gaussianIntegralTableF16,
           sizeof(gpu::g_gaussianIntegralTableF16));
    memcpy(featherTextureData + gpu::GAUSSIAN_TABLE_SIZE,
           gpu::g_inverseGaussianIntegralTableF16,
           sizeof(gpu::g_inverseGaussianIntegralTableF16));
    static_assert(FEATHER_FUNCTION_ARRAY_INDEX == 0);
    static_assert(FEATHER_INVERSE_FUNCTION_ARRAY_INDEX == 1);
    m_featherTexture = m_vk->makeTexture2D({
        .format = VK_FORMAT_R16_SFLOAT,
        .extent =
            {
                .width = gpu::GAUSSIAN_TABLE_SIZE,
                .height = FEATHER_TEXTURE_1D_ARRAY_LENGTH,
            },
    });
    m_featherTexture->scheduleUpload(featherTextureData,
                                     sizeof(featherTextureData));

    m_tessSpanIndexBuffer = m_vk->makeBuffer(
        {
            .size = sizeof(gpu::kTessSpanIndices),
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        },
        vkutil::Mappability::writeOnly);
    memcpy(m_tessSpanIndexBuffer->contents(),
           gpu::kTessSpanIndices,
           sizeof(gpu::kTessSpanIndices));
    m_tessSpanIndexBuffer->flushContents();

    m_pathPatchVertexBuffer = m_vk->makeBuffer(
        {
            .size = kPatchVertexBufferCount * sizeof(gpu::PatchVertex),
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        },
        vkutil::Mappability::writeOnly);
    m_pathPatchIndexBuffer = m_vk->makeBuffer(
        {
            .size = kPatchIndexBufferCount * sizeof(uint16_t),
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        },
        vkutil::Mappability::writeOnly);
    gpu::GeneratePatchBufferData(
        reinterpret_cast<PatchVertex*>(m_pathPatchVertexBuffer->contents()),
        reinterpret_cast<uint16_t*>(m_pathPatchIndexBuffer->contents()));
    m_pathPatchVertexBuffer->flushContents();
    m_pathPatchIndexBuffer->flushContents();

    m_imageRectVertexBuffer = m_vk->makeBuffer(
        {
            .size = sizeof(gpu::kImageRectVertices),
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        },
        vkutil::Mappability::writeOnly);
    memcpy(m_imageRectVertexBuffer->contents(),
           gpu::kImageRectVertices,
           sizeof(gpu::kImageRectVertices));
    m_imageRectVertexBuffer->flushContents();
    m_imageRectIndexBuffer = m_vk->makeBuffer(
        {
            .size = sizeof(gpu::kImageRectIndices),
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        },
        vkutil::Mappability::writeOnly);
    memcpy(m_imageRectIndexBuffer->contents(),
           gpu::kImageRectIndices,
           sizeof(gpu::kImageRectIndices));
    m_imageRectIndexBuffer->flushContents();
}

RenderContextVulkanImpl::~RenderContextVulkanImpl()
{
    // These should all have gotten recycled at the end of the last frame.
    assert(m_flushUniformBuffer == nullptr);
    assert(m_imageDrawUniformBuffer == nullptr);
    assert(m_pathBuffer == nullptr);
    assert(m_paintBuffer == nullptr);
    assert(m_paintAuxBuffer == nullptr);
    assert(m_contourBuffer == nullptr);
    assert(m_gradSpanBuffer == nullptr);
    assert(m_tessSpanBuffer == nullptr);
    assert(m_triangleBuffer == nullptr);

    // Tell the context we are entering our shutdown cycle. After this point,
    // all resources will be deleted immediately upon their refCount reaching
    // zero, as opposed to being kept alive for in-flight command buffers.
    m_vk->shutdown();
}

void RenderContextVulkanImpl::resizeGradientTexture(uint32_t width,
                                                    uint32_t height)
{
    width = std::max(width, 1u);
    height = std::max(height, 1u);

    m_gradTexture = m_vk->makeTexture2D({
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = {width, height},
        .usage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    });

    m_gradTextureFramebuffer = m_vk->makeFramebuffer({
        .renderPass = m_colorRampPipeline->renderPass(),
        .attachmentCount = 1,
        .pAttachments = m_gradTexture->vkImageViewAddressOf(),
        .width = width,
        .height = height,
        .layers = 1,
    });
}

void RenderContextVulkanImpl::resizeTessellationTexture(uint32_t width,
                                                        uint32_t height)
{
    width = std::max(width, 1u);
    height = std::max(height, 1u);

    m_tessTexture = m_vk->makeTexture2D({
        .format = VK_FORMAT_R32G32B32A32_UINT,
        .extent = {width, height},
        .usage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    });

    m_tessTextureFramebuffer = m_vk->makeFramebuffer({
        .renderPass = m_tessellatePipeline->renderPass(),
        .attachmentCount = 1,
        .pAttachments = m_tessTexture->vkImageViewAddressOf(),
        .width = width,
        .height = height,
        .layers = 1,
    });
}

void RenderContextVulkanImpl::resizeAtlasTexture(uint32_t width,
                                                 uint32_t height)
{
    width = std::max(width, 1u);
    height = std::max(height, 1u);

    m_atlasTexture = m_vk->makeTexture2D({
        .format = m_pipelineManager->atlasFormat(),
        .extent = {width, height},
        .usage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    });

    m_atlasFramebuffer = m_vk->makeFramebuffer({
        .renderPass = m_atlasPipeline->renderPass(),
        .attachmentCount = 1,
        .pAttachments = m_atlasTexture->vkImageViewAddressOf(),
        .width = width,
        .height = height,
        .layers = 1,
    });
}

void RenderContextVulkanImpl::resizeTransientPLSBacking(uint32_t width,
                                                        uint32_t height,
                                                        uint32_t planeCount)
{
    // Erase the backings and allocate them lazily. Our Vulkan backend needs
    // different allocations based on interlock mode and other factors.
    m_plsExtent = {width, height, 1};
    m_plsTransientPlaneCount = planeCount;
    m_plsTransientImageArray.reset();
    m_plsTransientCoverageView.reset();
    m_plsTransientClipView.reset();
    m_plsTransientScratchColorTexture.reset();
    m_plsOffscreenColorTexture.reset();
}

void RenderContextVulkanImpl::resizeAtomicCoverageBacking(uint32_t width,
                                                          uint32_t height)
{
    m_plsAtomicCoverageTexture.reset();

    if (width != 0 && height != 0)
    {
        m_plsAtomicCoverageTexture = m_vk->makeTexture2D({
            .format = VK_FORMAT_R32_UINT,
            .extent = {width, height},
            .usage =
                VK_IMAGE_USAGE_STORAGE_BIT |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT, // For vkCmdClearColorImage
        });
    }
}

void RenderContextVulkanImpl::resizeCoverageBuffer(size_t sizeInBytes)
{
    if (sizeInBytes == 0)
    {
        m_coverageBuffer = nullptr;
    }
    else
    {
        m_coverageBuffer = m_vk->makeBuffer(
            {
                .size = sizeInBytes,
                .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            },
            vkutil::Mappability::none);
    }
}

vkutil::Image* RenderContextVulkanImpl::plsTransientImageArray()
{
    assert(m_plsExtent.width != 0);
    assert(m_plsExtent.height != 0);
    assert(m_plsExtent.depth == 1);
    assert(m_plsTransientPlaneCount != 0);

    if (m_plsTransientImageArray == nullptr)
    {
        m_plsTransientImageArray = m_vk->makeImage({
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_R32_UINT,
            .extent = m_plsExtent,
            .arrayLayers = std::min(m_plsTransientPlaneCount, 2u),
            .usage = m_plsTransientUsageFlags,
        });
    }

    return m_plsTransientImageArray.get();
}

vkutil::ImageView* RenderContextVulkanImpl::plsTransientCoverageView()
{
    if (m_plsTransientCoverageView == nullptr)
    {
        m_plsTransientCoverageView = m_vk->makeImageView(
            ref_rcp(plsTransientImageArray()),
            {
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = VK_FORMAT_R32_UINT,
                .subresourceRange =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
            });
    }

    return m_plsTransientCoverageView.get();
}

vkutil::ImageView* RenderContextVulkanImpl::plsTransientClipView()
{
    if (m_plsTransientClipView == nullptr)
    {
        assert(m_plsTransientPlaneCount != 0);
        if (m_plsTransientPlaneCount == 1)
        {
            // When planeCount is 1, the shaders are guaranteed to only use
            // 1 single plane in an entire render pass, so we can just alias
            // the coverage & clip views to each other to keep the bindings and
            // validation happy.
            m_plsTransientClipView = ref_rcp(plsTransientCoverageView());
        }
        else
        {
            m_plsTransientClipView = m_vk->makeImageView(
                ref_rcp(plsTransientImageArray()),
                {
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = VK_FORMAT_R32_UINT,
                    .subresourceRange =
                        {
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                            .levelCount = 1,
                            .baseArrayLayer = 1,
                            .layerCount = 1,
                        },
                });
        }
    }

    return m_plsTransientClipView.get();
}

vkutil::Texture2D* RenderContextVulkanImpl::plsTransientScratchColorTexture()
{
    assert(m_plsExtent.width != 0);
    assert(m_plsExtent.height != 0);
    assert(m_plsExtent.depth == 1);
    assert(m_plsTransientPlaneCount != 0);

    if (m_plsTransientScratchColorTexture == nullptr)
    {
        m_plsTransientScratchColorTexture = m_vk->makeTexture2D({
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .extent = m_plsExtent,
            .usage = m_plsTransientUsageFlags,
        });
    }

    return m_plsTransientScratchColorTexture.get();
}

vkutil::Texture2D* RenderContextVulkanImpl::accessPLSOffscreenColorTexture(
    VkCommandBuffer commandBuffer,
    const vkutil::ImageAccess& dstAccess,
    vkutil::ImageAccessAction imageAccessAction)
{
    assert(m_plsExtent.width != 0);
    assert(m_plsExtent.height != 0);
    assert(m_plsExtent.depth == 1);
    assert(m_plsTransientPlaneCount != 0);

    if (m_plsOffscreenColorTexture == nullptr)
    {
        m_plsOffscreenColorTexture = m_vk->makeTexture2D({
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .extent = m_plsExtent,
            .usage = (m_plsTransientUsageFlags |
                      // For copying back to the main render target.
                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT) &
                     // This texture is never transient.
                     ~VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
        });
    }

    m_plsOffscreenColorTexture->barrier(commandBuffer,
                                        dstAccess,
                                        imageAccessAction);

    return m_plsOffscreenColorTexture.get();
}

vkutil::Texture2D* RenderContextVulkanImpl::clearPLSOffscreenColorTexture(
    VkCommandBuffer commandBuffer,
    ColorInt clearColor,
    const vkutil::ImageAccess& dstAccessAfterClear)
{
    m_vk->clearColorImage(
        commandBuffer,
        clearColor,
        accessPLSOffscreenColorTexture(
            commandBuffer,
            {
                .pipelineStages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .layout = VK_IMAGE_LAYOUT_GENERAL,
            },
            vkutil::ImageAccessAction::invalidateContents)
            ->vkImage(),
        VK_IMAGE_LAYOUT_GENERAL);

    return accessPLSOffscreenColorTexture(commandBuffer, dstAccessAfterClear);
}

vkutil::Texture2D* RenderContextVulkanImpl::
    copyRenderTargetToPLSOffscreenColorTexture(
        VkCommandBuffer commandBuffer,
        RenderTargetVulkan* renderTarget,
        const IAABB& copyBounds,
        const vkutil::ImageAccess& dstAccessAfterCopy)
{
    m_vk->blitSubRect(commandBuffer,
                      renderTarget->accessTargetImage(
                          commandBuffer,
                          {
                              .pipelineStages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                              .accessMask = VK_ACCESS_TRANSFER_READ_BIT,
                              .layout = VK_IMAGE_LAYOUT_GENERAL,
                          }),
                      VK_IMAGE_LAYOUT_GENERAL,
                      accessPLSOffscreenColorTexture(
                          commandBuffer,
                          {
                              .pipelineStages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                              .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                              .layout = VK_IMAGE_LAYOUT_GENERAL,
                          },
                          vkutil::ImageAccessAction::invalidateContents)
                          ->vkImage(),
                      VK_IMAGE_LAYOUT_GENERAL,
                      copyBounds);

    return accessPLSOffscreenColorTexture(commandBuffer, dstAccessAfterCopy);
}

void RenderContextVulkanImpl::prepareToFlush(uint64_t nextFrameNumber,
                                             uint64_t safeFrameNumber)
{
    // These should all have gotten recycled at the end of the last frame.
    assert(m_flushUniformBuffer == nullptr);
    assert(m_imageDrawUniformBuffer == nullptr);
    assert(m_pathBuffer == nullptr);
    assert(m_paintBuffer == nullptr);
    assert(m_paintAuxBuffer == nullptr);
    assert(m_contourBuffer == nullptr);
    assert(m_gradSpanBuffer == nullptr);
    assert(m_tessSpanBuffer == nullptr);
    assert(m_triangleBuffer == nullptr);

    // Advance the context frame and delete resources that are no longer
    // referenced by in-flight command buffers.
    m_vk->advanceFrameNumber(nextFrameNumber, safeFrameNumber);

    // Acquire buffers for the flush.
    m_flushUniformBuffer = m_flushUniformBufferPool.acquire();
    m_imageDrawUniformBuffer = m_imageDrawUniformBufferPool.acquire();
    m_pathBuffer = m_pathBufferPool.acquire();
    m_paintBuffer = m_paintBufferPool.acquire();
    m_paintAuxBuffer = m_paintAuxBufferPool.acquire();
    m_contourBuffer = m_contourBufferPool.acquire();
    m_gradSpanBuffer = m_gradSpanBufferPool.acquire();
    m_tessSpanBuffer = m_tessSpanBufferPool.acquire();
    m_triangleBuffer = m_triangleBufferPool.acquire();
}

namespace descriptor_pool_limits
{
constexpr static uint32_t kMaxUniformUpdates = 3;
constexpr static uint32_t kMaxDynamicUniformUpdates = 1;
constexpr static uint32_t kMaxImageTextureUpdates = 256;
constexpr static uint32_t kMaxSampledImageUpdates =
    4 + kMaxImageTextureUpdates; // tess + grad + feather + atlas + images
constexpr static uint32_t kMaxStorageImageUpdates =
    4; // color/coverage/clip/scratch in clockwise mode.
constexpr static uint32_t kMaxStorageBufferUpdates =
    7 + // path/paint/uniform buffers
    1;  // coverage buffer in clockwiseAtomic mode
constexpr static uint32_t kMaxDescriptorSets = 3 + kMaxImageTextureUpdates;
} // namespace descriptor_pool_limits

RenderContextVulkanImpl::DescriptorSetPool::DescriptorSetPool(
    rcp<VulkanContext> vulkanContext) :
    vkutil::Resource(std::move(vulkanContext))
{
    VkDescriptorPoolSize descriptorPoolSizes[] = {
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = descriptor_pool_limits::kMaxUniformUpdates,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .descriptorCount =
                descriptor_pool_limits::kMaxDynamicUniformUpdates,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = descriptor_pool_limits::kMaxSampledImageUpdates,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = descriptor_pool_limits::kMaxImageTextureUpdates,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = descriptor_pool_limits::kMaxStorageImageUpdates,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = descriptor_pool_limits::kMaxStorageBufferUpdates,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
            .descriptorCount = 4,
        },
    };

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = descriptor_pool_limits::kMaxDescriptorSets,
        .poolSizeCount = std::size(descriptorPoolSizes),
        .pPoolSizes = descriptorPoolSizes,
    };

    VK_CHECK(vk()->CreateDescriptorPool(vk()->device,
                                        &descriptorPoolCreateInfo,
                                        nullptr,
                                        &m_vkDescriptorPool));
}

RenderContextVulkanImpl::DescriptorSetPool::~DescriptorSetPool()
{
    vk()->DestroyDescriptorPool(vk()->device, m_vkDescriptorPool, nullptr);
}

VkDescriptorSet RenderContextVulkanImpl::DescriptorSetPool::
    allocateDescriptorSet(VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_vkDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };

    VkDescriptorSet descriptorSet;
    VK_CHECK(vk()->AllocateDescriptorSets(vk()->device,
                                          &descriptorSetAllocateInfo,
                                          &descriptorSet));

    return descriptorSet;
}

void RenderContextVulkanImpl::DescriptorSetPool::reset()
{
    vk()->ResetDescriptorPool(vk()->device, m_vkDescriptorPool, 0);
}

rcp<RenderContextVulkanImpl::DescriptorSetPool> RenderContextVulkanImpl::
    DescriptorSetPoolPool::acquire()
{
    auto descriptorSetPool =
        static_rcp_cast<DescriptorSetPool>(GPUResourcePool::acquire());
    if (descriptorSetPool == nullptr)
    {
        descriptorSetPool = make_rcp<DescriptorSetPool>(
            static_rcp_cast<VulkanContext>(m_manager));
    }
    else
    {
        descriptorSetPool->reset();
    }
    return descriptorSetPool;
}

void RenderContextVulkanImpl::flush(const FlushDescriptor& desc)
{
    constexpr static VkDeviceSize ZERO_OFFSET[1] = {0};
    constexpr static uint32_t ZERO_OFFSET_32[1] = {0};

    auto* renderTarget = static_cast<RenderTargetVulkan*>(desc.renderTarget);

    IAABB drawBounds = desc.renderTargetUpdateBounds;
    if (drawBounds.empty())
    {
        return;
    }

    if (desc.interlockMode == gpu::InterlockMode::msaa)
    {
        // Vulkan does not support partial MSAA resolves.
        // TODO: We should consider adding a new subpass that reads the MSAA
        // buffer and resolves it manually for partial updates.
        drawBounds = renderTarget->bounds();
    }

    auto commandBuffer =
        reinterpret_cast<VkCommandBuffer>(desc.externalCommandBuffer);
    rcp<DescriptorSetPool> descriptorSetPool =
        m_descriptorSetPoolPool->acquire();

    // Apply pending texture updates.
    m_featherTexture->prepareForFragmentShaderRead(commandBuffer);
    m_nullImageTexture->prepareForFragmentShaderRead(commandBuffer);
    for (const DrawBatch& batch : *desc.drawList)
    {
        if (auto imageTextureVulkan =
                static_cast<vkutil::Texture2D*>(batch.imageTexture))
        {
            imageTextureVulkan->prepareForFragmentShaderRead(commandBuffer);
        }
    }

    // Create a per-flush descriptor set.
    VkDescriptorSet perFlushDescriptorSet =
        descriptorSetPool->allocateDescriptorSet(
            m_pipelineManager->perFlushDescriptorSetLayout());

    m_vk->updateBufferDescriptorSets(
        perFlushDescriptorSet,
        {
            .dstBinding = FLUSH_UNIFORM_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        },
        {{
            .buffer = *m_flushUniformBuffer,
            .offset = desc.flushUniformDataOffsetInBytes,
            .range = sizeof(gpu::FlushUniforms),
        }});

    m_vk->updateBufferDescriptorSets(
        perFlushDescriptorSet,
        {
            .dstBinding = IMAGE_DRAW_UNIFORM_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        },
        {{
            .buffer = *m_imageDrawUniformBuffer,
            .offset = 0,
            .range = sizeof(gpu::ImageDrawUniforms),
        }});

    m_vk->updateBufferDescriptorSets(
        perFlushDescriptorSet,
        {
            .dstBinding = PATH_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        },
        {{
            .buffer = *m_pathBuffer,
            .offset = desc.firstPath * sizeof(gpu::PathData),
            .range = VK_WHOLE_SIZE,
        }});

    m_vk->updateBufferDescriptorSets(
        perFlushDescriptorSet,
        {
            .dstBinding = PAINT_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        },
        {{
            .buffer = *m_paintBuffer,
            .offset = desc.firstPaint * sizeof(gpu::PaintData),
            .range = VK_WHOLE_SIZE,
        }});

    // NOTE: This technically could be part of the above call (passing two
    // buffers instead of one to set them both at once), but there is a bug on
    // some Adreno devices where the second one does not get applied properly
    // (all reads from PaintAuxBuffer end up reading 0s), so instead we'll do it
    // as a separate call.
    m_vk->updateBufferDescriptorSets(
        perFlushDescriptorSet,
        {
            .dstBinding = PAINT_AUX_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        },
        {{
            .buffer = *m_paintAuxBuffer,
            .offset = desc.firstPaintAux * sizeof(gpu::PaintAuxData),
            .range = VK_WHOLE_SIZE,
        }});
    static_assert(PAINT_AUX_BUFFER_IDX == PAINT_BUFFER_IDX + 1);

    m_vk->updateBufferDescriptorSets(
        perFlushDescriptorSet,
        {
            .dstBinding = CONTOUR_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        },
        {{
            .buffer = *m_contourBuffer,
            .offset = desc.firstContour * sizeof(gpu::ContourData),
            .range = VK_WHOLE_SIZE,
        }});

    if (desc.interlockMode == gpu::InterlockMode::clockwiseAtomic &&
        m_coverageBuffer != nullptr)
    {
        m_vk->updateBufferDescriptorSets(
            perFlushDescriptorSet,
            {
                .dstBinding = COVERAGE_BUFFER_IDX,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            },
            {{
                .buffer = *m_coverageBuffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE,
            }});
    }

    m_vk->updateImageDescriptorSets(
        perFlushDescriptorSet,
        {
            .dstBinding = TESS_VERTEX_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        },
        {{
            .imageView = m_tessTexture->vkImageView(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        }});

    m_vk->updateImageDescriptorSets(
        perFlushDescriptorSet,
        {
            .dstBinding = GRAD_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        },
        {{
            .imageView = m_gradTexture->vkImageView(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        }});

    m_vk->updateImageDescriptorSets(
        perFlushDescriptorSet,
        {
            .dstBinding = FEATHER_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        },
        {{
            .imageView = m_featherTexture->vkImageView(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        }});

    m_vk->updateImageDescriptorSets(
        perFlushDescriptorSet,
        {
            .dstBinding = ATLAS_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        },
        {{
            .imageView = m_atlasTexture->vkImageView(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        }});

    // Render the complex color ramps to the gradient texture.
    if (desc.gradSpanCount > 0)
    {
        // Wait for previous accesses to finish before rendering to the gradient
        // texture.
        m_gradTexture->barrier(
            commandBuffer,
            {
                .pipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .accessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
            vkutil::ImageAccessAction::invalidateContents);

        VkRect2D renderArea = {
            .extent = {gpu::kGradTextureWidth, desc.gradDataHeight},
        };

        VkRenderPassBeginInfo renderPassBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = m_colorRampPipeline->renderPass(),
            .framebuffer = *m_gradTextureFramebuffer,
            .renderArea = renderArea,
        };

        m_vk->CmdBeginRenderPass(commandBuffer,
                                 &renderPassBeginInfo,
                                 VK_SUBPASS_CONTENTS_INLINE);

        m_vk->CmdBindPipeline(commandBuffer,
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              m_colorRampPipeline->renderPipeline());

        m_vk->CmdSetViewport(commandBuffer,
                             0,
                             1,
                             vkutil::ViewportFromRect2D(renderArea));

        m_vk->CmdSetScissor(commandBuffer, 0, 1, &renderArea);

        VkBuffer gradSpanBuffer = *m_gradSpanBuffer;
        VkDeviceSize gradSpanOffset =
            desc.firstGradSpan * sizeof(gpu::GradientSpan);
        m_vk->CmdBindVertexBuffers(commandBuffer,
                                   0,
                                   1,
                                   &gradSpanBuffer,
                                   &gradSpanOffset);

        m_vk->CmdBindDescriptorSets(commandBuffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_colorRampPipeline->pipelineLayout(),
                                    PER_FLUSH_BINDINGS_SET,
                                    1,
                                    &perFlushDescriptorSet,
                                    1,
                                    ZERO_OFFSET_32);

        m_vk->CmdDraw(commandBuffer,
                      gpu::GRAD_SPAN_TRI_STRIP_VERTEX_COUNT,
                      desc.gradSpanCount,
                      0,
                      0);

        m_vk->CmdEndRenderPass(commandBuffer);

        // The render pass transitioned the gradient texture to
        // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
        m_gradTexture->lastAccess().layout =
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    // Ensure the gradient texture has finished updating before the path
    // fragment shaders read it.
    m_gradTexture->barrier(
        commandBuffer,
        {
            .pipelineStages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .accessMask = VK_ACCESS_SHADER_READ_BIT,
            .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        });

    VkDescriptorSet immutableSamplerDescriptorSet =
        m_pipelineManager->immutableSamplerDescriptorSet();

    // Tessellate all curves into vertices in the tessellation texture.
    if (desc.tessVertexSpanCount > 0)
    {
        // Don't render new vertices until the previous flush has finished using
        // the tessellation texture.
        m_tessTexture->barrier(
            commandBuffer,
            {
                .pipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .accessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
            vkutil::ImageAccessAction::invalidateContents);

        VkRect2D renderArea = {
            .extent = {gpu::kTessTextureWidth, desc.tessDataHeight},
        };

        VkRenderPassBeginInfo renderPassBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = m_tessellatePipeline->renderPass(),
            .framebuffer = *m_tessTextureFramebuffer,
            .renderArea = renderArea,
        };

        m_vk->CmdBeginRenderPass(commandBuffer,
                                 &renderPassBeginInfo,
                                 VK_SUBPASS_CONTENTS_INLINE);

        m_vk->CmdBindPipeline(commandBuffer,
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              m_tessellatePipeline->renderPipeline());

        m_vk->CmdSetViewport(commandBuffer,
                             0,
                             1,
                             vkutil::ViewportFromRect2D(renderArea));

        m_vk->CmdSetScissor(commandBuffer, 0, 1, &renderArea);

        VkBuffer tessBuffer = *m_tessSpanBuffer;
        VkDeviceSize tessOffset =
            desc.firstTessVertexSpan * sizeof(gpu::TessVertexSpan);
        m_vk->CmdBindVertexBuffers(commandBuffer,
                                   0,
                                   1,
                                   &tessBuffer,
                                   &tessOffset);

        m_vk->CmdBindIndexBuffer(commandBuffer,
                                 *m_tessSpanIndexBuffer,
                                 0,
                                 VK_INDEX_TYPE_UINT16);

        m_vk->CmdBindDescriptorSets(commandBuffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_tessellatePipeline->pipelineLayout(),
                                    PER_FLUSH_BINDINGS_SET,
                                    1,
                                    &perFlushDescriptorSet,
                                    1,
                                    ZERO_OFFSET_32);
        m_vk->CmdBindDescriptorSets(commandBuffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_tessellatePipeline->pipelineLayout(),
                                    IMMUTABLE_SAMPLER_BINDINGS_SET,
                                    1,
                                    &immutableSamplerDescriptorSet,
                                    0,
                                    nullptr);

        m_vk->CmdDrawIndexed(commandBuffer,
                             std::size(gpu::kTessSpanIndices),
                             desc.tessVertexSpanCount,
                             0,
                             0,
                             0);

        m_vk->CmdEndRenderPass(commandBuffer);

        // The render pass transitioned the tessellation texture to
        // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
        m_tessTexture->lastAccess().layout =
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    // Ensure the tessellation texture has finished rendering before the path
    // vertex shaders read it.
    m_tessTexture->barrier(
        commandBuffer,
        {
            .pipelineStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
            .accessMask = VK_ACCESS_SHADER_READ_BIT,
            .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        });

    // Render the atlas if we have any offscreen feathers.
    if ((desc.atlasFillBatchCount | desc.atlasStrokeBatchCount) != 0)
    {
        // Don't render new vertices until the previous flush has finished using
        // the atlas texture.
        m_atlasTexture->barrier(
            commandBuffer,
            {
                .pipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .accessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
            vkutil::ImageAccessAction::invalidateContents);

        VkRect2D renderArea = {
            .extent = {desc.atlasContentWidth, desc.atlasContentHeight},
        };

        VkClearValue atlasClearValue = {};

        VkRenderPassBeginInfo renderPassBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = m_atlasPipeline->renderPass(),
            .framebuffer = *m_atlasFramebuffer,
            .renderArea = renderArea,
            .clearValueCount = 1,
            .pClearValues = &atlasClearValue,
        };

        m_vk->CmdBeginRenderPass(commandBuffer,
                                 &renderPassBeginInfo,
                                 VK_SUBPASS_CONTENTS_INLINE);
        m_vk->CmdSetViewport(commandBuffer,
                             0,
                             1,
                             vkutil::ViewportFromRect2D(renderArea));
        m_vk->CmdBindVertexBuffers(commandBuffer,
                                   0,
                                   1,
                                   m_pathPatchVertexBuffer->vkBufferAddressOf(),
                                   ZERO_OFFSET);
        m_vk->CmdBindIndexBuffer(commandBuffer,
                                 *m_pathPatchIndexBuffer,
                                 0,
                                 VK_INDEX_TYPE_UINT16);
        m_vk->CmdBindDescriptorSets(commandBuffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_atlasPipeline->pipelineLayout(),
                                    PER_FLUSH_BINDINGS_SET,
                                    1,
                                    &perFlushDescriptorSet,
                                    1,
                                    ZERO_OFFSET_32);
        m_vk->CmdBindDescriptorSets(commandBuffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_atlasPipeline->pipelineLayout(),
                                    IMMUTABLE_SAMPLER_BINDINGS_SET,
                                    1,
                                    &immutableSamplerDescriptorSet,
                                    0,
                                    nullptr);
        if (desc.atlasFillBatchCount != 0)
        {
            m_vk->CmdBindPipeline(commandBuffer,
                                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  m_atlasPipeline->fillPipeline());
            for (size_t i = 0; i < desc.atlasFillBatchCount; ++i)
            {
                const gpu::AtlasDrawBatch& fillBatch = desc.atlasFillBatches[i];
                VkRect2D scissor = {
                    .offset = {fillBatch.scissor.left, fillBatch.scissor.top},
                    .extent = {fillBatch.scissor.width(),
                               fillBatch.scissor.height()},
                };
                m_vk->CmdSetScissor(commandBuffer, 0, 1, &scissor);
                m_vk->CmdDrawIndexed(commandBuffer,
                                     gpu::kMidpointFanCenterAAPatchIndexCount,
                                     fillBatch.patchCount,
                                     gpu::kMidpointFanCenterAAPatchBaseIndex,
                                     0,
                                     fillBatch.basePatch);
            }
        }

        if (desc.atlasStrokeBatchCount != 0)
        {
            m_vk->CmdBindPipeline(commandBuffer,
                                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  m_atlasPipeline->strokePipeline());
            for (size_t i = 0; i < desc.atlasStrokeBatchCount; ++i)
            {
                const gpu::AtlasDrawBatch& strokeBatch =
                    desc.atlasStrokeBatches[i];
                VkRect2D scissor = {
                    .offset = {strokeBatch.scissor.left,
                               strokeBatch.scissor.top},
                    .extent = {strokeBatch.scissor.width(),
                               strokeBatch.scissor.height()},
                };
                m_vk->CmdSetScissor(commandBuffer, 0, 1, &scissor);
                m_vk->CmdDrawIndexed(commandBuffer,
                                     gpu::kMidpointFanPatchBorderIndexCount,
                                     strokeBatch.patchCount,
                                     gpu::kMidpointFanPatchBaseIndex,
                                     0,
                                     strokeBatch.basePatch);
            }
        }

        m_vk->CmdEndRenderPass(commandBuffer);

        // The render pass transitioned the atlas texture to
        // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
        m_atlasTexture->lastAccess().layout =
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    // Ensure the atlas texture has finished rendering before the fragment
    // shaders read it.
    m_atlasTexture->barrier(
        commandBuffer,
        {
            .pipelineStages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .accessMask = VK_ACCESS_SHADER_READ_BIT,
            .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        });

    // In the case of Vulkan, fixedFunctionColorOutput means the color buffer
    // will never be bound as an input attachment.
    const bool fixedFunctionColorOutput = desc.fixedFunctionColorOutput;

    // Ensures any previous accesses to a color attachment complete before we
    // begin rendering.
    const vkutil::ImageAccess colorLoadAccess = {
        // "Load" operations always occur in
        // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT.
        .pipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .accessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .layout = fixedFunctionColorOutput
                      ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                      : VK_IMAGE_LAYOUT_GENERAL,
    };

    const bool renderAreaIsFullTarget = drawBounds.contains(
        IAABB{0,
              0,
              static_cast<int32_t>(renderTarget->width()),
              static_cast<int32_t>(renderTarget->height())});

    const vkutil::ImageAccessAction targetAccessAction =
        renderAreaIsFullTarget &&
                desc.colorLoadAction != gpu::LoadAction::preserveRenderTarget
            ? vkutil::ImageAccessAction::invalidateContents
            : vkutil::ImageAccessAction::preserveContents;

    using PLSBackingType = PipelineManagerVulkan::PLSBackingType;
    const PLSBackingType plsBackingType =
        m_pipelineManager->plsBackingType(desc.interlockMode);

    VkImageView colorImageView = VK_NULL_HANDLE;
    bool colorAttachmentIsOffscreen = false;

    VkImageView msaaResolveImageView = VK_NULL_HANDLE;
    VkImageView msaaColorSeedImageView = VK_NULL_HANDLE;

    auto pipelineLayoutOptions = DrawPipelineLayoutVulkan::Options::none;
    if (fixedFunctionColorOutput)
    {
        pipelineLayoutOptions |=
            DrawPipelineLayoutVulkan::Options::fixedFunctionColorOutput;
    }

    if (desc.interlockMode == gpu::InterlockMode::msaa)
    {
        // MSAA mode always renders to a transient MSAA color buffer. (The
        // render target is single-sampled.)
        renderTarget->msaaColorTexture()->barrier(
            commandBuffer,
            colorLoadAccess,
            vkutil::ImageAccessAction::invalidateContents);
        colorImageView = renderTarget->msaaColorTexture()->vkImageView();

        if (desc.colorLoadAction == gpu::LoadAction::preserveRenderTarget &&
            renderTarget->targetUsageFlags() &
                VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)
        {
            // We can seed from, and resolve to the the same texture.
            msaaColorSeedImageView = msaaResolveImageView =
                renderTarget->accessTargetImageView(
                    commandBuffer,
                    {
                        // Apply a barrier for reading from this texture as an
                        // input attachment.
                        // vkCmdNextSubpass() will handle the barrier between
                        // reading this texture and resolving to it.
                        .pipelineStages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        .accessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                        .layout = VK_IMAGE_LAYOUT_GENERAL,
                    });
        }
        else
        {
            if (desc.colorLoadAction == gpu::LoadAction::preserveRenderTarget)
            {
                // We have to seed the MSAA attachment from a separate texture
                // because the render target doesn't support being bound as an
                // input attachment.
                msaaColorSeedImageView =
                    renderTarget
                        ->copyTargetImageToOffscreenColorTexture(
                            commandBuffer,
                            {
                                .pipelineStages =
                                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                .accessMask =
                                    VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                                .layout = VK_IMAGE_LAYOUT_GENERAL,
                            },
                            drawBounds)
                        ->vkImageView();
                pipelineLayoutOptions |= DrawPipelineLayoutVulkan::Options::
                    msaaSeedFromOffscreenTexture;
            }
            msaaResolveImageView = renderTarget->accessTargetImageView(
                commandBuffer,
                {
                    .pipelineStages =
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .accessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                },
                renderAreaIsFullTarget
                    ? vkutil::ImageAccessAction::invalidateContents
                    : vkutil::ImageAccessAction::preserveContents);
        }
    }
    else if (fixedFunctionColorOutput ||
             ((desc.interlockMode == gpu::InterlockMode::rasterOrdering ||
               desc.interlockMode == gpu::InterlockMode::atomics) &&
              (renderTarget->targetUsageFlags() &
               VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)))
    {
        // We can render directly to the render target.
        colorImageView =
            renderTarget->accessTargetImageView(commandBuffer,
                                                colorLoadAccess,
                                                targetAccessAction);
    }
    else if (plsBackingType == PLSBackingType::storageTexture)
    {
        constexpr static vkutil::ImageAccess PLS_STORAGE_TEXTURE_ACCESS = {
            .pipelineStages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .accessMask =
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            .layout = VK_IMAGE_LAYOUT_GENERAL,
        };

        if ((renderTarget->targetUsageFlags() & VK_IMAGE_USAGE_STORAGE_BIT) &&
            renderTarget->framebufferFormat() == VK_FORMAT_R8G8B8A8_UNORM)
        {
            // We can bind the renderTarget as a storage texture directly to the
            // color plane_.
            if (desc.colorLoadAction == gpu::LoadAction::clear)
            {
                colorImageView = renderTarget->clearTargetImageView(
                    commandBuffer,
                    desc.colorClearValue,
                    PLS_STORAGE_TEXTURE_ACCESS);
            }
            else
            {
                colorImageView = renderTarget->accessTargetImageView(
                    commandBuffer,
                    PLS_STORAGE_TEXTURE_ACCESS,
                    targetAccessAction);
            }
        }
        else
        {
            // We have to bind a separate texture to the color plane.
            switch (desc.colorLoadAction)
            {
                case gpu::LoadAction::clear:
                    colorImageView = clearPLSOffscreenColorTexture(
                                         commandBuffer,
                                         desc.colorClearValue,
                                         PLS_STORAGE_TEXTURE_ACCESS)
                                         ->vkImageView();
                    break;
                case gpu::LoadAction::preserveRenderTarget:
                    // Preserve the target texture by copying its contents into
                    // our offscreen color texture.
                    colorImageView = copyRenderTargetToPLSOffscreenColorTexture(
                                         commandBuffer,
                                         renderTarget,
                                         drawBounds,
                                         PLS_STORAGE_TEXTURE_ACCESS)
                                         ->vkImageView();
                    break;
                case gpu::LoadAction::dontCare:
                    colorImageView =
                        accessPLSOffscreenColorTexture(
                            commandBuffer,
                            PLS_STORAGE_TEXTURE_ACCESS,
                            vkutil::ImageAccessAction::invalidateContents)
                            ->vkImageView();
                    break;
            }
            colorAttachmentIsOffscreen = true;
        }
    }
    else
    {
        // The renderTarget doesn't support input attachments, so we have to
        // attach a separate texture to the framebuffer for color.
        if (desc.colorLoadAction == gpu::LoadAction::preserveRenderTarget)
        {
            // Preserve the target texture by copying its contents into our
            // offscreen color texture.
            colorImageView =
                renderTarget
                    ->copyTargetImageToOffscreenColorTexture(commandBuffer,
                                                             colorLoadAccess,
                                                             drawBounds)
                    ->vkImageView();
        }
        else
        {
            colorImageView =
                renderTarget
                    ->accessOffscreenColorTexture(
                        commandBuffer,
                        colorLoadAccess,
                        vkutil::ImageAccessAction::invalidateContents)
                    ->vkImageView();
        }
        if (desc.interlockMode == gpu::InterlockMode::atomics)
        {
            pipelineLayoutOptions |=
                DrawPipelineLayoutVulkan::Options::coalescedResolveAndTransfer;
        }
        colorAttachmentIsOffscreen = true;
    }

    RenderPassVulkan& renderPass = m_pipelineManager->getRenderPassSynchronous(
        desc.interlockMode,
        pipelineLayoutOptions,
        renderTarget->framebufferFormat(),
        desc.colorLoadAction);
    const DrawPipelineLayoutVulkan& pipelineLayout =
        *renderPass.drawPipelineLayout();

    // Create the framebuffer.
    StackVector<VkImageView, PLS_PLANE_COUNT> framebufferViews;
    StackVector<VkClearValue, PLS_PLANE_COUNT> clearValues;
    if (plsBackingType == PLSBackingType::inputAttachment ||
        fixedFunctionColorOutput)
    {
        assert(framebufferViews.size() == COLOR_PLANE_IDX);
        framebufferViews.push_back(colorImageView);
        clearValues.push_back(
            {.color = vkutil::color_clear_rgba32f(desc.colorClearValue)});
    }
    if (desc.interlockMode == gpu::InterlockMode::rasterOrdering)
    {
        assert(framebufferViews.size() == CLIP_PLANE_IDX);
        framebufferViews.push_back(*plsTransientClipView());
        clearValues.push_back({});

        assert(framebufferViews.size() == SCRATCH_COLOR_PLANE_IDX);
        framebufferViews.push_back(
            plsTransientScratchColorTexture()->vkImageView());
        clearValues.push_back({});

        assert(framebufferViews.size() == COVERAGE_PLANE_IDX);
        framebufferViews.push_back(*plsTransientCoverageView());
        clearValues.push_back(
            {.color = vkutil::color_clear_r32ui(desc.coverageClearValue)});
    }
    else if (desc.interlockMode == gpu::InterlockMode::atomics)
    {
        assert(framebufferViews.size() == CLIP_PLANE_IDX);
        framebufferViews.push_back(
            plsTransientScratchColorTexture()->vkImageView());
        clearValues.push_back({});

        if (pipelineLayout.options() &
            DrawPipelineLayoutVulkan::Options::coalescedResolveAndTransfer)
        {
            assert(framebufferViews.size() == COALESCED_ATOMIC_RESOLVE_IDX);
            framebufferViews.push_back(renderTarget->accessTargetImageView(
                commandBuffer,
                {
                    .pipelineStages =
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .accessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                },
                renderAreaIsFullTarget
                    ? vkutil::ImageAccessAction::invalidateContents
                    : vkutil::ImageAccessAction::preserveContents));
            clearValues.push_back({});
        }
    }
    else if (desc.interlockMode == gpu::InterlockMode::msaa)
    {
        assert(framebufferViews.size() == MSAA_DEPTH_STENCIL_IDX);
        framebufferViews.push_back(
            renderTarget->msaaDepthStencilTexture()->vkImageView());
        clearValues.push_back(
            {.depthStencil = {desc.depthClearValue, desc.stencilClearValue}});

        assert(framebufferViews.size() == MSAA_RESOLVE_IDX);
        framebufferViews.push_back(msaaResolveImageView);
        clearValues.push_back({});

        if (pipelineLayout.options() &
            DrawPipelineLayoutVulkan::Options::msaaSeedFromOffscreenTexture)
        {
            assert(desc.colorLoadAction ==
                   gpu::LoadAction::preserveRenderTarget);
            assert(msaaColorSeedImageView != VK_NULL_HANDLE);
            assert(msaaColorSeedImageView != msaaResolveImageView);
            assert(framebufferViews.size() == MSAA_COLOR_SEED_IDX);
            framebufferViews.push_back(msaaColorSeedImageView);
            clearValues.push_back({});
        }
    }

    rcp<vkutil::Framebuffer> framebuffer = m_vk->makeFramebuffer({
        .renderPass = renderPass,
        .attachmentCount = framebufferViews.size(),
        .pAttachments = framebufferViews.data(),
        .width = static_cast<uint32_t>(renderTarget->width()),
        .height = static_cast<uint32_t>(renderTarget->height()),
        .layers = 1,
    });

    VkRect2D renderArea = {
        .offset = {drawBounds.left, drawBounds.top},
        .extent = {static_cast<uint32_t>(drawBounds.width()),
                   static_cast<uint32_t>(drawBounds.height())},
    };

    const bool usesStorageTextures =
        plsBackingType == PLSBackingType::storageTexture ||
        desc.interlockMode == gpu::InterlockMode::atomics;
    if (usesStorageTextures)
    {
        // Clear the PLS planes that are bound as storage textures.
        const VkImage storageImageToClear =
            (desc.interlockMode == gpu::InterlockMode::atomics)
                ? m_plsAtomicCoverageTexture->vkImage()
                : *plsTransientImageArray();

        VkPipelineStageFlags srcStages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        VkPipelineStageFlags dstStages = VK_PIPELINE_STAGE_TRANSFER_BIT;
        StackVector<VkImageMemoryBarrier, 2> barriers;

        // Don't clear the storageImageToClear until shaders in previous flushes
        // have finished using it.
        // NOTE: This currently only works becauses we never support PLS as
        // storage texture (i.e., clockwise) and rasterOrdering at the same
        // time. If that changes, we will need to consider that the
        // plsTransientImageArray may have been bound previously as input
        // attachments.
        assert(plsBackingType == PLSBackingType::inputAttachment ||
               !m_platformFeatures.supportsRasterOrderingMode);
        barriers.push_back({
            .srcAccessMask =
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .image = storageImageToClear,
        });

        // The scratch color texture may also need a barrier.
        // NOTE: atomic mode uses the scratch color texture for clipping because
        // of its RGBA format.
        const bool usesScratchColorTexture =
            desc.interlockMode == gpu::InterlockMode::atomics ||
            !fixedFunctionColorOutput;
        if (usesScratchColorTexture)
        {
            // Don't use the scratch color texture until shaders in previous
            // render passes have finished using it.
            // NOTE: The scratch texture may have been bound as an input
            // attachment OR a storage texture.
            const VkAccessFlags storageTextureAccess =
                m_platformFeatures.supportsClockwiseMode
                    ? VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
                    : 0;
            srcStages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dstStages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            barriers.push_back({
                .srcAccessMask =
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | storageTextureAccess,
                .dstAccessMask =
                    VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | storageTextureAccess,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                .image = plsTransientScratchColorTexture()->vkImage(),
            });
        }

        m_vk->imageMemoryBarriers(commandBuffer,
                                  srcStages,
                                  dstStages,
                                  0,
                                  barriers.size(),
                                  barriers.data());

        // Clear the entire storageImageToClear, even if we aren't going to use
        // the whole thing. There is a future world where we may want to
        // consider a "renderPassInitialize" draw to clear only the
        // renderTargetUpdateBounds, but in most cases, a full clear will almost
        // definitely be faster due to hardware optimizations.
        if (desc.interlockMode == gpu::InterlockMode::atomics)
        {
            const VkClearColorValue coverageClearValue =
                vkutil::color_clear_r32ui(desc.coverageClearValue);

            const VkImageSubresourceRange clearRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            };

            m_vk->CmdClearColorImage(commandBuffer,
                                     m_plsAtomicCoverageTexture->vkImage(),
                                     VK_IMAGE_LAYOUT_GENERAL,
                                     &coverageClearValue,
                                     1,
                                     &clearRange);
        }
        else
        {
            assert(desc.coverageClearValue == 0);
            const VkClearColorValue zeroClearValue = {};

            const VkImageSubresourceRange transientClearRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = (desc.combinedShaderFeatures &
                               gpu::ShaderFeatures::ENABLE_CLIPPING)
                                  ? 2u  // coverage and clip.
                                  : 1u, // coverage only.
            };

            m_vk->CmdClearColorImage(commandBuffer,
                                     *plsTransientImageArray(),
                                     VK_IMAGE_LAYOUT_GENERAL,
                                     &zeroClearValue,
                                     1,
                                     &transientClearRange);
        }

        // Don't use the storageImageToClear in shaders until the clear
        // finishes.
        m_vk->imageMemoryBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            {
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask =
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                .image = storageImageToClear,
            });
    }
    else
    {
        // Ensure any color writes in prior frames complete before we execute
        // the load operations.
        // NOTE: This currently only works becauses we never support PLS as
        // storage texture (i.e., clockwise) and rasterOrdering at the same
        // time. If that changes, we will need to consider that the
        // plsTransientImageArray may have been bound previously as storage
        // textures.
        assert(desc.interlockMode != gpu::InterlockMode::rasterOrdering ||
               !m_platformFeatures.supportsClockwiseMode);
        m_vk->memoryBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            // "Load" operations always occur in
            // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT.
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0,
            {
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            });
    }

    if (desc.interlockMode == gpu::InterlockMode::clockwiseAtomic)
    {
        VkPipelineStageFlags lastCoverageBufferStage =
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        VkAccessFlags lastCoverageBufferAccess = VK_ACCESS_SHADER_WRITE_BIT;

        if (desc.needsCoverageBufferClear)
        {
            assert(m_coverageBuffer != nullptr);

            // Don't clear the coverage buffer until shaders in the previous
            // flush have finished accessing it.
            m_vk->bufferMemoryBarrier(
                commandBuffer,
                lastCoverageBufferStage,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                {
                    .srcAccessMask = lastCoverageBufferAccess,
                    .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                    .buffer = *m_coverageBuffer,
                });

            m_vk->CmdFillBuffer(commandBuffer,
                                *m_coverageBuffer,
                                0,
                                m_coverageBuffer->info().size,
                                0);

            lastCoverageBufferStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            lastCoverageBufferAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
        }

        if (m_coverageBuffer != nullptr)
        {
            // Don't use the coverage buffer until prior clears/accesses
            // have completed.
            m_vk->bufferMemoryBarrier(
                commandBuffer,
                lastCoverageBufferStage,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                {
                    .srcAccessMask = lastCoverageBufferAccess,
                    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                    .buffer = *m_coverageBuffer,
                });
        }
    }

    assert(clearValues.size() == framebufferViews.size());
    VkRenderPassBeginInfo renderPassBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass,
        .framebuffer = *framebuffer,
        .renderArea = renderArea,
        .clearValueCount = clearValues.size(),
        .pClearValues = clearValues.data(),
    };

    m_vk->CmdBeginRenderPass(commandBuffer,
                             &renderPassBeginInfo,
                             VK_SUBPASS_CONTENTS_INLINE);

    m_vk->CmdSetViewport(
        commandBuffer,
        0,
        1,
        vkutil::ViewportFromRect2D(
            {.extent = {renderTarget->width(), renderTarget->height()}}));

    m_vk->CmdSetScissor(commandBuffer, 0, 1, &renderArea);

    // Update the PLS input attachment descriptor sets.
    VkDescriptorSet inputAttachmentDescriptorSet = VK_NULL_HANDLE;
    if (pipelineLayout.plsLayout() != VK_NULL_HANDLE)
    {
        const VkDescriptorType plsDescriptorType =
            (plsBackingType ==
             PipelineManagerVulkan::PLSBackingType::storageTexture)
                ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                : VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        inputAttachmentDescriptorSet = descriptorSetPool->allocateDescriptorSet(
            pipelineLayout.plsLayout());

        if (!fixedFunctionColorOutput)
        {
            m_vk->updateImageDescriptorSets(
                inputAttachmentDescriptorSet,
                {
                    .dstBinding = COLOR_PLANE_IDX,
                    .descriptorType = plsDescriptorType,
                },
                {{
                    .imageView = colorImageView,
                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                }});
        }

        if (desc.interlockMode == gpu::InterlockMode::rasterOrdering ||
            desc.interlockMode == gpu::InterlockMode::atomics ||
            desc.interlockMode == gpu::InterlockMode::clockwise)
        {
            m_vk->updateImageDescriptorSets(
                inputAttachmentDescriptorSet,
                {
                    .dstBinding = CLIP_PLANE_IDX,
                    .descriptorType = plsDescriptorType,
                },
                {{
                    .imageView =
                        (desc.interlockMode == gpu::InterlockMode::atomics)
                            ? plsTransientScratchColorTexture()->vkImageView()
                            : *plsTransientClipView(),
                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                }});

            if (desc.interlockMode == gpu::InterlockMode::rasterOrdering ||
                (desc.interlockMode == gpu::InterlockMode::clockwise &&
                 !fixedFunctionColorOutput))
            {
                m_vk->updateImageDescriptorSets(
                    inputAttachmentDescriptorSet,
                    {
                        .dstBinding = SCRATCH_COLOR_PLANE_IDX,
                        .descriptorType = plsDescriptorType,
                    },
                    {{
                        .imageView =
                            plsTransientScratchColorTexture()->vkImageView(),
                        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                    }});
            }

            m_vk->updateImageDescriptorSets(
                inputAttachmentDescriptorSet,
                {
                    .dstBinding = COVERAGE_PLANE_IDX,
                    .descriptorType =
                        (desc.interlockMode == gpu::InterlockMode::atomics)
                            ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                            : plsDescriptorType,
                },
                {{
                    .imageView =
                        (desc.interlockMode == gpu::InterlockMode::atomics)
                            ? m_plsAtomicCoverageTexture->vkImageView()
                            : *plsTransientCoverageView(),
                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                }});
        }

        if (msaaColorSeedImageView != VK_NULL_HANDLE)
        {
            assert(desc.interlockMode == gpu::InterlockMode::msaa &&
                   desc.colorLoadAction ==
                       gpu::LoadAction::preserveRenderTarget);
            m_vk->updateImageDescriptorSets(
                inputAttachmentDescriptorSet,
                {
                    .dstBinding = MSAA_COLOR_SEED_IDX,
                    .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                },
                {{
                    .imageView = msaaColorSeedImageView,
                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                }});
        }
    }

    // Bind the descriptor sets for this draw pass.
    // (The imageTexture and imageDraw dynamic uniform offsets might have to
    // update between draws, but this is otherwise all we need to bind!)
    VkDescriptorSet drawDescriptorSets[] = {
        perFlushDescriptorSet,
        m_pipelineManager->nullImageDescriptorSet(),
        m_pipelineManager->immutableSamplerDescriptorSet(),
        inputAttachmentDescriptorSet,
    };
    static_assert(PER_FLUSH_BINDINGS_SET == 0);
    static_assert(PER_DRAW_BINDINGS_SET == 1);
    static_assert(IMMUTABLE_SAMPLER_BINDINGS_SET == 2);
    static_assert(PLS_TEXTURE_BINDINGS_SET == 3);
    static_assert(BINDINGS_SET_COUNT == 4);

    m_vk->CmdBindDescriptorSets(commandBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                *pipelineLayout,
                                PER_FLUSH_BINDINGS_SET,
                                pipelineLayout.plsLayout() != VK_NULL_HANDLE
                                    ? BINDINGS_SET_COUNT
                                    : BINDINGS_SET_COUNT - 1,
                                drawDescriptorSets,
                                1,
                                ZERO_OFFSET_32);

    // Execute the DrawList.
    uint32_t imageTextureUpdateCount = 0;
    for (const DrawBatch& batch : *desc.drawList)
    {
        assert(batch.elementCount > 0);
        const DrawType drawType = batch.drawType;

        if (batch.imageTexture != nullptr)
        {
            // Update the imageTexture binding and the dynamic offset into the
            // imageDraw uniform buffer.
            auto imageTexture =
                static_cast<vkutil::Texture2D*>(batch.imageTexture);
            VkDescriptorSet imageDescriptorSet =
                imageTexture->getCachedDescriptorSet(m_vk->currentFrameNumber(),
                                                     batch.imageSampler);
            if (imageDescriptorSet == VK_NULL_HANDLE)
            {
                // Update the image's "texture binding" descriptor set. (These
                // expire every frame, so we need to make a new one each frame.)
                if (imageTextureUpdateCount >=
                    descriptor_pool_limits::kMaxImageTextureUpdates)
                {
                    // We ran out of room for image texture updates. Allocate a
                    // new pool.
                    m_descriptorSetPoolPool->recycle(
                        std::move(descriptorSetPool));
                    descriptorSetPool = m_descriptorSetPoolPool->acquire();
                    imageTextureUpdateCount = 0;
                }

                imageDescriptorSet = descriptorSetPool->allocateDescriptorSet(
                    m_pipelineManager->perDrawDescriptorSetLayout());

                m_vk->updateImageDescriptorSets(
                    imageDescriptorSet,
                    {
                        .dstBinding = IMAGE_TEXTURE_IDX,
                        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                    },
                    {{
                        .imageView = imageTexture->vkImageView(),
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    }});

                m_vk->updateImageDescriptorSets(
                    imageDescriptorSet,
                    {
                        .dstBinding = IMAGE_SAMPLER_IDX,
                        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                    },
                    {{
                        .sampler = m_pipelineManager->imageSampler(
                            batch.imageSampler.asKey()),
                    }});

                ++imageTextureUpdateCount;
                imageTexture->updateCachedDescriptorSet(
                    imageDescriptorSet,
                    m_vk->currentFrameNumber(),
                    batch.imageSampler);
            }

            VkDescriptorSet imageDescriptorSets[] = {
                perFlushDescriptorSet, // Dynamic offset to imageDraw uniforms.
                imageDescriptorSet,    // imageTexture.
            };
            static_assert(PER_DRAW_BINDINGS_SET == PER_FLUSH_BINDINGS_SET + 1);

            m_vk->CmdBindDescriptorSets(commandBuffer,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        *pipelineLayout,
                                        PER_FLUSH_BINDINGS_SET,
                                        std::size(imageDescriptorSets),
                                        imageDescriptorSets,
                                        1,
                                        &batch.imageDrawDataOffset);
        }

        // Setup the pipeline for this specific drawType and shaderFeatures.
        gpu::ShaderFeatures shaderFeatures =
            desc.interlockMode == gpu::InterlockMode::atomics
                ? desc.combinedShaderFeatures
                : batch.shaderFeatures;

        auto shaderMiscFlags = batch.shaderMiscFlags;
        if (fixedFunctionColorOutput)
        {
            shaderMiscFlags |= gpu::ShaderMiscFlags::fixedFunctionColorOutput;
        }
        if (desc.interlockMode == gpu::InterlockMode::rasterOrdering &&
            (batch.drawContents & gpu::DrawContents::clockwiseFill))
        {
            shaderMiscFlags |= gpu::ShaderMiscFlags::clockwiseFill;
        }
        if ((pipelineLayout.options() &
             DrawPipelineLayoutVulkan::Options::coalescedResolveAndTransfer) &&
            (drawType == gpu::DrawType::renderPassResolve))
        {
            assert(desc.interlockMode == gpu::InterlockMode::atomics);
            shaderMiscFlags |=
                gpu::ShaderMiscFlags::coalescedResolveAndTransfer;
        }

        auto drawPipelineOptions = DrawPipelineVulkan::Options::none;
        if (desc.wireframe && m_vk->features.fillModeNonSolid)
        {
            drawPipelineOptions |= DrawPipelineVulkan::Options::wireframe;
        }

        gpu::PipelineState pipelineState;
        gpu::get_pipeline_state(batch,
                                desc,
                                m_platformFeatures,
                                &pipelineState);

        const DrawPipelineVulkan* drawPipeline =
            m_pipelineManager->tryGetPipeline(
                {
                    .drawType = drawType,
                    .shaderFeatures = shaderFeatures,
                    .interlockMode = desc.interlockMode,
                    .shaderMiscFlags = shaderMiscFlags,

                    .pipelineState = pipelineState,
                    .drawPipelineOptions = drawPipelineOptions,
                    .pipelineLayoutOptions = pipelineLayoutOptions,
                    .renderTargetFormat = renderTarget->framebufferFormat(),
                    .colorLoadAction = desc.colorLoadAction,
#ifdef WITH_RIVE_TOOLS
                    .synthesizedFailureType = desc.synthesizedFailureType,
#endif
                },
                m_platformFeatures);

        if (batch.barriers & (gpu::BarrierFlags::plsAtomicPreResolve |
                              gpu::BarrierFlags::msaaPostInit))
        {
            // vkCmdNextSubpass() supersedes the pipeline barrier we would
            // insert for plsAtomic | dstBlend. So if those flags are also in
            // the barrier, we can just call vkCmdNextSubpass() and skip
            // vkCmdPipelineBarrier().
            assert(
                !(batch.barriers &
                  ~(gpu::BarrierFlags::plsAtomicPreResolve |
                    gpu::BarrierFlags::msaaPostInit | BarrierFlags::plsAtomic |
                    BarrierFlags::dstBlend | BarrierFlags::drawBatchBreak)));
            m_vk->CmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
        }
        else if (batch.barriers &
                 (BarrierFlags::plsAtomic | BarrierFlags::dstBlend))
        {
            // Wait for color attachment writes to complete before we read the
            // input attachments again.
            assert(desc.interlockMode == gpu::InterlockMode::atomics ||
                   desc.interlockMode == gpu::InterlockMode::msaa);
            assert(drawType != gpu::DrawType::renderPassResolve);
            assert(!(batch.barriers &
                     ~(BarrierFlags::plsAtomic | BarrierFlags::dstBlend |
                       BarrierFlags::drawBatchBreak)));
            m_vk->memoryBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_DEPENDENCY_BY_REGION_BIT,
                {
                    // TODO: We should add SHADER_READ/SHADER_WRITE flags for
                    // the coverage buffer as well, but ironically, adding those
                    // causes artifacts on Qualcomm. Leave them out for now
                    // unless we find a case where we don't work without them.
                    .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    .dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                });
        }
        else if (batch.barriers & BarrierFlags::clockwiseBorrowedCoverage)
        {
            // Wait for prior fragment shaders to finish updating the coverage
            // buffer before we read it again.
            assert(desc.interlockMode == gpu::InterlockMode::clockwiseAtomic);
            assert(
                !(batch.barriers & ~(BarrierFlags::clockwiseBorrowedCoverage |
                                     BarrierFlags::drawBatchBreak)));
            m_vk->memoryBarrier(commandBuffer,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                VK_DEPENDENCY_BY_REGION_BIT,
                                {
                                    .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                                    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                                });
        }

        if (drawPipeline == nullptr)
        {
            continue;
        }

        m_vk->CmdBindPipeline(commandBuffer,
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              *drawPipeline);

        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::midpointFanCenterAAPatches:
            case DrawType::outerCurvePatches:
            case DrawType::msaaOuterCubics:
            case DrawType::msaaStrokes:
            case DrawType::msaaMidpointFanBorrowedCoverage:
            case DrawType::msaaMidpointFans:
            case DrawType::msaaMidpointFanStencilReset:
            case DrawType::msaaMidpointFanPathsStencil:
            case DrawType::msaaMidpointFanPathsCover:
            {
                // Draw patches that connect the tessellation vertices.
                m_vk->CmdBindVertexBuffers(
                    commandBuffer,
                    0,
                    1,
                    m_pathPatchVertexBuffer->vkBufferAddressOf(),
                    ZERO_OFFSET);
                m_vk->CmdBindIndexBuffer(commandBuffer,
                                         *m_pathPatchIndexBuffer,
                                         0,
                                         VK_INDEX_TYPE_UINT16);
                m_vk->CmdDrawIndexed(commandBuffer,
                                     gpu::PatchIndexCount(drawType),
                                     batch.elementCount,
                                     gpu::PatchBaseIndex(drawType),
                                     0,
                                     batch.baseElement);
                break;
            }

            case DrawType::msaaStencilClipReset:
            case DrawType::interiorTriangulation:
            case DrawType::atlasBlit:
            {
                VkBuffer buffer = *m_triangleBuffer;
                m_vk->CmdBindVertexBuffers(commandBuffer,
                                           0,
                                           1,
                                           &buffer,
                                           ZERO_OFFSET);
                m_vk->CmdDraw(commandBuffer,
                              batch.elementCount,
                              1,
                              batch.baseElement,
                              0);
                break;
            }

            case DrawType::imageRect:
            {
                assert(desc.interlockMode == gpu::InterlockMode::atomics);
                m_vk->CmdBindVertexBuffers(
                    commandBuffer,
                    0,
                    1,
                    m_imageRectVertexBuffer->vkBufferAddressOf(),
                    ZERO_OFFSET);
                m_vk->CmdBindIndexBuffer(commandBuffer,
                                         *m_imageRectIndexBuffer,
                                         0,
                                         VK_INDEX_TYPE_UINT16);
                m_vk->CmdDrawIndexed(commandBuffer,
                                     std::size(gpu::kImageRectIndices),
                                     1,
                                     batch.baseElement,
                                     0,
                                     0);
                break;
            }

            case DrawType::imageMesh:
            {
                LITE_RTTI_CAST_OR_BREAK(vertexBuffer,
                                        RenderBufferVulkanImpl*,
                                        batch.vertexBuffer);
                LITE_RTTI_CAST_OR_BREAK(uvBuffer,
                                        RenderBufferVulkanImpl*,
                                        batch.uvBuffer);
                LITE_RTTI_CAST_OR_BREAK(indexBuffer,
                                        RenderBufferVulkanImpl*,
                                        batch.indexBuffer);
                m_vk->CmdBindVertexBuffers(
                    commandBuffer,
                    0,
                    1,
                    vertexBuffer->currentBuffer()->vkBufferAddressOf(),
                    ZERO_OFFSET);
                m_vk->CmdBindVertexBuffers(
                    commandBuffer,
                    1,
                    1,
                    uvBuffer->currentBuffer()->vkBufferAddressOf(),
                    ZERO_OFFSET);
                m_vk->CmdBindIndexBuffer(commandBuffer,
                                         *indexBuffer->currentBuffer(),
                                         0,
                                         VK_INDEX_TYPE_UINT16);
                m_vk->CmdDrawIndexed(commandBuffer,
                                     batch.elementCount,
                                     1,
                                     batch.baseElement,
                                     0,
                                     0);
                break;
            }

            case DrawType::renderPassInitialize:
            case DrawType::renderPassResolve:
            {
                m_vk->CmdDraw(commandBuffer, 4, 1, 0, 0);
                break;
            }
        }
    }

    if (desc.interlockMode == InterlockMode::msaa)
    {
        // MSAA needs a follow-up subpass to resolve the MSAA buffer to the
        // single-sampled render target. No actually rendering is necessary, it
        // is taken care of entirely via the render pass mechanisms. This is
        // a workaround for what appears to be a driver bug in some early Adreno
        // drivers.
        m_vk->CmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    }

    m_vk->CmdEndRenderPass(commandBuffer);

    if (colorAttachmentIsOffscreen &&
        !(pipelineLayout.options() &
          DrawPipelineLayoutVulkan::Options::coalescedResolveAndTransfer))
    {
        // Copy from the offscreen texture back to the target.
        constexpr static vkutil::ImageAccess ACCESS_COPY_FROM = {
            .pipelineStages = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .accessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .layout = VK_IMAGE_LAYOUT_GENERAL,
        };

        m_vk->blitSubRect(
            commandBuffer,
            ((plsBackingType == PLSBackingType::storageTexture)
                 ? accessPLSOffscreenColorTexture(commandBuffer,
                                                  ACCESS_COPY_FROM)
                 : renderTarget->accessOffscreenColorTexture(commandBuffer,
                                                             ACCESS_COPY_FROM))
                ->vkImage(),
            ACCESS_COPY_FROM.layout,
            renderTarget->accessTargetImage(
                commandBuffer,
                {
                    .pipelineStages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                    .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                    .layout = VK_IMAGE_LAYOUT_GENERAL,
                },
                vkutil::ImageAccessAction::invalidateContents),
            VK_IMAGE_LAYOUT_GENERAL,
            drawBounds);
    }

    m_descriptorSetPoolPool->recycle(std::move(descriptorSetPool));
}

void RenderContextVulkanImpl::postFlush(const RenderContext::FlushResources&)
{
    // Recycle buffers.
    m_flushUniformBufferPool.recycle(std::move(m_flushUniformBuffer));
    m_imageDrawUniformBufferPool.recycle(std::move(m_imageDrawUniformBuffer));
    m_pathBufferPool.recycle(std::move(m_pathBuffer));
    m_paintBufferPool.recycle(std::move(m_paintBuffer));
    m_paintAuxBufferPool.recycle(std::move(m_paintAuxBuffer));
    m_contourBufferPool.recycle(std::move(m_contourBuffer));
    m_gradSpanBufferPool.recycle(std::move(m_gradSpanBuffer));
    m_tessSpanBufferPool.recycle(std::move(m_tessSpanBuffer));
    m_triangleBufferPool.recycle(std::move(m_triangleBuffer));
}

void RenderContextVulkanImpl::hotloadShaders(
    rive::Span<const uint32_t> spirvData)
{
    m_pipelineManager->clearCache();
    spirv::hotload_shaders(spirvData);

    // Delete and replace old shaders
    m_colorRampPipeline =
        std::make_unique<ColorRampPipeline>(m_pipelineManager.get());
    m_tessellatePipeline =
        std::make_unique<TessellatePipeline>(m_pipelineManager.get());
    m_atlasPipeline = std::make_unique<AtlasPipeline>(m_pipelineManager.get());
}

std::unique_ptr<RenderContext> RenderContextVulkanImpl::MakeContext(
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    const VulkanFeatures& features,
    PFN_vkGetInstanceProcAddr pfnvkGetInstanceProcAddr,
    const ContextOptions& contextOptions)
{
    rcp<VulkanContext> vk = make_rcp<VulkanContext>(instance,
                                                    physicalDevice,
                                                    device,
                                                    features,
                                                    pfnvkGetInstanceProcAddr);
    VkPhysicalDeviceProperties physicalDeviceProps;
    vk->GetPhysicalDeviceProperties(vk->physicalDevice, &physicalDeviceProps);
    std::unique_ptr<RenderContextVulkanImpl> impl(
        new RenderContextVulkanImpl(std::move(vk),
                                    physicalDeviceProps,
                                    contextOptions));
    if (contextOptions.forceAtomicMode &&
        !impl->platformFeatures().supportsClockwiseAtomicMode)
    {
        fprintf(stderr,
                "ERROR: Requested \"atomic\" mode but Vulkan does not support "
                "fragmentStoresAndAtomics on this platform.\n");
        return nullptr;
    }
    impl->initGPUObjects(contextOptions.shaderCompilationMode,
                         physicalDeviceProps.vendorID);
    return std::make_unique<RenderContext>(std::move(impl));
}
} // namespace rive::gpu
