/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/gpu.hpp"
#include "draw_shader_vulkan.hpp"
#include "render_pass_vulkan.hpp"

namespace rive::gpu
{
class PipelineManagerVulkan;

class DrawPipelineVulkan
{
public:
    enum class Options
    {
        none = 0,
        wireframe = 1 << 0,
    };
    constexpr static int OPTION_COUNT = 1;

    struct PipelineProps
    {
        DrawType drawType;
        ShaderFeatures shaderFeatures;
        InterlockMode interlockMode;
        ShaderMiscFlags shaderMiscFlags;

        // Vulkan has pipeline props beyond just the standard ones

        gpu::PipelineState pipelineState;

        Options drawPipelineOptions;
        RenderPassOptionsVulkan renderPassOptions;
        VkFormat renderTargetFormat;
        LoadAction colorLoadAction;
        uint32_t subpassIndex;

#ifdef WITH_RIVE_TOOLS
        SynthesizedFailureType synthesizedFailureType =
            SynthesizedFailureType::none;
#endif

        uint64_t createKey() const;
    };

    using VertexShaderType = DrawShaderVulkan;
    using FragmentShaderType = DrawShaderVulkan;

    DrawPipelineVulkan(PipelineManagerVulkan*,
                       const DrawPipelineLayoutVulkan&,
                       const PipelineProps&,
                       VkRenderPass
#ifdef WITH_RIVE_TOOLS
                       ,
                       SynthesizedFailureType
#endif
    );
    ~DrawPipelineVulkan();

    DrawPipelineVulkan(const DrawPipelineVulkan&) = delete;
    DrawPipelineVulkan& operator=(const DrawPipelineVulkan&) = delete;

    operator VkPipeline() const { return m_vkPipeline; }

private:
    const rcp<VulkanContext> m_vk;
    VkPipeline m_vkPipeline = VK_NULL_HANDLE;
};

RIVE_MAKE_ENUM_BITSET(DrawPipelineVulkan::Options);
} // namespace rive::gpu
