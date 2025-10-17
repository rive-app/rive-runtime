/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"
#include "rive/renderer/vulkan/vulkan_context.hpp"
#include "rive/renderer/stack_vector.hpp"
#include "shaders/constants.glsl"
#include "common_layouts.hpp"
#include "draw_pipeline_vulkan.hpp"
#include "pipeline_manager_vulkan.hpp"
#include "render_pass_vulkan.hpp"

namespace rive::gpu
{
static VkStencilOp vk_stencil_op(StencilOp op)
{
    switch (op)
    {
        case StencilOp::keep:
            return VK_STENCIL_OP_KEEP;
        case StencilOp::replace:
            return VK_STENCIL_OP_REPLACE;
        case StencilOp::zero:
            return VK_STENCIL_OP_ZERO;
        case StencilOp::decrClamp:
            return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case StencilOp::incrWrap:
            return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case StencilOp::decrWrap:
            return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    }
    RIVE_UNREACHABLE();
}

static VkCompareOp vk_compare_op(gpu::StencilCompareOp op)
{
    switch (op)
    {
        case gpu::StencilCompareOp::less:
            return VK_COMPARE_OP_LESS;
        case gpu::StencilCompareOp::equal:
            return VK_COMPARE_OP_EQUAL;
        case gpu::StencilCompareOp::lessOrEqual:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
        case gpu::StencilCompareOp::notEqual:
            return VK_COMPARE_OP_NOT_EQUAL;
        case gpu::StencilCompareOp::always:
            return VK_COMPARE_OP_ALWAYS;
    }
    RIVE_UNREACHABLE();
}

static VkCullModeFlags vk_cull_mode(CullFace cullFace)
{
    switch (cullFace)
    {
        case CullFace::none:
            return VK_CULL_MODE_NONE;
        case CullFace::clockwise:
            return VK_CULL_MODE_FRONT_BIT;
        case CullFace::counterclockwise:
            return VK_CULL_MODE_BACK_BIT;
    }
    RIVE_UNREACHABLE();
}

static VkBlendOp vk_blend_op(gpu::BlendEquation equation)
{
    switch (equation)
    {
        case gpu::BlendEquation::none:
        case gpu::BlendEquation::srcOver:
            return VK_BLEND_OP_ADD;
        case gpu::BlendEquation::plus:
            return VK_BLEND_OP_ADD;
        case gpu::BlendEquation::max:
            return VK_BLEND_OP_MAX;
        case gpu::BlendEquation::screen:
            return VK_BLEND_OP_SCREEN_EXT;
        case gpu::BlendEquation::overlay:
            return VK_BLEND_OP_OVERLAY_EXT;
        case gpu::BlendEquation::darken:
            return VK_BLEND_OP_DARKEN_EXT;
        case gpu::BlendEquation::lighten:
            return VK_BLEND_OP_LIGHTEN_EXT;
        case gpu::BlendEquation::colorDodge:
            return VK_BLEND_OP_COLORDODGE_EXT;
        case gpu::BlendEquation::colorBurn:
            return VK_BLEND_OP_COLORBURN_EXT;
        case gpu::BlendEquation::hardLight:
            return VK_BLEND_OP_HARDLIGHT_EXT;
        case gpu::BlendEquation::softLight:
            return VK_BLEND_OP_SOFTLIGHT_EXT;
        case gpu::BlendEquation::difference:
            return VK_BLEND_OP_DIFFERENCE_EXT;
        case gpu::BlendEquation::exclusion:
            return VK_BLEND_OP_EXCLUSION_EXT;
        case gpu::BlendEquation::multiply:
            return VK_BLEND_OP_MULTIPLY_EXT;
        case gpu::BlendEquation::hue:
            return VK_BLEND_OP_HSL_HUE_EXT;
        case gpu::BlendEquation::saturation:
            return VK_BLEND_OP_HSL_SATURATION_EXT;
        case gpu::BlendEquation::color:
            return VK_BLEND_OP_HSL_COLOR_EXT;
        case gpu::BlendEquation::luminosity:
            return VK_BLEND_OP_HSL_LUMINOSITY_EXT;
    }
    RIVE_UNREACHABLE();
}

uint64_t DrawPipelineVulkan::PipelineProps::createKey() const
{
    uint64_t key = gpu::ShaderUniqueKey(drawType,
                                        shaderFeatures,
                                        interlockMode,
                                        shaderMiscFlags);

    const uint32_t renderPassKey = RenderPassVulkan::Key(interlockMode,
                                                         pipelineLayoutOptions,
                                                         renderTargetFormat,
                                                         colorLoadAction);

    const uint32_t pipelineStateKey = pipelineState.uniqueKey;
    assert(key << OPTION_COUNT >> OPTION_COUNT == key);
    key = (key << OPTION_COUNT) | static_cast<uint32_t>(drawPipelineOptions);

    assert(key << gpu::PipelineState::UNIQUE_KEY_BIT_COUNT >>
               gpu::PipelineState::UNIQUE_KEY_BIT_COUNT ==
           key);
    assert(pipelineStateKey < 1 << gpu::PipelineState::UNIQUE_KEY_BIT_COUNT);
    key = (key << gpu::PipelineState::UNIQUE_KEY_BIT_COUNT) | pipelineStateKey;

    assert(key << RenderPassVulkan::KEY_BIT_COUNT >>
               RenderPassVulkan::KEY_BIT_COUNT ==
           key);
    assert(renderPassKey < 1 << RenderPassVulkan::KEY_BIT_COUNT);
    key = (key << RenderPassVulkan::KEY_BIT_COUNT) | renderPassKey;

    return key;
}

uint32_t subpass_index(gpu::DrawType drawType,
                       gpu::LoadAction colorLoadAction,
                       gpu::InterlockMode interlockMode)
{
    const uint32_t mainSubpassIdx =
        (interlockMode == gpu::InterlockMode::msaa &&
         colorLoadAction == gpu::LoadAction::preserveRenderTarget)
            ? 1
            : 0;
    switch (drawType)
    {
        case gpu::DrawType::renderPassInitialize:
            assert(mainSubpassIdx == 1);
            return 0;
        case gpu::DrawType::midpointFanPatches:
        case gpu::DrawType::midpointFanCenterAAPatches:
        case gpu::DrawType::outerCurvePatches:
        case gpu::DrawType::interiorTriangulation:
        case gpu::DrawType::atlasBlit:
        case gpu::DrawType::imageRect:
        case gpu::DrawType::imageMesh:
        case gpu::DrawType::msaaStrokes:
        case gpu::DrawType::msaaMidpointFanBorrowedCoverage:
        case gpu::DrawType::msaaMidpointFans:
        case gpu::DrawType::msaaMidpointFanStencilReset:
        case gpu::DrawType::msaaMidpointFanPathsStencil:
        case gpu::DrawType::msaaMidpointFanPathsCover:
        case gpu::DrawType::msaaOuterCubics:
        case gpu::DrawType::msaaStencilClipReset:
            return mainSubpassIdx;
        case gpu::DrawType::renderPassResolve:
            return mainSubpassIdx + 1;
    }

    RIVE_UNREACHABLE();
}

DrawPipelineVulkan::DrawPipelineVulkan(
    PipelineManagerVulkan* pipelineManager,
    const DrawPipelineLayoutVulkan& pipelineLayout,
    const PipelineProps& props,
    VkRenderPass vkRenderPass
#ifdef WITH_RIVE_TOOLS
    ,
    SynthesizedFailureType synthesizedFailureType
#endif

    ) :
    m_vk(ref_rcp(pipelineManager->vulkanContext()))
{
#ifdef WITH_RIVE_TOOLS
    if (synthesizedFailureType == SynthesizedFailureType::pipelineCreation)
    {
        return;
    }
#endif

    const gpu::PipelineState& pipelineState = props.pipelineState;
    const gpu::InterlockMode interlockMode = pipelineLayout.interlockMode();
    uint32_t subpassIndex =
        subpass_index(props.drawType, props.colorLoadAction, interlockMode);

    auto& vertShader =
        pipelineManager->getVertexShaderSynchronous(props.drawType,
                                                    props.shaderFeatures,
                                                    interlockMode);

    if (vertShader.module() == VK_NULL_HANDLE)
    {
        return;
    }
    auto& fragShader =
        pipelineManager->getFragmentShaderSynchronous(props.drawType,
                                                      props.shaderFeatures,
                                                      interlockMode,
                                                      props.shaderMiscFlags);
    if (fragShader.module() == VK_NULL_HANDLE)
    {
        return;
    }

    uint32_t shaderPermutationFlags[SPECIALIZATION_COUNT] = {
        props.shaderFeatures & gpu::ShaderFeatures::ENABLE_CLIPPING,
        props.shaderFeatures & gpu::ShaderFeatures::ENABLE_CLIP_RECT,
        props.shaderFeatures & gpu::ShaderFeatures::ENABLE_ADVANCED_BLEND,
        props.shaderFeatures & gpu::ShaderFeatures::ENABLE_FEATHER,
        props.shaderFeatures & gpu::ShaderFeatures::ENABLE_EVEN_ODD,
        props.shaderFeatures & gpu::ShaderFeatures::ENABLE_NESTED_CLIPPING,
        props.shaderFeatures & gpu::ShaderFeatures::ENABLE_HSL_BLEND_MODES,
        props.shaderMiscFlags & gpu::ShaderMiscFlags::clockwiseFill,
        props.shaderMiscFlags & gpu::ShaderMiscFlags::borrowedCoveragePrepass,
        pipelineManager->vendorID(),
    };
    static_assert(CLIPPING_SPECIALIZATION_IDX == 0);
    static_assert(CLIP_RECT_SPECIALIZATION_IDX == 1);
    static_assert(ADVANCED_BLEND_SPECIALIZATION_IDX == 2);
    static_assert(FEATHER_SPECIALIZATION_IDX == 3);
    static_assert(EVEN_ODD_SPECIALIZATION_IDX == 4);
    static_assert(NESTED_CLIPPING_SPECIALIZATION_IDX == 5);
    static_assert(HSL_BLEND_MODES_SPECIALIZATION_IDX == 6);
    static_assert(CLOCKWISE_FILL_SPECIALIZATION_IDX == 7);
    static_assert(BORROWED_COVERAGE_PREPASS_SPECIALIZATION_IDX == 8);
    static_assert(VULKAN_VENDOR_ID_SPECIALIZATION_IDX == 9);
    static_assert(SPECIALIZATION_COUNT == 10);

    VkSpecializationMapEntry permutationMapEntries[SPECIALIZATION_COUNT];
    for (uint32_t i = 0; i < SPECIALIZATION_COUNT; ++i)
    {
        permutationMapEntries[i] = {
            .constantID = i,
            .offset = i * static_cast<uint32_t>(sizeof(uint32_t)),
            .size = sizeof(uint32_t),
        };
    }

    VkSpecializationInfo specializationInfo = {
        .mapEntryCount = SPECIALIZATION_COUNT,
        .pMapEntries = permutationMapEntries,
        .dataSize = sizeof(shaderPermutationFlags),
        .pData = &shaderPermutationFlags,
    };

    VkPipelineShaderStageCreateInfo stages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertShader.module(),
            .pName = "main",
            .pSpecializationInfo = &specializationInfo,
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragShader.module(),
            .pName = "main",
            .pSpecializationInfo = &specializationInfo,
        },
    };

    VkPipelineRasterizationStateCreateInfo
        pipelineRasterizationStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = (props.drawPipelineOptions & Options::wireframe)
                               ? VK_POLYGON_MODE_LINE
                               : VK_POLYGON_MODE_FILL,
            .cullMode = vk_cull_mode(pipelineState.cullFace),
            .frontFace = VK_FRONT_FACE_CLOCKWISE,
            .lineWidth = 1.0,
        };

    gpu::BlendEquation blendEquation = pipelineState.blendEquation;
    bool blendEquationPremultiplied = true;
    // The pipeline state gets generated under the assumption that pixel local
    // storage can still be written when colorWriteEnabled is false. So when
    // Vulkan implements PLS via color attachments, we need to override the
    // colorWriteEnabled state.
    const bool usesColorAttachmentsForPLS =
        interlockMode != gpu::InterlockMode::msaa;
    bool colorWriteEnabled =
        pipelineState.colorWriteEnabled || usesColorAttachmentsForPLS;
    if (interlockMode == gpu::InterlockMode::atomics &&
        !(props.shaderMiscFlags &
          gpu::ShaderMiscFlags::coalescedResolveAndTransfer))
    {
        // Vulkan deviates from the other renderers by enabling src-over
        // blending for PLS planes in atomic mode.
        //
        // Advanced blend modes are handled by rearranging the math such that
        // the correct color isn't reached until *AFTER* this blend state is
        // applied. This primarily benefits us by hinting to the hardware that
        // it doesn't need to read or write anything when a == 0, but it also
        // saves flops by offloading the blend work onto the ROP blending unit.
        //
        // Clip also has blend enabled, which allows us to preserve both clip
        // and color contents by just emitting a=0 (instead of loading and
        // re-emitting the current value) when a PLS plane needs to remain
        // unchanged during a fragment invocation.
        blendEquation = gpu::BlendEquation::srcOver;
        // Image draws use premultiplied src-over so they can blend the image
        // with the previous paint together, out of order. (Premultiplied
        // src-over is associative.)
        //
        // Otherwise we save 3 flops and let the ROP multiply alpha into the
        // color when it does the blending.
        blendEquationPremultiplied = gpu::DrawTypeIsImageDraw(props.drawType);
    }
