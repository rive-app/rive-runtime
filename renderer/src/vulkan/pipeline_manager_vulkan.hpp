/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/async_pipeline_manager.hpp"
#include "draw_pipeline_vulkan.hpp"
#include "render_pass_vulkan.hpp"

namespace rive::gpu
{
class PipelineManagerVulkan : public AsyncPipelineManager<DrawPipelineVulkan>
{
    using Super = AsyncPipelineManager<DrawPipelineVulkan>;

public:
    // Returns null if the driver fails to create our objects.
    static std::unique_ptr<PipelineManagerVulkan> make(
        rcp<VulkanContext>,
        ShaderCompilationMode,
        VkImageView nullTextureView);

    ~PipelineManagerVulkan();

    RenderPassVulkan& getRenderPassSynchronous(InterlockMode,
                                               RenderPassOptionsVulkan,
                                               VkFormat,
                                               LoadAction);

    DrawPipelineLayoutVulkan& getDrawPipelineLayoutSynchronous(
        InterlockMode,
        RenderPassOptionsVulkan);

    uint32_t vendorID() const
    {
        return m_vk->physicalDeviceProperties.vendorID;
    }

    VkFormat featherAtlasFormat() const { return m_featherAtlasFormat; }

    VulkanContext* vulkanContext() const { return m_vk.get(); }
    VkDescriptorSetLayout perFlushDescriptorSetLayout() const
    {
        return m_perFlushDescriptorSetLayout;
    }
    VkDescriptorSetLayout perDrawDescriptorSetLayout() const
    {
        return m_perDrawDescriptorSetLayout;
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

    void forEachUbershaderPermutation(
        InterlockMode,
        VkFormat renderTargetFormat,
        VkImageUsageFlags renderTargetUsage,
        LoadAction,
        const PlatformFeatures&,
        const std::function<bool(const PipelineProps&)>& props);

#if !defined(NDEBUG)
    virtual bool isValidUbershaderPipelineProps(
        const PipelineProps& props,
        const PlatformFeatures& platformFeatures) override;
#endif

    void queueUbershaderPipelineCreation(InterlockMode,
                                         VkFormat renderTargetFormat,
                                         VkImageUsageFlags renderTargetUsage,
                                         LoadAction,
                                         const PlatformFeatures&);

private:
    PipelineManagerVulkan(rcp<VulkanContext>, ShaderCompilationMode);

    bool init(VkImageView nullTextureView);

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
        const PipelineProps& props,
        const PlatformFeatures&) override;

    virtual PipelineStatus getPipelineStatus(
        const DrawPipelineVulkan&) const override;

    virtual void clearCacheInternal() override;

    std::unordered_map<uint32_t, std::unique_ptr<DrawPipelineLayoutVulkan>>
        m_drawPipelineLayouts;

    std::unordered_map<uint32_t, std::unique_ptr<RenderPassVulkan>>
        m_renderPasses;

    rcp<VulkanContext> m_vk;
    VkFormat m_featherAtlasFormat;

    // Samplers.
    VkSampler m_linearSampler = VK_NULL_HANDLE;
    VkSampler m_imageSamplers[ImageSampler::MAX_SAMPLER_PERMUTATIONS] = {};

    // With the exception of PLS texture bindings, which differ by interlock
    // mode, all other shaders use the same shared descriptor set layouts.
    VkDescriptorSetLayout m_perFlushDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_perDrawDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_emptyDescriptorSetLayout =
        VK_NULL_HANDLE; // For when a set isn't used by a shader.
    VkDescriptorPool m_staticDescriptorPool =
        VK_NULL_HANDLE; // For descriptorSets that never change between frames.
    VkDescriptorSet m_nullImageDescriptorSet = VK_NULL_HANDLE;
};

} // namespace rive::gpu
