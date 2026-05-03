/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_context.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/rive_types.hpp"
#include "ore_vulkan_dsl.hpp"

#include <cassert>

namespace rive::ore
{

// hasStencilLocal() is defined in ore_context_vulkan.cpp. When this file is
// compiled standalone (VK-only), the context file is a separate TU so we need
// our own copy. When included by ore_context_vk_gl.cpp (VK+GL dispatch), the
// context file is included first and already provides it.
#if !defined(ORE_BACKEND_GL)
static bool hasStencilLocal(TextureFormat fmt)
{
    return fmt == TextureFormat::depth24plusStencil8 ||
           fmt == TextureFormat::depth32floatStencil8;
}
#endif

// ============================================================================
// Enum → Vulkan conversion helpers
// ============================================================================

static VkFormat oreVertexFormatToVk(VertexFormat fmt)
{
    switch (fmt)
    {
        case VertexFormat::float1:
            return VK_FORMAT_R32_SFLOAT;
        case VertexFormat::float2:
            return VK_FORMAT_R32G32_SFLOAT;
        case VertexFormat::float3:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case VertexFormat::float4:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case VertexFormat::uint8x4:
            return VK_FORMAT_R8G8B8A8_UINT;
        case VertexFormat::sint8x4:
            return VK_FORMAT_R8G8B8A8_SINT;
        case VertexFormat::unorm8x4:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case VertexFormat::snorm8x4:
            return VK_FORMAT_R8G8B8A8_SNORM;
        case VertexFormat::uint16x2:
            return VK_FORMAT_R16G16_UINT;
        case VertexFormat::sint16x2:
            return VK_FORMAT_R16G16_SINT;
        case VertexFormat::unorm16x2:
            return VK_FORMAT_R16G16_UNORM;
        case VertexFormat::snorm16x2:
            return VK_FORMAT_R16G16_SNORM;
        case VertexFormat::uint16x4:
            return VK_FORMAT_R16G16B16A16_UINT;
        case VertexFormat::sint16x4:
            return VK_FORMAT_R16G16B16A16_SINT;
        case VertexFormat::float16x2:
            return VK_FORMAT_R16G16_SFLOAT;
        case VertexFormat::float16x4:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case VertexFormat::uint32:
            return VK_FORMAT_R32_UINT;
        case VertexFormat::uint32x2:
            return VK_FORMAT_R32G32_UINT;
        case VertexFormat::uint32x3:
            return VK_FORMAT_R32G32B32_UINT;
        case VertexFormat::uint32x4:
            return VK_FORMAT_R32G32B32A32_UINT;
        case VertexFormat::sint32:
            return VK_FORMAT_R32_SINT;
        case VertexFormat::sint32x2:
            return VK_FORMAT_R32G32_SINT;
        case VertexFormat::sint32x3:
            return VK_FORMAT_R32G32B32_SINT;
        case VertexFormat::sint32x4:
            return VK_FORMAT_R32G32B32A32_SINT;
    }
    RIVE_UNREACHABLE();
}

static VkPrimitiveTopology oreTopologyToVk(PrimitiveTopology t)
{
    switch (t)
    {
        case PrimitiveTopology::pointList:
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case PrimitiveTopology::lineList:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case PrimitiveTopology::lineStrip:
            return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case PrimitiveTopology::triangleList:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case PrimitiveTopology::triangleStrip:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    }
    RIVE_UNREACHABLE();
}

static VkCullModeFlags oreCullModeToVk(CullMode c)
{
    switch (c)
    {
        case CullMode::none:
            return VK_CULL_MODE_NONE;
        case CullMode::front:
            return VK_CULL_MODE_FRONT_BIT;
        case CullMode::back:
            return VK_CULL_MODE_BACK_BIT;
    }
    RIVE_UNREACHABLE();
}

static VkFrontFace oreWindingToVk(FaceWinding w)
{
    // Vulkan's default is counter-clockwise = front.
    return (w == FaceWinding::counterClockwise)
               ? VK_FRONT_FACE_COUNTER_CLOCKWISE
               : VK_FRONT_FACE_CLOCKWISE;
}

static VkBlendFactor oreBlendFactorToVk(BlendFactor f)
{
    switch (f)
    {
        case BlendFactor::zero:
            return VK_BLEND_FACTOR_ZERO;
        case BlendFactor::one:
            return VK_BLEND_FACTOR_ONE;
        case BlendFactor::srcColor:
            return VK_BLEND_FACTOR_SRC_COLOR;
        case BlendFactor::oneMinusSrcColor:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case BlendFactor::srcAlpha:
            return VK_BLEND_FACTOR_SRC_ALPHA;
        case BlendFactor::oneMinusSrcAlpha:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::dstColor:
            return VK_BLEND_FACTOR_DST_COLOR;
        case BlendFactor::oneMinusDstColor:
            return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case BlendFactor::dstAlpha:
            return VK_BLEND_FACTOR_DST_ALPHA;
        case BlendFactor::oneMinusDstAlpha:
            return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case BlendFactor::srcAlphaSaturated:
            return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        case BlendFactor::blendColor:
            return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case BlendFactor::oneMinusBlendColor:
            return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    }
    RIVE_UNREACHABLE();
}

static VkBlendOp oreBlendOpToVk(BlendOp op)
{
    switch (op)
    {
        case BlendOp::add:
            return VK_BLEND_OP_ADD;
        case BlendOp::subtract:
            return VK_BLEND_OP_SUBTRACT;
        case BlendOp::reverseSubtract:
            return VK_BLEND_OP_REVERSE_SUBTRACT;
        case BlendOp::min:
            return VK_BLEND_OP_MIN;
        case BlendOp::max:
            return VK_BLEND_OP_MAX;
    }
    RIVE_UNREACHABLE();
}

static VkColorComponentFlags oreColorWriteMaskToVk(ColorWriteMask mask)
{
    VkColorComponentFlags result = 0;
    if ((mask & ColorWriteMask::red) != ColorWriteMask::none)
        result |= VK_COLOR_COMPONENT_R_BIT;
    if ((mask & ColorWriteMask::green) != ColorWriteMask::none)
        result |= VK_COLOR_COMPONENT_G_BIT;
    if ((mask & ColorWriteMask::blue) != ColorWriteMask::none)
        result |= VK_COLOR_COMPONENT_B_BIT;
    if ((mask & ColorWriteMask::alpha) != ColorWriteMask::none)
        result |= VK_COLOR_COMPONENT_A_BIT;
    return result;
}

static VkCompareOp oreCompareFuncToVk(CompareFunction f)
{
    switch (f)
    {
        case CompareFunction::none:
        case CompareFunction::always:
            return VK_COMPARE_OP_ALWAYS;
        case CompareFunction::never:
            return VK_COMPARE_OP_NEVER;
        case CompareFunction::less:
            return VK_COMPARE_OP_LESS;
        case CompareFunction::equal:
            return VK_COMPARE_OP_EQUAL;
        case CompareFunction::lessEqual:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
        case CompareFunction::greater:
            return VK_COMPARE_OP_GREATER;
        case CompareFunction::notEqual:
            return VK_COMPARE_OP_NOT_EQUAL;
        case CompareFunction::greaterEqual:
            return VK_COMPARE_OP_GREATER_OR_EQUAL;
    }
    RIVE_UNREACHABLE();
}

static VkStencilOp oreStencilOpToVk(StencilOp op)
{
    switch (op)
    {
        case StencilOp::keep:
            return VK_STENCIL_OP_KEEP;
        case StencilOp::zero:
            return VK_STENCIL_OP_ZERO;
        case StencilOp::replace:
            return VK_STENCIL_OP_REPLACE;
        case StencilOp::incrementClamp:
            return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case StencilOp::decrementClamp:
            return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case StencilOp::invert:
            return VK_STENCIL_OP_INVERT;
        case StencilOp::incrementWrap:
            return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case StencilOp::decrementWrap:
            return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    }
    RIVE_UNREACHABLE();
}

static VkStencilOpState oreStencilFaceToVk(const StencilFaceState& s,
                                           uint8_t readMask,
                                           uint8_t writeMask)
{
    VkStencilOpState state{};
    state.failOp = oreStencilOpToVk(s.failOp);
    state.passOp = oreStencilOpToVk(s.passOp);
    state.depthFailOp = oreStencilOpToVk(s.depthFailOp);
    state.compareOp = oreCompareFuncToVk(s.compare);
    state.compareMask = readMask;
    state.writeMask = writeMask;
    state.reference = 0; // Dynamic state — set via vkCmdSetStencilReference.
    return state;
}

// ============================================================================
// Pipeline::onRefCntReachedZero
//
// Phase E: per-group VkDescriptorSetLayouts are now owned by BindGroupLayout
// objects (referenced by m_layouts[]). Pipeline only destroys its own
// VkPipeline + VkPipelineLayout; DSL teardown is handled by
// BindGroupLayout::onRefCntReachedZero when its rcp drops.
// ============================================================================

#if !defined(ORE_BACKEND_GL)

void Pipeline::onRefCntReachedZero() const
{
    VkDevice dev = m_vkDevice;
    VkPipeline pipe = m_vkPipeline;
    VkPipelineLayout layout = m_vkPipelineLayout;
    auto destroyPipe = m_vkDestroyPipeline;
    auto destroyLayout = m_vkDestroyPipelineLayout;
    Context* ctx = m_vkOreContext;

    auto destroy = [=]() {
        if (pipe != VK_NULL_HANDLE && destroyPipe != nullptr)
            destroyPipe(dev, pipe, nullptr);
        if (layout != VK_NULL_HANDLE && destroyLayout != nullptr)
            destroyLayout(dev, layout, nullptr);
    };

    delete this;

    if (ctx != nullptr)
        ctx->vkDeferDestroy(std::move(destroy));
    else
        destroy();
}

// ============================================================================
// BindGroupLayout::onRefCntReachedZero (Vulkan)
// ============================================================================

void BindGroupLayout::onRefCntReachedZero() const
{
    VkDevice dev = m_vkDevice;
    VkDescriptorSetLayout dsl = m_vkDSL;
    auto destroyDSL = m_vkDestroyDescriptorSetLayout;
    Context* ctx = m_context;

    auto destroy = [=]() {
        if (dsl != VK_NULL_HANDLE && destroyDSL != nullptr)
            destroyDSL(dev, dsl, nullptr);
    };

    delete this;

    if (ctx != nullptr)
        ctx->vkDeferDestroy(std::move(destroy));
    else
        destroy();
}

// ============================================================================
// Context::makePipeline — called from ore_context_vulkan.cpp
// ============================================================================

rcp<Pipeline> Context::makePipeline(const PipelineDesc& desc,
                                    std::string* outError)
{
    auto pipeline = rcp<Pipeline>(new Pipeline(desc));
    pipeline->m_vkDevice = m_vkDevice;
    pipeline->m_vkOreContext = this;
    pipeline->m_vkTopology = oreTopologyToVk(desc.topology);

    // --- Validate user-supplied layouts against shader binding map ---
    {
        std::string err;
        if (!validateLayoutsAgainstBindingMap(pipeline->m_bindingMap,
                                              desc.bindGroupLayouts,
                                              desc.bindGroupLayoutCount,
                                              &err))
        {
            if (outError)
                *outError = err;
            else
                setLastError("makePipeline: %s", err.c_str());
            return nullptr;
        }
    }

    // --- Pipeline layout — composed from user-supplied BGLs ---
    // Vulkan spec (VUID-VkPipelineLayoutCreateInfo-
    // graphicsPipelineLibrary-06753) requires every pSetLayouts entry to
    // be a valid handle. Null slots get the shared empty DSL.
    VkDescriptorSetLayout dsls[kVkMaxGroups] = {};
    VkDescriptorSetLayout emptyDSL = VK_NULL_HANDLE;
    for (uint32_t g = 0; g < desc.bindGroupLayoutCount && g < kVkMaxGroups; ++g)
    {
        if (desc.bindGroupLayouts[g] != nullptr)
        {
            dsls[g] = desc.bindGroupLayouts[g]->m_vkDSL;
        }
        else
        {
            if (emptyDSL == VK_NULL_HANDLE)
                emptyDSL = vkGetOrCreateEmptyDSL();
            dsls[g] = emptyDSL;
        }
    }
    VkPipelineLayoutCreateInfo layoutCI{};
    layoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCI.setLayoutCount = desc.bindGroupLayoutCount;
    layoutCI.pSetLayouts = desc.bindGroupLayoutCount > 0 ? dsls : nullptr;
    m_vk.CreatePipelineLayout(m_vkDevice,
                              &layoutCI,
                              nullptr,
                              &pipeline->m_vkPipelineLayout);
    pipeline->m_vkDestroyPipelineLayout = m_vk.DestroyPipelineLayout;

    // --- Shader stages ---
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = desc.vertexModule->m_vkShaderModule;
    stages[0].pName = desc.vertexEntryPoint;

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = desc.fragmentModule->m_vkShaderModule;
    stages[1].pName = desc.fragmentEntryPoint;

    // --- Vertex input ---
    constexpr uint32_t kMaxBindings = 8;
    constexpr uint32_t kMaxAttribs = 32;
    VkVertexInputBindingDescription bindings[kMaxBindings];
    VkVertexInputAttributeDescription attribs[kMaxAttribs];
    uint32_t attribIdx = 0;

    for (uint32_t b = 0; b < desc.vertexBufferCount; ++b)
    {
        const VertexBufferLayout& layout = desc.vertexBuffers[b];
        bindings[b].binding = b;
        bindings[b].stride = layout.stride;
        bindings[b].inputRate = (layout.stepMode == VertexStepMode::instance)
                                    ? VK_VERTEX_INPUT_RATE_INSTANCE
                                    : VK_VERTEX_INPUT_RATE_VERTEX;
        for (uint32_t a = 0; a < layout.attributeCount; ++a)
        {
            assert(attribIdx < kMaxAttribs);
            const VertexAttribute& attr = layout.attributes[a];
            attribs[attribIdx].location = attr.shaderSlot;
            attribs[attribIdx].binding = b;
            attribs[attribIdx].format = oreVertexFormatToVk(attr.format);
            attribs[attribIdx].offset = attr.offset;
            ++attribIdx;
        }
    }

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = desc.vertexBufferCount;
    vertexInput.pVertexBindingDescriptions = bindings;
    vertexInput.vertexAttributeDescriptionCount = attribIdx;
    vertexInput.pVertexAttributeDescriptions = attribs;

    // --- Input assembly ---
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = pipeline->m_vkTopology;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // --- Viewport / scissor (dynamic) ---
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // --- Rasterization ---
    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = oreCullModeToVk(desc.cullMode);
    raster.frontFace = oreWindingToVk(desc.winding);
    raster.lineWidth = 1.0f;
    if (desc.depthStencil.depthBias != 0 ||
        desc.depthStencil.depthBiasSlopeScale != 0.0f)
    {
        raster.depthBiasEnable = VK_TRUE;
        raster.depthBiasConstantFactor =
            static_cast<float>(desc.depthStencil.depthBias);
        raster.depthBiasSlopeFactor = desc.depthStencil.depthBiasSlopeScale;
        raster.depthBiasClamp = desc.depthStencil.depthBiasClamp;
    }

    // --- Multisample ---
    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples =
        static_cast<VkSampleCountFlagBits>(desc.sampleCount);

    // --- Depth / stencil ---
    // Use rgba8unorm as a sentinel for "no depth/stencil attachment".
    // Vulkan forbids enabling depth/stencil test when the render pass has no
    // depth/stencil attachment
    // (VUID-VkGraphicsPipelineCreateInfo-renderPass-07752).
    const bool hasDepthStencil =
        (desc.depthStencil.format != TextureFormat::rgba8unorm);

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable =
        (hasDepthStencil &&
         desc.depthStencil.depthCompare != CompareFunction::always)
            ? VK_TRUE
            : VK_FALSE;
    depthStencil.depthWriteEnable =
        (hasDepthStencil && desc.depthStencil.depthWriteEnabled) ? VK_TRUE
                                                                 : VK_FALSE;
    depthStencil.depthCompareOp =
        oreCompareFuncToVk(desc.depthStencil.depthCompare);
    depthStencil.stencilTestEnable =
        hasDepthStencil && hasStencilLocal(desc.depthStencil.format) ? VK_TRUE
                                                                     : VK_FALSE;
    depthStencil.front = oreStencilFaceToVk(desc.stencilFront,
                                            desc.stencilReadMask,
                                            desc.stencilWriteMask);
    depthStencil.back = oreStencilFaceToVk(desc.stencilBack,
                                           desc.stencilReadMask,
                                           desc.stencilWriteMask);

    // --- Color blend ---
    VkPipelineColorBlendAttachmentState blendAttachments[4]{};
    for (uint32_t i = 0; i < desc.colorCount; ++i)
    {
        const ColorTargetState& ct = desc.colorTargets[i];
        VkPipelineColorBlendAttachmentState& ba = blendAttachments[i];
        ba.colorWriteMask = oreColorWriteMaskToVk(ct.writeMask);
        ba.blendEnable = ct.blendEnabled ? VK_TRUE : VK_FALSE;
        if (ct.blendEnabled)
        {
            ba.srcColorBlendFactor = oreBlendFactorToVk(ct.blend.srcColor);
            ba.dstColorBlendFactor = oreBlendFactorToVk(ct.blend.dstColor);
            ba.colorBlendOp = oreBlendOpToVk(ct.blend.colorOp);
            ba.srcAlphaBlendFactor = oreBlendFactorToVk(ct.blend.srcAlpha);
            ba.dstAlphaBlendFactor = oreBlendFactorToVk(ct.blend.dstAlpha);
            ba.alphaBlendOp = oreBlendOpToVk(ct.blend.alphaOp);
        }
    }

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = desc.colorCount;
    colorBlend.pAttachments = blendAttachments;

    // --- Dynamic state ---
    constexpr VkDynamicState kDynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_STENCIL_REFERENCE,
        VK_DYNAMIC_STATE_BLEND_CONSTANTS,
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(
        sizeof(kDynamicStates) / sizeof(kDynamicStates[0]));
    dynamicState.pDynamicStates = kDynamicStates;

    // --- Render pass (format-only, DONT_CARE ops for pipeline compatibility)
    // --- Build a VKRenderPassKey from the pipeline's color/depth formats.
    struct VKRenderPassKey key{};
    key.colorCount = desc.colorCount;
    key.sampleCount = desc.sampleCount;
    for (uint32_t i = 0; i < desc.colorCount; ++i)
    {
        key.colorFormats[i] = desc.colorTargets[i].format;
        key.colorLoadOps[i] = LoadOp::dontCare;
        key.colorStoreOps[i] = StoreOp::discard;
    }
    if (desc.depthStencil.format != TextureFormat::rgba8unorm)
    {
        key.depthFormat = desc.depthStencil.format;
        key.depthLoadOp = LoadOp::dontCare;
        key.depthStoreOp = StoreOp::discard;
        key.hasDepth = true;
    }

    VkRenderPass compatRenderPass = getOrCreateRenderPass(key);

    // --- Assemble VkGraphicsPipelineCreateInfo ---
    VkGraphicsPipelineCreateInfo pipelineCI{};
    pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCI.stageCount = 2;
    pipelineCI.pStages = stages;
    pipelineCI.pVertexInputState = &vertexInput;
    pipelineCI.pInputAssemblyState = &inputAssembly;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pRasterizationState = &raster;
    pipelineCI.pMultisampleState = &multisample;
    pipelineCI.pDepthStencilState = &depthStencil;
    pipelineCI.pColorBlendState = &colorBlend;
    pipelineCI.pDynamicState = &dynamicState;
    pipelineCI.layout = pipeline->m_vkPipelineLayout;
    pipelineCI.renderPass = compatRenderPass;
    pipelineCI.subpass = 0;

    VkResult vkr = m_vk.CreateGraphicsPipelines(m_vkDevice,
                                                VK_NULL_HANDLE,
                                                1,
                                                &pipelineCI,
                                                nullptr,
                                                &pipeline->m_vkPipeline);
    if (vkr != VK_SUCCESS)
    {
        setLastError("Ore Vulkan: vkCreateGraphicsPipelines failed (%d)", vkr);
        if (outError)
            *outError = m_lastError;
        return nullptr;
    }
    pipeline->m_vkDestroyPipeline = m_vk.DestroyPipeline;

    return pipeline;
}

#endif // !ORE_BACKEND_GL

} // namespace rive::ore