#ifndef NDEBUG
    else if (props.shaderMiscFlags &
             gpu::ShaderMiscFlags::coalescedResolveAndTransfer)
    {
        assert(interlockMode == gpu::InterlockMode::atomics);
        assert(blendEquation == gpu::BlendEquation::none);
    }
#endif
    else if (interlockMode == gpu::InterlockMode::clockwiseAtomic)
    {
        // Clockwise mode is still an experimental Vulkan-only feature.
        // Override the pipeline blend state.
        if (props.shaderMiscFlags &
            gpu::ShaderMiscFlags::borrowedCoveragePrepass)
        {
            // Borrowed coverage clockwise draws only update the coverage buffer
            // (which is not a render target attachment).
            blendEquation = gpu::BlendEquation::none;
            colorWriteEnabled = false;
        }
        else
        {
            // Forward coverage clockwise draws are all unmultiplied src-over.
            blendEquation = gpu::BlendEquation::srcOver;
            blendEquationPremultiplied = false;
        }
    }

    StackVector<VkPipelineColorBlendAttachmentState, PLS_PLANE_COUNT>
        blendStates;
    blendStates.push_back_n(
        pipelineLayout.colorAttachmentCount(subpassIndex),
        {
            .blendEnable = blendEquation != gpu::BlendEquation::none,
            .srcColorBlendFactor = blendEquationPremultiplied
                                       ? VK_BLEND_FACTOR_ONE
                                       : VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp = vk_blend_op(pipelineState.blendEquation),
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .alphaBlendOp = vk_blend_op(pipelineState.blendEquation),
            .colorWriteMask = colorWriteEnabled ? vkutil::kColorWriteMaskRGBA
                                                : vkutil::kColorWriteMaskNone,
        });
    VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = blendStates.size(),
        .pAttachments = blendStates.data(),
    };

    if (interlockMode == gpu::InterlockMode::rasterOrdering &&
        m_vk->features.rasterizationOrderColorAttachmentAccess)
    {
        pipelineColorBlendStateCreateInfo.flags |=
            VK_PIPELINE_COLOR_BLEND_STATE_CREATE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_BIT_EXT;
    }

    VkPipelineDepthStencilStateCreateInfo depthStencilState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = pipelineState.depthTestEnabled,
        .depthWriteEnable = pipelineState.depthWriteEnabled,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = pipelineState.stencilTestEnabled,
        .minDepthBounds = gpu::DEPTH_MIN,
        .maxDepthBounds = gpu::DEPTH_MAX,
    };
    if (pipelineState.stencilTestEnabled)
    {
        depthStencilState.front = {
            .failOp = vk_stencil_op(pipelineState.stencilFrontOps.failOp),
            .passOp = vk_stencil_op(pipelineState.stencilFrontOps.passOp),
            .depthFailOp =
                vk_stencil_op(pipelineState.stencilFrontOps.depthFailOp),
            .compareOp = vk_compare_op(pipelineState.stencilFrontOps.compareOp),
            .compareMask = pipelineState.stencilCompareMask,
            .writeMask = pipelineState.stencilWriteMask,
            .reference = pipelineState.stencilReference,
        };
        depthStencilState.back =
            !pipelineState.stencilDoubleSided
                ? depthStencilState.front
                : VkStencilOpState{
                      .failOp =
                          vk_stencil_op(pipelineState.stencilBackOps.failOp),
                      .passOp =
                          vk_stencil_op(pipelineState.stencilBackOps.passOp),
                      .depthFailOp = vk_stencil_op(
                          pipelineState.stencilBackOps.depthFailOp),
                      .compareOp =
                          vk_compare_op(pipelineState.stencilBackOps.compareOp),
                      .compareMask = pipelineState.stencilCompareMask,
                      .writeMask = pipelineState.stencilWriteMask,
                      .reference = pipelineState.stencilReference,
                  };
    }

    VkPipelineMultisampleStateCreateInfo msaaState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = interlockMode == gpu::InterlockMode::msaa
                                    ? VK_SAMPLE_COUNT_4_BIT
                                    : VK_SAMPLE_COUNT_1_BIT,
    };

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = stages,
        .pViewportState = &layout::SINGLE_VIEWPORT,
        .pRasterizationState = &pipelineRasterizationStateCreateInfo,
        .pMultisampleState = &msaaState,
        .pDepthStencilState = interlockMode == gpu::InterlockMode::msaa
                                  ? &depthStencilState
                                  : nullptr,
        .pColorBlendState = &pipelineColorBlendStateCreateInfo,
        .pDynamicState = &layout::DYNAMIC_VIEWPORT_SCISSOR,
        .layout = *pipelineLayout,
        .renderPass = vkRenderPass,
        .subpass = subpassIndex,
    };

    switch (props.drawType)
    {
        case DrawType::midpointFanPatches:
        case DrawType::midpointFanCenterAAPatches:
        case DrawType::outerCurvePatches:
        case DrawType::msaaOuterCubics:
        case DrawType::msaaStrokes:
        case DrawType::msaaMidpointFanBorrowedCoverage:
        case DrawType::msaaMidpointFans:
        case DrawType::msaaMidpointFanStencilReset:
        case DrawType::msaaMidpointFanPathsStencil:
        case DrawType::msaaMidpointFanPathsCover:
            pipelineCreateInfo.pVertexInputState =
                &layout::PATH_VERTEX_INPUT_STATE;
            pipelineCreateInfo.pInputAssemblyState =
                &layout::INPUT_ASSEMBLY_TRIANGLE_LIST;
            break;

        case DrawType::msaaStencilClipReset:
        case DrawType::interiorTriangulation:
        case DrawType::atlasBlit:
            pipelineCreateInfo.pVertexInputState =
                &layout::INTERIOR_TRI_VERTEX_INPUT_STATE;
            pipelineCreateInfo.pInputAssemblyState =
                &layout::INPUT_ASSEMBLY_TRIANGLE_LIST;
            break;

        case DrawType::imageRect:
            pipelineCreateInfo.pVertexInputState =
                &layout::IMAGE_RECT_VERTEX_INPUT_STATE;
            pipelineCreateInfo.pInputAssemblyState =
                &layout::INPUT_ASSEMBLY_TRIANGLE_LIST;
            break;

        case DrawType::imageMesh:
            pipelineCreateInfo.pVertexInputState =
                &layout::IMAGE_MESH_VERTEX_INPUT_STATE;
            pipelineCreateInfo.pInputAssemblyState =
                &layout::INPUT_ASSEMBLY_TRIANGLE_LIST;
            break;

        case DrawType::renderPassResolve:
        case DrawType::renderPassInitialize:
            pipelineCreateInfo.pVertexInputState =
                &layout::EMPTY_VERTEX_INPUT_STATE;
            pipelineCreateInfo.pInputAssemblyState =
                &layout::INPUT_ASSEMBLY_TRIANGLE_STRIP;
            break;
    }

    if (m_vk->CreateGraphicsPipelines(m_vk->device,
                                      VK_NULL_HANDLE,
                                      1,
                                      &pipelineCreateInfo,
                                      nullptr,
                                      &m_vkPipeline) != VK_SUCCESS)
    {
        m_vkPipeline = VK_NULL_HANDLE;
    }
}

DrawPipelineVulkan::~DrawPipelineVulkan()
{
    m_vk->DestroyPipeline(m_vk->device, m_vkPipeline, nullptr);
}
} // namespace rive::gpu
