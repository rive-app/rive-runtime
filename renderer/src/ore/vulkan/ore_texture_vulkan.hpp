#pragma once
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/vulkan/render_target_vulkan.hpp"
#include "rive/renderer/vulkan/vulkan_context.hpp"
#include <vulkan/vulkan.h>
VK_DEFINE_HANDLE(VmaAllocation);

namespace rive::ore
{
class ContextVulkan;
class BufferVulkan;
class TextureVulkan : public LITE_RTTI_OVERRIDE(Texture, TextureVulkan)
{
public:
    TextureVulkan(rcp<rive::gpu::GPUResourceManager> manager,
                  const TextureDesc& desc) :
        lite_rtti_override(std::move(manager), desc)
    {}
    ~TextureVulkan() override;
    void upload(const TextureDataDesc& data) override;

private:
    friend class ContextVulkan;
    friend class RenderPassVulkan;
    VkImage m_vkImage = VK_NULL_HANDLE;
    VmaAllocation m_vmaAllocation = VK_NULL_HANDLE;
    VkImageLayout m_vkLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkDevice m_vkDevice = VK_NULL_HANDLE; // Weak ref.
    rcp<rive::gpu::VulkanContext> m_vk;
    // Back-ref so upload() can route through ContextVulkan. Weak ref.
    ContextVulkan* m_vkOreContext = nullptr;
};

class TextureViewVulkan
    : public LITE_RTTI_OVERRIDE(TextureView, TextureViewVulkan)
{
public:
    TextureViewVulkan(rcp<rive::gpu::GPUResourceManager> manager,
                      rcp<Texture> texture,
                      const TextureViewDesc& desc) :
        lite_rtti_override(std::move(manager), std::move(texture), desc)
    {}
    ~TextureViewVulkan() override;

private:
    friend class ContextVulkan;
    VkDevice m_vkDevice = VK_NULL_HANDLE; // Weak ref.
    VkImageView m_vkImageView = VK_NULL_HANDLE;
    PFN_vkDestroyImageView m_vkDestroyImageView = nullptr;
    rive::gpu::RenderTargetVulkan* m_vkRenderTarget = nullptr;
};
} // namespace rive::ore
