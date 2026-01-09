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
#include "common_layouts.hpp"
#include "draw_pipeline_layout_vulkan.hpp"
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

constexpr static VkFormat LAST_NON_SPARSE_VK_FORMAT =
    VK_FORMAT_ASTC_12x12_SRGB_BLOCK;

// The VkFormat values are very sparse after LAST_NON_SPARSE_VK_FORMAT. This
// table converts the sparse formats to a 0-based, tightly-packed index that can
// be used to build a key.
static uint32_t vk_sparse_format_index(VkFormat format)
{
    assert(format > LAST_NON_SPARSE_VK_FORMAT);
    switch (format)
    {
        // Turn off clang-format so we can fit our case labels on one line.
        // clang-format off
        case VK_FORMAT_G8B8G8R8_422_UNORM: return 0;
        case VK_FORMAT_B8G8R8G8_422_UNORM: return 1;
        case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM: return 2;
        case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM: return 3;
        case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM: return 4;
        case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM: return 5;
        case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM: return 6;
        case VK_FORMAT_R10X6_UNORM_PACK16: return 7;
        case VK_FORMAT_R10X6G10X6_UNORM_2PACK16: return 8;
        case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16: return 9;
        case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16: return 10;
        case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16: return 11;
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16: return 12;
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16: return 13;
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16: return 14;
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16: return 15;
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16: return 16;
        case VK_FORMAT_R12X4_UNORM_PACK16: return 17;
        case VK_FORMAT_R12X4G12X4_UNORM_2PACK16: return 18;
        case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16: return 19;
        case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16: return 20;
        case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16: return 21;
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16: return 22;
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16: return 23;
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16: return 24;
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16: return 25;
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16: return 26;
        case VK_FORMAT_G16B16G16R16_422_UNORM: return 27;
        case VK_FORMAT_B16G16R16G16_422_UNORM: return 28;
        case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM: return 29;
        case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM: return 30;
        case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM: return 31;
        case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM: return 32;
        case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM: return 33;
        case VK_FORMAT_G8_B8R8_2PLANE_444_UNORM: return 34;
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16: return 35;
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16: return 36;
        case VK_FORMAT_G16_B16R16_2PLANE_444_UNORM: return 37;
        case VK_FORMAT_A4R4G4B4_UNORM_PACK16: return 38;
        case VK_FORMAT_A4B4G4R4_UNORM_PACK16: return 39;
        case VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK: return 40;
        case VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK: return 41;
        case VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK: return 42;
        case VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK: return 43;
        case VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK: return 44;
        case VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK: return 45;
        case VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK: return 46;
        case VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK: return 47;
        case VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK: return 48;
        case VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK: return 49;
        case VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK: return 50;
        case VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK: return 51;
        case VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK: return 52;
        case VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK: return 53;
        case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG: return 56;
        case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG: return 57;
        case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG: return 58;
        case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG: return 59;
        case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG: return 60;
        case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG: return 61;
        case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG: return 62;
        case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG: return 63;
#ifndef __APPLE__
        // Apple clang++ intentionally prioritizes '/usr/local/include' over any
        // search paths provided via -I or -isystem. This means we get the
        // locally installed MoltenVk headers instead of the Rive-official
        // Vulkan headers when building for Apple.
        // The following VkFormats are not defined in MoltenVK's headers.
        case VK_FORMAT_A1B5G5R5_UNORM_PACK16: return 54;
        case VK_FORMAT_A8_UNORM: return 55;
        case VK_FORMAT_R8_BOOL_ARM: return 64;
        case VK_FORMAT_R16G16_SFIXED5_NV: return 65;
        case VK_FORMAT_R10X6_UINT_PACK16_ARM: return 66;
        case VK_FORMAT_R10X6G10X6_UINT_2PACK16_ARM: return 67;
        case VK_FORMAT_R10X6G10X6B10X6A10X6_UINT_4PACK16_ARM: return 68;
        case VK_FORMAT_R12X4_UINT_PACK16_ARM: return 69;
        case VK_FORMAT_R12X4G12X4_UINT_2PACK16_ARM: return 70;
        case VK_FORMAT_R12X4G12X4B12X4A12X4_UINT_4PACK16_ARM: return 71;
        case VK_FORMAT_R14X2_UINT_PACK16_ARM: return 72;
        case VK_FORMAT_R14X2G14X2_UINT_2PACK16_ARM: return 73;
        case VK_FORMAT_R14X2G14X2B14X2A14X2_UINT_4PACK16_ARM: return 74;
        case VK_FORMAT_R14X2_UNORM_PACK16_ARM: return 75;
        case VK_FORMAT_R14X2G14X2_UNORM_2PACK16_ARM: return 76;
        case VK_FORMAT_R14X2G14X2B14X2A14X2_UNORM_4PACK16_ARM: return 77;
        case VK_FORMAT_G14X2_B14X2R14X2_2PLANE_420_UNORM_3PACK16_ARM: return 78;
        case VK_FORMAT_G14X2_B14X2R14X2_2PLANE_422_UNORM_3PACK16_ARM: return 79;
#endif
        default: break;
            // clang-format on
    }
    assert(false && "Given sparse VkFormat is not supported");
    return (1 << RenderPassVulkan::FORMAT_BIT_COUNT) - 1 -
           (LAST_NON_SPARSE_VK_FORMAT + 1);
}

