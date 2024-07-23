/*
 * Copyright 2023 Rive
 */

#include "rive/pls/vulkan/pls_render_context_vulkan_impl.hpp"

#include "rive/pls/pls_image.hpp"
#include "shaders/constants.glsl"

#include "generated/shaders/spirv/color_ramp.vert.h"
#include "generated/shaders/spirv/color_ramp.frag.h"
#include "generated/shaders/spirv/tessellate.vert.h"
#include "generated/shaders/spirv/tessellate.frag.h"
#include "generated/shaders/spirv/draw_path.vert.h"
#include "generated/shaders/spirv/draw_path.frag.h"
#include "generated/shaders/spirv/draw_interior_triangles.vert.h"
#include "generated/shaders/spirv/draw_interior_triangles.frag.h"
#include "generated/shaders/spirv/draw_image_mesh.vert.h"
#include "generated/shaders/spirv/draw_image_mesh.frag.h"

#ifdef RIVE_DECODERS
#include "rive/decoders/bitmap_decoder.hpp"
#endif

namespace rive::pls
{
static VkBufferUsageFlagBits render_buffer_usage_flags(RenderBufferType renderBufferType)
{
    switch (renderBufferType)
    {
        case RenderBufferType::index:
            return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        case RenderBufferType::vertex:
            return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    RIVE_UNREACHABLE();
}

class RenderBufferVulkanImpl : public RenderBuffer
{
public:
    RenderBufferVulkanImpl(rcp<vkutil::Allocator> allocator,
                           RenderBufferType renderBufferType,
                           RenderBufferFlags renderBufferFlags,
                           size_t sizeInBytes) :
        RenderBuffer(renderBufferType, renderBufferFlags, sizeInBytes),
        m_bufferRing(std::move(allocator),
                     render_buffer_usage_flags(renderBufferType),
                     vkutil::Mappability::writeOnly,
                     sizeInBytes)
    {}

    VkBuffer frontVkBuffer() const
    {
        assert(m_bufferRingIdx >= 0); // Call map() first.
        return m_bufferRing.vkBufferAt(m_bufferRingIdx);
    }

    const VkBuffer* frontVkBufferAddressOf() const
    {
        assert(m_bufferRingIdx >= 0); // Call map() first.
        return m_bufferRing.vkBufferAtAddressOf(m_bufferRingIdx);
    }

protected:
    void* onMap() override
    {
        m_bufferRingIdx = (m_bufferRingIdx + 1) % pls::kBufferRingSize;
        m_bufferRing.synchronizeSizeAt(m_bufferRingIdx);
        return m_bufferRing.contentsAt(m_bufferRingIdx);
    }

    void onUnmap() override { m_bufferRing.flushMappedContentsAt(m_bufferRingIdx); }

private:
    vkutil::BufferRing m_bufferRing;
    int m_bufferRingIdx = -1;
};

rcp<RenderBuffer> PLSRenderContextVulkanImpl::makeRenderBuffer(RenderBufferType type,
                                                               RenderBufferFlags flags,
                                                               size_t sizeInBytes)
{
    return make_rcp<RenderBufferVulkanImpl>(m_allocator, type, flags, sizeInBytes);
}

class PLSTextureVulkanImpl : public PLSTexture
{
public:
    PLSTextureVulkanImpl(rcp<vkutil::Allocator> allocator,
                         uint32_t width,
                         uint32_t height,
                         uint32_t mipLevelCount,
                         const uint8_t imageDataRGBA[]) :
        PLSTexture(width, height),
        m_texture(allocator->makeTexture({
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .extent = {width, height, 1},
            .mipLevels = mipLevelCount,
            .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        })),
        m_textureView(allocator->makeTextureView(m_texture)),
        m_imageUploadBuffer(allocator->makeBuffer(
            {
                .size = height * width * 4,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            },
            vkutil::Mappability::writeOnly))
    {
        memcpy(vkutil::ScopedBufferFlush(*m_imageUploadBuffer),
               imageDataRGBA,
               m_imageUploadBuffer->info().size);
    }

    bool hasUpdates() const { return m_imageUploadBuffer != nullptr; }

    void synchronize(VkCommandBuffer commandBuffer, PLSRenderContextVulkanImpl* plsImplVulkan) const
    {
        assert(hasUpdates());

        // Upload the new image.
        VkBufferImageCopy bufferImageCopy = {
            .imageSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .layerCount = 1,
                },
            .imageExtent = {width(), height(), 1},
        };

        vkutil::insert_image_memory_barrier(commandBuffer,
                                            *m_texture,
                                            VK_IMAGE_LAYOUT_UNDEFINED,
                                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                            0,
                                            m_texture->info().mipLevels);

        vkCmdCopyBufferToImage(commandBuffer,
                               *m_imageUploadBuffer,
                               *m_texture,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1,
                               &bufferImageCopy);

        uint32_t mipLevels = m_texture->info().mipLevels;
        if (mipLevels > 1)
        {
            // Generate mipmaps.
            int2 dstSize, srcSize = {static_cast<int32_t>(width()), static_cast<int32_t>(height())};
            for (uint32_t level = 1; level < mipLevels; ++level, srcSize = dstSize)
            {
                dstSize = simd::max(srcSize >> 1, int2(1));

                VkImageBlit imageBlit = {
                    .srcSubresource =
                        {
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                            .mipLevel = level - 1,
                            .layerCount = 1,
                        },
                    .dstSubresource =
                        {
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                            .mipLevel = level,
                            .layerCount = 1,
                        },
                };

                imageBlit.srcOffsets[0] = {0, 0, 0};
                imageBlit.srcOffsets[1] = {srcSize.x, srcSize.y, 1};

                imageBlit.dstOffsets[0] = {0, 0, 0};
                imageBlit.dstOffsets[1] = {dstSize.x, dstSize.y, 1};

                vkutil::insert_image_memory_barrier(commandBuffer,
                                                    *m_texture,
                                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                    level - 1,
                                                    1);

                vkCmdBlitImage(commandBuffer,
                               *m_texture,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               *m_texture,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1,
                               &imageBlit,
                               VK_FILTER_LINEAR);
            }

            vkutil::insert_image_memory_barrier(commandBuffer,
                                                *m_texture,
                                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                0,
                                                mipLevels - 1);
        }

        vkutil::insert_image_memory_barrier(commandBuffer,
                                            *m_texture,
                                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                            mipLevels - 1,
                                            1);

        m_imageUploadBuffer = nullptr;
    }

private:
    friend class PLSRenderContextVulkanImpl;

    rcp<vkutil::Texture> m_texture;
    rcp<vkutil::TextureView> m_textureView;

    mutable rcp<vkutil::Buffer> m_imageUploadBuffer;

    // Location for PLSRenderContextVulkanImpl to store a descriptor set for the
    // current flush that binds this image texture.
    mutable VkDescriptorSet m_imageTextureDescriptorSet = VK_NULL_HANDLE;
    mutable uint64_t m_descriptorSetFrameIdx = std::numeric_limits<size_t>::max();
};

rcp<PLSTexture> PLSRenderContextVulkanImpl::decodeImageTexture(Span<const uint8_t> encodedBytes)
{
#ifdef RIVE_DECODERS
    auto bitmap = Bitmap::decode(encodedBytes.data(), encodedBytes.size());
    if (bitmap)
    {
        // For now, PLSRenderContextImpl::makeImageTexture() only accepts RGBA.
        if (bitmap->pixelFormat() != Bitmap::PixelFormat::RGBA)
        {
            bitmap->pixelFormat(Bitmap::PixelFormat::RGBA);
        }
        uint32_t width = bitmap->width();
        uint32_t height = bitmap->height();
        uint32_t mipLevelCount = math::msb(height | width);
        return make_rcp<PLSTextureVulkanImpl>(m_allocator,
                                              width,
                                              height,
                                              mipLevelCount,
                                              bitmap->bytes());
    }
#endif
    return nullptr;
}

// Renders color ramps to the gradient texture.
class PLSRenderContextVulkanImpl::ColorRampPipeline
{
public:
    ColorRampPipeline(VkDevice device) : m_device(device)
    {
        VkDescriptorSetLayoutBinding descriptorSetLayoutBinding = {
            .binding = FLUSH_UNIFORM_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        };
        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &descriptorSetLayoutBinding,
        };

        VK_CHECK(vkCreateDescriptorSetLayout(m_device,
                                             &descriptorSetLayoutCreateInfo,
                                             nullptr,
                                             &m_descriptorSetLayout));

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &m_descriptorSetLayout,
        };

        VK_CHECK(vkCreatePipelineLayout(m_device,
                                        &pipelineLayoutCreateInfo,
                                        nullptr,
                                        &m_pipelineLayout));

