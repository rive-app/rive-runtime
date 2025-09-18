/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"
#include "shaders/constants.glsl"
#include "pipeline_manager_vulkan.hpp"

namespace rive::gpu
{
static VkSamplerMipmapMode vk_sampler_mipmap_mode(rive::ImageFilter option)
{
    switch (option)
    {
        case rive::ImageFilter::trilinear:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        case rive::ImageFilter::nearest:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    }

    RIVE_UNREACHABLE();
}

static VkSamplerAddressMode vk_sampler_address_mode(rive::ImageWrap option)
{
    switch (option)
    {
        case rive::ImageWrap::clamp:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case rive::ImageWrap::repeat:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case rive::ImageWrap::mirror:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    }

    RIVE_UNREACHABLE();
}

static VkFilter vk_filter(rive::ImageFilter option)
{
    switch (option)
    {
        case rive::ImageFilter::trilinear:
            return VK_FILTER_LINEAR;
        case rive::ImageFilter::nearest:
            return VK_FILTER_NEAREST;
    }

    RIVE_UNREACHABLE();
}

PipelineManagerVulkan::PipelineManagerVulkan(rcp<VulkanContext> vk,
                                             ShaderCompilationMode mode,
                                             uint32_t vendorID,
                                             VkImageView nullTextureView) :
    Super(mode),
    m_vk(std::move(vk)),
    m_atlasFormat(m_vk->isFormatSupportedWithFeatureFlags(
                      VK_FORMAT_R32_SFLOAT,
                      VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT)
                      ? VK_FORMAT_R32_SFLOAT
                      : VK_FORMAT_R16_SFLOAT),
    m_vendorID(vendorID)
{
    // Create the immutable samplers.
    VkSamplerCreateInfo linearSamplerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .minLod = 0,
        .maxLod = 0,
    };

    VK_CHECK(m_vk->CreateSampler(m_vk->device,
                                 &linearSamplerCreateInfo,
                                 nullptr,
                                 &m_linearSampler));

    for (size_t i = 0; i < ImageSampler::MAX_SAMPLER_PERMUTATIONS; ++i)
    {
        ImageWrap wrapX = ImageSampler::GetWrapXOptionFromKey(i);
        ImageWrap wrapY = ImageSampler::GetWrapYOptionFromKey(i);
        ImageFilter filter = ImageSampler::GetFilterOptionFromKey(i);
        VkFilter minMagFilter = vk_filter(filter);

        VkSamplerCreateInfo samplerCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = minMagFilter,
            .minFilter = minMagFilter,
            .mipmapMode = vk_sampler_mipmap_mode(filter),
            .addressModeU = vk_sampler_address_mode(wrapX),
            .addressModeV = vk_sampler_address_mode(wrapY),
            .minLod = 0,
            .maxLod = VK_LOD_CLAMP_NONE,
        };

