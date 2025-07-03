/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"

#include "draw_shader_vulkan.hpp"
#include "rive/renderer/stack_vector.hpp"
#include "rive/renderer/texture.hpp"
#include "rive/renderer/rive_render_buffer.hpp"
#include "rive/renderer/vulkan/render_target_vulkan.hpp"
#include "shaders/constants.glsl"

// Common layout descriptors shared by various pipelines.
namespace layout
{
constexpr static VkVertexInputBindingDescription PATH_INPUT_BINDINGS[] = {{
    .binding = 0,
    .stride = sizeof(rive::gpu::PatchVertex),
    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
}};
constexpr static VkVertexInputAttributeDescription PATH_VERTEX_ATTRIBS[] = {
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
};
constexpr static VkPipelineVertexInputStateCreateInfo PATH_VERTEX_INPUT_STATE =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = std::size(PATH_INPUT_BINDINGS),
        .pVertexBindingDescriptions = PATH_INPUT_BINDINGS,
        .vertexAttributeDescriptionCount = std::size(PATH_VERTEX_ATTRIBS),
        .pVertexAttributeDescriptions = PATH_VERTEX_ATTRIBS,
};

constexpr static VkVertexInputBindingDescription INTERIOR_TRI_INPUT_BINDINGS[] =
    {{
        .binding = 0,
        .stride = sizeof(rive::gpu::TriangleVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    }};
constexpr static VkVertexInputAttributeDescription
    INTERIOR_TRI_VERTEX_ATTRIBS[] = {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = 0,
        },
};
constexpr static VkPipelineVertexInputStateCreateInfo
    INTERIOR_TRI_VERTEX_INPUT_STATE = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = std::size(INTERIOR_TRI_INPUT_BINDINGS),
        .pVertexBindingDescriptions = INTERIOR_TRI_INPUT_BINDINGS,
        .vertexAttributeDescriptionCount =
            std::size(INTERIOR_TRI_VERTEX_ATTRIBS),
        .pVertexAttributeDescriptions = INTERIOR_TRI_VERTEX_ATTRIBS,
};

constexpr static VkVertexInputBindingDescription IMAGE_RECT_INPUT_BINDINGS[] = {
    {
        .binding = 0,
        .stride = sizeof(rive::gpu::ImageRectVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    }};
constexpr static VkVertexInputAttributeDescription IMAGE_RECT_VERTEX_ATTRIBS[] =
    {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = 0,
        },
};
constexpr static VkPipelineVertexInputStateCreateInfo
    IMAGE_RECT_VERTEX_INPUT_STATE = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = std::size(IMAGE_RECT_INPUT_BINDINGS),
        .pVertexBindingDescriptions = IMAGE_RECT_INPUT_BINDINGS,
        .vertexAttributeDescriptionCount = std::size(IMAGE_RECT_VERTEX_ATTRIBS),
        .pVertexAttributeDescriptions = IMAGE_RECT_VERTEX_ATTRIBS,
};

constexpr static VkVertexInputBindingDescription IMAGE_MESH_INPUT_BINDINGS[] = {
    {
        .binding = 0,
        .stride = sizeof(float) * 2,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    },
    {
        .binding = 1,
        .stride = sizeof(float) * 2,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    },
};
constexpr static VkVertexInputAttributeDescription IMAGE_MESH_VERTEX_ATTRIBS[] =
    {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = 0,
        },
        {
            .location = 1,
            .binding = 1,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = 0,
        },
};
constexpr static VkPipelineVertexInputStateCreateInfo
    IMAGE_MESH_VERTEX_INPUT_STATE = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = std::size(IMAGE_MESH_INPUT_BINDINGS),
        .pVertexBindingDescriptions = IMAGE_MESH_INPUT_BINDINGS,
        .vertexAttributeDescriptionCount = std::size(IMAGE_MESH_VERTEX_ATTRIBS),
        .pVertexAttributeDescriptions = IMAGE_MESH_VERTEX_ATTRIBS,
};

constexpr static VkPipelineVertexInputStateCreateInfo EMPTY_VERTEX_INPUT_STATE =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .vertexAttributeDescriptionCount = 0,
};

constexpr static VkPipelineInputAssemblyStateCreateInfo
    INPUT_ASSEMBLY_TRIANGLE_STRIP = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
};

constexpr static VkPipelineInputAssemblyStateCreateInfo
    INPUT_ASSEMBLY_TRIANGLE_LIST = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
};

constexpr static VkPipelineViewportStateCreateInfo SINGLE_VIEWPORT = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .scissorCount = 1,
};

constexpr static VkPipelineRasterizationStateCreateInfo
    RASTER_STATE_CULL_BACK_CCW = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.f,
};

constexpr static VkPipelineRasterizationStateCreateInfo
    RASTER_STATE_CULL_BACK_CW = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.f,
};

constexpr static VkPipelineMultisampleStateCreateInfo MSAA_DISABLED = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
};

constexpr static VkPipelineColorBlendAttachmentState BLEND_DISABLED_VALUES = {
    .colorWriteMask = rive::gpu::vkutil::kColorWriteMaskRGBA};
constexpr static VkPipelineColorBlendStateCreateInfo
    SINGLE_ATTACHMENT_BLEND_DISABLED = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &BLEND_DISABLED_VALUES,
};

constexpr static VkDynamicState DYNAMIC_VIEWPORT_SCISSOR_VALUES[] = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
};
constexpr static VkPipelineDynamicStateCreateInfo DYNAMIC_VIEWPORT_SCISSOR = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = std::size(DYNAMIC_VIEWPORT_SCISSOR_VALUES),
    .pDynamicStates = DYNAMIC_VIEWPORT_SCISSOR_VALUES,
};

constexpr static VkAttachmentReference SINGLE_ATTACHMENT_SUBPASS_REFERENCE = {
    .attachment = 0,
    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
};
constexpr static VkSubpassDescription SINGLE_ATTACHMENT_SUBPASS = {
    .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
    .colorAttachmentCount = 1,
    .pColorAttachments = &SINGLE_ATTACHMENT_SUBPASS_REFERENCE,
};
} // namespace layout

