/*
 * Copyright 2025 Rive
 */

#include "draw_pipeline_layout_vulkan.hpp"

#include "rive/renderer/stack_vector.hpp"
#include "rive/renderer/vulkan/vulkan_context.hpp"
#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"
#include "shaders/constants.glsl"
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
    StackVector<VkDescriptorSetLayoutBinding, PLS_PLANE_COUNT>
        plsLayoutBindings;
    if (!(m_options & Options::fixedFunctionColorOutput))
    {
        plsLayoutBindings.push_back({
            .binding = COLOR_PLANE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        });
    }
    if (interlockMode == gpu::InterlockMode::rasterOrdering ||
        interlockMode == gpu::InterlockMode::atomics)
    {
        plsLayoutBindings.push_back({
            .binding = CLIP_PLANE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        });
        if (interlockMode == gpu::InterlockMode::rasterOrdering)
        {
            plsLayoutBindings.push_back({
                .binding = SCRATCH_COLOR_PLANE_IDX,
                .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            });
        }
        plsLayoutBindings.push_back({
            .binding = COVERAGE_PLANE_IDX,
            .descriptorType = m_interlockMode == gpu::InterlockMode::atomics
                                  ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                                  : VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        });
    }
    else if (interlockMode == gpu::InterlockMode::msaa)
    {
        // TODO: pipeline layouts aren't currently keyed by loadAction, but if
        // they were, we could only include this binding with
        // preserveRenderTarget.
        plsLayoutBindings.push_back({
            .binding = MSAA_COLOR_SEED_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        });
    }

    if (plsLayoutBindings.size() > 0)
    {
        VkDescriptorSetLayoutCreateInfo plsLayoutInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = plsLayoutBindings.size(),
            .pBindings = plsLayoutBindings.data(),
        };

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
            assert(subpassIndex == 0 || subpassIndex == 1);
            return 1;
        case gpu::InterlockMode::clockwise:
            RIVE_UNREACHABLE();
    }
    RIVE_UNREACHABLE();
}
} // namespace rive::gpu
