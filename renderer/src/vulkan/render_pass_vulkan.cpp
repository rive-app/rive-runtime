/*
 * Copyright 2025 Rive
 */

#include <vulkan/vulkan.h>
#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"
#include "rive/renderer/stack_vector.hpp"
#include "shaders/constants.glsl"
#include "pipeline_manager_vulkan.hpp"
#include "render_pass_vulkan.hpp"

namespace rive::gpu
{
constexpr static VkAttachmentLoadOp vk_load_op(gpu::LoadAction loadAction,
                                               gpu::InterlockMode interlockMode)
{
    switch (loadAction)
    {
        case gpu::LoadAction::preserveRenderTarget:
            return (interlockMode == gpu::InterlockMode::msaa)
                       // In MSAA we need to implement the loadOp with a manual
                       // draw instead, since the MSAA attachment is transient
                       // and its color is seeded from the actual render target.
                       ? VK_ATTACHMENT_LOAD_OP_DONT_CARE
                       : VK_ATTACHMENT_LOAD_OP_LOAD;
        case gpu::LoadAction::clear:
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case gpu::LoadAction::dontCare:
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
    RIVE_UNREACHABLE();
}

static uint32_t vk_render_format_key(VkFormat format)
{
    uint32_t key = static_cast<uint32_t>(format);
    if (key > VK_FORMAT_ASTC_12x12_SRGB_BLOCK)
    {
        // There is a large gap between VK_FORMAT_ASTC_12x12_SRGB_BLOCK (184),
        // and VK_FORMAT_G8B8G8R8_422_UNORM (1000156000). Removing this gap gets
        // our FORMAT_BIT_COUNT down to 19.
        // NOTE: Some compressed ASTC and PVRTC formats live in this gap, but
        // they're non-renderable.
        assert(key >= VK_FORMAT_G8B8G8R8_422_UNORM);
        key -=
            VK_FORMAT_G8B8G8R8_422_UNORM - VK_FORMAT_ASTC_12x12_SRGB_BLOCK - 1;
        assert(key > VK_FORMAT_ASTC_12x12_SRGB_BLOCK);
    }
    assert(key <= 1 << RenderPassVulkan::FORMAT_BIT_COUNT);
    return key;
}

uint32_t RenderPassVulkan::KeyNoInterlockMode(
    DrawPipelineLayoutVulkan::Options layoutOptions,
    VkFormat renderTargetFormat,
    gpu::LoadAction loadAction)
{
    // gpu::LoadAction.
    assert(static_cast<uint32_t>(loadAction) < 1 << LOAD_OP_BIT_COUNT);
    uint32_t key = static_cast<uint32_t>(loadAction);

    // VkFormat.
    const uint32_t renderFormatKey = vk_render_format_key(renderTargetFormat);
    assert(key << FORMAT_BIT_COUNT >> FORMAT_BIT_COUNT == key);
    assert(renderFormatKey < 1 << FORMAT_BIT_COUNT);
    key = (key << FORMAT_BIT_COUNT) | renderFormatKey;

    // DrawPipelineLayoutVulkan::Options.
    assert(key << DrawPipelineLayoutVulkan::OPTION_COUNT >>
               DrawPipelineLayoutVulkan::OPTION_COUNT ==
           key);
    assert(static_cast<uint32_t>(layoutOptions) <
           1 << DrawPipelineLayoutVulkan::OPTION_COUNT);
    key = (key << DrawPipelineLayoutVulkan::OPTION_COUNT) |
          static_cast<uint32_t>(layoutOptions);

    assert(key < 1 << KEY_NO_INTERLOCK_MODE_BIT_COUNT);
    return key;
}

uint32_t RenderPassVulkan::Key(gpu::InterlockMode interlockMode,
                               DrawPipelineLayoutVulkan::Options layoutOptions,
                               VkFormat renderTargetFormat,
                               gpu::LoadAction loadAction)
{
    uint32_t key =
        KeyNoInterlockMode(layoutOptions, renderTargetFormat, loadAction);

    // gpu::InterlockMode.
    assert(key << gpu::INTERLOCK_MODE_BIT_COUNT >>
               gpu::INTERLOCK_MODE_BIT_COUNT ==
           key);
    assert(static_cast<uint32_t>(interlockMode) <
           1 << gpu::INTERLOCK_MODE_BIT_COUNT);
    key = (key << gpu::INTERLOCK_MODE_BIT_COUNT) |
          static_cast<uint32_t>(interlockMode);

    assert(key < 1 << KEY_BIT_COUNT);
    return key;
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
        (interlockMode == gpu::InterlockMode::msaa) ? VK_SAMPLE_COUNT_4_BIT
                                                    : VK_SAMPLE_COUNT_1_BIT;
    StackVector<VkAttachmentDescription, PLS_PLANE_COUNT> attachments;
    StackVector<VkAttachmentReference, PLS_PLANE_COUNT> colorAttachmentRefs;
    StackVector<VkAttachmentReference, 1> plsResolveAttachmentRef;
    StackVector<VkAttachmentReference, 1> depthStencilAttachmentRef;
    StackVector<VkAttachmentReference, 1> msaaResolveAttachmentRef;
    assert(attachments.size() == COLOR_PLANE_IDX);
    assert(colorAttachmentRefs.size() == COLOR_PLANE_IDX);
    attachments.push_back({
        .format = renderTargetFormat,
        .samples = msaaSampleCount,
        .loadOp = vk_load_op(loadAction, interlockMode),
        .storeOp =
            ((layoutOptions &
              DrawPipelineLayoutVulkan::Options::coalescedResolveAndTransfer) ||
             interlockMode == gpu::InterlockMode::msaa)
                ? VK_ATTACHMENT_STORE_OP_DONT_CARE
                : VK_ATTACHMENT_STORE_OP_STORE,
        // This could be VK_IMAGE_LAYOUT_UNDEFINED more often, but it would
        // invalidate the portion outside the renderArea when it isn't the full
        // renderTarget, and currently we don't have separate render passes for
        // "full renderTarget bounds" and "partial renderTarget bounds".
        // Instead, we rely on vkutil::ImageAccessAction::invalidateContents
        // to invalidate the color attachment when we can.
        .initialLayout =
            (((layoutOptions & DrawPipelineLayoutVulkan::Options::
                                   coalescedResolveAndTransfer) &&
              loadAction != gpu::LoadAction::preserveRenderTarget) ||
             interlockMode == gpu::InterlockMode::msaa)
                ? VK_IMAGE_LAYOUT_UNDEFINED
                : colorAttachmentLayout,
        .finalLayout = colorAttachmentLayout,
    });
    colorAttachmentRefs.push_back({
        .attachment = COLOR_PLANE_IDX,
        .layout = colorAttachmentLayout,
    });

    if (interlockMode == gpu::InterlockMode::rasterOrdering ||
        interlockMode == gpu::InterlockMode::atomics)
    {
        // CLIP attachment.
        assert(attachments.size() == CLIP_PLANE_IDX);
        assert(colorAttachmentRefs.size() == CLIP_PLANE_IDX);
        attachments.push_back({
            // The clip buffer is encoded as RGBA8 in atomic mode so we can
            // block writes by emitting alpha=0.
            .format = (interlockMode == gpu::InterlockMode::atomics)
                          ? VK_FORMAT_R8G8B8A8_UNORM
                          : VK_FORMAT_R32_UINT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
        });
        colorAttachmentRefs.push_back({
            .attachment = CLIP_PLANE_IDX,
            .layout = VK_IMAGE_LAYOUT_GENERAL,
        });
    }

    if (interlockMode == gpu::InterlockMode::rasterOrdering)
    {
        // SCRATCH_COLOR attachment.
        assert(attachments.size() == SCRATCH_COLOR_PLANE_IDX);
        assert(colorAttachmentRefs.size() == SCRATCH_COLOR_PLANE_IDX);
        attachments.push_back({
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
        });
        colorAttachmentRefs.push_back({
            .attachment = SCRATCH_COLOR_PLANE_IDX,
            .layout = VK_IMAGE_LAYOUT_GENERAL,
        });

        // COVERAGE attachment.
        assert(attachments.size() == COVERAGE_PLANE_IDX);
        assert(colorAttachmentRefs.size() == COVERAGE_PLANE_IDX);
        attachments.push_back({
            .format = VK_FORMAT_R32_UINT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
        });
        colorAttachmentRefs.push_back({
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
                // would invalidate the portion outside the renderArea when it
                // isn't the full renderTarget, and currently we don't have
                // separate render passes for "full renderTarget bounds" and
                // "partial renderTarget bounds". Instead, we rely on
                // vkutil::ImageAccessAction::invalidateContents to invalidate
                // the atomic resolve attachment when we can.
                .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            });

            // The resolve subpass only renders to the resolve texture.
            // And the "coalesced" resolve shader outputs to color
            // attachment 0, so alias the COALESCED_ATOMIC_RESOLVE
            // attachment on output 0 for this subpass.
            assert(plsResolveAttachmentRef.size() == 0);
            plsResolveAttachmentRef.push_back({
                .attachment = COALESCED_ATOMIC_RESOLVE_IDX,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            });
        }
        else
        {
            // When not in "coalesced" mode, the resolve texture is the
            // same as the COLOR texture.
            static_assert(COLOR_PLANE_IDX == 0);
            assert(plsResolveAttachmentRef.size() == 0);
            plsResolveAttachmentRef.push_back(colorAttachmentRefs[0]);
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
        depthStencilAttachmentRef.push_back({
            .attachment = MSAA_DEPTH_STENCIL_IDX,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        });

        // MSAA_RESOLVE attachment.
        const bool readsMSAAResolveAttachment =
            loadAction == gpu::LoadAction::preserveRenderTarget &&
            !(layoutOptions &
              DrawPipelineLayoutVulkan::Options::msaaSeedFromOffscreenTexture);
        const VkImageLayout msaaResolveLayout =
            readsMSAAResolveAttachment
                ? VK_IMAGE_LAYOUT_GENERAL
                : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        assert(attachments.size() == MSAA_RESOLVE_IDX);
        attachments.push_back({
            .format = renderTargetFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = readsMSAAResolveAttachment
                          ? VK_ATTACHMENT_LOAD_OP_LOAD
                          : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout =
                readsMSAAResolveAttachment
                    ? msaaResolveLayout
                    // NOTE: This can only be VK_IMAGE_LAYOUT_UNDEFINED because
                    // Vulkan does not support partial MSAA resolves, so every
                    // MSAA render pass covers the entire render area.
                    // TODO: If we add a new subpass that reads the MSAA buffer
                    // and resolves it manually for partial updates, this will
                    // need to change to "msaaResolveLayout".
                    : VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = msaaResolveLayout,
        });
        msaaResolveAttachmentRef.push_back({
            .attachment = MSAA_RESOLVE_IDX,
            .layout = msaaResolveLayout,
        });
        assert(msaaResolveAttachmentRef.size() == colorAttachmentRefs.size());

        if (layoutOptions &
            DrawPipelineLayoutVulkan::Options::msaaSeedFromOffscreenTexture)
        {
            // MSAA_SEED attachment.
            assert(loadAction == gpu::LoadAction::preserveRenderTarget);
            assert(attachments.size() == MSAA_COLOR_SEED_IDX);
            attachments.push_back({
                .format = renderTargetFormat,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_GENERAL,
                .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
            });
        }
    }

    // Input attachments.
    StackVector<VkAttachmentReference, PLS_PLANE_COUNT> inputAttachmentRefs;
    StackVector<VkAttachmentReference, 1> msaaColorSeedInputAttachmentRef;
    if (interlockMode != gpu::InterlockMode::clockwiseAtomic)
    {
        inputAttachmentRefs.push_back_n(colorAttachmentRefs.size(),
                                        colorAttachmentRefs.data());
        if (layoutOptions &
            DrawPipelineLayoutVulkan::Options::fixedFunctionColorOutput)
        {
            // COLOR is not an input attachment if we're using fixed function
            // blending.
            if (inputAttachmentRefs.size() > 1)
            {
                inputAttachmentRefs[0] = {.attachment = VK_ATTACHMENT_UNUSED};
            }
            else
            {
                inputAttachmentRefs.clear();
            }
        }
        if (interlockMode == gpu::InterlockMode::msaa &&
            loadAction == gpu::LoadAction::preserveRenderTarget)
        {
            msaaColorSeedInputAttachmentRef.push_back({
                .attachment =
                    (layoutOptions & DrawPipelineLayoutVulkan::Options::
                                         msaaSeedFromOffscreenTexture)
                        ? MSAA_COLOR_SEED_IDX
                        : MSAA_RESOLVE_IDX,
                .layout = VK_IMAGE_LAYOUT_GENERAL,
            });
        }
    }

    const bool rasterOrderedAttachmentAccess =
        interlockMode == gpu::InterlockMode::rasterOrdering &&
        m_vk->features.rasterizationOrderColorAttachmentAccess;

    constexpr uint32_t MAX_SUBPASSES = 3;
    StackVector<VkSubpassDescription, MAX_SUBPASSES> subpassDescs;

    constexpr uint32_t MAX_SUBPASS_DEPS = MAX_SUBPASSES + 1;
    StackVector<VkSubpassDependency, MAX_SUBPASS_DEPS> subpassDeps;

    // MSAA resolve needs its own set of input references vs.
    // inputAttachmentRefs, specifically because the color buffer always needs
    // to be used for the resolve (whereas normally it's occasioanlly set to
    // VK_ATTACHMENT_UNUSED because we only use it when doing advanced blend)
    StackVector<VkAttachmentReference, PLS_PLANE_COUNT>
        msaaResolveInputAttachmentRefs;

    // MSAA color-load subpass.
    if (interlockMode == gpu::InterlockMode::msaa &&
        loadAction == gpu::LoadAction::preserveRenderTarget)
    {
        assert(msaaColorSeedInputAttachmentRef.size() ==
               colorAttachmentRefs.size());
        assert(subpassDescs.size() == 0);
        subpassDescs.push_back({
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = msaaColorSeedInputAttachmentRef.size(),
            .pInputAttachments = msaaColorSeedInputAttachmentRef.data(),
            .colorAttachmentCount = colorAttachmentRefs.size(),
            .pColorAttachments = colorAttachmentRefs.data(),
        });
        // The color-load subpass has a self dependency because it reads the
        // result of seed attachment's loadOp when it draws it into the MSAA
        // attachment. (loadOps always occur in
        // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT.)
        subpassDeps.push_back({
            .srcSubpass = 0,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        });
    }

    // Main subpass.
    const uint32_t mainSubpassIdx = subpassDescs.size();
    assert(colorAttachmentRefs.size() ==
           m_drawPipelineLayout->colorAttachmentCount(0));
    assert(msaaResolveAttachmentRef.size() == 0 ||
           msaaResolveAttachmentRef.size() == colorAttachmentRefs.size());
    subpassDescs.push_back({
        .flags =
            rasterOrderedAttachmentAccess
                ? VK_SUBPASS_DESCRIPTION_RASTERIZATION_ORDER_ATTACHMENT_COLOR_ACCESS_BIT_EXT
                : 0u,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = inputAttachmentRefs.size(),
        .pInputAttachments = inputAttachmentRefs.data(),
        .colorAttachmentCount = colorAttachmentRefs.size(),
        .pColorAttachments = colorAttachmentRefs.data(),
        .pDepthStencilAttachment = depthStencilAttachmentRef.dataOrNull(),
    });

    if (msaaResolveAttachmentRef.size() > 0)
    {
        // Some Android drivers (some Android 12 and earlier Adreno drivers)
        // have issues with having both a self-dependency and resolve
        // attachments. The resolve can instead be done as a second pass (in
        // which no actual rendering occurs), which eliminates some corruption
        // during blending on the affected devices.

        msaaResolveInputAttachmentRefs.push_back_n(colorAttachmentRefs.size(),
                                                   colorAttachmentRefs.data());
        msaaResolveInputAttachmentRefs[0].layout = VK_IMAGE_LAYOUT_GENERAL;

        subpassDescs.push_back({
            .flags = 0u,
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = msaaResolveInputAttachmentRefs.size(),
            .pInputAttachments = msaaResolveInputAttachmentRefs.data(),
            .colorAttachmentCount = colorAttachmentRefs.size(),
            .pColorAttachments = colorAttachmentRefs.data(),
            .pResolveAttachments = msaaResolveAttachmentRef.data(),
        });
    }

    if ((interlockMode == gpu::InterlockMode::rasterOrdering &&
         !rasterOrderedAttachmentAccess) ||
        interlockMode == gpu::InterlockMode::atomics ||
        (interlockMode == gpu::InterlockMode::msaa &&
         !(layoutOptions &
           DrawPipelineLayoutVulkan::Options::fixedFunctionColorOutput)))
    {
        // Any subpass that reads the framebuffer or PLS planes has a self
        // dependency.
        //
        // In implicit rasterOrdering mode (meaning
        // EXT_rasterization_order_attachment_access is not present, but
        // we're on ARM hardware and know the hardware is raster ordered
        // anyway), we also need to declare this dependency even though
        // we won't be issuing any barriers.
        subpassDeps.push_back({
            .srcSubpass = mainSubpassIdx,
            .dstSubpass = mainSubpassIdx,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            // TODO: We should add SHADER_READ/SHADER_WRITE flags for the
            // coverage buffer as well, but ironically, adding those seems to
            // cause artifacts on Qualcomm. Leave them out for now until we can
            // investigate further.
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
        assert(plsResolveAttachmentRef.size() ==
               m_drawPipelineLayout->colorAttachmentCount(1));
        subpassDescs.push_back({
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = inputAttachmentRefs.size(),
            .pInputAttachments = inputAttachmentRefs.data(),
            .colorAttachmentCount = plsResolveAttachmentRef.size(),
            .pColorAttachments = plsResolveAttachmentRef.data(),
        });
    }

    for (uint32_t i = 1; i < subpassDescs.size(); ++i)
    {
        // In the case of multiple subpasses, supbass N always consumes the
        // color outputs from supbass N-1.
        subpassDeps.push_back({
            .srcSubpass = i - 1,
            .dstSubpass = i,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            // TODO: Atomic mode should add SHADER_READ/SHADER_WRITE flags for
            // the coverage buffer as well, and add
            // VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT to the srcStageMask, but
            // ironically, adding those causes artifacts on Qualcomm. Leave them
            // out for now unless we find a case where we don't work without
            // them.
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