namespace rive::gpu
{
// This is the render pass attachment index for the final color output in the
// "coalesced" atomic resolve.
// NOTE: This attachment is still referenced as color attachment 0 by the
// resolve subpass, so the shader doesn't need to know about it.
// NOTE: Atomic mode does not use SCRATCH_COLOR_PLANE_IDX, which is why we chose
// to alias this one.
constexpr static uint32_t COALESCED_ATOMIC_RESOLVE_IDX =
    SCRATCH_COLOR_PLANE_IDX;

// MSAA doesn't use other planes besides color, so depth & resolve start right
// after color.
constexpr static uint32_t MSAA_DEPTH_STENCIL_IDX = COLOR_PLANE_IDX + 1;
constexpr static uint32_t MSAA_RESOLVE_IDX = COLOR_PLANE_IDX + 2;

// The most attachments we currently use is 4, in rasterOrdering mode.
constexpr static uint32_t MAX_FRAMEBUFFER_ATTACHMENTS = PLS_PLANE_COUNT;

static VkStencilOp vk_stencil_op(StencilOp op)
{
    switch (op)
    {
        case StencilOp::keep:
            return VK_STENCIL_OP_KEEP;
        case StencilOp::replace:
            return VK_STENCIL_OP_REPLACE;
        case StencilOp::zero:
            return VK_STENCIL_OP_ZERO;
        case StencilOp::decrClamp:
            return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case StencilOp::incrWrap:
            return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case StencilOp::decrWrap:
            return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    }
    RIVE_UNREACHABLE();
}

static VkCompareOp vk_compare_op(gpu::StencilCompareOp op)
{
    switch (op)
    {
        case gpu::StencilCompareOp::less:
            return VK_COMPARE_OP_LESS;
        case gpu::StencilCompareOp::equal:
            return VK_COMPARE_OP_EQUAL;
        case gpu::StencilCompareOp::lessOrEqual:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
        case gpu::StencilCompareOp::notEqual:
            return VK_COMPARE_OP_NOT_EQUAL;
        case gpu::StencilCompareOp::always:
            return VK_COMPARE_OP_ALWAYS;
    }
    RIVE_UNREACHABLE();
}

static VkCullModeFlags vk_cull_mode(CullFace cullFace)
{
    switch (cullFace)
    {
        case CullFace::none:
            return VK_CULL_MODE_NONE;
        case CullFace::clockwise:
            return VK_CULL_MODE_FRONT_BIT;
        case CullFace::counterclockwise:
            return VK_CULL_MODE_BACK_BIT;
    }
    RIVE_UNREACHABLE();
}

static VkBlendOp vk_blend_op(gpu::BlendEquation equation)
{
    switch (equation)
    {
        case gpu::BlendEquation::none:
        case gpu::BlendEquation::srcOver:
            return VK_BLEND_OP_ADD;
        case gpu::BlendEquation::plus:
            return VK_BLEND_OP_ADD;
        case gpu::BlendEquation::max:
            return VK_BLEND_OP_MAX;
        case gpu::BlendEquation::screen:
            return VK_BLEND_OP_SCREEN_EXT;
        case gpu::BlendEquation::overlay:
            return VK_BLEND_OP_OVERLAY_EXT;
        case gpu::BlendEquation::darken:
            return VK_BLEND_OP_DARKEN_EXT;
        case gpu::BlendEquation::lighten:
            return VK_BLEND_OP_LIGHTEN_EXT;
        case gpu::BlendEquation::colorDodge:
            return VK_BLEND_OP_COLORDODGE_EXT;
        case gpu::BlendEquation::colorBurn:
            return VK_BLEND_OP_COLORBURN_EXT;
        case gpu::BlendEquation::hardLight:
            return VK_BLEND_OP_HARDLIGHT_EXT;
        case gpu::BlendEquation::softLight:
            return VK_BLEND_OP_SOFTLIGHT_EXT;
        case gpu::BlendEquation::difference:
            return VK_BLEND_OP_DIFFERENCE_EXT;
        case gpu::BlendEquation::exclusion:
            return VK_BLEND_OP_EXCLUSION_EXT;
        case gpu::BlendEquation::multiply:
            return VK_BLEND_OP_MULTIPLY_EXT;
        case gpu::BlendEquation::hue:
            return VK_BLEND_OP_HSL_HUE_EXT;
        case gpu::BlendEquation::saturation:
            return VK_BLEND_OP_HSL_SATURATION_EXT;
        case gpu::BlendEquation::color:
            return VK_BLEND_OP_HSL_COLOR_EXT;
        case gpu::BlendEquation::luminosity:
            return VK_BLEND_OP_HSL_LUMINOSITY_EXT;
    }
}

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

// Renders color ramps to the gradient texture.
class RenderContextVulkanImpl::ColorRampPipeline
{
public:
    ColorRampPipeline(RenderContextVulkanImpl* impl) :
        m_vk(ref_rcp(impl->vulkanContext()))
    {
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &impl->m_perFlushDescriptorSetLayout,
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
    TessellatePipeline(RenderContextVulkanImpl* impl) :
        m_vk(ref_rcp(impl->vulkanContext()))
    {
        VkDescriptorSetLayout pipelineDescriptorSetLayouts[] = {
            impl->m_perFlushDescriptorSetLayout,
            impl->m_emptyDescriptorSetLayout,
            impl->m_immutableSamplerDescriptorSetLayout,
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
    AtlasPipeline(RenderContextVulkanImpl* impl) :
        m_vk(ref_rcp(impl->vulkanContext()))
    {
        VkDescriptorSetLayout pipelineDescriptorSetLayouts[] = {
            impl->m_perFlushDescriptorSetLayout,
            impl->m_emptyDescriptorSetLayout,
            impl->m_immutableSamplerDescriptorSetLayout,
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
            .format = impl->m_atlasFormat,
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

// VkPipelineLayout wrapper for Rive flushes.
class RenderContextVulkanImpl::DrawPipelineLayout
{
public:
    // Number of render pass variants that can be used with a single
    // DrawPipelineLayout (framebufferFormat x loadOp).
    constexpr static int kRenderPassVariantCount = 6;

    DrawPipelineLayout(RenderContextVulkanImpl* impl,
                       gpu::InterlockMode interlockMode,
                       DrawPipelineLayoutOptions options) :
        m_vk(ref_rcp(impl->vulkanContext())),
        m_interlockMode(interlockMode),
        m_options(options)
    {
        // PLS planes get bound per flush as input attachments or storage
        // textures.
        VkDescriptorSetLayoutBinding plsLayoutBindings[] = {
            {
                .binding = COLOR_PLANE_IDX,
                .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            {
                .binding = CLIP_PLANE_IDX,
                .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            {
                .binding = SCRATCH_COLOR_PLANE_IDX,
                .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            {
                .binding = COVERAGE_PLANE_IDX,
                .descriptorType = m_interlockMode == gpu::InterlockMode::atomics
                                      ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                                      : VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
        };
        static_assert(COLOR_PLANE_IDX == 0);
        static_assert(CLIP_PLANE_IDX == 1);
        static_assert(SCRATCH_COLOR_PLANE_IDX == 2);
        static_assert(COVERAGE_PLANE_IDX == 3);

        VkDescriptorSetLayoutCreateInfo plsLayoutInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        };

        if (interlockMode == gpu::InterlockMode::rasterOrdering ||
            interlockMode == gpu::InterlockMode::atomics)
        {
            plsLayoutInfo.bindingCount = std::size(plsLayoutBindings);
            plsLayoutInfo.pBindings = plsLayoutBindings;
        }
        else if (interlockMode == gpu::InterlockMode::msaa)
        {
            plsLayoutInfo.bindingCount = 1;
            plsLayoutInfo.pBindings = plsLayoutBindings;
        }

        if (m_options & DrawPipelineLayoutOptions::fixedFunctionColorOutput)
        {
            // Drop the COLOR input attachment when using
            // fixedFunctionColorOutput.
            assert(plsLayoutInfo.pBindings[0].binding == COLOR_PLANE_IDX);
            ++plsLayoutInfo.pBindings;
            --plsLayoutInfo.bindingCount;
        }

        if (plsLayoutInfo.bindingCount > 0)
        {
            VK_CHECK(m_vk->CreateDescriptorSetLayout(
                m_vk->device,
                &plsLayoutInfo,
                nullptr,
                &m_plsTextureDescriptorSetLayout));
        }

        VkDescriptorSetLayout pipelineDescriptorSetLayouts[] = {
            impl->m_perFlushDescriptorSetLayout,
            impl->m_perDrawDescriptorSetLayout,
            impl->m_immutableSamplerDescriptorSetLayout,
            m_plsTextureDescriptorSetLayout,
        };
        static_assert(COLOR_PLANE_IDX == 0);
        static_assert(CLIP_PLANE_IDX == 1);
        static_assert(SCRATCH_COLOR_PLANE_IDX == 2);
        static_assert(COVERAGE_PLANE_IDX == 3);
        static_assert(BINDINGS_SET_COUNT == 4);

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = m_plsTextureDescriptorSetLayout != VK_NULL_HANDLE
                                  ? BINDINGS_SET_COUNT
                                  : BINDINGS_SET_COUNT - 1u,
            .pSetLayouts = pipelineDescriptorSetLayouts,
        };

        VK_CHECK(m_vk->CreatePipelineLayout(m_vk->device,
                                            &pipelineLayoutCreateInfo,
                                            nullptr,
                                            &m_pipelineLayout));
    }

    ~DrawPipelineLayout()
    {
        m_vk->DestroyDescriptorSetLayout(m_vk->device,
                                         m_plsTextureDescriptorSetLayout,
                                         nullptr);
        m_vk->DestroyPipelineLayout(m_vk->device, m_pipelineLayout, nullptr);
    }

    gpu::InterlockMode interlockMode() const { return m_interlockMode; }
    DrawPipelineLayoutOptions options() const { return m_options; }

    uint32_t colorAttachmentCount(uint32_t subpassIndex) const
    {
        switch (m_interlockMode)
        {
            case gpu::InterlockMode::rasterOrdering:
                assert(subpassIndex == 0);
                return 4;
            case gpu::InterlockMode::atomics:
                assert(subpassIndex <= 1);
                return 2u - subpassIndex; // Subpass 0 -> 2, subpass 1 -> 1.
            case gpu::InterlockMode::clockwiseAtomic:
                assert(subpassIndex == 0);
                return 1;
            case gpu::InterlockMode::msaa:
                assert(subpassIndex == 0);
                return 1;
        }
    }

    VkDescriptorSetLayout plsLayout() const
    {
        return m_plsTextureDescriptorSetLayout;
    }

    VkPipelineLayout operator*() const { return m_pipelineLayout; }
    VkPipelineLayout vkPipelineLayout() const { return m_pipelineLayout; }

private:
    const rcp<VulkanContext> m_vk;
    const gpu::InterlockMode m_interlockMode;
    const DrawPipelineLayoutOptions m_options;

    VkDescriptorSetLayout m_plsTextureDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout;
};

constexpr static VkAttachmentLoadOp vk_load_op(gpu::LoadAction loadAction)
{
    switch (loadAction)
    {
        case gpu::LoadAction::preserveRenderTarget:
            return VK_ATTACHMENT_LOAD_OP_LOAD;
        case gpu::LoadAction::clear:
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case gpu::LoadAction::dontCare:
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
    RIVE_UNREACHABLE();
}

// VkRenderPass wrapper for Rive flushes.
class RenderContextVulkanImpl::RenderPass
{
public:
    constexpr static uint64_t FORMAT_BIT_COUNT = 19;
    constexpr static uint64_t LOAD_OP_BIT_COUNT = 2;
    constexpr static uint64_t KEY_BIT_COUNT =
        FORMAT_BIT_COUNT + DRAW_PIPELINE_LAYOUT_BIT_COUNT + LOAD_OP_BIT_COUNT;
    static_assert(KEY_BIT_COUNT <= 32);

    static uint32_t Key(gpu::InterlockMode interlockMode,
                        DrawPipelineLayoutOptions layoutOptions,
                        VkFormat renderTargetFormat,
                        gpu::LoadAction loadAction)
    {
        uint32_t formatKey = static_cast<uint32_t>(renderTargetFormat);
        if (formatKey > VK_FORMAT_ASTC_12x12_SRGB_BLOCK)
        {
            assert(formatKey >= VK_FORMAT_G8B8G8R8_422_UNORM);
            formatKey -= VK_FORMAT_G8B8G8R8_422_UNORM -
                         VK_FORMAT_ASTC_12x12_SRGB_BLOCK - 1;
        }
        assert(formatKey <= 1 << FORMAT_BIT_COUNT);

        uint32_t drawPipelineLayoutIdx =
            DrawPipelineLayoutIdx(interlockMode, layoutOptions);
        assert(drawPipelineLayoutIdx < 1 << DRAW_PIPELINE_LAYOUT_BIT_COUNT);

        assert(static_cast<uint32_t>(loadAction) < 1 << LOAD_OP_BIT_COUNT);

        return (formatKey << (DRAW_PIPELINE_LAYOUT_BIT_COUNT +
                              LOAD_OP_BIT_COUNT)) |
               (drawPipelineLayoutIdx << LOAD_OP_BIT_COUNT) |
               static_cast<uint32_t>(loadAction);
    }

    RenderPass(RenderContextVulkanImpl* impl,
               gpu::InterlockMode interlockMode,
               DrawPipelineLayoutOptions layoutOptions,
               VkFormat renderTargetFormat,
               gpu::LoadAction loadAction) :
        m_vk(ref_rcp(impl->vulkanContext()))
    {
        uint32_t pipelineLayoutIdx =
            DrawPipelineLayoutIdx(interlockMode, layoutOptions);
        assert(pipelineLayoutIdx < impl->m_drawPipelineLayouts.size());
        if (impl->m_drawPipelineLayouts[pipelineLayoutIdx] == nullptr)
        {
            impl->m_drawPipelineLayouts[pipelineLayoutIdx] =
                std::make_unique<DrawPipelineLayout>(impl,
                                                     interlockMode,
                                                     layoutOptions);
        }
        m_drawPipelineLayout =
            impl->m_drawPipelineLayouts[pipelineLayoutIdx].get();

        // COLOR attachment.
        const VkImageLayout colorAttachmentLayout =
            (layoutOptions &
             DrawPipelineLayoutOptions::fixedFunctionColorOutput)
                ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                : VK_IMAGE_LAYOUT_GENERAL;
        const VkSampleCountFlagBits msaaSampleCount =
            interlockMode == gpu::InterlockMode::msaa ? VK_SAMPLE_COUNT_4_BIT
                                                      : VK_SAMPLE_COUNT_1_BIT;
        StackVector<VkAttachmentDescription, MAX_FRAMEBUFFER_ATTACHMENTS>
            attachments;
        StackVector<VkAttachmentReference, MAX_FRAMEBUFFER_ATTACHMENTS>
            colorAttachments;
        StackVector<VkAttachmentReference, MAX_FRAMEBUFFER_ATTACHMENTS>
            plsResolveAttachments;
        StackVector<VkAttachmentReference, 1> depthStencilAttachment;
        StackVector<VkAttachmentReference, MAX_FRAMEBUFFER_ATTACHMENTS>
            msaaResolveAttachments;
        assert(attachments.size() == COLOR_PLANE_IDX);
        assert(colorAttachments.size() == COLOR_PLANE_IDX);
        attachments.push_back({
            .format = renderTargetFormat,
            .samples = msaaSampleCount,
            .loadOp = vk_load_op(loadAction),
            .storeOp = (layoutOptions &
                        DrawPipelineLayoutOptions::coalescedResolveAndTransfer)
                           ? VK_ATTACHMENT_STORE_OP_DONT_CARE
                           : VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = loadAction == gpu::LoadAction::preserveRenderTarget
                                 ? colorAttachmentLayout
                                 : VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = colorAttachmentLayout,
        });
        colorAttachments.push_back({
            .attachment = COLOR_PLANE_IDX,
            .layout = colorAttachmentLayout,
        });

        if (interlockMode == gpu::InterlockMode::rasterOrdering ||
            interlockMode == gpu::InterlockMode::atomics)
        {
            // CLIP attachment.
            assert(attachments.size() == CLIP_PLANE_IDX);
            assert(colorAttachments.size() == CLIP_PLANE_IDX);
            attachments.push_back({
                // The clip buffer is encoded as RGBA8 in atomic mode so we can
                // block writes by emitting alpha=0.
                .format = interlockMode == gpu::InterlockMode::atomics
                              ? VK_FORMAT_R8G8B8A8_UNORM
                              : VK_FORMAT_R32_UINT,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
            });
            colorAttachments.push_back({
                .attachment = CLIP_PLANE_IDX,
                .layout = VK_IMAGE_LAYOUT_GENERAL,
            });
        }

        if (interlockMode == gpu::InterlockMode::rasterOrdering)
        {
            // SCRATCH_COLOR attachment.
            assert(attachments.size() == SCRATCH_COLOR_PLANE_IDX);
            assert(colorAttachments.size() == SCRATCH_COLOR_PLANE_IDX);
            attachments.push_back({
                .format = renderTargetFormat,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
            });
            colorAttachments.push_back({
                .attachment = SCRATCH_COLOR_PLANE_IDX,
                .layout = VK_IMAGE_LAYOUT_GENERAL,
            });

            // COVERAGE attachment.
            assert(attachments.size() == COVERAGE_PLANE_IDX);
            assert(colorAttachments.size() == COVERAGE_PLANE_IDX);
            attachments.push_back({
                .format = VK_FORMAT_R32_UINT,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
            });
            colorAttachments.push_back({
                .attachment = COVERAGE_PLANE_IDX,
                .layout = VK_IMAGE_LAYOUT_GENERAL,
            });
        }
        else if (interlockMode == gpu::InterlockMode::atomics)
        {
            if (layoutOptions &
                DrawPipelineLayoutOptions::coalescedResolveAndTransfer)
            {
                // COALESCED_ATOMIC_RESOLVE attachment (primary render target).
                assert(attachments.size() == COALESCED_ATOMIC_RESOLVE_IDX);
                attachments.push_back({
                    .format = renderTargetFormat,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    // This could sometimes be VK_IMAGE_LAYOUT_UNDEFINED, but it
                    // might not preserve the portion outside the renderArea
                    // when it isn't the full render target. Instead we rely on
                    // accessTargetImageView() to invalidate the target texture
                    // when we can.
                    .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                });

                // The resolve subpass only renders to the resolve texture.
                // And the "coalesced" resolve shader outputs to color
                // attachment 0, so alias the COALESCED_ATOMIC_RESOLVE
                // attachment on output 0 for this subpass.
                assert(plsResolveAttachments.size() == 0);
                plsResolveAttachments.push_back({
                    .attachment = COALESCED_ATOMIC_RESOLVE_IDX,
                    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                });
            }
            else
            {
                // When not in "coalesced" mode, the resolve texture is the
                // same as the COLOR texture.
                static_assert(COLOR_PLANE_IDX == 0);
                assert(plsResolveAttachments.size() == 0);
                plsResolveAttachments.push_back(colorAttachments[0]);
            }
        }
        else if (interlockMode == gpu::InterlockMode::msaa)
        {
            // DEPTH attachment.
            assert(attachments.size() == MSAA_DEPTH_STENCIL_IDX);
            attachments.push_back({
                .format = vkutil::get_preferred_depth_stencil_format(
                    m_vk->supportsD24S8()),
                .samples = msaaSampleCount,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            });
            depthStencilAttachment.push_back({
                .attachment = MSAA_DEPTH_STENCIL_IDX,
                .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            });

            // MSAA_RESOLVE attachment.
            assert(attachments.size() == MSAA_RESOLVE_IDX);
            attachments.push_back({
                .format = renderTargetFormat,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            });
            msaaResolveAttachments.push_back({
                .attachment = MSAA_RESOLVE_IDX,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            });
            assert(msaaResolveAttachments.size() == colorAttachments.size());
        }

        // Input attachments.
        StackVector<VkAttachmentReference, MAX_FRAMEBUFFER_ATTACHMENTS>
            inputAttachments;
        if (interlockMode != gpu::InterlockMode::clockwiseAtomic)
        {
            inputAttachments.push_back_n(colorAttachments.size(),
                                         colorAttachments.data());
            if (layoutOptions &
                DrawPipelineLayoutOptions::fixedFunctionColorOutput)
            {
                // COLOR is not an input attachment if we're using fixed
                // function blending.
                if (inputAttachments.size() > 1)
                {
                    inputAttachments[0] = {.attachment = VK_ATTACHMENT_UNUSED};
                }
                else
                {
                    inputAttachments.clear();
                }
            }
        }

        // Draw subpass.
        constexpr uint32_t MAX_SUBPASSES = 2;
        const bool rasterOrderedAttachmentAccess =
            interlockMode == gpu::InterlockMode::rasterOrdering &&
            m_vk->features.rasterizationOrderColorAttachmentAccess;
        StackVector<VkSubpassDescription, MAX_SUBPASSES> subpassDescs;
        StackVector<VkSubpassDependency, MAX_SUBPASSES> subpassDeps;
        assert(subpassDescs.size() == 0);
        assert(subpassDeps.size() == 0);
        assert(colorAttachments.size() ==
               m_drawPipelineLayout->colorAttachmentCount(0));
        assert(msaaResolveAttachments.size() == 0 ||
               msaaResolveAttachments.size() == colorAttachments.size());
        subpassDescs.push_back({
            .flags =
                rasterOrderedAttachmentAccess
                    ? VK_SUBPASS_DESCRIPTION_RASTERIZATION_ORDER_ATTACHMENT_COLOR_ACCESS_BIT_EXT
                    : 0u,
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = inputAttachments.size(),
            .pInputAttachments = inputAttachments.data(),
            .colorAttachmentCount = colorAttachments.size(),
            .pColorAttachments = colorAttachments.data(),
            .pResolveAttachments = msaaResolveAttachments.dataOrNull(),
            .pDepthStencilAttachment = depthStencilAttachment.dataOrNull(),
        });

        // Draw subpass self-dependencies.
        if ((interlockMode == gpu::InterlockMode::rasterOrdering &&
             !rasterOrderedAttachmentAccess) ||
            interlockMode == gpu::InterlockMode::atomics ||
            (interlockMode == gpu::InterlockMode::msaa &&
             !(layoutOptions &
               DrawPipelineLayoutOptions::fixedFunctionColorOutput)))
        {
            // Normally our dependencies are input attachments.
            //
            // In implicit rasterOrdering mode (meaning
            // EXT_rasterization_order_attachment_access is not present, but
            // we're on ARM hardware and know the hardware is raster ordered
            // anyway), we also need to declare this dependency even though
            // we won't be issuing any barriers.
            subpassDeps.push_back({
                .srcSubpass = 0,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                // TODO: We should add SHADER_READ/SHADER_WRITE flags for the
                // coverage buffer as well, but ironically, adding those causes
                // artifacts on Qualcomm. Leave them out for now unless we find
                // a case where we don't work without them.
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            });
        }
        else if (interlockMode == gpu::InterlockMode::clockwiseAtomic)
        {
            // clockwiseAtomic mode has a dependency when we switch from
            // borrowed coverage into forward.
            subpassDeps.push_back({
                .srcSubpass = 0,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            });
        }

        // PLS-resolve subpass (atomic mode only).
        if (interlockMode == gpu::InterlockMode::atomics)
        {
            // The resolve happens in a separate subpass.
            assert(subpassDescs.size() == 1);
            assert(plsResolveAttachments.size() ==
                   m_drawPipelineLayout->colorAttachmentCount(1));
            subpassDescs.push_back({
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .inputAttachmentCount = inputAttachments.size(),
                .pInputAttachments = inputAttachments.data(),
                .colorAttachmentCount = plsResolveAttachments.size(),
                .pColorAttachments = plsResolveAttachments.data(),
            });

            // The resolve subpass depends on the previous one, but not itself.
            subpassDeps.push_back({
                .srcSubpass = 0,
                .dstSubpass = 1,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                // TODO: We should add SHADER_READ/SHADER_WRITE flags for the
                // coverage buffer as well, but ironically, adding those causes
                // artifacts on Qualcomm. Leave them out for now unless we find
                // a case where we don't work without them.
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            });
        }

        VkRenderPassCreateInfo renderPassCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = attachments.size(),
            .pAttachments = attachments.data(),
            .subpassCount = subpassDescs.size(),
            .pSubpasses = subpassDescs.data(),
            .dependencyCount = subpassDeps.size(),
            .pDependencies = subpassDeps.data(),
        };

        VK_CHECK(m_vk->CreateRenderPass(m_vk->device,
                                        &renderPassCreateInfo,
                                        nullptr,
                                        &m_renderPass));
    }

    ~RenderPass()
    {
        // Don't touch m_drawPipelineLayout in the destructor since destruction
        // order of us vs. impl->m_drawPipelineLayouts is uncertain.
        m_vk->DestroyRenderPass(m_vk->device, m_renderPass, nullptr);
    }

    const DrawPipelineLayout* drawPipelineLayout() const
    {
        return m_drawPipelineLayout;
    }

    operator VkRenderPass() const { return m_renderPass; }

private:
    const rcp<VulkanContext> m_vk;
    // Raw pointer into impl->m_drawPipelineLayouts. RenderContextVulkanImpl
    // ensures the pipline layouts outlive this RenderPass instance.
    const DrawPipelineLayout* m_drawPipelineLayout;
    VkRenderPass m_renderPass;
};

// Pipeline options that don't affect the shader.
enum class DrawPipelineOptions
{
    none = 0,
    wireframe = 1 << 0,
};
constexpr static int DRAW_PIPELINE_OPTION_COUNT = 1;
RIVE_MAKE_ENUM_BITSET(DrawPipelineOptions);

// VkPipeline wrapper for Rive draw calls.
class RenderContextVulkanImpl::DrawPipeline
{
public:
    static uint64_t Key(DrawType drawType,
                        ShaderFeatures shaderFeatures,
                        InterlockMode interlockMode,
                        ShaderMiscFlags shaderMiscFlags,
                        DrawPipelineOptions drawPipelineOptions,
                        uint32_t renderPassKey,
                        uint32_t pipelineStateKey)
    {
        uint64_t key = gpu::ShaderUniqueKey(drawType,
                                            shaderFeatures,
                                            interlockMode,
                                            shaderMiscFlags);

        assert(key << DRAW_PIPELINE_OPTION_COUNT >>
                   DRAW_PIPELINE_OPTION_COUNT ==
               key);
        key = (key << DRAW_PIPELINE_OPTION_COUNT) |
              static_cast<uint32_t>(drawPipelineOptions);

        assert(key << gpu::PipelineState::UNIQUE_KEY_BIT_COUNT >>
                   gpu::PipelineState::UNIQUE_KEY_BIT_COUNT ==
               key);
        assert(pipelineStateKey <
               1 << gpu::PipelineState::UNIQUE_KEY_BIT_COUNT);
        key = (key << gpu::PipelineState::UNIQUE_KEY_BIT_COUNT) |
              pipelineStateKey;

        assert(key << RenderPass::KEY_BIT_COUNT >> RenderPass::KEY_BIT_COUNT ==
               key);
        assert(renderPassKey < 1 << RenderPass::KEY_BIT_COUNT);
        key = (key << RenderPass::KEY_BIT_COUNT) | renderPassKey;

        return key;
    }

    DrawPipeline(RenderContextVulkanImpl* impl,
                 gpu::DrawType drawType,
                 const DrawPipelineLayout& pipelineLayout,
                 gpu::ShaderFeatures shaderFeatures,
                 gpu::ShaderMiscFlags shaderMiscFlags,
                 DrawPipelineOptions drawPipelineOptions,
                 const gpu::PipelineState& pipelineState,
                 VkRenderPass vkRenderPass) :
        m_vk(ref_rcp(impl->vulkanContext()))
    {
        gpu::InterlockMode interlockMode = pipelineLayout.interlockMode();
        uint32_t shaderKey = gpu::ShaderUniqueKey(drawType,
                                                  shaderFeatures,
                                                  interlockMode,
                                                  shaderMiscFlags);
        const uint32_t subpassIndex =
            drawType == gpu::DrawType::atomicResolve ? 1u : 0u;

        std::unique_ptr<DrawShaderVulkan>& drawShader =
            impl->m_drawShaders[shaderKey];
        if (drawShader == nullptr)
        {
            drawShader = std::make_unique<DrawShaderVulkan>(m_vk.get(),
                                                            drawType,
                                                            interlockMode,
                                                            shaderFeatures,
                                                            shaderMiscFlags);
        }

        uint32_t shaderPermutationFlags[SPECIALIZATION_COUNT] = {
            shaderFeatures & gpu::ShaderFeatures::ENABLE_CLIPPING,
            shaderFeatures & gpu::ShaderFeatures::ENABLE_CLIP_RECT,
            shaderFeatures & gpu::ShaderFeatures::ENABLE_ADVANCED_BLEND,
            shaderFeatures & gpu::ShaderFeatures::ENABLE_FEATHER,
            shaderFeatures & gpu::ShaderFeatures::ENABLE_EVEN_ODD,
            shaderFeatures & gpu::ShaderFeatures::ENABLE_NESTED_CLIPPING,
            shaderFeatures & gpu::ShaderFeatures::ENABLE_HSL_BLEND_MODES,
            shaderMiscFlags & gpu::ShaderMiscFlags::clockwiseFill,
            shaderMiscFlags & gpu::ShaderMiscFlags::borrowedCoveragePrepass,
            impl->m_vendorID,
        };
        static_assert(CLIPPING_SPECIALIZATION_IDX == 0);
        static_assert(CLIP_RECT_SPECIALIZATION_IDX == 1);
        static_assert(ADVANCED_BLEND_SPECIALIZATION_IDX == 2);
        static_assert(FEATHER_SPECIALIZATION_IDX == 3);
        static_assert(EVEN_ODD_SPECIALIZATION_IDX == 4);
        static_assert(NESTED_CLIPPING_SPECIALIZATION_IDX == 5);
        static_assert(HSL_BLEND_MODES_SPECIALIZATION_IDX == 6);
        static_assert(CLOCKWISE_FILL_SPECIALIZATION_IDX == 7);
        static_assert(BORROWED_COVERAGE_PREPASS_SPECIALIZATION_IDX == 8);
        static_assert(VULKAN_VENDOR_ID_SPECIALIZATION_IDX == 9);
        static_assert(SPECIALIZATION_COUNT == 10);

        VkSpecializationMapEntry permutationMapEntries[SPECIALIZATION_COUNT];
        for (uint32_t i = 0; i < SPECIALIZATION_COUNT; ++i)
        {
            permutationMapEntries[i] = {
                .constantID = i,
                .offset = i * static_cast<uint32_t>(sizeof(uint32_t)),
                .size = sizeof(uint32_t),
            };
        }

        VkSpecializationInfo specializationInfo = {
            .mapEntryCount = SPECIALIZATION_COUNT,
            .pMapEntries = permutationMapEntries,
            .dataSize = sizeof(shaderPermutationFlags),
            .pData = &shaderPermutationFlags,
        };

        VkPipelineShaderStageCreateInfo stages[] = {
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = drawShader->vertexModule(),
                .pName = "main",
                .pSpecializationInfo = &specializationInfo,
            },
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = drawShader->fragmentModule(),
                .pName = "main",
                .pSpecializationInfo = &specializationInfo,
            },
        };

        VkPipelineRasterizationStateCreateInfo
            pipelineRasterizationStateCreateInfo = {
                .sType =
                    VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .polygonMode =
                    (drawPipelineOptions & DrawPipelineOptions::wireframe)
                        ? VK_POLYGON_MODE_LINE
                        : VK_POLYGON_MODE_FILL,
                .cullMode = vk_cull_mode(pipelineState.cullFace),
                .frontFace = VK_FRONT_FACE_CLOCKWISE,
                .lineWidth = 1.0,
            };

        VkPipelineColorBlendAttachmentState blendState;
        if (interlockMode == gpu::InterlockMode::rasterOrdering ||
            (shaderMiscFlags &
             gpu::ShaderMiscFlags::coalescedResolveAndTransfer))
        {
            blendState = {
                .blendEnable = VK_FALSE,
                .colorWriteMask = vkutil::kColorWriteMaskRGBA,
            };
        }
        else if (shaderMiscFlags &
                 gpu::ShaderMiscFlags::borrowedCoveragePrepass)
        {
            // Borrowed coverage draws only update the coverage buffer.
            blendState = {
                .blendEnable = VK_FALSE,
                .colorWriteMask = vkutil::kColorWriteMaskNone,
            };
        }
        else
        {
            // Atomic mode blends the color and clip both as independent RGBA8
            // values.
            //
            // Advanced blend modes are handled by rearranging the math
            // such that the correct color isn't reached until *AFTER* this
            // blend state is applied.
            //
            // This allows us to preserve clip and color contents by just
            // emitting a=0 instead of loading the current value. It also saves
            // flops by offloading the blending work onto the ROP blending
            // unit, and serves as a hint to the hardware that it doesn't need
            // to read or write anything when a == 0.
            blendState = {
                .blendEnable = VK_TRUE,
                // Image draws use premultiplied src-over so they can blend
                // the image with the previous paint together, out of order.
                // (Premultiplied src-over is associative.)
                //
                // Otherwise we save 3 flops and let the ROP multiply alpha
                // into the color when it does the blending.
                .srcColorBlendFactor =
                    (interlockMode == gpu::InterlockMode::atomics &&
                     gpu::DrawTypeIsImageDraw(drawType)) ||
                            interlockMode == gpu::InterlockMode::msaa
                        ? VK_BLEND_FACTOR_ONE
                        : VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp = vk_blend_op(pipelineState.blendEquation),
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .alphaBlendOp = vk_blend_op(pipelineState.blendEquation),
                .colorWriteMask = pipelineState.colorWriteEnabled
                                      ? vkutil::kColorWriteMaskRGBA
                                      : vkutil::kColorWriteMaskNone,
            };
        }

        StackVector<VkPipelineColorBlendAttachmentState,
                    MAX_FRAMEBUFFER_ATTACHMENTS>
            blendStates;
        blendStates.push_back_n(
            pipelineLayout.colorAttachmentCount(subpassIndex),
            blendState);
        VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo =
            {
                .sType =
                    VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .attachmentCount = blendStates.size(),
                .pAttachments = blendStates.data(),
            };

        if (interlockMode == gpu::InterlockMode::rasterOrdering &&
            m_vk->features.rasterizationOrderColorAttachmentAccess)
        {
            pipelineColorBlendStateCreateInfo.flags |=
                VK_PIPELINE_COLOR_BLEND_STATE_CREATE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_BIT_EXT;
        }

        VkPipelineDepthStencilStateCreateInfo depthStencilState = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = pipelineState.depthTestEnabled,
            .depthWriteEnable = pipelineState.depthWriteEnabled,
            .depthCompareOp = VK_COMPARE_OP_LESS,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = pipelineState.stencilTestEnabled,
            .minDepthBounds = gpu::DEPTH_MIN,
            .maxDepthBounds = gpu::DEPTH_MAX,
        };
        if (pipelineState.stencilTestEnabled)
        {
            depthStencilState.front = {
                .failOp = vk_stencil_op(pipelineState.stencilFrontOps.failOp),
                .passOp = vk_stencil_op(pipelineState.stencilFrontOps.passOp),
                .depthFailOp =
                    vk_stencil_op(pipelineState.stencilFrontOps.depthFailOp),
                .compareOp =
                    vk_compare_op(pipelineState.stencilFrontOps.compareOp),
                .compareMask = pipelineState.stencilCompareMask,
                .writeMask = pipelineState.stencilWriteMask,
                .reference = pipelineState.stencilReference,
            };
            depthStencilState.back =
                !pipelineState.stencilDoubleSided
                    ? depthStencilState.front
                    : VkStencilOpState{
                          .failOp = vk_stencil_op(
                              pipelineState.stencilBackOps.failOp),
                          .passOp = vk_stencil_op(
                              pipelineState.stencilBackOps.passOp),
                          .depthFailOp = vk_stencil_op(
                              pipelineState.stencilBackOps.depthFailOp),
                          .compareOp = vk_compare_op(
                              pipelineState.stencilBackOps.compareOp),
                          .compareMask = pipelineState.stencilCompareMask,
                          .writeMask = pipelineState.stencilWriteMask,
                          .reference = pipelineState.stencilReference,
                      };
        }

        VkPipelineMultisampleStateCreateInfo msaaState = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = interlockMode == gpu::InterlockMode::msaa
                                        ? VK_SAMPLE_COUNT_4_BIT
                                        : VK_SAMPLE_COUNT_1_BIT,
        };

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = stages,
            .pViewportState = &layout::SINGLE_VIEWPORT,
            .pRasterizationState = &pipelineRasterizationStateCreateInfo,
            .pMultisampleState = &msaaState,
            .pDepthStencilState = interlockMode == gpu::InterlockMode::msaa
                                      ? &depthStencilState
                                      : nullptr,
            .pColorBlendState = &pipelineColorBlendStateCreateInfo,
            .pDynamicState = &layout::DYNAMIC_VIEWPORT_SCISSOR,
            .layout = *pipelineLayout,
            .renderPass = vkRenderPass,
            .subpass = subpassIndex,
        };

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
                pipelineCreateInfo.pVertexInputState =
                    &layout::PATH_VERTEX_INPUT_STATE;
                pipelineCreateInfo.pInputAssemblyState =
                    &layout::INPUT_ASSEMBLY_TRIANGLE_LIST;
                break;

            case DrawType::msaaStencilClipReset:
            case DrawType::interiorTriangulation:
            case DrawType::atlasBlit:
                pipelineCreateInfo.pVertexInputState =
                    &layout::INTERIOR_TRI_VERTEX_INPUT_STATE;
                pipelineCreateInfo.pInputAssemblyState =
                    &layout::INPUT_ASSEMBLY_TRIANGLE_LIST;
                break;

            case DrawType::imageRect:
                pipelineCreateInfo.pVertexInputState =
                    &layout::IMAGE_RECT_VERTEX_INPUT_STATE;
                pipelineCreateInfo.pInputAssemblyState =
                    &layout::INPUT_ASSEMBLY_TRIANGLE_LIST;
                break;

            case DrawType::imageMesh:
                pipelineCreateInfo.pVertexInputState =
                    &layout::IMAGE_MESH_VERTEX_INPUT_STATE;
                pipelineCreateInfo.pInputAssemblyState =
                    &layout::INPUT_ASSEMBLY_TRIANGLE_LIST;
                break;

            case DrawType::atomicResolve:
                pipelineCreateInfo.pVertexInputState =
                    &layout::EMPTY_VERTEX_INPUT_STATE;
                pipelineCreateInfo.pInputAssemblyState =
                    &layout::INPUT_ASSEMBLY_TRIANGLE_STRIP;
                break;

            case DrawType::atomicInitialize:
                RIVE_UNREACHABLE();
        }

        VK_CHECK(m_vk->CreateGraphicsPipelines(m_vk->device,
                                               VK_NULL_HANDLE,
                                               1,
                                               &pipelineCreateInfo,
                                               nullptr,
                                               &m_vkPipeline));
    }

    ~DrawPipeline()
    {
        m_vk->DestroyPipeline(m_vk->device, m_vkPipeline, nullptr);
    }

    operator VkPipeline() const { return m_vkPipeline; }

private:
    const rcp<VulkanContext> m_vk;
    VkPipeline m_vkPipeline;
};

RenderContextVulkanImpl::RenderContextVulkanImpl(
    rcp<VulkanContext> vk,
    const VkPhysicalDeviceProperties& physicalDeviceProps) :
    m_vk(std::move(vk)),
    m_vendorID(physicalDeviceProps.vendorID),
    m_atlasFormat(m_vk->isFormatSupportedWithFeatureFlags(
                      VK_FORMAT_R32_SFLOAT,
                      VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT)
                      ? VK_FORMAT_R32_SFLOAT
                      : VK_FORMAT_R16_SFLOAT),
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
    m_platformFeatures.supportsRasterOrdering =
        m_vk->features.rasterizationOrderColorAttachmentAccess;
    m_platformFeatures.supportsFragmentShaderAtomics =
        m_vk->features.fragmentStoresAndAtomics;
    m_platformFeatures.supportsClockwiseAtomicRendering =
        m_vk->features.fragmentStoresAndAtomics;
    m_platformFeatures.supportsClipPlanes =
        m_vk->features.shaderClipDistance &&
        // The Vulkan spec mandates that the minimum value for maxClipDistances
        // is 8, but we might as well make this >= 4 check to be more clear
        // about how we're using it.
        physicalDeviceProps.limits.maxClipDistances >= 4;
    m_platformFeatures.clipSpaceBottomUp = false;
    m_platformFeatures.framebufferBottomUp = false;
    m_platformFeatures.maxCoverageBufferLength =
        std::min(physicalDeviceProps.limits.maxStorageBufferRange, 1u << 28) /
        sizeof(uint32_t);

    switch (m_vendorID)
    {
        case VULKAN_VENDOR_QUALCOMM:
            // Qualcomm advertises EXT_rasterization_order_attachment_access,
            // but it's slow. Use atomics instead on this platform.
            m_platformFeatures.supportsRasterOrdering = false;
            // Pixel4 struggles with fine-grained fp16 path IDs.
            m_platformFeatures.pathIDGranularity = 2;
            break;

        case VULKAN_VENDOR_ARM:
            // This is undocumented, but raster ordering always works on ARM
            // Mali GPUs if you define a subpass dependency, even without
            // EXT_rasterization_order_attachment_access.
            m_platformFeatures.supportsRasterOrdering = true;
            break;
    }
}

static VkFilter vk_filter(rive::ImageFilter option)
{
    switch (option)
    {
        case rive::ImageFilter::trilinear:
            return VK_FILTER_LINEAR;
        case rive::ImageFilter::nearest:
            return VK_FILTER_NEAREST;
    }

    RIVE_UNREACHABLE();
}

static VkSamplerMipmapMode vk_sampler_mipmap_mode(rive::ImageFilter option)
{
    switch (option)
    {
        case rive::ImageFilter::trilinear:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        case rive::ImageFilter::nearest:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    }

    RIVE_UNREACHABLE();
}

static VkSamplerAddressMode vk_sampler_address_mode(rive::ImageWrap option)
{
    switch (option)
    {
        case rive::ImageWrap::clamp:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case rive::ImageWrap::repeat:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case rive::ImageWrap::mirror:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    }

    RIVE_UNREACHABLE();
}

void RenderContextVulkanImpl::initGPUObjects()
{
    // Create the immutable samplers.
    VkSamplerCreateInfo linearSamplerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .minLod = 0,
        .maxLod = 0,
    };