        VkShaderModuleCreateInfo shaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = std::size(color_ramp_vert) * sizeof(uint32_t),
            .pCode = color_ramp_vert,
        };

        VkShaderModule vertexShader;
        VK_CHECK(vkCreateShaderModule(m_device, &shaderModuleCreateInfo, nullptr, &vertexShader));

        shaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = std::size(color_ramp_frag) * sizeof(uint32_t),
            .pCode = color_ramp_frag,
        };

        VkShaderModule fragmentShader;
        VK_CHECK(vkCreateShaderModule(m_device, &shaderModuleCreateInfo, nullptr, &fragmentShader));

        VkPipelineShaderStageCreateInfo stages[] = {
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vertexShader,
                .pName = "main",
            },
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = fragmentShader,
                .pName = "main",
            },
        };

        VkVertexInputBindingDescription vertexInputBindingDescription = {
            .binding = 0,
            .stride = sizeof(pls::GradientSpan),
            .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
        };

        VkVertexInputAttributeDescription vertexAttributeDescription = {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32A32_UINT,
        };

        VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &vertexInputBindingDescription,
            .vertexAttributeDescriptionCount = 1,
            .pVertexAttributeDescriptions = &vertexAttributeDescription,
        };

        VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        };

        VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };

        VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .lineWidth = 1.0,
        };

        VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        };

        VkPipelineColorBlendAttachmentState blendColorAttachment = {
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };
        VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &blendColorAttachment,
        };

        VkAttachmentDescription attachment = {
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
        VkAttachmentReference attachmentReference = {
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
        VkSubpassDescription subpassDescription = {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &attachmentReference,
        };
        VkRenderPassCreateInfo renderPassCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &attachment,
            .subpassCount = 1,
            .pSubpasses = &subpassDescription,
        };

        VK_CHECK(vkCreateRenderPass(m_device, &renderPassCreateInfo, nullptr, &m_renderPass));

        VkDynamicState dynamicStates[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = 2,
            .pDynamicStates = dynamicStates,
        };

        VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = stages,
            .pVertexInputState = &pipelineVertexInputStateCreateInfo,
            .pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo,
            .pViewportState = &pipelineViewportStateCreateInfo,
            .pRasterizationState = &pipelineRasterizationStateCreateInfo,
            .pMultisampleState = &pipelineMultisampleStateCreateInfo,
            .pColorBlendState = &pipelineColorBlendStateCreateInfo,
            .pDynamicState = &pipelineDynamicStateCreateInfo,
            .layout = m_pipelineLayout,
            .renderPass = m_renderPass,
        };

        VK_CHECK(vkCreateGraphicsPipelines(m_device,
                                           VK_NULL_HANDLE,
                                           1,
                                           &graphicsPipelineCreateInfo,
                                           nullptr,
                                           &m_renderPipeline));

        vkDestroyShaderModule(m_device, vertexShader, nullptr);
        vkDestroyShaderModule(m_device, fragmentShader, nullptr);
    }

    ~ColorRampPipeline()
    {
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        vkDestroyPipeline(m_device, m_renderPipeline, nullptr);
    }

    const VkDescriptorSetLayout& descriptorSetLayout() const { return m_descriptorSetLayout; }
    const VkPipelineLayout& pipelineLayout() const { return m_pipelineLayout; }
    const VkRenderPass& renderPass() const { return m_renderPass; }
    const VkPipeline& renderPipeline() const { return m_renderPipeline; }

private:
    VkDescriptorSetLayout m_descriptorSetLayout;
    VkPipelineLayout m_pipelineLayout;
    VkRenderPass m_renderPass;
    VkPipeline m_renderPipeline;
    VkDevice m_device;
};

// Renders tessellated vertices to the tessellation texture.
class PLSRenderContextVulkanImpl::TessellatePipeline
{
public:
    TessellatePipeline(VkDevice device) : m_device(device)
    {
        VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[] = {
            {
                .binding = PATH_BUFFER_IDX,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            },
            {
                .binding = CONTOUR_BUFFER_IDX,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            },
            {
                .binding = FLUSH_UNIFORM_BUFFER_IDX,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            },
        };
        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 3,
            .pBindings = descriptorSetLayoutBindings,
        };

        VK_CHECK(vkCreateDescriptorSetLayout(m_device,
                                             &descriptorSetLayoutCreateInfo,
                                             nullptr,
                                             &m_descriptorSetLayout));

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &m_descriptorSetLayout,
        };

        VK_CHECK(vkCreatePipelineLayout(m_device,
                                        &pipelineLayoutCreateInfo,
                                        nullptr,
                                        &m_pipelineLayout));

        VkShaderModuleCreateInfo shaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = std::size(tessellate_vert) * sizeof(uint32_t),
            .pCode = tessellate_vert,
        };

        VkShaderModule vertexShader;
        VK_CHECK(vkCreateShaderModule(m_device, &shaderModuleCreateInfo, nullptr, &vertexShader));

        shaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = std::size(tessellate_frag) * sizeof(uint32_t),
            .pCode = tessellate_frag,
        };

        VkShaderModule fragmentShader;
        VK_CHECK(vkCreateShaderModule(m_device, &shaderModuleCreateInfo, nullptr, &fragmentShader));

        VkPipelineShaderStageCreateInfo stages[] = {
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vertexShader,
                .pName = "main",
            },
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = fragmentShader,
                .pName = "main",
            },
        };

        VkVertexInputBindingDescription vertexInputBindingDescription = {
            .binding = 0,
            .stride = sizeof(pls::TessVertexSpan),
            .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
        };

        VkVertexInputAttributeDescription vertexAttributeDescriptions[] = {
            {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = 0,
            },
            {
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = 4 * sizeof(float),
            },
            {
                .location = 2,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = 8 * sizeof(float),
            },
            {
                .location = 3,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32A32_UINT,
                .offset = 12 * sizeof(float),
            },
        };

        VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &vertexInputBindingDescription,
            .vertexAttributeDescriptionCount = 4,
            .pVertexAttributeDescriptions = vertexAttributeDescriptions,
        };

        VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        };

        VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };

        VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .lineWidth = 1.0,
        };

        VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        };

        VkPipelineColorBlendAttachmentState blendColorAttachment = {
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };
        VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &blendColorAttachment,
        };

        VkAttachmentDescription attachment = {
            .format = VK_FORMAT_R32G32B32A32_UINT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
        VkAttachmentReference attachmentReference = {
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
        VkSubpassDescription subpassDescription = {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &attachmentReference,
        };
        VkRenderPassCreateInfo renderPassCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &attachment,
            .subpassCount = 1,
            .pSubpasses = &subpassDescription,
        };

        VK_CHECK(vkCreateRenderPass(m_device, &renderPassCreateInfo, nullptr, &m_renderPass));

        VkDynamicState dynamicStates[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = 2,
            .pDynamicStates = dynamicStates,
        };

        VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = stages,
            .pVertexInputState = &pipelineVertexInputStateCreateInfo,
            .pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo,
            .pViewportState = &pipelineViewportStateCreateInfo,
            .pRasterizationState = &pipelineRasterizationStateCreateInfo,
            .pMultisampleState = &pipelineMultisampleStateCreateInfo,
            .pColorBlendState = &pipelineColorBlendStateCreateInfo,
            .pDynamicState = &pipelineDynamicStateCreateInfo,
            .layout = m_pipelineLayout,
            .renderPass = m_renderPass,
        };

        VK_CHECK(vkCreateGraphicsPipelines(m_device,
                                           VK_NULL_HANDLE,
                                           1,
                                           &graphicsPipelineCreateInfo,
                                           nullptr,
                                           &m_renderPipeline));

        vkDestroyShaderModule(m_device, vertexShader, nullptr);
        vkDestroyShaderModule(m_device, fragmentShader, nullptr);
    }

    ~TessellatePipeline()
    {
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        vkDestroyPipeline(m_device, m_renderPipeline, nullptr);
    }

    const VkDescriptorSetLayout& descriptorSetLayout() const { return m_descriptorSetLayout; }
    const VkPipelineLayout& pipelineLayout() const { return m_pipelineLayout; }
    const VkRenderPass& renderPass() const { return m_renderPass; }
    const VkPipeline& renderPipeline() const { return m_renderPipeline; }

private:
    VkDescriptorSetLayout m_descriptorSetLayout;
    VkPipelineLayout m_pipelineLayout;
    VkRenderPass m_renderPass;
    VkPipeline m_renderPipeline;
    VkDevice m_device;
};

