/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/async_pipeline_manager.hpp"
#include "draw_pipeline_vulkan.hpp"
#include "draw_pipeline_layout_vulkan.hpp"
#include "render_pass_vulkan.hpp"
namespace rive::gpu
{
class PipelineManagerVulkan : public AsyncPipelineManager<DrawPipelineVulkan>
{
    using Super = AsyncPipelineManager<DrawPipelineVulkan>;

public:
    PipelineManagerVulkan(rcp<VulkanContext>,
                          ShaderCompilationMode,
                          VkImageView nullTextureView);
    ~PipelineManagerVulkan();

    RenderPassVulkan& getRenderPassSynchronous(
        InterlockMode,
        DrawPipelineLayoutVulkan::Options,
        VkFormat,
        LoadAction);

    DrawPipelineLayoutVulkan& getDrawPipelineLayoutSynchronous(
        InterlockMode,
        DrawPipelineLayoutVulkan::Options);

    uint32_t vendorID() const
    {
        return m_vk->physicalDeviceProperties().vendorID;
    }

    VkFormat atlasFormat() const { return m_atlasFormat; }

    VulkanContext* vulkanContext() const { return m_vk.get(); }
    VkDescriptorSetLayout perFlushDescriptorSetLayout() const
    {
        return m_perFlushDescriptorSetLayout;
    }
    VkDescriptorSetLayout perDrawDescriptorSetLayout() const
    {
        return m_perDrawDescriptorSetLayout;
    }
    VkDescriptorSetLayout immutableSamplerDescriptorSetLayout() const
    {
        return m_immutableSamplerDescriptorSetLayout;
    }
    VkDescriptorSetLayout emptyDescriptorSetLayout() const
    {
        return m_emptyDescriptorSetLayout;
    }

    VkSampler linearSampler() const { return m_linearSampler; }
    VkSampler imageSampler(uint32_t i) const { return m_imageSamplers[i]; }

    VkDescriptorSet nullImageDescriptorSet() const
    {
        return m_nullImageDescriptorSet;
    }
    VkDescriptorSet immutableSamplerDescriptorSet() const
    {
        return m_immutableSamplerDescriptorSet;
    }

    enum class PLSBackingType : bool
    {
        inputAttachment,
        storageTexture,
    };

    PLSBackingType plsBackingType(gpu::InterlockMode interlockMode)
    {
        if (interlockMode == gpu::InterlockMode::clockwise)
        {
            assert(m_vk->features.fragmentShaderPixelInterlock);
            return PLSBackingType::storageTexture;
        }
        return PLSBackingType::inputAttachment;
    }

private:
    virtual std::unique_ptr<DrawShaderVulkan> createVertexShader(
        DrawType drawType,
        ShaderFeatures shaderFeatures,
        InterlockMode interlockMode) override;

    virtual std::unique_ptr<DrawShaderVulkan> createFragmentShader(
        DrawType drawType,
        ShaderFeatures shaderFeatures,
        InterlockMode interlockMode,
        ShaderMiscFlags miscFlags) override;

    virtual std::unique_ptr<DrawPipelineVulkan> createPipeline(
        PipelineCreateType createType,
        uint64_t key,
        const PipelineProps& props) override;

    virtual PipelineStatus getPipelineStatus(
        const DrawPipelineVulkan&) const override;

    virtual void clearCacheInternal() override;

    std::unordered_map<uint32_t, std::unique_ptr<DrawPipelineLayoutVulkan>>
        m_drawPipelineLayouts;

    std::unordered_map<uint32_t, std::unique_ptr<RenderPassVulkan>>
        m_renderPasses;

    rcp<VulkanContext> m_vk;
    VkFormat m_atlasFormat;

    // Samplers.
    VkSampler m_linearSampler;
    VkSampler m_imageSamplers[ImageSampler::MAX_SAMPLER_PERMUTATIONS];

    // With the exception of PLS texture bindings, which differ by interlock
    // mode, all other shaders use the same shared descriptor set layouts.
    VkDescriptorSetLayout m_perFlushDescriptorSetLayout;
    VkDescriptorSetLayout m_perDrawDescriptorSetLayout;
    VkDescriptorSetLayout m_immutableSamplerDescriptorSetLayout;
    VkDescriptorSetLayout m_emptyDescriptorSetLayout; // For when a set isn't
                                                      // used by a shader.
    VkDescriptorPool m_staticDescriptorPool; // For descriptorSets that never
                                             // change between frames.
    VkDescriptorSet m_nullImageDescriptorSet;
    VkDescriptorSet m_immutableSamplerDescriptorSet; // Empty since samplers are
                                                     // immutable, but also
                                                     // required by Vulkan.
};

} // namespace rive::gpu
