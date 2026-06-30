#pragma once
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_context_vulkan.hpp"
#include <vector>

namespace rive::ore
{
class BufferVulkan;

class BindGroupVulkan : public LITE_RTTI_OVERRIDE(BindGroup, BindGroupVulkan)
{
public:
    BindGroupVulkan(rcp<rive::gpu::GPUResourceManager> manager) :
        lite_rtti_override(std::move(manager))
    {}
    ~BindGroupVulkan() override;

    // Descriptor set for the current backings of this group's UBOs. A UBO that
    // orphaned onto a fresh backing gets a freshly written set. Descriptor sets
    // are immutable once written, so the binding must re-resolve, not the set.
    VkDescriptorSet resolveDescriptorSet();

    // Stamp every UBO's current backing as bound this frame.
    void markUBOsBound();

private:
    friend class ContextVulkan;
    friend class RenderPassVulkan;

    // Replayable write recipe captured at makeBindGroup so a set can be
    // rewritten against a UBO's new backing. Buffers stay alive via
    // m_retainedBuffers, views and samplers via m_retainedViews and
    // m_retainedSamplers.
    struct UBOWrite
    {
        BufferVulkan* buffer;
        uint32_t dstBinding;
        uint32_t offset;
        uint32_t range;
        VkDescriptorType type;
    };
    struct ImageWrite
    {
        uint32_t dstBinding;
        VkDescriptorType type;
        VkImageView imageView;
        VkImageLayout imageLayout;
        VkSampler sampler;
    };
    std::vector<UBOWrite> m_uboWrites;
    std::vector<ImageWrite> m_imageWrites;

    VkDescriptorSetLayout m_vkDSL = VK_NULL_HANDLE;

    // One set per distinct combination of UBO backings, bounded by
    // frames-in-flight. Keyed on the current VkBuffer of each UBO write.
    struct CachedSet
    {
        std::vector<VkBuffer> key;
        VkDescriptorSet set = VK_NULL_HANDLE;
        rcp<DescriptorPoolGeneration> pool;
    };
    std::vector<CachedSet> m_setCache;
};
} // namespace rive::ore
