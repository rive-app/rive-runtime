#pragma once
#include "rive/renderer/ore/ore_pipeline.hpp"
#include <vulkan/vulkan.h>

namespace rive::ore
{
class ContextVulkan;

class PipelineVulkan : public LITE_RTTI_OVERRIDE(Pipeline, PipelineVulkan)
{
public:
    PipelineVulkan(rcp<rive::gpu::GPUResourceManager> manager,
                   const PipelineDesc& desc) :
        lite_rtti_override(std::move(manager), desc)
    {}
    ~PipelineVulkan() override;

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
};
} // namespace rive::ore
