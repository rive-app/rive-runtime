/*
 * Copyright 2025 Rive
 */

#pragma once

#include <vulkan/vulkan.h>
#include "rive/renderer/vulkan/vulkan_context.hpp"
#include "rive/renderer/gpu.hpp"
#include "draw_pipeline_layout_vulkan.hpp"

namespace rive::gpu
{
class PipelineManagerVulkan;

class RenderPassVulkan
{
public:
    constexpr static uint64_t FORMAT_BIT_COUNT = 19;
    constexpr static uint64_t LOAD_OP_BIT_COUNT = 2;
    constexpr static uint64_t KEY_NO_INTERLOCK_MODE_BIT_COUNT =
        FORMAT_BIT_COUNT + DrawPipelineLayoutVulkan::OPTION_COUNT +
        LOAD_OP_BIT_COUNT;
    constexpr static uint64_t KEY_BIT_COUNT =
        KEY_NO_INTERLOCK_MODE_BIT_COUNT + gpu::INTERLOCK_MODE_BIT_COUNT;
    static_assert(KEY_BIT_COUNT <= 32);

    // Shader unique keys also include the interlock mode, so we don't always
    // need it in the render pass key.
    static uint32_t KeyNoInterlockMode(DrawPipelineLayoutVulkan::Options,
                                       VkFormat renderTargetFormat,
                                       gpu::LoadAction);

    static uint32_t Key(gpu::InterlockMode,
                        DrawPipelineLayoutVulkan::Options,
                        VkFormat renderTargetFormat,
                        gpu::LoadAction);

    RenderPassVulkan(PipelineManagerVulkan*,
                     gpu::InterlockMode,
                     DrawPipelineLayoutVulkan::Options,
                     VkFormat renderTargetFormat,
                     gpu::LoadAction);
    ~RenderPassVulkan();

    RenderPassVulkan(const RenderPassVulkan&) = delete;
    RenderPassVulkan& operator=(const RenderPassVulkan&) = delete;

    const DrawPipelineLayoutVulkan* drawPipelineLayout() const
    {
        return m_drawPipelineLayout;
    }

    operator VkRenderPass() const { return m_renderPass; }

private:
    const rcp<VulkanContext> m_vk;
    // Raw pointer into impl->m_drawPipelineLayouts. RenderContextVulkanImpl
    // ensures the pipline layouts outlive this RenderPass instance.
    const DrawPipelineLayoutVulkan* m_drawPipelineLayout = nullptr;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
};
} // namespace rive::gpu