// Wraps vertex and fragment shader modules for a specific combination of DrawType,
// InterlockMode, and ShaderFeatures.
class PLSRenderContextVulkanImpl::DrawShader
{
public:
    DrawShader(VkDevice device,
               pls::DrawType drawType,
               pls::InterlockMode,
               pls::ShaderFeatures shaderFeatures) :
        m_device(device)
    {
        VkShaderModuleCreateInfo vertexShaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        };

        VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        };

        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::outerCurvePatches:
                vertexShaderModuleCreateInfo.codeSize =
                    std::size(draw_path_vert) * sizeof(uint32_t);
                vertexShaderModuleCreateInfo.pCode = draw_path_vert;
                fragmentShaderModuleCreateInfo.codeSize =
                    std::size(draw_path_frag) * sizeof(uint32_t);
                fragmentShaderModuleCreateInfo.pCode = draw_path_frag;
                break;

            case DrawType::interiorTriangulation:
                vertexShaderModuleCreateInfo.codeSize =
                    std::size(draw_interior_triangles_vert) * sizeof(uint32_t);
                vertexShaderModuleCreateInfo.pCode = draw_interior_triangles_vert;
                fragmentShaderModuleCreateInfo.codeSize =
                    std::size(draw_interior_triangles_frag) * sizeof(uint32_t);
                fragmentShaderModuleCreateInfo.pCode = draw_interior_triangles_frag;
                break;

            case DrawType::imageRect:
                break;

            case DrawType::imageMesh:
                vertexShaderModuleCreateInfo.codeSize =
                    std::size(draw_image_mesh_vert) * sizeof(uint32_t);
                vertexShaderModuleCreateInfo.pCode = draw_image_mesh_vert;

                fragmentShaderModuleCreateInfo.codeSize =
                    std::size(draw_image_mesh_frag) * sizeof(uint32_t);
                fragmentShaderModuleCreateInfo.pCode = draw_image_mesh_frag;
                break;

            case DrawType::plsAtomicResolve:
            case DrawType::plsAtomicInitialize:
            case DrawType::stencilClipReset:
                RIVE_UNREACHABLE();
        }

        VK_CHECK(vkCreateShaderModule(m_device,
                                      &vertexShaderModuleCreateInfo,
                                      nullptr,
                                      &m_vertexModule));

        VK_CHECK(vkCreateShaderModule(m_device,
                                      &fragmentShaderModuleCreateInfo,
                                      nullptr,
                                      &m_fragmentModule));
    }

    ~DrawShader()
    {
        vkDestroyShaderModule(m_device, m_vertexModule, nullptr);
        vkDestroyShaderModule(m_device, m_fragmentModule, nullptr);
    }

    VkShaderModule vertexModule() const { return m_vertexModule; }
    VkShaderModule fragmentModule() const { return m_fragmentModule; }

private:
    const VkDevice m_device;
    VkShaderModule m_vertexModule = nullptr;
    VkShaderModule m_fragmentModule = nullptr;
};

// Pipeline options that don't affect the shader.
enum class DrawPipelineOptions
{
    none = 0,
    wireframe = 1 << 0,
};
constexpr static int kDrawPipelineOptionCount = 1;
RIVE_MAKE_ENUM_BITSET(DrawPipelineOptions);

class PLSRenderContextVulkanImpl::DrawPipeline
{
public:
    // Number of render pass variants that can be used with a single DrawPipeline
    // (framebufferFormat x loadOp).
    constexpr static int kRenderPassVariantCount = 4;

    static int RenderPassVariantIdx(VkFormat framebufferFormat, pls::LoadAction loadAction)
    {
        assert(loadAction == pls::LoadAction::preserveRenderTarget ||
               loadAction == pls::LoadAction::clear);
        assert(framebufferFormat == VK_FORMAT_B8G8R8A8_UNORM ||
               framebufferFormat == VK_FORMAT_R8G8B8A8_UNORM);
        int idx = (loadAction == pls::LoadAction::preserveRenderTarget ? 0x1 : 0) |
                  (framebufferFormat == VK_FORMAT_B8G8R8A8_UNORM ? 0x2 : 0);
        assert(0 <= idx && idx < kRenderPassVariantCount);
        return idx;
    }

    constexpr static VkFormat FormatFromRenderPassVariant(int idx)
    {
        return (idx & 0x2) ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R8G8B8A8_UNORM;
    }

    constexpr static VkAttachmentLoadOp LoadOpFromRenderPassVariant(int idx)
    {
        return (idx & 0x1) ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
    }

    // Creates a render pass that can be used with the DrawPipeline at matching
    // renderPassVariantIdx.
    static VkRenderPass CreateVkRenderPass(VkDevice device, int renderPassVariantIdx)
    {
        VkAttachmentDescription attachmentDescriptions[] = {
            {
                .format = FormatFromRenderPassVariant(renderPassVariantIdx),
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = LoadOpFromRenderPassVariant(renderPassVariantIdx),
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                // TODO: VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR once we start using
                // extensions!
                .initialLayout = VK_IMAGE_LAYOUT_GENERAL,
                .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
            },
            {
                .format = VK_FORMAT_R32_UINT,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_GENERAL,
                .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
            },
            {
                .format = VK_FORMAT_R32_UINT,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_GENERAL,
                .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
            },
            {
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
        };
        VkAttachmentReference attachmentReferences[] = {
            {
                .attachment = FRAMEBUFFER_PLANE_IDX,
                .layout = VK_IMAGE_LAYOUT_GENERAL,
            },
            {
                .attachment = CLIP_PLANE_IDX,
                .layout = VK_IMAGE_LAYOUT_GENERAL,
            },
            {
                .attachment = COVERAGE_PLANE_IDX,
                .layout = VK_IMAGE_LAYOUT_GENERAL,
            },
            {
                .attachment = ORIGINAL_DST_COLOR_PLANE_IDX,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
        };

        VkAttachmentReference inputAttachmentReferences[] = {
            {
                .attachment = FRAMEBUFFER_PLANE_IDX,
                .layout = VK_IMAGE_LAYOUT_GENERAL,
            },
            {
                .attachment = CLIP_PLANE_IDX,
                .layout = VK_IMAGE_LAYOUT_GENERAL,
            },
        };
        VkSubpassDescription subpassDescription = {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = 0,
            .pInputAttachments = inputAttachmentReferences,
            .colorAttachmentCount = 4,
            .pColorAttachments = attachmentReferences,
        };
        VkSubpassDependency subpassDependency = {
            .srcSubpass = 0,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        };
        VkRenderPassCreateInfo renderPassCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 4,
            .pAttachments = attachmentDescriptions,
            .subpassCount = 1,
            .pSubpasses = &subpassDescription,
            .dependencyCount = 1,
            .pDependencies = &subpassDependency,
        };

        VkRenderPass renderPass;
        VK_CHECK(vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass));

        return renderPass;
    }

