/*
 * Copyright 2025 Rive
 */

#pragma once

#include <vulkan/vulkan.h>

#include "rive/renderer/gpu.hpp"
#include "rive/refcnt.hpp"
#include "render_pass_vulkan.hpp"

namespace rive::gpu
{
class PipelineManagerVulkan;
class VulkanContext;

// VkPipelineLayout wrapper for Rive flushes.
class DrawPipelineLayoutVulkan
{
public:
    DrawPipelineLayoutVulkan(PipelineManagerVulkan*,
                             gpu::InterlockMode,
                             RenderPassOptionsVulkan);
    ~DrawPipelineLayoutVulkan();

    DrawPipelineLayoutVulkan(const DrawPipelineLayoutVulkan&) = delete;
    DrawPipelineLayoutVulkan& operator=(const DrawPipelineLayoutVulkan&) =
        delete;

    gpu::InterlockMode interlockMode() const { return m_interlockMode; }
    RenderPassOptionsVulkan renderPassOptions() const
    {
        return m_renderPassOptions;
    }

    // Returns whether this layout's shaders declare the emulated
    // color-write-disable push constant.
    // NOTE: Even if we have VK_EXT_color_write_enable, the push constant still
    // gets declared (though never used) because specialization constants can't
    // remove a declaration.
    bool hasColorWriteDisablePushConstant() const
    {
        // For now, only MSAA gets it -- specialization.glsl leaves
        // @EMULATE_DYNAMIC_COLOR_WRITE_DISABLE undefined elsewhere.
        return m_interlockMode == gpu::InterlockMode::msaa;
    }

    uint32_t colorAttachmentCount(uint32_t subpassIndex,
                                  RenderPassOptionsVulkan) const;

    VkDescriptorSetLayout plsLayout() const
    {
        return m_plsTextureDescriptorSetLayout;
    }

    VkPipelineLayout operator*() const { return m_pipelineLayout; }
    VkPipelineLayout vkPipelineLayout() const { return m_pipelineLayout; }

private:
    const rcp<VulkanContext> m_vk;
    const gpu::InterlockMode m_interlockMode;
    const RenderPassOptionsVulkan m_renderPassOptions;

    VkDescriptorSetLayout m_plsTextureDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
};
} // namespace rive::gpu
