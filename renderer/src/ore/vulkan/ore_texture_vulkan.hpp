#pragma once
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/vulkan/render_target_vulkan.hpp"
#include <vulkan/vulkan.h>
VK_DEFINE_HANDLE(VmaAllocator);
VK_DEFINE_HANDLE(VmaAllocation);

namespace rive::ore
{
class ContextVulkan;

class TextureVulkan : public LITE_RTTI_OVERRIDE(Texture, TextureVulkan)
{
public:
    TextureVulkan(const TextureDesc& desc) : lite_rtti_override(desc) {}
    // Destructor defers GPU image destruction via ContextVulkan.
    ~TextureVulkan() override = default;
    void upload(const TextureDataDesc& data) override;
    // Defers vmaDestroyImage through ContextVulkan::vkDeferDestroy().
    void onRefCntReachedZero() const override;

private:
    friend class ContextVulkan;
    friend class RenderPassVulkan;
    VkImage m_vkImage = VK_NULL_HANDLE;
    VmaAllocation m_vmaAllocation = VK_NULL_HANDLE;
    VkImageLayout m_vkLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkDevice m_vkDevice = VK_NULL_HANDLE;         // Weak ref.
    VmaAllocator m_vmaAllocator = VK_NULL_HANDLE; // Weak ref.
    // Back-ref so upload() and onRefCntReachedZero() can route through
    // ContextVulkan. Weak ref.
    ContextVulkan* m_vkOreContext = nullptr;
};

class TextureViewVulkan
    : public LITE_RTTI_OVERRIDE(TextureView, TextureViewVulkan)
{
public:
    TextureViewVulkan(rcp<Texture> texture, const TextureViewDesc& desc) :
        lite_rtti_override(std::move(texture), desc)
    {}
    ~TextureViewVulkan() override = default;
    // Defers vkDestroyImageView through ContextVulkan::vkDeferDestroy().
    void onRefCntReachedZero() const override;

private:
    friend class ContextVulkan;
    VkDevice m_vkDevice = VK_NULL_HANDLE; // Weak ref.
    VkImageView m_vkImageView = VK_NULL_HANDLE;
    PFN_vkDestroyImageView m_vkDestroyImageView = nullptr;
    rive::gpu::RenderTargetVulkan* m_vkRenderTarget = nullptr;
    // Back-ref for deferred destruction. Weak ref.
    ContextVulkan* m_vkOreContext = nullptr;
};
} // namespace rive::ore