    DrawPipeline(PLSRenderContextVulkanImpl* plsImplVulkan,
                 pls::DrawType drawType,
                 pls::InterlockMode interlockMode,
                 pls::ShaderFeatures shaderFeatures,
                 DrawPipelineOptions drawPipelineOptions) :
        m_device(plsImplVulkan->m_device)
    {
        uint32_t shaderKey = pls::ShaderUniqueKey(drawType,
                                                  shaderFeatures,
                                                  interlockMode,
                                                  pls::ShaderMiscFlags::none);
        const DrawShader& drawShader =
            plsImplVulkan->m_drawShaders
                .try_emplace(shaderKey, m_device, drawType, interlockMode, shaderFeatures)
                .first->second;

        VkPipelineShaderStageCreateInfo stages[] = {
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = drawShader.vertexModule(),
                .pName = "main",
            },
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = drawShader.fragmentModule(),
                .pName = "main",
            },
        };

        std::array<VkVertexInputBindingDescription, 2> vertexInputBindingDescriptions;
        std::array<VkVertexInputAttributeDescription, 2> vertexAttributeDescriptions;
        VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 0,
            .pVertexBindingDescriptions = vertexInputBindingDescriptions.data(),
            .vertexAttributeDescriptionCount = 0,
            .pVertexAttributeDescriptions = vertexAttributeDescriptions.data(),
        };

        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::outerCurvePatches:
            {
                vertexInputBindingDescriptions = {{{
                    .binding = 0,
                    .stride = sizeof(pls::PatchVertex),
                    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
                }}};
                vertexAttributeDescriptions = {{
                    {
                        .location = 0,
                        .binding = 0,
                        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                        .offset = 0,
                    },
                    {
                        .location = 1,
                        .binding = 0,
                        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                        .offset = 4 * sizeof(float),
                    },
                }};

                pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
                pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = 2;

                break;
            }

            case DrawType::interiorTriangulation:
            {
                vertexInputBindingDescriptions = {{{
                    .binding = 0,
                    .stride = sizeof(pls::TriangleVertex),
                    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
                }}};
                vertexAttributeDescriptions = {{
                    {
                        .location = 0,
                        .binding = 0,
                        .format = VK_FORMAT_R32G32B32_SFLOAT,
                        .offset = 0,
                    },
                }};

                pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
                pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = 1;

                break;
            }

            case DrawType::imageRect:
                RIVE_UNREACHABLE();

            case DrawType::imageMesh:
            {
                vertexInputBindingDescriptions = {{
                    {
                        .binding = 0,
                        .stride = sizeof(float) * 2,
                        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
                    },
                    {
                        .binding = 1,
                        .stride = sizeof(float) * 2,
                        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
                    },
                }};
                vertexAttributeDescriptions = {{
                    {
                        .location = 0,
                        .binding = 0,
                        .format = VK_FORMAT_R32G32_SFLOAT,
                        .offset = 0,
                    },
                    {
                        .location = 1,
                        .binding = 1,
                        .format = VK_FORMAT_R32G32_SFLOAT,
                        .offset = 0,
                    },
                }};

                pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 2;
                pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = 2;

                break;
            }

            case DrawType::plsAtomicInitialize:
            case DrawType::plsAtomicResolve:
            case DrawType::stencilClipReset:
                RIVE_UNREACHABLE();
        }

        VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        };

        VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };

        VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = (drawPipelineOptions & DrawPipelineOptions::wireframe)
                               ? VK_POLYGON_MODE_LINE
                               : VK_POLYGON_MODE_FILL,
            .cullMode = static_cast<VkCullModeFlags>(
                drawType != DrawType::imageMesh ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE),
            .frontFace = VK_FRONT_FACE_CLOCKWISE,
            .lineWidth = 1.0,
        };

        VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        };

        VkPipelineColorBlendAttachmentState blendColorAttachments[4] = {};
        VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 4,
            .pAttachments = blendColorAttachments,
        };

        // In rasterOrdering mode, the PLS planes are accessed as color
        // attachments.
        blendColorAttachments[FRAMEBUFFER_PLANE_IDX].colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
        blendColorAttachments[COVERAGE_PLANE_IDX].colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
        blendColorAttachments[CLIP_PLANE_IDX].colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
        blendColorAttachments[ORIGINAL_DST_COLOR_PLANE_IDX].colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;

        VkDynamicState dynamicStates[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = 2,
            .pDynamicStates = dynamicStates,
        };

        VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = stages,
            .pVertexInputState = &pipelineVertexInputStateCreateInfo,
            .pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo,
            .pViewportState = &pipelineViewportStateCreateInfo,
            .pRasterizationState = &pipelineRasterizationStateCreateInfo,
            .pMultisampleState = &pipelineMultisampleStateCreateInfo,
            .pColorBlendState = &pipelineColorBlendStateCreateInfo,
            .pDynamicState = &pipelineDynamicStateCreateInfo,
            .layout = plsImplVulkan->m_drawPipelineLayout,
        };

        for (int i = 0; i < kRenderPassVariantCount; ++i)
        {
            graphicsPipelineCreateInfo.renderPass = plsImplVulkan->m_drawRenderPasses[i];

            VK_CHECK(vkCreateGraphicsPipelines(m_device,
                                               VK_NULL_HANDLE,
                                               1,
                                               &graphicsPipelineCreateInfo,
                                               nullptr,
                                               &m_vkPipelines[i]));
        }
    }

    ~DrawPipeline()
    {
        for (VkPipeline vkPipeline : m_vkPipelines)
        {
            vkDestroyPipeline(m_device, vkPipeline, nullptr);
        }
    }

    const VkPipeline vkPipelineAt(int renderPassVariantIdx) const
    {
        return m_vkPipelines[renderPassVariantIdx];
    }

private:
    const VkDevice m_device;
    VkPipeline m_vkPipelines[kRenderPassVariantCount];
};

PLSRenderContextVulkanImpl::PLSRenderContextVulkanImpl(rcp<vkutil::Allocator> allocator) :
    m_allocator(std::move(allocator)),
    m_device(m_allocator->device()),
    m_flushUniformBufferRing(m_allocator,
                             VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                             vkutil::Mappability::writeOnly),
    m_imageDrawUniformBufferRing(m_allocator,
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 vkutil::Mappability::writeOnly),
    m_pathBufferRing(m_allocator,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     vkutil::Mappability::writeOnly),
    m_paintBufferRing(m_allocator,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                      vkutil::Mappability::writeOnly),
    m_paintAuxBufferRing(m_allocator,
                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                         vkutil::Mappability::writeOnly),
    m_contourBufferRing(m_allocator,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        vkutil::Mappability::writeOnly),
    m_simpleColorRampsBufferRing(m_allocator,
                                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                 vkutil::Mappability::writeOnly),
    m_gradSpanBufferRing(m_allocator,
                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                         vkutil::Mappability::writeOnly),
    m_tessSpanBufferRing(m_allocator,
                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                         vkutil::Mappability::writeOnly),
    m_triangleBufferRing(m_allocator,
                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                         vkutil::Mappability::writeOnly),
    m_colorRampPipeline(std::make_unique<ColorRampPipeline>(m_device)),
    m_tessellatePipeline(std::make_unique<TessellatePipeline>(m_device))
{
    static_assert(sizeof(m_drawDescriptorSetLayouts) ==
                  BINDINGS_SET_COUNT * sizeof(VkDescriptorSetLayout));
    m_allocator->setPLSContextImpl(this);
    m_platformFeatures.invertOffscreenY = false;
    m_platformFeatures.uninvertOnScreenY = true;
}

