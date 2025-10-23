/*
 * Copyright 2025 Rive
 */

#pragma once

#include <vulkan/vulkan.h>

#include "rive/renderer/gpu.hpp"
#include "rive/refcnt.hpp"

namespace rive::gpu
{
class PipelineManagerVulkan;
class VulkanContext;

// VkPipelineLayout wrapper for Rive flushes.
class DrawPipelineLayoutVulkan
{
public:
    // Rive-specific options for configuring a flush's VkPipelineLayout.
    enum class Options
    {
        none = 0,

        // No need to attach the COLOR texture as an input attachment. There are
        // no advanced blend modes so we can use built-in hardware blending.
        fixedFunctionColorOutput = 1 << 0,

        // Atomic mode only: Use an offscreen texture to render color, but also
        // attach the real target texture at the COALESCED_ATOMIC_RESOLVE index,
        // and render to it directly in the atomic resolve step.
        coalescedResolveAndTransfer = 1 << 1,

        // MSAA only, while using LoadAction::preserveRenderTarget: We have to
        // initialize the (transient) MSAA color attachment from an offscreen
        // texture bound as an input attachment, because the final render target
        // itself can't be bound as an input attachment.
        msaaSeedFromOffscreenTexture = 1 << 2,
    };

    constexpr static int OPTION_COUNT = 3;

    DrawPipelineLayoutVulkan(PipelineManagerVulkan*,
                             gpu::InterlockMode,
                             Options);
    ~DrawPipelineLayoutVulkan();

    DrawPipelineLayoutVulkan(const DrawPipelineLayoutVulkan&) = delete;
    DrawPipelineLayoutVulkan& operator=(const DrawPipelineLayoutVulkan&) =
        delete;

    gpu::InterlockMode interlockMode() const { return m_interlockMode; }
    Options options() const { return m_options; }

    uint32_t colorAttachmentCount(uint32_t subpassIndex) const;

    VkDescriptorSetLayout plsLayout() const
    {
        return m_plsTextureDescriptorSetLayout;
    }

    VkPipelineLayout operator*() const { return m_pipelineLayout; }
    VkPipelineLayout vkPipelineLayout() const { return m_pipelineLayout; }

private:
    const rcp<VulkanContext> m_vk;
    const gpu::InterlockMode m_interlockMode;
    const Options m_options;

    VkDescriptorSetLayout m_plsTextureDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
};

// A VkPipelineLayout for each
// interlockMode x [all DrawPipelineLayoutOptions permutations].
constexpr static uint32_t DrawPipelineLayoutIdx(
    gpu::InterlockMode interlockMode,
    DrawPipelineLayoutVulkan::Options options)
{
    return (static_cast<int>(interlockMode)
            << DrawPipelineLayoutVulkan::OPTION_COUNT) |
           static_cast<int>(options);
}

RIVE_MAKE_ENUM_BITSET(DrawPipelineLayoutVulkan::Options);

} // namespace rive::gpu