    VK_CHECK(m_vk->CreateSampler(m_vk->device,
                                 &linearSamplerCreateInfo,
                                 nullptr,
                                 &m_linearSampler));

    for (size_t i = 0; i < ImageSampler::MAX_SAMPLER_PERMUTATIONS; ++i)
    {
        ImageWrap wrapX = ImageSampler::GetWrapXOptionFromKey(i);
        ImageWrap wrapY = ImageSampler::GetWrapYOptionFromKey(i);
        ImageFilter filter = ImageSampler::GetFilterOptionFromKey(i);
        VkFilter minMagFilter = vk_filter(filter);

        VkSamplerCreateInfo samplerCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = minMagFilter,
            .minFilter = minMagFilter,
            .mipmapMode = vk_sampler_mipmap_mode(filter),
            .addressModeU = vk_sampler_address_mode(wrapX),
            .addressModeV = vk_sampler_address_mode(wrapY),
            .minLod = 0,
            .maxLod = VK_LOD_CLAMP_NONE,
        };

        VK_CHECK(m_vk->CreateSampler(m_vk->device,
                                     &samplerCreateInfo,
                                     nullptr,
                                     m_imageSamplers + i));
    }

    // Bound when there is not an image paint.
    constexpr static uint8_t black[] = {0, 0, 0, 1};
    m_nullImageTexture = m_vk->makeTexture2D({
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = {1, 1},
    });
    m_nullImageTexture->scheduleUpload(black, sizeof(black));