        VK_CHECK(m_vk->CreateSampler(m_vk->device,
                                     &samplerCreateInfo,
                                     nullptr,
                                     m_imageSamplers + i));
    }

    // All pipelines share the same perFlush bindings.
    VkDescriptorSetLayoutBinding perFlushLayoutBindings[] = {
        {
            .binding = FLUSH_UNIFORM_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags =
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = IMAGE_DRAW_UNIFORM_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .descriptorCount = 1,
            .stageFlags =
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = PATH_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        },
        {
            .binding = PAINT_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags =
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = PAINT_AUX_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags =
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = CONTOUR_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        },
        {
            .binding = COVERAGE_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = TESS_VERTEX_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        },
        {
            .binding = GRAD_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = FEATHER_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1,
            .stageFlags =
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = ATLAS_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };

    VkDescriptorSetLayoutCreateInfo perFlushLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = std::size(perFlushLayoutBindings),
        .pBindings = perFlushLayoutBindings,
    };

    VK_CHECK(m_vk->CreateDescriptorSetLayout(m_vk->device,
                                             &perFlushLayoutInfo,
                                             nullptr,
                                             &m_perFlushDescriptorSetLayout));

    // The imageTexture gets updated with every draw that uses it.
    VkDescriptorSetLayoutBinding perDrawLayoutBindings[] = {
        {
            .binding = IMAGE_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = IMAGE_SAMPLER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };

    VkDescriptorSetLayoutCreateInfo perDrawLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = std::size(perDrawLayoutBindings),
        .pBindings = perDrawLayoutBindings,
    };

    VK_CHECK(m_vk->CreateDescriptorSetLayout(m_vk->device,
                                             &perDrawLayoutInfo,
                                             nullptr,
                                             &m_perDrawDescriptorSetLayout));

    // Every shader uses the same immutable sampler layout.
    VkDescriptorSetLayoutBinding immutableSamplerLayoutBindings[] = {
        {
            .binding = GRAD_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = &m_linearSampler,
        },
        {
            .binding = FEATHER_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags =
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = &m_linearSampler,
        },
        {
            .binding = ATLAS_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = &m_linearSampler,
        },
    };

    VkDescriptorSetLayoutCreateInfo immutableSamplerLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = std::size(immutableSamplerLayoutBindings),
        .pBindings = immutableSamplerLayoutBindings,
    };

    VK_CHECK(m_vk->CreateDescriptorSetLayout(
        m_vk->device,
        &immutableSamplerLayoutInfo,
        nullptr,
        &m_immutableSamplerDescriptorSetLayout));

    // For when a set isn't used at all by a shader.
    VkDescriptorSetLayoutCreateInfo emptyLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 0,
    };

    VK_CHECK(m_vk->CreateDescriptorSetLayout(m_vk->device,
                                             &emptyLayoutInfo,
                                             nullptr,
                                             &m_emptyDescriptorSetLayout));

    // Create static descriptor sets.
    VkDescriptorPoolSize staticDescriptorPoolSizes[] = {
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1, // m_nullImageTexture
        },
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 4, // grad, feather, atlas, image samplers
        },
    };

    VkDescriptorPoolCreateInfo staticDescriptorPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 2,
        .poolSizeCount = std::size(staticDescriptorPoolSizes),
        .pPoolSizes = staticDescriptorPoolSizes,
    };

    VK_CHECK(m_vk->CreateDescriptorPool(m_vk->device,
                                        &staticDescriptorPoolCreateInfo,
                                        nullptr,
                                        &m_staticDescriptorPool));

    // Create a descriptor set to bind m_nullImageTexture when there is no image
    // paint.
    VkDescriptorSetAllocateInfo nullImageDescriptorSetInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_staticDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_perDrawDescriptorSetLayout,
    };

    VK_CHECK(m_vk->AllocateDescriptorSets(m_vk->device,
                                          &nullImageDescriptorSetInfo,
                                          &m_nullImageDescriptorSet));

    m_vk->updateImageDescriptorSets(
        m_nullImageDescriptorSet,
        {
            .dstBinding = IMAGE_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        },
        {{
            .imageView = nullTextureView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        }});

    m_vk->updateImageDescriptorSets(
        m_nullImageDescriptorSet,
        {
            .dstBinding = IMAGE_SAMPLER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        },
        {{
            .sampler = m_imageSamplers[ImageSampler::LINEAR_CLAMP_SAMPLER_KEY],
        }});

    // Create an empty descriptor set for IMMUTABLE_SAMPLER_BINDINGS_SET. Vulkan
    // requires this even though the samplers are all immutable.
    VkDescriptorSetAllocateInfo immutableSamplerDescriptorSetInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_staticDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_immutableSamplerDescriptorSetLayout,
    };

    VK_CHECK(m_vk->AllocateDescriptorSets(m_vk->device,
                                          &immutableSamplerDescriptorSetInfo,
                                          &m_immutableSamplerDescriptorSet));
}

