#pragma once
#include "rive/renderer/ore/ore_pipeline.hpp"
#include <vulkan/vulkan.h>

namespace rive::ore
{
class ContextVulkan;

class PipelineVulkan : public LITE_RTTI_OVERRIDE(Pipeline, PipelineVulkan)
{
public:
    PipelineVulkan(const PipelineDesc& desc) : lite_rtti_override(desc) {}
    ~PipelineVulkan() override = default;
    // Defers vkDestroyPipeline and vkDestroyPipelineLayout through
    // ContextVulkan::vkDeferDestroy().
    void onRefCntReachedZero() const override;

private:
    friend class ContextVulkan;
    friend class RenderPassVulkan;
    VkDevice m_vkDevice = VK_NULL_HANDLE; // Weak ref.
    VkPipeline m_vkPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_vkPipelineLayout = VK_NULL_HANDLE;
    VkPrimitiveTopology m_vkTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // Read by RenderPassVulkan::setPipeline to gate vkCmdSetStencilReference.
    bool m_vkStencilTestEnabled = false;
    // Function pointers for cleanup (loaded by Context::makePipeline).
    PFN_vkDestroyPipeline m_vkDestroyPipeline = nullptr;
    PFN_vkDestroyPipelineLayout m_vkDestroyPipelineLayout = nullptr;
    // Back-ref for deferred destruction. Weak ref.
    ContextVulkan* m_vkOreContext = nullptr;
};
} // namespace rive::ore