    // All pipelines share the same perFlush bindings.
    VkDescriptorSetLayoutBinding perFlushLayoutBindings[] = {
        {
            .binding = FLUSH_UNIFORM_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags =
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = IMAGE_DRAW_UNIFORM_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .descriptorCount = 1,
            .stageFlags =
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = PATH_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        },
        {
            .binding = PAINT_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags =
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = PAINT_AUX_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags =
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = CONTOUR_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        },
        {
            .binding = COVERAGE_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = TESS_VERTEX_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        },
        {
            .binding = GRAD_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = FEATHER_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1,
            .stageFlags =
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = ATLAS_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };

    VkDescriptorSetLayoutCreateInfo perFlushLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = std::size(perFlushLayoutBindings),
        .pBindings = perFlushLayoutBindings,
    };

    VK_CHECK(m_vk->CreateDescriptorSetLayout(m_vk->device,
                                             &perFlushLayoutInfo,
                                             nullptr,
                                             &m_perFlushDescriptorSetLayout));

    // The imageTexture gets updated with every draw that uses it.
    VkDescriptorSetLayoutBinding perDrawLayoutBindings[] = {
        {
            .binding = IMAGE_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = IMAGE_SAMPLER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };

    VkDescriptorSetLayoutCreateInfo perDrawLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = std::size(perDrawLayoutBindings),
        .pBindings = perDrawLayoutBindings,
    };

