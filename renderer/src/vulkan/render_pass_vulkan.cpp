/*
 * Copyright 2025 Rive
 */

#include <vulkan/vulkan.h>
#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"
#include "rive/renderer/stack_vector.hpp"
#include "shaders/constants.glsl"
#include "pipeline_manager_vulkan.hpp"
#include "render_pass_vulkan.hpp"
#include "vulkan_constants.hpp"

namespace rive::gpu
{
constexpr static VkAttachmentLoadOp vk_load_op(gpu::LoadAction loadAction)
{
    switch (loadAction)
    {
        case gpu::LoadAction::preserveRenderTarget:
            return VK_ATTACHMENT_LOAD_OP_LOAD;
        case gpu::LoadAction::clear:
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case gpu::LoadAction::dontCare:
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
    RIVE_UNREACHABLE();
}

uint32_t RenderPassVulkan::Key(gpu::InterlockMode interlockMode,
                               DrawPipelineLayoutVulkan::Options layoutOptions,
                               VkFormat renderTargetFormat,
                               gpu::LoadAction loadAction)
{
    uint32_t formatKey = static_cast<uint32_t>(renderTargetFormat);
    if (formatKey > VK_FORMAT_ASTC_12x12_SRGB_BLOCK)
    {
        assert(formatKey >= VK_FORMAT_G8B8G8R8_422_UNORM);
        formatKey -=
            VK_FORMAT_G8B8G8R8_422_UNORM - VK_FORMAT_ASTC_12x12_SRGB_BLOCK - 1;
    }
    assert(formatKey <= 1 << FORMAT_BIT_COUNT);

    uint32_t drawPipelineLayoutIdx =
        DrawPipelineLayoutIdx(interlockMode, layoutOptions);
    assert(drawPipelineLayoutIdx < 1 << DrawPipelineLayoutVulkan::BIT_COUNT);

    assert(static_cast<uint32_t>(loadAction) < 1 << LOAD_OP_BIT_COUNT);

    return (formatKey << (DrawPipelineLayoutVulkan::BIT_COUNT +
                          LOAD_OP_BIT_COUNT)) |
           (drawPipelineLayoutIdx << LOAD_OP_BIT_COUNT) |
           static_cast<uint32_t>(loadAction);
}

RenderPassVulkan::RenderPassVulkan(
    PipelineManagerVulkan* pipelineManager,
    gpu::InterlockMode interlockMode,
    DrawPipelineLayoutVulkan::Options layoutOptions,
    VkFormat renderTargetFormat,
    gpu::LoadAction loadAction) :
    m_vk(ref_rcp(pipelineManager->vulkanContext()))
{
    m_drawPipelineLayout =
        &pipelineManager->getDrawPipelineLayoutSynchronous(interlockMode,
                                                           layoutOptions);

    // COLOR attachment.
    const VkImageLayout colorAttachmentLayout =
        (layoutOptions &
         DrawPipelineLayoutVulkan::Options::fixedFunctionColorOutput)
            ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            : VK_IMAGE_LAYOUT_GENERAL;
    const VkSampleCountFlagBits msaaSampleCount =
        interlockMode == gpu::InterlockMode::msaa ? VK_SAMPLE_COUNT_4_BIT
                                                  : VK_SAMPLE_COUNT_1_BIT;
    StackVector<VkAttachmentDescription, MAX_FRAMEBUFFER_ATTACHMENTS>
        attachments;
    StackVector<VkAttachmentReference, MAX_FRAMEBUFFER_ATTACHMENTS>
        colorAttachments;
    StackVector<VkAttachmentReference, MAX_FRAMEBUFFER_ATTACHMENTS>
        plsResolveAttachments;
    StackVector<VkAttachmentReference, 1> depthStencilAttachment;
    StackVector<VkAttachmentReference, MAX_FRAMEBUFFER_ATTACHMENTS>
        msaaResolveAttachments;
    assert(attachments.size() == COLOR_PLANE_IDX);
    assert(colorAttachments.size() == COLOR_PLANE_IDX);
    attachments.push_back({
        .format = renderTargetFormat,
        .samples = msaaSampleCount,
        .loadOp = vk_load_op(loadAction),
        .storeOp =
            (layoutOptions &
             DrawPipelineLayoutVulkan::Options::coalescedResolveAndTransfer)
                ? VK_ATTACHMENT_STORE_OP_DONT_CARE
                : VK_ATTACHMENT_STORE_OP_STORE,
        .initialLayout = loadAction == gpu::LoadAction::preserveRenderTarget
                             ? colorAttachmentLayout
                             : VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = colorAttachmentLayout,
    });
    colorAttachments.push_back({
        .attachment = COLOR_PLANE_IDX,
        .layout = colorAttachmentLayout,
    });

    if (interlockMode == gpu::InterlockMode::rasterOrdering ||
        interlockMode == gpu::InterlockMode::atomics)
    {
        // CLIP attachment.
        assert(attachments.size() == CLIP_PLANE_IDX);
        assert(colorAttachments.size() == CLIP_PLANE_IDX);
        attachments.push_back({
            // The clip buffer is encoded as RGBA8 in atomic mode so we can
            // block writes by emitting alpha=0.
            .format = interlockMode == gpu::InterlockMode::atomics
                          ? VK_FORMAT_R8G8B8A8_UNORM
                          : VK_FORMAT_R32_UINT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
        });
        colorAttachments.push_back({
            .attachment = CLIP_PLANE_IDX,
            .layout = VK_IMAGE_LAYOUT_GENERAL,
        });
    }

    if (interlockMode == gpu::InterlockMode::rasterOrdering)
    {
        // SCRATCH_COLOR attachment.
        assert(attachments.size() == SCRATCH_COLOR_PLANE_IDX);
        assert(colorAttachments.size() == SCRATCH_COLOR_PLANE_IDX);
        attachments.push_back({
            .format = renderTargetFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
        });
        colorAttachments.push_back({
            .attachment = SCRATCH_COLOR_PLANE_IDX,
            .layout = VK_IMAGE_LAYOUT_GENERAL,
        });

        // COVERAGE attachment.
        assert(attachments.size() == COVERAGE_PLANE_IDX);
        assert(colorAttachments.size() == COVERAGE_PLANE_IDX);
        attachments.push_back({
            .format = VK_FORMAT_R32_UINT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
        });
        colorAttachments.push_back({
            .attachment = COVERAGE_PLANE_IDX,
            .layout = VK_IMAGE_LAYOUT_GENERAL,
        });
    }
    else if (interlockMode == gpu::InterlockMode::atomics)
    {
        if (layoutOptions &
            DrawPipelineLayoutVulkan::Options::coalescedResolveAndTransfer)
        {
            // COALESCED_ATOMIC_RESOLVE attachment (primary render target).
            assert(attachments.size() == COALESCED_ATOMIC_RESOLVE_IDX);
            attachments.push_back({
                .format = renderTargetFormat,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                // This could sometimes be VK_IMAGE_LAYOUT_UNDEFINED, but it
                // might not preserve the portion outside the renderArea
                // when it isn't the full render target. Instead we rely on
                // accessTargetImageView() to invalidate the target texture
                // when we can.
                .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            });

            // The resolve subpass only renders to the resolve texture.
            // And the "coalesced" resolve shader outputs to color
            // attachment 0, so alias the COALESCED_ATOMIC_RESOLVE
            // attachment on output 0 for this subpass.
            assert(plsResolveAttachments.size() == 0);
            plsResolveAttachments.push_back({
                .attachment = COALESCED_ATOMIC_RESOLVE_IDX,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            });
        }
        else
        {
            // When not in "coalesced" mode, the resolve texture is the
            // same as the COLOR texture.
            static_assert(COLOR_PLANE_IDX == 0);
            assert(plsResolveAttachments.size() == 0);
            plsResolveAttachments.push_back(colorAttachments[0]);
        }
    }
    else if (interlockMode == gpu::InterlockMode::msaa)
    {
        // DEPTH attachment.
        assert(attachments.size() == MSAA_DEPTH_STENCIL_IDX);
        attachments.push_back({
            .format = vkutil::get_preferred_depth_stencil_format(
                m_vk->supportsD24S8()),
            .samples = msaaSampleCount,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        });
        depthStencilAttachment.push_back({
            .attachment = MSAA_DEPTH_STENCIL_IDX,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        });

        // MSAA_RESOLVE attachment.
        assert(attachments.size() == MSAA_RESOLVE_IDX);
        attachments.push_back({
            .format = renderTargetFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        });
        msaaResolveAttachments.push_back({
            .attachment = MSAA_RESOLVE_IDX,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        });
        assert(msaaResolveAttachments.size() == colorAttachments.size());
    }

    // Input attachments.
    StackVector<VkAttachmentReference, MAX_FRAMEBUFFER_ATTACHMENTS>
        inputAttachments;
    if (interlockMode != gpu::InterlockMode::clockwiseAtomic)
    {
        inputAttachments.push_back_n(colorAttachments.size(),
                                     colorAttachments.data());
        if (layoutOptions &
            DrawPipelineLayoutVulkan::Options::fixedFunctionColorOutput)
        {
            // COLOR is not an input attachment if we're using fixed
            // function blending.
            if (inputAttachments.size() > 1)
            {
                inputAttachments[0] = {.attachment = VK_ATTACHMENT_UNUSED};
            }
            else
            {
                inputAttachments.clear();
            }
        }
    }

    // Draw subpass.
    constexpr uint32_t MAX_SUBPASSES = 2;
    const bool rasterOrderedAttachmentAccess =
        interlockMode == gpu::InterlockMode::rasterOrdering &&
        m_vk->features.rasterizationOrderColorAttachmentAccess;
    StackVector<VkSubpassDescription, MAX_SUBPASSES> subpassDescs;
    StackVector<VkSubpassDependency, MAX_SUBPASSES> subpassDeps;
    assert(subpassDescs.size() == 0);
    assert(subpassDeps.size() == 0);
    assert(colorAttachments.size() ==
           m_drawPipelineLayout->colorAttachmentCount(0));
    assert(msaaResolveAttachments.size() == 0 ||
           msaaResolveAttachments.size() == colorAttachments.size());
    subpassDescs.push_back({
        .flags =
            rasterOrderedAttachmentAccess
                ? VK_SUBPASS_DESCRIPTION_RASTERIZATION_ORDER_ATTACHMENT_COLOR_ACCESS_BIT_EXT
                : 0u,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = inputAttachments.size(),
        .pInputAttachments = inputAttachments.data(),
        .colorAttachmentCount = colorAttachments.size(),
        .pColorAttachments = colorAttachments.data(),
        .pResolveAttachments = msaaResolveAttachments.dataOrNull(),
        .pDepthStencilAttachment = depthStencilAttachment.dataOrNull(),
    });

    // Draw subpass self-dependencies.
    if ((interlockMode == gpu::InterlockMode::rasterOrdering &&
         !rasterOrderedAttachmentAccess) ||
        interlockMode == gpu::InterlockMode::atomics ||
        (interlockMode == gpu::InterlockMode::msaa &&
         !(layoutOptions &
           DrawPipelineLayoutVulkan::Options::fixedFunctionColorOutput)))
    {
        // Normally our dependencies are input attachments.
        //
        // In implicit rasterOrdering mode (meaning
        // EXT_rasterization_order_attachment_access is not present, but
        // we're on ARM hardware and know the hardware is raster ordered
        // anyway), we also need to declare this dependency even though
        // we won't be issuing any barriers.
        subpassDeps.push_back({
            .srcSubpass = 0,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            // TODO: We should add SHADER_READ/SHADER_WRITE flags for the
            // coverage buffer as well, but ironically, adding those causes
            // artifacts on Qualcomm. Leave them out for now unless we find
            // a case where we don't work without them.
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        });
    }
    else if (interlockMode == gpu::InterlockMode::clockwiseAtomic)
    {
        // clockwiseAtomic mode has a dependency when we switch from
        // borrowed coverage into forward.
        subpassDeps.push_back({
            .srcSubpass = 0,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        });
    }

    // PLS-resolve subpass (atomic mode only).
    if (interlockMode == gpu::InterlockMode::atomics)
    {
        // The resolve happens in a separate subpass.
        assert(subpassDescs.size() == 1);
        assert(plsResolveAttachments.size() ==
               m_drawPipelineLayout->colorAttachmentCount(1));
        subpassDescs.push_back({
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = inputAttachments.size(),
            .pInputAttachments = inputAttachments.data(),
            .colorAttachmentCount = plsResolveAttachments.size(),
            .pColorAttachments = plsResolveAttachments.data(),
        });

        // The resolve subpass depends on the previous one, but not itself.
        subpassDeps.push_back({
            .srcSubpass = 0,
            .dstSubpass = 1,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            // TODO: We should add SHADER_READ/SHADER_WRITE flags for the
            // coverage buffer as well, but ironically, adding those causes
            // artifacts on Qualcomm. Leave them out for now unless we find
            // a case where we don't work without them.
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        });
    }

    VkRenderPassCreateInfo renderPassCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = attachments.size(),
        .pAttachments = attachments.data(),
        .subpassCount = subpassDescs.size(),
        .pSubpasses = subpassDescs.data(),
        .dependencyCount = subpassDeps.size(),
        .pDependencies = subpassDeps.data(),
    };

    VK_CHECK(m_vk->CreateRenderPass(m_vk->device,
                                    &renderPassCreateInfo,
                                    nullptr,
                                    &m_renderPass));
}

RenderPassVulkan::~RenderPassVulkan()
{
    // Don't touch m_drawPipelineLayout in the destructor since destruction
    // order of us vs. impl->m_drawPipelineLayouts is uncertain.
    m_vk->DestroyRenderPass(m_vk->device, m_renderPass, VK_NULL_HANDLE);
}
} // namespace rive::gpu