void PLSRenderContextVulkanImpl::initGPUObjects()
{
    VkDescriptorSetLayoutBinding perFlushDescriptorSetLayoutBindings[] = {
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
            .binding = PATH_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        },
        {
            .binding = PAINT_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = PAINT_AUX_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = CONTOUR_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        },
        {
            .binding = FLUSH_UNIFORM_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        },
        {
            .binding = IMAGE_DRAW_UNIFORM_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };
    VkDescriptorSetLayoutCreateInfo perFlushDescriptorSetLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = std::size(perFlushDescriptorSetLayoutBindings),
        .pBindings = perFlushDescriptorSetLayoutBindings,
    };
    VK_CHECK(vkCreateDescriptorSetLayout(m_device,
                                         &perFlushDescriptorSetLayoutCreateInfo,
                                         nullptr,
                                         &m_drawDescriptorSetLayouts[PER_FLUSH_BINDINGS_SET]));

    VkDescriptorSetLayoutBinding perDrawDescriptorSetLayoutBindings[] = {
        {
            .binding = IMAGE_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };
    VkDescriptorSetLayoutCreateInfo perDrawDescriptorSetLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = std::size(perDrawDescriptorSetLayoutBindings),
        .pBindings = perDrawDescriptorSetLayoutBindings,
    };
    VK_CHECK(vkCreateDescriptorSetLayout(m_device,
                                         &perDrawDescriptorSetLayoutCreateInfo,
                                         nullptr,
                                         &m_drawDescriptorSetLayouts[PER_DRAW_BINDINGS_SET]));

    VkDescriptorSetLayoutBinding samplerDescriptorSetLayoutBindings[] = {
        {
            .binding = GRAD_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = IMAGE_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };
    VkDescriptorSetLayoutCreateInfo samplerDescriptorSetLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = samplerDescriptorSetLayoutBindings,
    };
    VK_CHECK(vkCreateDescriptorSetLayout(m_device,
                                         &samplerDescriptorSetLayoutCreateInfo,
                                         nullptr,
                                         &m_drawDescriptorSetLayouts[SAMPLER_BINDINGS_SET]));

    VkDescriptorSetLayoutBinding plsTextureDescriptorSetLayoutBindings[] = {
        {
            .binding = COVERAGE_PLANE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };
    VkDescriptorSetLayoutCreateInfo plsTextureDescriptorSetLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = plsTextureDescriptorSetLayoutBindings,
    };
    VK_CHECK(vkCreateDescriptorSetLayout(m_device,
                                         &plsTextureDescriptorSetLayoutCreateInfo,
                                         nullptr,
                                         &m_drawDescriptorSetLayouts[PLS_TEXTURE_BINDINGS_SET]));

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

    VK_CHECK(vkCreateSampler(m_device, &linearSamplerCreateInfo, nullptr, &m_linearSampler));

    VkSamplerCreateInfo mipmapSamplerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .minLod = 0,
        .maxLod = VK_LOD_CLAMP_NONE,
    };

    VK_CHECK(vkCreateSampler(m_device, &mipmapSamplerCreateInfo, nullptr, &m_mipmapSampler));

    VkDescriptorPoolSize samplerDescriptorPoolSizes[] = {
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 2,
        },
    };
    VkDescriptorPoolCreateInfo samplerDescriptorPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1,
        .poolSizeCount = std::size(samplerDescriptorPoolSizes),
        .pPoolSizes = samplerDescriptorPoolSizes,
    };

    VK_CHECK(vkCreateDescriptorPool(m_device,
                                    &samplerDescriptorPoolCreateInfo,
                                    nullptr,
                                    &m_samplerDescriptorPool));

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_samplerDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_drawDescriptorSetLayouts[SAMPLER_BINDINGS_SET],
    };

    VK_CHECK(
        vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_samplerDescriptorSet));

    VkDescriptorImageInfo linearDescriptorImageInfo = {
        .sampler = m_linearSampler,
    };
    VkDescriptorImageInfo mipmapDescriptorImageInfo = {
        .sampler = m_mipmapSampler,
    };

    VkWriteDescriptorSet writeDescriptorSet[] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_samplerDescriptorSet,
            .dstBinding = GRAD_TEXTURE_IDX,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .pImageInfo = &linearDescriptorImageInfo,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_samplerDescriptorSet,
            .dstBinding = IMAGE_TEXTURE_IDX,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .pImageInfo = &mipmapDescriptorImageInfo,
        }};

    vkUpdateDescriptorSets(m_device, 2, writeDescriptorSet, 0, nullptr);

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = BINDINGS_SET_COUNT,
        .pSetLayouts = m_drawDescriptorSetLayouts,
    };

    VK_CHECK(vkCreatePipelineLayout(m_device,
                                    &pipelineLayoutCreateInfo,
                                    nullptr,
                                    &m_drawPipelineLayout));

    m_tessSpanIndexBuffer = m_allocator->makeBuffer(
        {
            .size = sizeof(pls::kTessSpanIndices),
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        },
        vkutil::Mappability::writeOnly);
    memcpy(vkutil::ScopedBufferFlush(*m_tessSpanIndexBuffer),
           pls::kTessSpanIndices,
           sizeof(pls::kTessSpanIndices));

    m_pathPatchVertexBuffer = m_allocator->makeBuffer(
        {
            .size = kPatchVertexBufferCount * sizeof(pls::PatchVertex),
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        },
        vkutil::Mappability::writeOnly);
    m_pathPatchIndexBuffer = m_allocator->makeBuffer(
        {
            .size = kPatchIndexBufferCount * sizeof(uint16_t),
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        },
        vkutil::Mappability::writeOnly);
    pls::GeneratePatchBufferData(
        vkutil::ScopedBufferFlush(*m_pathPatchVertexBuffer).as<PatchVertex*>(),
        vkutil::ScopedBufferFlush(*m_pathPatchIndexBuffer).as<uint16_t*>());

    m_imageRectVertexBuffer = m_allocator->makeBuffer(
        {
            .size = sizeof(pls::kImageRectVertices),
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        },
        vkutil::Mappability::writeOnly);
    m_imageRectIndexBuffer = m_allocator->makeBuffer(
        {
            .size = sizeof(pls::kImageRectIndices),
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        },
        vkutil::Mappability::writeOnly);
    memcpy(vkutil::ScopedBufferFlush(*m_imageRectVertexBuffer),
           pls::kImageRectVertices,
           sizeof(pls::kImageRectVertices));
    memcpy(vkutil::ScopedBufferFlush(*m_imageRectIndexBuffer),
           pls::kImageRectIndices,
           sizeof(pls::kImageRectIndices));

    constexpr static uint8_t black[] = {0, 0, 0, 1};
    m_nullImagePaintTexture = make_rcp<PLSTextureVulkanImpl>(m_allocator, 1, 1, 1, black);

    static_assert(sizeof(m_drawRenderPasses) ==
                  DrawPipeline::kRenderPassVariantCount * sizeof(VkRenderPass));
    for (int i = 0; i < DrawPipeline::kRenderPassVariantCount; ++i)
    {
        m_drawRenderPasses[i] = DrawPipeline::CreateVkRenderPass(m_device, i);
    }
}

PLSRenderContextVulkanImpl::~PLSRenderContextVulkanImpl()
{
    // Wait for all fences before cleaning up.
    for (const rcp<pls::CommandBufferCompletionFence>& fence : m_frameCompletionFences)
    {
        if (fence != nullptr)
        {
            fence->wait();
        }
    }

    // Disassociate from m_allocator before cleaning anything up, so rendering
    // objects just get deleted instead of coming back to us.
    m_allocator->didDestroyPLSContext();

    vkDestroySampler(m_device, m_linearSampler, nullptr);
    vkDestroySampler(m_device, m_mipmapSampler, nullptr);
    vkFreeDescriptorSets(m_device, m_samplerDescriptorPool, 1, &m_samplerDescriptorSet);
    vkDestroyDescriptorPool(m_device, m_samplerDescriptorPool, nullptr);

    for (auto& layout : m_drawDescriptorSetLayouts)
    {
        vkDestroyDescriptorSetLayout(m_device, layout, nullptr);
    }

    vkDestroyPipelineLayout(m_device, m_drawPipelineLayout, nullptr);

    for (auto& renderPass : m_drawRenderPasses)
    {
        vkDestroyRenderPass(m_device, renderPass, nullptr);
    }
}

void PLSRenderContextVulkanImpl::resizeGradientTexture(uint32_t width, uint32_t height)
{
    width = std::max(width, 1u);
    height = std::max(height, 1u);
    if (m_gradientTexture == nullptr || m_gradientTexture->info().extent.width != width ||
        m_gradientTexture->info().extent.height != height)
    {
        m_gradientTexture = m_allocator->makeTexture({
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .extent = {width, height, 1},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        });

        m_gradTextureView = m_allocator->makeTextureView(m_gradientTexture);

        m_gradTextureFramebuffer = m_allocator->makeFramebuffer({
            .renderPass = m_colorRampPipeline->renderPass(),
            .attachmentCount = 1,
            .pAttachments = m_gradTextureView->vkImageViewAddressOf(),
            .width = width,
            .height = height,
            .layers = 1,
        });
    }
}

void PLSRenderContextVulkanImpl::resizeTessellationTexture(uint32_t width, uint32_t height)
{
    width = std::max(width, 1u);
    height = std::max(height, 1u);
    if (m_tessVertexTexture == nullptr || m_tessVertexTexture->info().extent.width != width ||
        m_tessVertexTexture->info().extent.height != height)
    {
        m_tessVertexTexture = m_allocator->makeTexture({
            .format = VK_FORMAT_R32G32B32A32_UINT,
            .extent = {width, height, 1},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        });

        m_tessVertexTextureView = m_allocator->makeTextureView(m_tessVertexTexture);

        m_tessTextureFramebuffer = m_allocator->makeFramebuffer({
            .renderPass = m_tessellatePipeline->renderPass(),
            .attachmentCount = 1,
            .pAttachments = m_tessVertexTextureView->vkImageViewAddressOf(),
            .width = width,
            .height = height,
            .layers = 1,
        });
    }
}

void PLSRenderContextVulkanImpl::prepareToMapBuffers()
{
    ++m_currentFrameIdx;
    m_bufferRingIdx = (m_bufferRingIdx + 1) % pls::kBufferRingSize;

    // Wait for the existing resources to finish before we release/recycle them.
    if (rcp<pls::CommandBufferCompletionFence> fence =
            std::move(m_frameCompletionFences[m_bufferRingIdx]))
    {
        fence->wait();
    }

    // Delete resources that are no longer referenced by in-flight command buffers.
    while (!m_resourcePurgatory.empty() &&
           m_resourcePurgatory.front().expirationFrameIdx <= m_currentFrameIdx)
    {
        m_resourcePurgatory.pop_front();
    }

    // Synchronize buffer sizes in the buffer rings.
    m_flushUniformBufferRing.synchronizeSizeAt(m_bufferRingIdx);
    m_imageDrawUniformBufferRing.synchronizeSizeAt(m_bufferRingIdx);
    m_pathBufferRing.synchronizeSizeAt(m_bufferRingIdx);
    m_paintBufferRing.synchronizeSizeAt(m_bufferRingIdx);
    m_paintAuxBufferRing.synchronizeSizeAt(m_bufferRingIdx);
    m_contourBufferRing.synchronizeSizeAt(m_bufferRingIdx);
    m_simpleColorRampsBufferRing.synchronizeSizeAt(m_bufferRingIdx);
    m_gradSpanBufferRing.synchronizeSizeAt(m_bufferRingIdx);
    m_tessSpanBufferRing.synchronizeSizeAt(m_bufferRingIdx);
    m_triangleBufferRing.synchronizeSizeAt(m_bufferRingIdx);
}