    VK_CHECK(m_vk->CreateDescriptorSetLayout(m_vk->device,
                                             &perDrawLayoutInfo,
                                             nullptr,
                                             &m_perDrawDescriptorSetLayout));

    // Every shader uses the same immutable sampler layout.
    VkDescriptorSetLayoutBinding immutableSamplerLayoutBindings[] = {
        {
            .binding = GRAD_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = &m_linearSampler,
        },
        {
            .binding = FEATHER_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags =
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = &m_linearSampler,
        },
        {
            .binding = ATLAS_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = &m_linearSampler,
        },
    };

    VkDescriptorSetLayoutCreateInfo immutableSamplerLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = std::size(immutableSamplerLayoutBindings),
        .pBindings = immutableSamplerLayoutBindings,
    };

    VK_CHECK(m_vk->CreateDescriptorSetLayout(
        m_vk->device,
        &immutableSamplerLayoutInfo,
        nullptr,
        &m_immutableSamplerDescriptorSetLayout));

    // For when a set isn't used at all by a shader.
    VkDescriptorSetLayoutCreateInfo emptyLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 0,
    };

    VK_CHECK(m_vk->CreateDescriptorSetLayout(m_vk->device,
                                             &emptyLayoutInfo,
                                             nullptr,
                                             &m_emptyDescriptorSetLayout));

    // Create static descriptor sets.
    VkDescriptorPoolSize staticDescriptorPoolSizes[] = {
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1, // m_nullImageTexture
        },
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 4, // grad, feather, atlas, image samplers
        },
    };

    VkDescriptorPoolCreateInfo staticDescriptorPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 2,
        .poolSizeCount = std::size(staticDescriptorPoolSizes),
        .pPoolSizes = staticDescriptorPoolSizes,
    };

    VK_CHECK(m_vk->CreateDescriptorPool(m_vk->device,
                                        &staticDescriptorPoolCreateInfo,
                                        nullptr,
                                        &m_staticDescriptorPool));

    // Create a descriptor set to bind m_nullImageTexture when there is no image
    // paint.
    VkDescriptorSetAllocateInfo nullImageDescriptorSetInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_staticDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_perDrawDescriptorSetLayout,
    };

    VK_CHECK(m_vk->AllocateDescriptorSets(m_vk->device,
                                          &nullImageDescriptorSetInfo,
                                          &m_nullImageDescriptorSet));

    m_vk->updateImageDescriptorSets(
        m_nullImageDescriptorSet,
        {
            .dstBinding = IMAGE_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        },
        {{
            .imageView = m_nullImageTexture->vkImageView(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        }});

    m_vk->updateImageDescriptorSets(
        m_nullImageDescriptorSet,
        {
            .dstBinding = IMAGE_SAMPLER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        },
        {{
            .sampler = m_imageSamplers[ImageSampler::LINEAR_CLAMP_SAMPLER_KEY],
        }});

    // Create an empty descriptor set for IMMUTABLE_SAMPLER_BINDINGS_SET. Vulkan
    // requires this even though the samplers are all immutable.
    VkDescriptorSetAllocateInfo immutableSamplerDescriptorSetInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_staticDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_immutableSamplerDescriptorSetLayout,
    };

    VK_CHECK(m_vk->AllocateDescriptorSets(m_vk->device,
                                          &immutableSamplerDescriptorSetInfo,
                                          &m_immutableSamplerDescriptorSet));

    // The pipelines reference our vulkan objects. Delete them first.
    m_colorRampPipeline = std::make_unique<ColorRampPipeline>(this);
    m_tessellatePipeline = std::make_unique<TessellatePipeline>(this);
    m_atlasPipeline = std::make_unique<AtlasPipeline>(this);

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

    m_vk->DestroyDescriptorPool(m_vk->device, m_staticDescriptorPool, nullptr);
    m_vk->DestroyDescriptorSetLayout(m_vk->device,
                                     m_perFlushDescriptorSetLayout,
                                     nullptr);
    m_vk->DestroyDescriptorSetLayout(m_vk->device,
                                     m_perDrawDescriptorSetLayout,
                                     nullptr);
    m_vk->DestroyDescriptorSetLayout(m_vk->device,
                                     m_immutableSamplerDescriptorSetLayout,
                                     nullptr);
    m_vk->DestroyDescriptorSetLayout(m_vk->device,
                                     m_emptyDescriptorSetLayout,
                                     nullptr);
    for (VkSampler sampler : m_imageSamplers)
    {
        m_vk->DestroySampler(m_vk->device, sampler, nullptr);
    }
    m_vk->DestroySampler(m_vk->device, m_linearSampler, nullptr);
}

