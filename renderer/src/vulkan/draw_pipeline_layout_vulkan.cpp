/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/vulkan/vulkan_context.hpp"
#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"
#include "shaders/constants.glsl"
#include "draw_pipeline_layout_vulkan.hpp"
#include "pipeline_manager_vulkan.hpp"

namespace rive::gpu
{
DrawPipelineLayoutVulkan::DrawPipelineLayoutVulkan(
    PipelineManagerVulkan* impl,
    gpu::InterlockMode interlockMode,
    DrawPipelineLayoutVulkan::Options options) :
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

    if (m_options & Options::fixedFunctionColorOutput)
    {
        // Drop the COLOR input attachment when using
        // fixedFunctionColorOutput.
        assert(plsLayoutInfo.pBindings[0].binding == COLOR_PLANE_IDX);
        ++plsLayoutInfo.pBindings;
        --plsLayoutInfo.bindingCount;
    }

    if (plsLayoutInfo.bindingCount > 0)
    {
        VK_CHECK(
            m_vk->CreateDescriptorSetLayout(m_vk->device,
                                            &plsLayoutInfo,
                                            nullptr,
                                            &m_plsTextureDescriptorSetLayout));
    }

    VkDescriptorSetLayout pipelineDescriptorSetLayouts[] = {
        impl->perFlushDescriptorSetLayout(),
        impl->perDrawDescriptorSetLayout(),
        impl->immutableSamplerDescriptorSetLayout(),
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

DrawPipelineLayoutVulkan::~DrawPipelineLayoutVulkan()
{
    m_vk->DestroyDescriptorSetLayout(m_vk->device,
                                     m_plsTextureDescriptorSetLayout,
                                     nullptr);
    m_vk->DestroyPipelineLayout(m_vk->device, m_pipelineLayout, nullptr);
}

uint32_t DrawPipelineLayoutVulkan::colorAttachmentCount(
    uint32_t subpassIndex) const
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
    RIVE_UNREACHABLE();
}
} // namespace rive::gpu