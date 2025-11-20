/*
 * Copyright 2025 Rive
 */

#include <optional>
#include <sstream>
#include <string>
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
    std::optional<VkAttachmentReference> plsResolveAttachmentRef;
    std::optional<VkAttachmentReference> depthStencilAttachmentRef;
    std::optional<VkAttachmentReference> msaaResolveAttachmentRef;
    if (pipelineManager->plsBackingType(interlockMode) ==
            PipelineManagerVulkan::PLSBackingType::inputAttachment ||
        (layoutOptions &
         DrawPipelineLayoutVulkan::Options::fixedFunctionColorOutput))
    {
        assert(attachments.size() == COLOR_PLANE_IDX);
        assert(colorAttachmentRefs.size() == COLOR_PLANE_IDX);
        attachments.push_back({
            .format = renderTargetFormat,
            .samples = msaaSampleCount,
            .loadOp = vk_load_op(loadAction, interlockMode),
            .storeOp = ((layoutOptions & DrawPipelineLayoutVulkan::Options::
                                             coalescedResolveAndTransfer) ||
                        interlockMode == gpu::InterlockMode::msaa)
                           ? VK_ATTACHMENT_STORE_OP_DONT_CARE
                           : VK_ATTACHMENT_STORE_OP_STORE,
            // This could be VK_IMAGE_LAYOUT_UNDEFINED more often, but it would
            // invalidate the portion outside the renderArea when it isn't the
            // full renderTarget, and currently we don't have separate render
            // passes for "full renderTarget bounds" and "partial renderTarget
            // bounds". Instead, we rely on
            // vkutil::ImageAccessAction::invalidateContents to invalidate the
            // color attachment when we can.
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
    }

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
            assert(!plsResolveAttachmentRef.has_value());
            plsResolveAttachmentRef = {
                .attachment = COALESCED_ATOMIC_RESOLVE_IDX,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };
        }
        else
        {
            // When not in "coalesced" mode, the resolve texture is the
            // same as the COLOR texture.
            static_assert(COLOR_PLANE_IDX == 0);
            assert(!plsResolveAttachmentRef.has_value());
            plsResolveAttachmentRef = colorAttachmentRefs[0];
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
        depthStencilAttachmentRef = {
            .attachment = MSAA_DEPTH_STENCIL_IDX,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };

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
        msaaResolveAttachmentRef = {
            .attachment = MSAA_RESOLVE_IDX,
            .layout = msaaResolveLayout,
        };

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

    constexpr uint32_t MAX_SUBPASS_DEPS = 9;
    StackVector<VkSubpassDependency, MAX_SUBPASS_DEPS> subpassDeps;

    // The standard initial external input dependency, to ensure that all
    // previous writes to subpass 0's color attachment are completed before this
    // render pass starts.
    static constexpr VkSubpassDependency EXTERNAL_COLOR_INPUT_DEPENDENCY = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_NONE,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                         VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
        .dependencyFlags = 0,
    };

    // Helper to add a typical dependency between the current subpass and the
    // next one, which blocks between the color attachment being written in the
    // current pass and fragment shader reads from the next.
    auto addStandardColorDependencyToNextSubpass =
        [&](uint32_t dstSubpassIndex) {
            subpassDeps.push_back({
                .srcSubpass = dstSubpassIndex - 1,
                .dstSubpass = dstSubpassIndex,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            });
        };

    // MSAA color-load subpass.
    if (interlockMode == gpu::InterlockMode::msaa &&
        loadAction == gpu::LoadAction::preserveRenderTarget)
    {
        assert(msaaColorSeedInputAttachmentRef.size() ==
               colorAttachmentRefs.size());
        assert(subpassDescs.size() == 0);

        // The color-load subpass takes the seed texture (which may be the same
        // as the resolve texture) and writes it out.
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
        // NOTE: This subpass, per the vulkan synchronization validation,
        // should not be necessary, as the external input subpass dependency
        // should handle it, but in practice without this extra barrier
        // everything fails to render properly on Adreno devices.
        subpassDeps.push_back({
            .srcSubpass = 0,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        });

        // This subpass needs an external dependency on the color stages to
        // ensure that all of the color rendering from before this renderpass
        // completes.
        subpassDeps.push_back(EXTERNAL_COLOR_INPUT_DEPENDENCY);

        if (layoutOptions &
            DrawPipelineLayoutVulkan::Options::msaaSeedFromOffscreenTexture)
        {
            // If we're seeding from offscreen texture, this pass needs an
            // external output dependency to ensure that any future writes
            // finish after we're done with it.
            subpassDeps.push_back({
                .srcSubpass = 0,
                .dstSubpass = VK_SUBPASS_EXTERNAL,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_NONE,
                .dependencyFlags = 0,
            });
        }

        // The next subpass (the main subpass) needs an external dependency on
        //  the depth buffer (which is not used in this subpass but is used in
        //  that one)
        subpassDeps.push_back({
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 1,
            .srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .srcAccessMask = VK_ACCESS_NONE,
            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0,
        });

        // Finally, the standard color dependency from subpass 0 -> subpass 1
        addStandardColorDependencyToNextSubpass(subpassDescs.size());
    }
    else
    {
        // Without the extra color-load subpass we need an external dependency
        // into the main subpass
        auto externalInDep = EXTERNAL_COLOR_INPUT_DEPENDENCY;
        if (interlockMode == gpu::InterlockMode::msaa)
        {
            // for msaa where the main subpass is first, the external dependency
            // additionally needs to cover depth/stencil.
            externalInDep.srcStageMask |=
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            externalInDep.dstStageMask |=
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            externalInDep.dstAccessMask |=
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }
        subpassDeps.push_back(externalInDep);
    }

    // Main subpass.
    const uint32_t mainSubpassIdx = subpassDescs.size();
    assert(colorAttachmentRefs.size() ==
           m_drawPipelineLayout->colorAttachmentCount(0, layoutOptions));
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
        .pDepthStencilAttachment = depthStencilAttachmentRef.has_value()
                                       ? &depthStencilAttachmentRef.value()
                                       : nullptr,
    });

    // Add any main subpass self-dependencies if needed
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
            .srcSubpass = mainSubpassIdx,
            .dstSubpass = mainSubpassIdx,
            .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        });
    }

    if (interlockMode == gpu::InterlockMode::msaa)
    {
        // Main subpass needs a separate external dependency for depth/stencil
        subpassDeps.push_back({
            .srcSubpass = subpassDescs.size() - 1,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_NONE,
            .dependencyFlags = 0,
        });

        // Add the dependency from main subpass to the resolve subpass (note
        // that this is different than the standard COLOR_ATTACHMENT_WRITE ->
        // INPUT_ATTACHMENT_READ barrier between subpasses).
        // NOTE: The fragment/input attachment bits here seem like they should
        // not be necessary (nothing renders during this path, it's purely a
        // resolve), but it seems on some Android devices at least, the resolve
        // may be getting done *as* a fragment shader (instead of during
        // COLOR_ATTACHMENT_OUTPUT, which is what seems to happen on desktop),
        // so it's necessary. Ditto the input attachment for this subpass, as
        // just having color and resolve *should* work, but doesn't on Android.
        subpassDeps.push_back({
            .srcSubpass = subpassDescs.size() - 1,
            .dstSubpass = subpassDescs.size(),
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        });

        // Some Android drivers (some Android 12 and earlier Adreno drivers)
        // have issues with having both a self-dependency and resolve
        // attachments. The resolve can instead be done as a second pass (in
        // which no actual rendering occurs), which eliminates some corruption
        // during blending on the affected devices.

        // This should be MSAA and there should only be a single color
        // attachment.
        assert(interlockMode == gpu::InterlockMode::msaa);
        assert(colorAttachmentRefs.size() == 1);
        assert(msaaResolveAttachmentRef.has_value());

        VkAttachmentReference msaaResolveInputAttachmentRef =
            colorAttachmentRefs[0];

        // the layout is not allowed to be COLOR_ATTACHMENT_OPTIMAL, so instead
        // use GENERAL.
        msaaResolveInputAttachmentRef.layout = VK_IMAGE_LAYOUT_GENERAL;

        subpassDescs.push_back({
            .flags = 0u,
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = 1,
            .pInputAttachments = &msaaResolveInputAttachmentRef,
            .colorAttachmentCount = 1,
            .pColorAttachments = colorAttachmentRefs.data(),
            .pResolveAttachments = &msaaResolveAttachmentRef.value(),
        });
    }

    // PLS-resolve subpass (atomic mode only).
    if (interlockMode == gpu::InterlockMode::atomics)
    {
        // Add the dependency from main subpass to the resolve subpass.
        addStandardColorDependencyToNextSubpass(subpassDescs.size());

        // The resolve happens in a separate subpass.
        assert(subpassDescs.size() == 1);
        assert(m_drawPipelineLayout->colorAttachmentCount(1, layoutOptions) ==
               1);
        assert(plsResolveAttachmentRef.has_value());
        subpassDescs.push_back({
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = inputAttachmentRefs.size(),
            .pInputAttachments = inputAttachmentRefs.data(),
            .colorAttachmentCount = 1,
            .pColorAttachments = &plsResolveAttachmentRef.value(),
        });
    }

    // There always needs to be a final external output dependency for the color
    // attachment
    subpassDeps.push_back({
        .srcSubpass = subpassDescs.size() - 1,
        .dstSubpass = VK_SUBPASS_EXTERNAL,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_NONE,
        .dependencyFlags = 0,
    });

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

    const std::string renderPipelineLabel =
        (std::ostringstream()
         << "RIVE_Draw{interlockMode=" << int(interlockMode)
         << ", layoutOptions=" << int(layoutOptions)
         << ", renderTargetFormat=" << int(renderTargetFormat)
         << ", loadAction=" << int(loadAction) << '}')
            .str();
    m_vk->setDebugNameIfEnabled(uint64_t(m_renderPass),
                                VK_OBJECT_TYPE_RENDER_PASS,
                                renderPipelineLabel.c_str());
}

RenderPassVulkan::~RenderPassVulkan()
{
    // Don't touch m_drawPipelineLayout in the destructor since destruction
    // order of us vs. impl->m_drawPipelineLayouts is uncertain.
    m_vk->DestroyRenderPass(m_vk->device, m_renderPass, VK_NULL_HANDLE);
}
} // namespace rive::gpu