void RenderContextVulkanImpl::resizeGradientTexture(uint32_t width,
                                                    uint32_t height)
{
    width = std::max(width, 1u);
    height = std::max(height, 1u);
    if (m_gradTexture == nullptr || m_gradTexture->width() != width ||
        m_gradTexture->height() != height)
    {
        m_gradTexture = m_vk->makeTexture2D({
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .extent = {width, height},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_SAMPLED_BIT,
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
}

void RenderContextVulkanImpl::resizeTessellationTexture(uint32_t width,
                                                        uint32_t height)
{
    width = std::max(width, 1u);
    height = std::max(height, 1u);
    if (m_tessTexture == nullptr || m_tessTexture->width() != width ||
        m_tessTexture->height() != height)
    {
        m_tessTexture = m_vk->makeTexture2D({
            .format = VK_FORMAT_R32G32B32A32_UINT,
            .extent = {width, height},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_SAMPLED_BIT,
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
}

void RenderContextVulkanImpl::resizeAtlasTexture(uint32_t width,
                                                 uint32_t height)
{
    width = std::max(width, 1u);
    height = std::max(height, 1u);
    if (m_atlasTexture == nullptr || m_atlasTexture->width() != width ||
        m_atlasTexture->height() != height)
    {
        m_atlasTexture = m_vk->makeTexture2D({
            .format = m_atlasFormat,
            .extent = {width, height},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_SAMPLED_BIT,
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
    1; // coverageAtomicTexture in atomic mode.
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

    if (desc.renderTargetUpdateBounds.empty())
    {
        return;
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
        descriptorSetPool->allocateDescriptorSet(m_perFlushDescriptorSetLayout);

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
        {
            {
                .buffer = *m_paintBuffer,
                .offset = desc.firstPaint * sizeof(gpu::PaintData),
                .range = VK_WHOLE_SIZE,
            },
            {
                .buffer = *m_paintAuxBuffer,
                .offset = desc.firstPaintAux * sizeof(gpu::PaintAuxData),
                .range = VK_WHOLE_SIZE,
            },
        });
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
                                    &m_immutableSamplerDescriptorSet,
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
                                    &m_immutableSamplerDescriptorSet,
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

    auto* renderTarget = static_cast<RenderTargetVulkan*>(desc.renderTarget);

    vkutil::Texture2D *clipTexture, *scratchColorTexture, *coverageTexture;
    if (desc.interlockMode == gpu::InterlockMode::rasterOrdering)
    {
        clipTexture = renderTarget->clipTextureR32UI();
        scratchColorTexture = renderTarget->scratchColorTexture();
        coverageTexture = renderTarget->coverageTexture();
    }
    else if (desc.interlockMode == gpu::InterlockMode::atomics)
    {
        clipTexture = renderTarget->clipTextureRGBA8();
        scratchColorTexture = nullptr;
        coverageTexture = renderTarget->coverageAtomicTexture();
    }

    const bool fixedFunctionColorOutput =
        desc.atomicFixedFunctionColorOutput ||
        // TODO: atomicFixedFunctionColorOutput could be generalized for MSAA as
        // well, if another backend starts needing this logic.
        (desc.interlockMode == gpu::InterlockMode::msaa &&
         !(desc.combinedShaderFeatures &
           gpu::ShaderFeatures::ENABLE_ADVANCED_BLEND));

    // Ensure any previous accesses to the color texture complete before we
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

    const bool renderAreaIsFullTarget = desc.renderTargetUpdateBounds.contains(
        IAABB{0,
              0,
              static_cast<int32_t>(renderTarget->width()),
              static_cast<int32_t>(renderTarget->height())});

    const vkutil::ImageAccessAction targetAccessAction =
        renderAreaIsFullTarget &&
                desc.colorLoadAction != gpu::LoadAction::preserveRenderTarget
            ? vkutil::ImageAccessAction::invalidateContents
            : vkutil::ImageAccessAction::preserveContents;

    VkImageView colorImageView = VK_NULL_HANDLE;
    bool colorAttachmentIsOffscreen = false;
    if (desc.interlockMode == gpu::InterlockMode::msaa)
    {
        renderTarget->msaaColorTexture()->barrier(commandBuffer,
                                                  colorLoadAccess,
                                                  targetAccessAction);
        colorImageView = renderTarget->msaaColorTexture()->vkImageView();
    }
    else if (fixedFunctionColorOutput || (renderTarget->targetUsageFlags() &
                                          VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT))
    {
        colorImageView =
            renderTarget->accessTargetImageView(commandBuffer,
                                                colorLoadAccess,
                                                targetAccessAction);
    }
    else if (desc.colorLoadAction != gpu::LoadAction::preserveRenderTarget)
    {
        colorImageView = renderTarget
                             ->accessOffscreenColorTexture(
                                 commandBuffer,
                                 colorLoadAccess,
                                 vkutil::ImageAccessAction::invalidateContents)
                             ->vkImageView();
        colorAttachmentIsOffscreen = true;
    }
    else
    {
        // Preserve the target texture by blitting its contents into our
        // offscreen color texture.
        m_vk->blitSubRect(
            commandBuffer,
            renderTarget->accessTargetImage(
                commandBuffer,
                {
                    .pipelineStages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                    .accessMask = VK_ACCESS_TRANSFER_READ_BIT,
                    .layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                }),
            renderTarget
                ->accessOffscreenColorTexture(
                    commandBuffer,
                    {
                        .pipelineStages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                        .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                        .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    },
                    vkutil::ImageAccessAction::invalidateContents)
                ->vkImage(),
            desc.renderTargetUpdateBounds);
        colorImageView =
            renderTarget
                ->accessOffscreenColorTexture(commandBuffer, colorLoadAccess)
                ->vkImageView();
        colorAttachmentIsOffscreen = true;
    }

    auto pipelineLayoutOptions = DrawPipelineLayoutOptions::none;
    if (fixedFunctionColorOutput)
    {
        pipelineLayoutOptions |=
            DrawPipelineLayoutOptions::fixedFunctionColorOutput;
    }
    else if (desc.interlockMode == gpu::InterlockMode::atomics &&
             colorAttachmentIsOffscreen)
    {
        pipelineLayoutOptions |=
            DrawPipelineLayoutOptions::coalescedResolveAndTransfer;
    }

    const uint32_t renderPassKey =
        RenderPass::Key(desc.interlockMode,
                        pipelineLayoutOptions,
                        renderTarget->framebufferFormat(),
                        desc.colorLoadAction);
    std::unique_ptr<RenderPass>& renderPass = m_renderPasses[renderPassKey];
    if (renderPass == nullptr)
    {
        renderPass =
            std::make_unique<RenderPass>(this,
                                         desc.interlockMode,
                                         pipelineLayoutOptions,
                                         renderTarget->framebufferFormat(),
                                         desc.colorLoadAction);
    }
    const DrawPipelineLayout& pipelineLayout =
        *renderPass->drawPipelineLayout();

    // Create the framebuffer.
    StackVector<VkImageView, MAX_FRAMEBUFFER_ATTACHMENTS> framebufferViews;
    StackVector<VkClearValue, MAX_FRAMEBUFFER_ATTACHMENTS> clearValues;
    assert(framebufferViews.size() == COLOR_PLANE_IDX);
    framebufferViews.push_back(colorImageView);
    clearValues.push_back(
        {.color = vkutil::color_clear_rgba32f(desc.colorClearValue)});
    if (desc.interlockMode == gpu::InterlockMode::rasterOrdering)
    {
        assert(framebufferViews.size() == CLIP_PLANE_IDX);
        framebufferViews.push_back(clipTexture->vkImageView());
        clearValues.push_back({});

        assert(framebufferViews.size() == SCRATCH_COLOR_PLANE_IDX);
        framebufferViews.push_back(scratchColorTexture->vkImageView());
        clearValues.push_back({});

        assert(framebufferViews.size() == COVERAGE_PLANE_IDX);
        framebufferViews.push_back(coverageTexture->vkImageView());
        clearValues.push_back(
            {.color = vkutil::color_clear_r32ui(desc.coverageClearValue)});
    }
    else if (desc.interlockMode == gpu::InterlockMode::atomics)
    {
        assert(framebufferViews.size() == CLIP_PLANE_IDX);
        framebufferViews.push_back(clipTexture->vkImageView());
        clearValues.push_back({});

        if (pipelineLayout.options() &
            DrawPipelineLayoutOptions::coalescedResolveAndTransfer)
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
        framebufferViews.push_back(renderTarget->accessTargetImageView(
            commandBuffer,
            {
                .pipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .accessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
            targetAccessAction));
        clearValues.push_back({});
    }

    rcp<vkutil::Framebuffer> framebuffer = m_vk->makeFramebuffer({
        .renderPass = *renderPass,
        .attachmentCount = framebufferViews.size(),
        .pAttachments = framebufferViews.data(),
        .width = static_cast<uint32_t>(renderTarget->width()),
        .height = static_cast<uint32_t>(renderTarget->height()),
        .layers = 1,
    });

    VkRect2D renderArea = {
        .offset = {desc.renderTargetUpdateBounds.left,
                   desc.renderTargetUpdateBounds.top},
        .extent = {static_cast<uint32_t>(desc.renderTargetUpdateBounds.width()),
                   static_cast<uint32_t>(
                       desc.renderTargetUpdateBounds.height())},
    };

    if (desc.interlockMode == gpu::InterlockMode::atomics)
    {
        // Clear the coverage texture, which is not an attachment.
        VkImageSubresourceRange clearRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
        };

        // Don't clear the coverage texture until shaders in the previous flush
        // have finished using it.
        m_vk->imageMemoryBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            {
                .srcAccessMask =
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .image = renderTarget->coverageAtomicTexture()->vkImage(),
            });

        const VkClearColorValue coverageClearValue =
            vkutil::color_clear_r32ui(desc.coverageClearValue);
        m_vk->CmdClearColorImage(
            commandBuffer,
            renderTarget->coverageAtomicTexture()->vkImage(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            &coverageClearValue,
            1,
            &clearRange);

        // Don't use the coverage texture in shaders until the clear finishes.
        m_vk->imageMemoryBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            {
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask =
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                .image = renderTarget->coverageAtomicTexture()->vkImage(),
            });
    }
    else if (desc.interlockMode == gpu::InterlockMode::clockwiseAtomic)
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

    // Ensure all reads to any internal attachments complete before we execute
    // the load operations.
    m_vk->memoryBarrier(
        commandBuffer,
        // "Load" operations always occur in
        // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT.
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        {
            .srcAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        });

    assert(clearValues.size() == framebufferViews.size());
    VkRenderPassBeginInfo renderPassBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = *renderPass,
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
        inputAttachmentDescriptorSet = descriptorSetPool->allocateDescriptorSet(
            pipelineLayout.plsLayout());

        if (!fixedFunctionColorOutput)
        {
            m_vk->updateImageDescriptorSets(
                inputAttachmentDescriptorSet,
                {
                    .dstBinding = COLOR_PLANE_IDX,
                    .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                },
                {{
                    .imageView = colorImageView,
                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                }});
        }

        if (desc.interlockMode != gpu::InterlockMode::msaa)
        {
            m_vk->updateImageDescriptorSets(
                inputAttachmentDescriptorSet,
                {
                    .dstBinding = CLIP_PLANE_IDX,
                    .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                },
                {{
                    .imageView = clipTexture->vkImageView(),
                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                }});

            if (desc.interlockMode == gpu::InterlockMode::rasterOrdering)
            {
                m_vk->updateImageDescriptorSets(
                    inputAttachmentDescriptorSet,
                    {
                        .dstBinding = SCRATCH_COLOR_PLANE_IDX,
                        .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                    },
                    {{
                        .imageView = scratchColorTexture->vkImageView(),
                        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                    }});
            }

            assert(coverageTexture != nullptr);
            m_vk->updateImageDescriptorSets(
                inputAttachmentDescriptorSet,
                {
                    .dstBinding = COVERAGE_PLANE_IDX,
                    .descriptorType =
                        desc.interlockMode == gpu::InterlockMode::atomics
                            ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                            : VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                },
                {{
                    .imageView = coverageTexture->vkImageView(),
                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                }});
        }
    }

    // Bind the descriptor sets for this draw pass.
    // (The imageTexture and imageDraw dynamic uniform offsets might have to
    // update between draws, but this is otherwise all we need to bind!)
    VkDescriptorSet drawDescriptorSets[] = {
        perFlushDescriptorSet,
        m_nullImageDescriptorSet,
        m_immutableSamplerDescriptorSet,
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
                    m_perDrawDescriptorSetLayout);

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
                        .sampler = m_imageSamplers[batch.imageSampler.asKey()],
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
             DrawPipelineLayoutOptions::coalescedResolveAndTransfer) &&
            (drawType == gpu::DrawType::atomicResolve))
        {
            shaderMiscFlags |=
                gpu::ShaderMiscFlags::coalescedResolveAndTransfer;
        }

        auto drawPipelineOptions = DrawPipelineOptions::none;
        if (desc.wireframe && m_vk->features.fillModeNonSolid)
        {
            drawPipelineOptions |= DrawPipelineOptions::wireframe;
        }

        gpu::PipelineState pipelineState;
        gpu::get_pipeline_state(batch,
                                desc,
                                m_platformFeatures,
                                &pipelineState);

        std::unique_ptr<DrawPipeline>& drawPipeline =
            m_drawPipelines[DrawPipeline::Key(drawType,
                                              shaderFeatures,
                                              desc.interlockMode,
                                              shaderMiscFlags,
                                              drawPipelineOptions,
                                              pipelineState.uniqueKey,
                                              renderPassKey)];
        if (drawPipeline == nullptr)
        {
            drawPipeline = std::make_unique<DrawPipeline>(this,
                                                          drawType,
                                                          pipelineLayout,
                                                          shaderFeatures,
                                                          shaderMiscFlags,
                                                          drawPipelineOptions,
                                                          pipelineState,
                                                          *renderPass);
        }

        m_vk->CmdBindPipeline(commandBuffer,
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              *drawPipeline);

        if (batch.barriers & BarrierFlags::plsAtomicPreResolve)
        {
            // The atomic resolve gets its barrier via vkCmdNextSubpass().
            assert(desc.interlockMode == gpu::InterlockMode::atomics);
            assert(drawType == gpu::DrawType::atomicResolve);
            m_vk->CmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
        }
        else if (batch.barriers &
                 (BarrierFlags::plsAtomicPostInit | BarrierFlags::plsAtomic |
                  BarrierFlags::dstBlend))
        {
            // Wait for color attachment writes to complete before we read the
            // input attachments again. (We also checked for "plsAtomicPostInit"
            // because this barrier has to occur after the Vulkan load
            // operations as well.)
            assert(desc.interlockMode == gpu::InterlockMode::atomics ||
                   desc.interlockMode == gpu::InterlockMode::msaa);
            assert(drawType != gpu::DrawType::atomicResolve);
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
            m_vk->memoryBarrier(commandBuffer,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                VK_DEPENDENCY_BY_REGION_BIT,
                                {
                                    .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                                    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                                });
        }

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

            case DrawType::atomicResolve:
            {
                assert(desc.interlockMode == gpu::InterlockMode::atomics);
                m_vk->CmdDraw(commandBuffer, 4, 1, 0, 0);
                break;
            }

            case DrawType::atomicInitialize:
                RIVE_UNREACHABLE();
        }
    }

    m_vk->CmdEndRenderPass(commandBuffer);

    if (colorAttachmentIsOffscreen &&
        !(pipelineLayout.options() &
          DrawPipelineLayoutOptions::coalescedResolveAndTransfer))
    {
        // Copy from the offscreen texture back to the target.
        m_vk->blitSubRect(
            commandBuffer,
            renderTarget
                ->accessOffscreenColorTexture(
                    commandBuffer,
                    {
                        .pipelineStages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                        .accessMask = VK_ACCESS_TRANSFER_READ_BIT,
                        .layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    })
                ->vkImage(),
            renderTarget->accessTargetImage(
                commandBuffer,
                {
                    .pipelineStages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                    .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                    .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                },
                vkutil::ImageAccessAction::invalidateContents),
            desc.renderTargetUpdateBounds);
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
    spirv::hotload_shaders(spirvData);

    // Delete and replace old shaders
    m_colorRampPipeline = std::make_unique<ColorRampPipeline>(this);
    m_tessellatePipeline = std::make_unique<TessellatePipeline>(this);
    m_atlasPipeline = std::make_unique<AtlasPipeline>(this);
    m_drawShaders.clear();
    m_drawPipelines.clear();
}

std::unique_ptr<RenderContext> RenderContextVulkanImpl::MakeContext(
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    const VulkanFeatures& features,
    PFN_vkGetInstanceProcAddr pfnvkGetInstanceProcAddr)
{
    rcp<VulkanContext> vk = make_rcp<VulkanContext>(instance,
                                                    physicalDevice,
                                                    device,
                                                    features,
                                                    pfnvkGetInstanceProcAddr);
    VkPhysicalDeviceProperties physicalDeviceProps;
    vk->GetPhysicalDeviceProperties(vk->physicalDevice, &physicalDeviceProps);
    std::unique_ptr<RenderContextVulkanImpl> impl(
        new RenderContextVulkanImpl(std::move(vk), physicalDeviceProps));
    if (!impl->platformFeatures().supportsRasterOrdering &&
        !impl->platformFeatures().supportsFragmentShaderAtomics)
    {
        return nullptr; // TODO: implement MSAA.
    }
    impl->initGPUObjects();
    return std::make_unique<RenderContext>(std::move(impl));
}
} // namespace rive::gpu