static uint32_t vk_format_key(VkFormat format)
{
    if (format <= LAST_NON_SPARSE_VK_FORMAT)
    {
        // Basic case: Almost all normal formats already fit in 8 bits.
        return static_cast<uint32_t>(format);
    }
    else
    {
        // Pack the sparse VkFormats into a tighter key.
        return vk_sparse_format_index(format) + LAST_NON_SPARSE_VK_FORMAT + 1;
    }
}

uint32_t RenderPassVulkan::KeyNoInterlockMode(
    RenderPassOptionsVulkan renderPassOptions,
    VkFormat renderTargetFormat,
    gpu::LoadAction loadAction)
{
    // gpu::LoadAction.
    assert(static_cast<uint32_t>(loadAction) < 1 << LOAD_OP_BIT_COUNT);
    uint32_t key = static_cast<uint32_t>(loadAction);

    // VkFormat.
    const uint32_t renderFormatKey = vk_format_key(renderTargetFormat);
    assert(renderFormatKey < 1 << FORMAT_BIT_COUNT);
    assert(key << FORMAT_BIT_COUNT >> FORMAT_BIT_COUNT == key);
    key = (key << FORMAT_BIT_COUNT) | renderFormatKey;

    // DrawPipelineLayoutVulkan::Options.
    assert(static_cast<uint32_t>(renderPassOptions) <
           1 << RENDER_PASS_OPTION_COUNT);
    assert(key << RENDER_PASS_OPTION_COUNT >> RENDER_PASS_OPTION_COUNT == key);
    key = (key << RENDER_PASS_OPTION_COUNT) |
          static_cast<uint32_t>(renderPassOptions);

    assert(key < 1 << KEY_NO_INTERLOCK_MODE_BIT_COUNT);
    return key;
}