namespace descriptor_pool_limits
{
constexpr static uint32_t kMaxUniformUpdates = 3;
constexpr static uint32_t kMaxDynamicUniformUpdates = 1;
constexpr static uint32_t kMaxImageTextureUpdates = 256;
constexpr static uint32_t kMaxSampledImageUpdates =
    2 + kMaxImageTextureUpdates; // tess + grad + imageTextures
constexpr static uint32_t kMaxStorageBufferUpdates = 6;
constexpr static uint32_t kMaxDescriptorSets = 3 + kMaxImageTextureUpdates;
} // namespace descriptor_pool_limits

PLSRenderContextVulkanImpl::DescriptorSetPool::DescriptorSetPool(PLSRenderContextVulkanImpl* impl) :
    RenderingResource(impl->m_allocator)
{
    VkDescriptorPoolSize descriptorPoolSizes[] = {
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = descriptor_pool_limits::kMaxUniformUpdates,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .descriptorCount = descriptor_pool_limits::kMaxDynamicUniformUpdates,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = descriptor_pool_limits::kMaxSampledImageUpdates,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = descriptor_pool_limits::kMaxStorageBufferUpdates,
        },
#if 0
          // For atomic mode
          {
              .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              .descriptorCount = 1,
          },
          // For rasterOrdering mode
          {
              .type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
              .descriptorCount = 4,
          },
#endif
    };

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = descriptor_pool_limits::kMaxDescriptorSets,
        .poolSizeCount = std::size(descriptorPoolSizes),
        .pPoolSizes = descriptorPoolSizes,
    };

    VK_CHECK(
        vkCreateDescriptorPool(device(), &descriptorPoolCreateInfo, nullptr, &m_vkDescriptorPool));
}

PLSRenderContextVulkanImpl::DescriptorSetPool::~DescriptorSetPool()
{
    freeDescriptorSets();
    vkDestroyDescriptorPool(device(), m_vkDescriptorPool, nullptr);
}

VkDescriptorSet PLSRenderContextVulkanImpl::DescriptorSetPool::allocateDescriptorSet(
    VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_vkDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };

    VK_CHECK(vkAllocateDescriptorSets(device(),
                                      &descriptorSetAllocateInfo,
                                      &m_descriptorSets.emplace_back()));

    return m_descriptorSets.back();
}

void PLSRenderContextVulkanImpl::DescriptorSetPool::freeDescriptorSets()
{
    if (!m_descriptorSets.empty())
    {
        vkFreeDescriptorSets(device(),
                             m_vkDescriptorPool,
                             m_descriptorSets.size(),
                             m_descriptorSets.data());
    }
}

void PLSRenderContextVulkanImpl::DescriptorSetPool::onRefCntReachedZero() const
{
    constexpr static uint32_t kMaxDescriptorSetPoolsInPool = 64;

    if (plsImplVulkan() != nullptr &&
        plsImplVulkan()->m_descriptorSetPoolPool.size() < kMaxDescriptorSetPoolsInPool)
    {
        // Hang out in the plsContext's m_descriptorSetPoolPool until in-flight
        // command buffers have finished using our descriptors.
        plsImplVulkan()->m_descriptorSetPoolPool.emplace_back(const_cast<DescriptorSetPool*>(this),
                                                              plsImplVulkan()->m_currentFrameIdx);
    }
    else
    {
        delete this;
    }
}

rcp<PLSRenderContextVulkanImpl::DescriptorSetPool> PLSRenderContextVulkanImpl::
    makeDescriptorSetPool()
{
    rcp<DescriptorSetPool> pool;
    if (!m_descriptorSetPoolPool.empty() &&
        m_descriptorSetPoolPool.front().expirationFrameIdx <= m_currentFrameIdx)
    {
        pool = ref_rcp(m_descriptorSetPoolPool.front().resource.release());
        pool->freeDescriptorSets();
        m_descriptorSetPoolPool.pop_front();
    }
    else
    {
        pool = make_rcp<DescriptorSetPool>(this);
    }
    assert(pool->debugging_refcnt() == 1);
    return pool;
}