PipelineManagerVulkan::~PipelineManagerVulkan()
{
    m_vk->DestroyDescriptorSetLayout(m_vk->device,
                                     m_perFlushDescriptorSetLayout,
                                     nullptr);
    m_vk->DestroyDescriptorSetLayout(m_vk->device,
                                     m_perDrawDescriptorSetLayout,
                                     nullptr);
    m_vk->DestroyDescriptorSetLayout(m_vk->device,
                                     m_immutableSamplerDescriptorSetLayout,
                                     nullptr);
    m_vk->DestroyDescriptorSetLayout(m_vk->device,
                                     m_emptyDescriptorSetLayout,
                                     nullptr);

    m_vk->DestroyDescriptorPool(m_vk->device, m_staticDescriptorPool, nullptr);
    for (VkSampler sampler : m_imageSamplers)
    {
        m_vk->DestroySampler(m_vk->device, sampler, nullptr);
    }
    m_vk->DestroySampler(m_vk->device, m_linearSampler, nullptr);
}

DrawPipelineLayoutVulkan& PipelineManagerVulkan::
    getDrawPipelineLayoutSynchronous(
        InterlockMode interlockMode,
        DrawPipelineLayoutVulkan::Options layoutOptions)
{
    return getSharedObjectSynchronous(
        DrawPipelineLayoutIdx(interlockMode, layoutOptions),
        m_drawPipelineLayouts,
        [&]() {
            return std::make_unique<DrawPipelineLayoutVulkan>(this,
                                                              interlockMode,
                                                              layoutOptions);
        });
}

std::unique_ptr<DrawShaderVulkan> PipelineManagerVulkan::createVertexShader(
    DrawType drawType,
    ShaderFeatures shaderFeatures,
    InterlockMode interlockMode)
{
    return std::make_unique<DrawShaderVulkan>(DrawShaderVulkan::Type::vertex,
                                              vulkanContext(),
                                              drawType,
                                              shaderFeatures,
                                              interlockMode,
                                              ShaderMiscFlags::none);
}

std::unique_ptr<DrawShaderVulkan> PipelineManagerVulkan::createFragmentShader(
    DrawType drawType,
    ShaderFeatures shaderFeatures,
    InterlockMode interlockMode,
    ShaderMiscFlags miscFlags)
{
    return std::make_unique<DrawShaderVulkan>(DrawShaderVulkan::Type::fragment,
                                              vulkanContext(),
                                              drawType,
                                              shaderFeatures,
                                              interlockMode,
                                              miscFlags);
}

RenderPassVulkan& PipelineManagerVulkan::getRenderPassSynchronous(
    InterlockMode interlockMode,
    DrawPipelineLayoutVulkan::Options pipelineLayoutOptions,
    VkFormat renderTargetFormat,
    LoadAction colorLoadAction)
{
    const uint32_t key = RenderPassVulkan::Key(interlockMode,
                                               pipelineLayoutOptions,
                                               renderTargetFormat,
                                               colorLoadAction);

    return getSharedObjectSynchronous(key, m_renderPasses, [&]() {
        return std::make_unique<RenderPassVulkan>(this,
                                                  interlockMode,
                                                  pipelineLayoutOptions,
                                                  renderTargetFormat,
                                                  colorLoadAction);
    });
}

std::unique_ptr<DrawPipelineVulkan> PipelineManagerVulkan::createPipeline(
    PipelineCreateType createType,
    uint64_t key,
    const PipelineProps& props)
{
    auto& renderPass = getRenderPassSynchronous(props.interlockMode,
                                                props.pipelineLayoutOptions,
                                                props.renderTargetFormat,
                                                props.colorLoadAction);

    return std::make_unique<DrawPipelineVulkan>(
        this,
        props.drawType,
        *renderPass.drawPipelineLayout(),
        props.shaderFeatures,
        props.shaderMiscFlags,
        props.drawPipelineOptions,
        props.pipelineState,
        renderPass
#ifdef WITH_RIVE_TOOLS
        ,
        props.synthesizedFailureType
#endif
    );
}

PipelineStatus PipelineManagerVulkan::getPipelineStatus(
    const DrawPipelineVulkan& pipeline) const
{
    return (VkPipeline(pipeline) == VK_NULL_HANDLE) ? PipelineStatus::errored
                                                    : PipelineStatus::ready;
}

void PipelineManagerVulkan::clearCacheInternal()
{
    m_drawPipelineLayouts.clear();
    m_renderPasses.clear();
}
} // namespace rive::gpu