uint32_t RenderPassVulkan::Key(gpu::InterlockMode interlockMode,
                               RenderPassOptionsVulkan renderPassOptions,
                               VkFormat renderTargetFormat,
                               gpu::LoadAction loadAction)
{
    uint32_t key =
        KeyNoInterlockMode(renderPassOptions, renderTargetFormat, loadAction);

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

RenderPassVulkan::RenderPassVulkan(PipelineManagerVulkan* pipelineManager,
                                   gpu::InterlockMode interlockMode,
                                   RenderPassOptionsVulkan renderPassOptions,
                                   VkFormat renderTargetFormat,
                                   gpu::LoadAction loadAction) :
    m_vk(ref_rcp(pipelineManager->vulkanContext()))
{
    m_drawPipelineLayout =
        &pipelineManager->getDrawPipelineLayoutSynchronous(interlockMode,
                                                           renderPassOptions);

    // COLOR attachment.
    const VkImageLayout colorAttachmentLayout =
        (renderPassOptions & RenderPassOptionsVulkan::fixedFunctionColorOutput)
            ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            : VK_IMAGE_LAYOUT_GENERAL;
    const VkSampleCountFlagBits msaaSampleCount =
        (interlockMode == gpu::InterlockMode::msaa) ? VK_SAMPLE_COUNT_4_BIT
                                                    : VK_SAMPLE_COUNT_1_BIT;
    StackVector<VkAttachmentDescription, layout::MAX_RENDER_PASS_ATTACHMENTS>
        attachments;
    StackVector<VkAttachmentReference, PLS_PLANE_COUNT> colorAttachmentRefs;
    std::optional<VkAttachmentReference> depthStencilAttachmentRef;
    std::optional<VkAttachmentReference> resolveAttachmentRef;
    if (pipelineManager->plsBackingType(interlockMode) ==
            PipelineManagerVulkan::PLSBackingType::inputAttachment ||
        (renderPassOptions & RenderPassOptionsVulkan::fixedFunctionColorOutput))
    {
        assert(attachments.size() == COLOR_PLANE_IDX);
        assert(colorAttachmentRefs.size() == COLOR_PLANE_IDX);
        attachments.push_back({
            .format = renderTargetFormat,
            .samples = msaaSampleCount,
            .loadOp = vk_load_op(loadAction, interlockMode),
            .storeOp = ((renderPassOptions &
                         (RenderPassOptionsVulkan::manuallyResolved |
                          RenderPassOptionsVulkan::
                              atomicCoalescedResolveAndTransfer)) ||
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
                (((renderPassOptions & RenderPassOptionsVulkan::
                                           atomicCoalescedResolveAndTransfer) &&
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
            .loadOp = (interlockMode == gpu::InterlockMode::rasterOrdering &&
                       (renderPassOptions &
                        RenderPassOptionsVulkan::rasterOrderingResume))
                          ? VK_ATTACHMENT_LOAD_OP_LOAD
                          : VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = (interlockMode == gpu::InterlockMode::rasterOrdering &&
                        (renderPassOptions &
                         RenderPassOptionsVulkan::rasterOrderingInterruptible))
                           ? VK_ATTACHMENT_STORE_OP_STORE
                           : VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout =
                (interlockMode == gpu::InterlockMode::rasterOrdering &&
                 (renderPassOptions &
                  RenderPassOptionsVulkan::rasterOrderingResume))
                    ? VK_IMAGE_LAYOUT_GENERAL
                    : VK_IMAGE_LAYOUT_UNDEFINED,
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
            .loadOp = (renderPassOptions &
                       RenderPassOptionsVulkan::rasterOrderingResume)
                          ? VK_ATTACHMENT_LOAD_OP_LOAD
                          : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp = (renderPassOptions &
                        RenderPassOptionsVulkan::rasterOrderingInterruptible)
                           ? VK_ATTACHMENT_STORE_OP_STORE
                           : VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = (renderPassOptions &
                              RenderPassOptionsVulkan::rasterOrderingResume)
                                 ? VK_IMAGE_LAYOUT_GENERAL
                                 : VK_IMAGE_LAYOUT_UNDEFINED,
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
            .loadOp = (renderPassOptions &
                       RenderPassOptionsVulkan::rasterOrderingResume)
                          ? VK_ATTACHMENT_LOAD_OP_LOAD
                          : VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = (renderPassOptions &
                        RenderPassOptionsVulkan::rasterOrderingInterruptible)
                           ? VK_ATTACHMENT_STORE_OP_STORE
                           : VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = (renderPassOptions &
                              RenderPassOptionsVulkan::rasterOrderingResume)
                                 ? VK_IMAGE_LAYOUT_GENERAL
                                 : VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
        });
        colorAttachmentRefs.push_back({
            .attachment = COVERAGE_PLANE_IDX,
            .layout = VK_IMAGE_LAYOUT_GENERAL,
        });

        if (renderPassOptions & RenderPassOptionsVulkan::manuallyResolved)
        {
            // The renderTarget does not support
            // VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, so we will instead use an
            // offscreen color texture for the main subpass, and then transfer
            // it into the renderTarget at the end of the render pass.
            assert(attachments.size() == PLS_PLANE_COUNT);
            attachments.push_back({
                .format = renderTargetFormat,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            });
            resolveAttachmentRef = {
                .attachment = PLS_PLANE_COUNT,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };
        }
    }
    else if (interlockMode == gpu::InterlockMode::atomics)
    {
        if (renderPassOptions &
            RenderPassOptionsVulkan::atomicCoalescedResolveAndTransfer)
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
            assert(!resolveAttachmentRef.has_value());
            resolveAttachmentRef = {
                .attachment = COALESCED_ATOMIC_RESOLVE_IDX,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };
        }
        else
        {
            // When not in "coalesced" mode, the resolve texture is the
            // same as the COLOR texture.
            static_assert(COLOR_PLANE_IDX == 0);
            assert(!resolveAttachmentRef.has_value());
            resolveAttachmentRef = colorAttachmentRefs[0];
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
            !(renderPassOptions &
              RenderPassOptionsVulkan::msaaSeedFromOffscreenTexture);
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
                (readsMSAAResolveAttachment ||
                 (renderPassOptions &
                  RenderPassOptionsVulkan::manuallyResolved))
                    ? msaaResolveLayout
                    // NOTE: This can only be VK_IMAGE_LAYOUT_UNDEFINED because
                    // Vulkan does not support partial resolves to MSAA resolve
                    // attachments. So every MSAA render pass without
                    // "manuallyResolved" covers the entire render area.
                    : VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = msaaResolveLayout,
        });
        resolveAttachmentRef = {
            .attachment = MSAA_RESOLVE_IDX,
            .layout = msaaResolveLayout,
        };
        assert(colorAttachmentRefs.size() == 1);

        if (renderPassOptions &
            RenderPassOptionsVulkan::msaaSeedFromOffscreenTexture)
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
        if (renderPassOptions &
            RenderPassOptionsVulkan::fixedFunctionColorOutput)
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
                    (renderPassOptions &
                     RenderPassOptionsVulkan::msaaSeedFromOffscreenTexture)
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

        if (renderPassOptions &
            RenderPassOptionsVulkan::msaaSeedFromOffscreenTexture)
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
        //  that one).
        VkSubpassDependency externalInputDeps = {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 1,
            .srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_NONE,
            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0,
        };

        if (!(renderPassOptions & RenderPassOptionsVulkan::manuallyResolved))
        {
            // If we are not doing the manual MSAA resolve, this pass also needs
            // barriers to protect the layout transition of the resolve target
            // from the load op (even though it's LOAD_OP_DONT_CARE, it is
            // possible that it performs a write), so we also need to specify
            // COLOR_ATTACHMENT_WRITE as a destination access flag.
            // (If we *were* doing the manual resolve the transition and load
            // would happen in that subpass instead of this one)
            externalInputDeps.dstStageMask |=
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            externalInputDeps.dstAccessMask |=
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        }

        subpassDeps.push_back(externalInputDeps);

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
           m_drawPipelineLayout->colorAttachmentCount(0, renderPassOptions));
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
        .pResolveAttachments =
            (interlockMode == gpu::InterlockMode::msaa &&
             !(renderPassOptions & RenderPassOptionsVulkan::manuallyResolved))
                ? &resolveAttachmentRef.value()
                : nullptr,
        .pDepthStencilAttachment = depthStencilAttachmentRef.has_value()
                                       ? &depthStencilAttachmentRef.value()
                                       : nullptr,
    });

    // Add any main subpass self-dependencies if needed
    if ((interlockMode == gpu::InterlockMode::rasterOrdering &&
         !rasterOrderedAttachmentAccess) ||
        interlockMode == gpu::InterlockMode::atomics ||
        (interlockMode == gpu::InterlockMode::msaa &&
         !(renderPassOptions &
           RenderPassOptionsVulkan::fixedFunctionColorOutput)))
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
    }

    // PLS-resolve subpass (atomic mode only).
    if (interlockMode == gpu::InterlockMode::atomics)
    {
        // Add the dependency from main subpass to the resolve subpass.
        addStandardColorDependencyToNextSubpass(subpassDescs.size());

        // The resolve happens in a separate subpass.
        assert(subpassDescs.size() == 1);
        assert(
            m_drawPipelineLayout->colorAttachmentCount(1, renderPassOptions) ==
            1);
        assert(resolveAttachmentRef.has_value());
        subpassDescs.push_back({
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = inputAttachmentRefs.size(),
            .pInputAttachments = inputAttachmentRefs.data(),
            .colorAttachmentCount = 1,
            .pColorAttachments = &resolveAttachmentRef.value(),
        });
    }
    else if (renderPassOptions & RenderPassOptionsVulkan::manuallyResolved)
    {
        assert(!(renderPassOptions &
                 RenderPassOptionsVulkan::fixedFunctionColorOutput));
        // Manually resolved render passes aren't currently compatible with
        // interruptions.
        assert(!(renderPassOptions &
                 RenderPassOptionsVulkan::rasterOrderingInterruptible));
        assert(inputAttachmentRefs[0].attachment == COLOR_PLANE_IDX);

        addStandardColorDependencyToNextSubpass(subpassDescs.size());

        subpassDescs.push_back({
            .flags =
                rasterOrderedAttachmentAccess
                    ? VK_SUBPASS_DESCRIPTION_RASTERIZATION_ORDER_ATTACHMENT_COLOR_ACCESS_BIT_EXT
                    : 0u,
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = 1u,
            .pInputAttachments = inputAttachmentRefs.data(),
            .colorAttachmentCount = 1u,
            .pColorAttachments = &resolveAttachmentRef.value(),
        });
    }

    // There always needs to be a final external output dependency for the final
    // rendering target.
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
         << ", renderPassOptions=" << int(renderPassOptions)
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