void PLSRenderTargetVulkan::synchronize(vkutil::Allocator* allocator, VkCommandBuffer commandBuffer)
{
    if (m_coverageTexture == nullptr)
    {
        m_coverageTexture = allocator->makeTexture({
            .format = VK_FORMAT_R32_UINT,
            .extent = {width(), height(), 1},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        });

        vkutil::insert_image_memory_barrier(commandBuffer,
                                            *m_coverageTexture,
                                            VK_IMAGE_LAYOUT_UNDEFINED,
                                            VK_IMAGE_LAYOUT_GENERAL);

        m_coverageTextureView = allocator->makeTextureView(m_coverageTexture);
    }

    if (m_clipTexture == nullptr)
    {
        m_clipTexture = allocator->makeTexture({
            .format = VK_FORMAT_R32_UINT,
            .extent = {width(), height(), 1},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        });

        vkutil::insert_image_memory_barrier(commandBuffer,
                                            *m_clipTexture,
                                            VK_IMAGE_LAYOUT_UNDEFINED,
                                            VK_IMAGE_LAYOUT_GENERAL);

        m_clipTextureView = allocator->makeTextureView(m_clipTexture);
    }

    if (m_originalDstColorTexture == nullptr)
    {
        m_originalDstColorTexture = allocator->makeTexture({
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .extent = {width(), height(), 1},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        });

        vkutil::insert_image_memory_barrier(commandBuffer,
                                            *m_originalDstColorTexture,
                                            VK_IMAGE_LAYOUT_UNDEFINED,
                                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        m_originalDstColorTextureView = allocator->makeTextureView(m_originalDstColorTexture);
    }
}

void PLSRenderContextVulkanImpl::flush(const FlushDescriptor& desc)
{
    auto vkCommandBuffer = reinterpret_cast<VkCommandBuffer>(desc.externalCommandBuffer);
    rcp<DescriptorSetPool> descriptorSetPool = makeDescriptorSetPool();

    constexpr static VkDeviceSize zeroOffset[1] = {0};
    constexpr static uint32_t zeroOffset32[1] = {0};

    vkutil::insert_image_memory_barrier(vkCommandBuffer,
                                        *m_gradientTexture,
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // Render the complex color ramps to the gradient texture.
    if (desc.complexGradSpanCount > 0)
    {
        VkDescriptorSet descriptorSet =
            descriptorSetPool->allocateDescriptorSet(m_colorRampPipeline->descriptorSetLayout());

        VkDescriptorBufferInfo descriptorBufferInfo = {
            .buffer = m_flushUniformBufferRing.vkBufferAt(m_bufferRingIdx),
            .offset = desc.flushUniformDataOffsetInBytes,
            .range = VK_WHOLE_SIZE,
        };

        VkWriteDescriptorSet writeDescriptorSet = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSet,
            .dstBinding = FLUSH_UNIFORM_BUFFER_IDX,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &descriptorBufferInfo,
        };

        vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);

        VkRect2D renderArea = {
            .offset = {0, static_cast<int32_t>(desc.complexGradRowsTop)},
            .extent = {pls::kGradTextureWidth, desc.complexGradRowsHeight},
        };

        VkRenderPassBeginInfo renderPassBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = m_colorRampPipeline->renderPass(),
            .framebuffer = *m_gradTextureFramebuffer,
            .renderArea = renderArea,
        };

        vkCmdBeginRenderPass(vkCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(vkCommandBuffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_colorRampPipeline->renderPipeline());

        vkCmdSetViewport(vkCommandBuffer, 0, 1, vkutil::ViewportFromRect2D(renderArea));

        vkCmdSetScissor(vkCommandBuffer, 0, 1, &renderArea);

        VkBuffer buffer = m_gradSpanBufferRing.vkBufferAt(m_bufferRingIdx);
        vkCmdBindVertexBuffers(vkCommandBuffer, 0, 1, &buffer, zeroOffset);

        vkCmdBindDescriptorSets(vkCommandBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_colorRampPipeline->pipelineLayout(),
                                PER_FLUSH_BINDINGS_SET,
                                1,
                                &descriptorSet,
                                0,
                                nullptr);

        vkCmdDraw(vkCommandBuffer, 4, desc.complexGradSpanCount, 0, desc.firstComplexGradSpan);

        vkCmdEndRenderPass(vkCommandBuffer);
    }

    vkutil::insert_image_memory_barrier(vkCommandBuffer,
                                        *m_gradientTexture,
                                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy the simple color ramps to the gradient texture.
    if (desc.simpleGradTexelsHeight > 0)
    {
        VkBufferImageCopy bufferImageCopy{
            .bufferOffset = desc.simpleGradDataOffsetInBytes,
            .bufferRowLength = pls::kGradTextureWidth,
            .imageSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .layerCount = 1,
                },
            .imageExtent =
                {
                    desc.simpleGradTexelsWidth,
                    desc.simpleGradTexelsHeight,
                    1,
                },
        };

        vkCmdCopyBufferToImage(vkCommandBuffer,
                               m_simpleColorRampsBufferRing.vkBufferAt(m_bufferRingIdx),
                               *m_gradientTexture,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1,
                               &bufferImageCopy);
    }

    vkutil::insert_image_memory_barrier(vkCommandBuffer,
                                        *m_gradientTexture,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkutil::insert_image_memory_barrier(vkCommandBuffer,
                                        *m_tessVertexTexture,
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // Tessellate all curves into vertices in the tessellation texture.
    if (desc.tessVertexSpanCount > 0)
    {
        VkDescriptorSet descriptorSet =
            descriptorSetPool->allocateDescriptorSet(m_tessellatePipeline->descriptorSetLayout());

        VkDescriptorBufferInfo descriptorBufferInfo = {
            .buffer = m_pathBufferRing.vkBufferAt(m_bufferRingIdx),
            .offset = desc.firstPath * sizeof(pls::PathData),
            .range = VK_WHOLE_SIZE,
        };

        VkWriteDescriptorSet writeDescriptorSet = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSet,
            .dstBinding = PATH_BUFFER_IDX,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &descriptorBufferInfo,
        };

        vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);

        descriptorBufferInfo.buffer = m_contourBufferRing.vkBufferAt(m_bufferRingIdx);
        descriptorBufferInfo.offset = desc.firstContour * sizeof(pls::ContourData);
        writeDescriptorSet.dstBinding = CONTOUR_BUFFER_IDX;

        vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);

        descriptorBufferInfo.buffer = m_flushUniformBufferRing.vkBufferAt(m_bufferRingIdx);
        descriptorBufferInfo.offset = desc.flushUniformDataOffsetInBytes;
        writeDescriptorSet.dstBinding = FLUSH_UNIFORM_BUFFER_IDX;
        writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

        vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);

        VkRect2D renderArea = {
            .extent = {pls::kTessTextureWidth, desc.tessDataHeight},
        };

        VkRenderPassBeginInfo renderPassBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = m_tessellatePipeline->renderPass(),
            .framebuffer = *m_tessTextureFramebuffer,
            .renderArea = renderArea,
        };

        vkCmdBeginRenderPass(vkCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(vkCommandBuffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_tessellatePipeline->renderPipeline());

        vkCmdSetViewport(vkCommandBuffer, 0, 1, vkutil::ViewportFromRect2D(renderArea));

        vkCmdSetScissor(vkCommandBuffer, 0, 1, &renderArea);

        VkBuffer buffer = m_tessSpanBufferRing.vkBufferAt(m_bufferRingIdx);
        vkCmdBindVertexBuffers(vkCommandBuffer, 0, 1, &buffer, zeroOffset);

        vkCmdBindIndexBuffer(vkCommandBuffer, *m_tessSpanIndexBuffer, 0, VK_INDEX_TYPE_UINT16);

        vkCmdBindDescriptorSets(vkCommandBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_tessellatePipeline->pipelineLayout(),
                                0,
                                1,
                                &descriptorSet,
                                0,
                                nullptr);

        vkCmdDrawIndexed(vkCommandBuffer,
                         std::size(pls::kTessSpanIndices),
                         desc.tessVertexSpanCount,
                         0,
                         0,
                         desc.firstTessVertexSpan);

        vkCmdEndRenderPass(vkCommandBuffer);
    }

    vkutil::insert_image_memory_barrier(vkCommandBuffer,
                                        *m_tessVertexTexture,
                                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Apply pending texture updates.
    if (m_nullImagePaintTexture->hasUpdates())
    {
        m_nullImagePaintTexture->synchronize(vkCommandBuffer, this);
    }
    for (const DrawBatch& batch : *desc.drawList)
    {
        if (auto imageTextureVulkan = static_cast<const PLSTextureVulkanImpl*>(batch.imageTexture))
        {
            if (imageTextureVulkan->hasUpdates())
            {
                imageTextureVulkan->synchronize(vkCommandBuffer, this);
            }
        }
    }

    VkClearColorValue clearColor{};
    if (desc.colorLoadAction == pls::LoadAction::clear)
    {
        UnpackColorToRGBA32F(desc.clearColor, clearColor.float32);
    }

    auto* renderTarget = static_cast<PLSRenderTargetVulkan*>(desc.renderTarget);
    renderTarget->synchronize(m_allocator.get(), vkCommandBuffer);

    int renderPassVariantIdx =
        DrawPipeline::RenderPassVariantIdx(renderTarget->m_framebufferFormat, desc.colorLoadAction);
    VkRenderPass drawRenderPass = m_drawRenderPasses[renderPassVariantIdx];

    VkImageView imageViews[] = {
        *renderTarget->m_targetTextureView,
        *renderTarget->m_coverageTextureView,
        *renderTarget->m_clipTextureView,
        *renderTarget->m_originalDstColorTextureView,
    };

    rcp<vkutil::Framebuffer> framebuffer = m_allocator->makeFramebuffer({
        .renderPass = drawRenderPass,
        .attachmentCount = 4,
        .pAttachments = imageViews,
        .width = static_cast<uint32_t>(renderTarget->width()),
        .height = static_cast<uint32_t>(renderTarget->height()),
        .layers = 1,
    });

    VkRect2D renderArea = {
        .extent = {static_cast<uint32_t>(renderTarget->width()),
                   static_cast<uint32_t>(renderTarget->height())},
    };

    VkClearValue clearValues[] = {
        {.color = clearColor},
        {},
        {},
    };

    VkRenderPassBeginInfo renderPassBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = drawRenderPass,
        .framebuffer = *framebuffer,
        .renderArea = renderArea,
        .clearValueCount = std::size(clearValues),
        .pClearValues = clearValues,
    };

    vkCmdBeginRenderPass(vkCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdSetViewport(vkCommandBuffer, 0, 1, vkutil::ViewportFromRect2D(renderArea));

    vkCmdSetScissor(vkCommandBuffer, 0, 1, &renderArea);

    vkCmdBindDescriptorSets(vkCommandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_drawPipelineLayout,
                            SAMPLER_BINDINGS_SET,
                            1,
                            &m_samplerDescriptorSet,
                            0,
                            nullptr);

    // Bind per-flush resources.
    VkDescriptorSet perFlushDescriptorSet = descriptorSetPool->allocateDescriptorSet(
        m_drawDescriptorSetLayouts[PER_FLUSH_BINDINGS_SET]);

    VkDescriptorImageInfo descriptorImageInfo = {
        .imageView = *m_tessVertexTextureView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkWriteDescriptorSet writeDescriptorSet = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = perFlushDescriptorSet,
        .dstBinding = TESS_VERTEX_TEXTURE_IDX,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = &descriptorImageInfo,
    };
    vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);

    writeDescriptorSet.dstBinding = GRAD_TEXTURE_IDX;
    descriptorImageInfo.imageView = *m_gradTextureView;
    vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);

    VkDescriptorBufferInfo descriptorBufferInfo = {
        .buffer = m_pathBufferRing.vkBufferAt(m_bufferRingIdx),
        .offset = desc.firstPath * sizeof(pls::PathData),
        .range = VK_WHOLE_SIZE,
    };

    writeDescriptorSet.dstBinding = PATH_BUFFER_IDX;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeDescriptorSet.pImageInfo = nullptr;
    writeDescriptorSet.pBufferInfo = &descriptorBufferInfo;
    vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);

    descriptorBufferInfo.buffer = m_paintBufferRing.vkBufferAt(m_bufferRingIdx);
    descriptorBufferInfo.offset = desc.firstPaint * sizeof(pls::PaintData);
    writeDescriptorSet.dstBinding = PAINT_BUFFER_IDX;
    vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);

    descriptorBufferInfo.buffer = m_paintAuxBufferRing.vkBufferAt(m_bufferRingIdx);
    descriptorBufferInfo.offset = desc.firstPaintAux * sizeof(pls::PaintAuxData);
    writeDescriptorSet.dstBinding = PAINT_AUX_BUFFER_IDX;
    vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);

    descriptorBufferInfo.buffer = m_contourBufferRing.vkBufferAt(m_bufferRingIdx);
    descriptorBufferInfo.offset = desc.firstContour * sizeof(pls::ContourData);
    writeDescriptorSet.dstBinding = CONTOUR_BUFFER_IDX;
    vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);

    descriptorBufferInfo.buffer = m_flushUniformBufferRing.vkBufferAt(m_bufferRingIdx),
    descriptorBufferInfo.offset = desc.flushUniformDataOffsetInBytes;
    writeDescriptorSet.dstBinding = FLUSH_UNIFORM_BUFFER_IDX;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);

    writeDescriptorSet.dstBinding = IMAGE_DRAW_UNIFORM_BUFFER_IDX;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    descriptorBufferInfo.buffer = m_imageDrawUniformBufferRing.vkBufferAt(m_bufferRingIdx);
    descriptorBufferInfo.offset = 0;
    descriptorBufferInfo.range = sizeof(pls::ImageDrawUniforms);
    vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);

    vkCmdBindDescriptorSets(vkCommandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_drawPipelineLayout,
                            PER_FLUSH_BINDINGS_SET,
                            1,
                            &perFlushDescriptorSet,
                            1,
                            zeroOffset32);

    // Execute the DrawList.
    const PLSTextureVulkanImpl* boundImageTexture = nullptr;
    uint32_t imageTextureUpdateCount = 0;
    for (const DrawBatch& batch : *desc.drawList)
    {
        if (batch.elementCount == 0)
        {
            continue;
        }

        DrawType drawType = batch.drawType;

        if (batch.imageDrawDataOffset != 0)
        {
            // Re-bind the per-flush descriptor set with its unique offset for
            // image-draw uniforms.
            vkCmdBindDescriptorSets(vkCommandBuffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_drawPipelineLayout,
                                    PER_FLUSH_BINDINGS_SET,
                                    1,
                                    &perFlushDescriptorSet,
                                    1,
                                    &batch.imageDrawDataOffset);
        }

        // Bind the appropriate image texture, if any.
        auto imageTexture = batch.imageTexture != nullptr
                                ? static_cast<const PLSTextureVulkanImpl*>(batch.imageTexture)
                                : m_nullImagePaintTexture.get();
        if (imageTexture != boundImageTexture)
        {
            if (imageTexture->m_descriptorSetFrameIdx != m_currentFrameIdx)
            {
                if (imageTextureUpdateCount >= descriptor_pool_limits::kMaxImageTextureUpdates)
                {
                    // We ran out of room for image texture updates. Allocate a new
                    // pool.
                    descriptorSetPool = makeDescriptorSetPool();
                    imageTextureUpdateCount = 0;
                }
                imageTexture->m_imageTextureDescriptorSet =
                    descriptorSetPool->allocateDescriptorSet(
                        m_drawDescriptorSetLayouts[PER_DRAW_BINDINGS_SET]);

                VkDescriptorImageInfo descriptorImageInfo = {
                    .imageView = *imageTexture->m_textureView,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                };

                VkWriteDescriptorSet writeDescriptorSet = {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = imageTexture->m_imageTextureDescriptorSet,
                    .dstBinding = IMAGE_TEXTURE_IDX,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                    .pImageInfo = &descriptorImageInfo,
                };

                vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);

                ++imageTextureUpdateCount;
                imageTexture->m_descriptorSetFrameIdx = m_currentFrameIdx;
            }

            vkCmdBindDescriptorSets(vkCommandBuffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_drawPipelineLayout,
                                    PER_DRAW_BINDINGS_SET,
                                    1,
                                    &imageTexture->m_imageTextureDescriptorSet,
                                    0,
                                    nullptr);

            boundImageTexture = imageTexture;
        }

        // Setup the pipeline for this specific drawType and shaderFeatures.
        pls::ShaderFeatures shaderFeatures = batch.shaderFeatures;
        // For now we don't specialize any shaders. Just take the most general
        // one.
        shaderFeatures = pls::ShaderFeaturesMaskFor(drawType, desc.interlockMode);
        uint32_t pipelineKey = pls::ShaderUniqueKey(drawType,
                                                    shaderFeatures,
                                                    desc.interlockMode,
                                                    pls::ShaderMiscFlags::none);
        auto drawPipelineOptions = DrawPipelineOptions::none;
        if (desc.wireframe)
        {
            drawPipelineOptions |= DrawPipelineOptions::wireframe;
        }
        assert(pipelineKey << kDrawPipelineOptionCount >> kDrawPipelineOptionCount == pipelineKey);
        pipelineKey =
            (pipelineKey << kDrawPipelineOptionCount) | static_cast<uint32_t>(drawPipelineOptions);
        const DrawPipeline& drawPipeline = m_drawPipelines
                                               .try_emplace(pipelineKey,
                                                            this,
                                                            drawType,
                                                            desc.interlockMode,
                                                            shaderFeatures,
                                                            drawPipelineOptions)
                                               .first->second;

        vkCmdBindPipeline(vkCommandBuffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          drawPipeline.vkPipelineAt(renderPassVariantIdx));

        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::outerCurvePatches:
            {
                // Draw PLS patches that connect the tessellation vertices.
                vkCmdBindVertexBuffers(vkCommandBuffer,
                                       0,
                                       1,
                                       m_pathPatchVertexBuffer->vkBufferAddressOf(),
                                       zeroOffset);

                vkCmdBindIndexBuffer(vkCommandBuffer,
                                     *m_pathPatchIndexBuffer,
                                     0,
                                     VK_INDEX_TYPE_UINT16);

                vkCmdDrawIndexed(vkCommandBuffer,
                                 pls::PatchIndexCount(drawType),
                                 batch.elementCount,
                                 pls::PatchBaseIndex(drawType),
                                 0,
                                 batch.baseElement);
                break;
            }

            case DrawType::interiorTriangulation:
            {
                VkBuffer buffer = m_triangleBufferRing.vkBufferAt(m_bufferRingIdx);
                vkCmdBindVertexBuffers(vkCommandBuffer, 0, 1, &buffer, zeroOffset);

                vkCmdDraw(vkCommandBuffer, batch.elementCount, 1, batch.baseElement, 0);
                break;
            }

            case DrawType::imageRect:
            {
                assert(desc.interlockMode == pls::InterlockMode::atomics);

                vkCmdBindVertexBuffers(vkCommandBuffer,
                                       0,
                                       1,
                                       m_imageRectVertexBuffer->vkBufferAddressOf(),
                                       zeroOffset);

                vkCmdDrawIndexed(vkCommandBuffer,
                                 std::size(pls::kImageRectIndices),
                                 1,
                                 batch.baseElement,
                                 0,
                                 0);
                break;
            }

            case DrawType::imageMesh:
            {
                auto vertexBuffer = static_cast<const RenderBufferVulkanImpl*>(batch.vertexBuffer);
                auto uvBuffer = static_cast<const RenderBufferVulkanImpl*>(batch.uvBuffer);
                auto indexBuffer = static_cast<const RenderBufferVulkanImpl*>(batch.indexBuffer);

                vkCmdBindVertexBuffers(vkCommandBuffer,
                                       0,
                                       1,
                                       vertexBuffer->frontVkBufferAddressOf(),
                                       zeroOffset);

                vkCmdBindVertexBuffers(vkCommandBuffer,
                                       1,
                                       1,
                                       uvBuffer->frontVkBufferAddressOf(),
                                       zeroOffset);

                vkCmdBindIndexBuffer(vkCommandBuffer,
                                     indexBuffer->frontVkBuffer(),
                                     0,
                                     VK_INDEX_TYPE_UINT16);

                vkCmdDrawIndexed(vkCommandBuffer, batch.elementCount, 1, batch.baseElement, 0, 0);
                break;
            }

            case DrawType::plsAtomicInitialize:
            case DrawType::plsAtomicResolve:
            case DrawType::stencilClipReset:
                RIVE_UNREACHABLE();
        }
    }

    vkCmdEndRenderPass(vkCommandBuffer);

    if (desc.isFinalFlushOfFrame)
    {
        m_frameCompletionFences[m_bufferRingIdx] = ref_rcp(desc.frameCompletionFence);
    }
}

std::unique_ptr<PLSRenderContext> PLSRenderContextVulkanImpl::MakeContext(
    rcp<vkutil::Allocator> allocator)
{
    std::unique_ptr<PLSRenderContextVulkanImpl> impl(
        new PLSRenderContextVulkanImpl(std::move(allocator)));
    impl->initGPUObjects();
    return std::make_unique<PLSRenderContext>(std::move(impl));
}
} // namespace rive::pls
