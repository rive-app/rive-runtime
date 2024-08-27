/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"

#include "rive/renderer/image.hpp"
#include "shaders/constants.glsl"

namespace spirv
{
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

#include "generated/shaders/spirv/atomic_draw_path.vert.h"
#include "generated/shaders/spirv/atomic_draw_path.frag.h"
#include "generated/shaders/spirv/atomic_draw_path.fixedblend_frag.h"
#include "generated/shaders/spirv/atomic_draw_interior_triangles.vert.h"
#include "generated/shaders/spirv/atomic_draw_interior_triangles.frag.h"
#include "generated/shaders/spirv/atomic_draw_interior_triangles.fixedblend_frag.h"
#include "generated/shaders/spirv/atomic_draw_image_rect.vert.h"
#include "generated/shaders/spirv/atomic_draw_image_rect.frag.h"
#include "generated/shaders/spirv/atomic_draw_image_rect.fixedblend_frag.h"
#include "generated/shaders/spirv/atomic_draw_image_mesh.vert.h"
#include "generated/shaders/spirv/atomic_draw_image_mesh.frag.h"
#include "generated/shaders/spirv/atomic_draw_image_mesh.fixedblend_frag.h"
#include "generated/shaders/spirv/atomic_resolve_pls.vert.h"
#include "generated/shaders/spirv/atomic_resolve_pls.frag.h"
#include "generated/shaders/spirv/atomic_resolve_pls.fixedblend_frag.h"
}; // namespace spirv

#ifdef RIVE_DECODERS
#include "rive/decoders/bitmap_decoder.hpp"
#endif

namespace rive::gpu
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
    RenderBufferVulkanImpl(rcp<VulkanContext> vk,
                           RenderBufferType renderBufferType,
                           RenderBufferFlags renderBufferFlags,
                           size_t sizeInBytes) :
        RenderBuffer(renderBufferType, renderBufferFlags, sizeInBytes),
        m_bufferRing(std::move(vk),
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
        m_bufferRingIdx = (m_bufferRingIdx + 1) % gpu::kBufferRingSize;
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
    return make_rcp<RenderBufferVulkanImpl>(m_vk, type, flags, sizeInBytes);
}

class PLSTextureVulkanImpl : public PLSTexture
{
public:
    PLSTextureVulkanImpl(rcp<VulkanContext> vk,
                         uint32_t width,
                         uint32_t height,
                         uint32_t mipLevelCount,
                         const uint8_t imageDataRGBA[]) :
        PLSTexture(width, height),
        m_vk(std::move(vk)),
        m_texture(m_vk->makeTexture({
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .extent = {width, height, 1},
            .mipLevels = mipLevelCount,
            .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        })),
        m_textureView(m_vk->makeTextureView(m_texture)),
        m_imageUploadBuffer(m_vk->makeBuffer(
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

    void synchronize(VkCommandBuffer commandBuffer) const
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

        m_vk->insertImageMemoryBarrier(commandBuffer,
                                       *m_texture,
                                       VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       0,
                                       m_texture->info().mipLevels);

        m_vk->CmdCopyBufferToImage(commandBuffer,
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

                m_vk->insertImageMemoryBarrier(commandBuffer,
                                               *m_texture,
                                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                               level - 1,
                                               1);

                m_vk->CmdBlitImage(commandBuffer,
                                   *m_texture,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   *m_texture,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   1,
                                   &imageBlit,
                                   VK_FILTER_LINEAR);
            }

            m_vk->insertImageMemoryBarrier(commandBuffer,
                                           *m_texture,
                                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                           0,
                                           mipLevels - 1);
        }

        m_vk->insertImageMemoryBarrier(commandBuffer,
                                       *m_texture,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                       mipLevels - 1,
                                       1);

        m_imageUploadBuffer = nullptr;
    }

private:
    friend class PLSRenderContextVulkanImpl;

    rcp<VulkanContext> m_vk;
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
        return make_rcp<PLSTextureVulkanImpl>(m_vk, width, height, mipLevelCount, bitmap->bytes());
    }
#endif
    return nullptr;
}

// Renders color ramps to the gradient texture.
class PLSRenderContextVulkanImpl::ColorRampPipeline
{
public:
    ColorRampPipeline(rcp<VulkanContext> vk) : m_vk(std::move(vk))
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

        VK_CHECK(m_vk->CreateDescriptorSetLayout(m_vk->device,
                                                 &descriptorSetLayoutCreateInfo,
                                                 nullptr,
                                                 &m_descriptorSetLayout));

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &m_descriptorSetLayout,
        };

        VK_CHECK(m_vk->CreatePipelineLayout(m_vk->device,
                                            &pipelineLayoutCreateInfo,
                                            nullptr,
                                            &m_pipelineLayout));

        VkShaderModuleCreateInfo shaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = sizeof(spirv::color_ramp_vert),
            .pCode = spirv::color_ramp_vert,
        };

        VkShaderModule vertexShader;
        VK_CHECK(m_vk->CreateShaderModule(m_vk->device,
                                          &shaderModuleCreateInfo,
                                          nullptr,
                                          &vertexShader));

        shaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = sizeof(spirv::color_ramp_frag),
            .pCode = spirv::color_ramp_frag,
        };

        VkShaderModule fragmentShader;
        VK_CHECK(m_vk->CreateShaderModule(m_vk->device,
                                          &shaderModuleCreateInfo,
                                          nullptr,
                                          &fragmentShader));

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
            .stride = sizeof(gpu::GradientSpan),
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
            .colorWriteMask = vkutil::kColorWriteMaskRGBA,
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

        VK_CHECK(
            m_vk->CreateRenderPass(m_vk->device, &renderPassCreateInfo, nullptr, &m_renderPass));

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

        VK_CHECK(m_vk->CreateGraphicsPipelines(m_vk->device,
                                               VK_NULL_HANDLE,
                                               1,
                                               &graphicsPipelineCreateInfo,
                                               nullptr,
                                               &m_renderPipeline));

        m_vk->DestroyShaderModule(m_vk->device, vertexShader, nullptr);
        m_vk->DestroyShaderModule(m_vk->device, fragmentShader, nullptr);
    }

    ~ColorRampPipeline()
    {
        m_vk->DestroyDescriptorSetLayout(m_vk->device, m_descriptorSetLayout, nullptr);
        m_vk->DestroyPipelineLayout(m_vk->device, m_pipelineLayout, nullptr);
        m_vk->DestroyRenderPass(m_vk->device, m_renderPass, nullptr);
        m_vk->DestroyPipeline(m_vk->device, m_renderPipeline, nullptr);
    }

    const VkDescriptorSetLayout& descriptorSetLayout() const { return m_descriptorSetLayout; }
    VkPipelineLayout pipelineLayout() const { return m_pipelineLayout; }
    VkRenderPass renderPass() const { return m_renderPass; }
    VkPipeline renderPipeline() const { return m_renderPipeline; }

private:
    rcp<VulkanContext> m_vk;
    VkDescriptorSetLayout m_descriptorSetLayout;
    VkPipelineLayout m_pipelineLayout;
    VkRenderPass m_renderPass;
    VkPipeline m_renderPipeline;
};

// Renders tessellated vertices to the tessellation texture.
class PLSRenderContextVulkanImpl::TessellatePipeline
{
public:
    TessellatePipeline(rcp<VulkanContext> vk) : m_vk(std::move(vk))
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
            .bindingCount = std::size(descriptorSetLayoutBindings),
            .pBindings = descriptorSetLayoutBindings,
        };

        VK_CHECK(m_vk->CreateDescriptorSetLayout(m_vk->device,
                                                 &descriptorSetLayoutCreateInfo,
                                                 nullptr,
                                                 &m_descriptorSetLayout));

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &m_descriptorSetLayout,
        };

        VK_CHECK(m_vk->CreatePipelineLayout(m_vk->device,
                                            &pipelineLayoutCreateInfo,
                                            nullptr,
                                            &m_pipelineLayout));

        VkShaderModuleCreateInfo shaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = sizeof(spirv::tessellate_vert),
            .pCode = spirv::tessellate_vert,
        };

        VkShaderModule vertexShader;
        VK_CHECK(m_vk->CreateShaderModule(m_vk->device,
                                          &shaderModuleCreateInfo,
                                          nullptr,
                                          &vertexShader));

        shaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = sizeof(spirv::tessellate_frag),
            .pCode = spirv::tessellate_frag,
        };

        VkShaderModule fragmentShader;
        VK_CHECK(m_vk->CreateShaderModule(m_vk->device,
                                          &shaderModuleCreateInfo,
                                          nullptr,
                                          &fragmentShader));

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
            .stride = sizeof(gpu::TessVertexSpan),
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
            .colorWriteMask = vkutil::kColorWriteMaskRGBA,
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

        VK_CHECK(
            m_vk->CreateRenderPass(m_vk->device, &renderPassCreateInfo, nullptr, &m_renderPass));

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

        VK_CHECK(m_vk->CreateGraphicsPipelines(m_vk->device,
                                               VK_NULL_HANDLE,
                                               1,
                                               &graphicsPipelineCreateInfo,
                                               nullptr,
                                               &m_renderPipeline));

        m_vk->DestroyShaderModule(m_vk->device, vertexShader, nullptr);
        m_vk->DestroyShaderModule(m_vk->device, fragmentShader, nullptr);
    }

    ~TessellatePipeline()
    {
        m_vk->DestroyDescriptorSetLayout(m_vk->device, m_descriptorSetLayout, nullptr);
        m_vk->DestroyPipelineLayout(m_vk->device, m_pipelineLayout, nullptr);
        m_vk->DestroyRenderPass(m_vk->device, m_renderPass, nullptr);
        m_vk->DestroyPipeline(m_vk->device, m_renderPipeline, nullptr);
    }

    const VkDescriptorSetLayout& descriptorSetLayout() const { return m_descriptorSetLayout; }
    VkPipelineLayout pipelineLayout() const { return m_pipelineLayout; }
    VkRenderPass renderPass() const { return m_renderPass; }
    VkPipeline renderPipeline() const { return m_renderPipeline; }

private:
    rcp<VulkanContext> m_vk;
    VkDescriptorSetLayout m_descriptorSetLayout;
    VkPipelineLayout m_pipelineLayout;
    VkRenderPass m_renderPass;
    VkPipeline m_renderPipeline;
};

enum class DrawPipelineLayoutOptions
{
    none = 0,
    fixedFunctionColorBlend = 1 << 0, // COLOR is not an input attachment.
};
RIVE_MAKE_ENUM_BITSET(DrawPipelineLayoutOptions);

class PLSRenderContextVulkanImpl::DrawPipelineLayout
{
public:
    static_assert(kDrawPipelineLayoutOptionCount == 1);

    // Number of render pass variants that can be used with a single DrawPipeline
    // (framebufferFormat x loadOp).
    constexpr static int kRenderPassVariantCount = 6;

    static int RenderPassVariantIdx(VkFormat framebufferFormat, gpu::LoadAction loadAction)
    {
        int loadActionIdx = static_cast<int>(loadAction);
        assert(0 <= loadActionIdx && loadActionIdx < 3);
        assert(framebufferFormat == VK_FORMAT_B8G8R8A8_UNORM ||
               framebufferFormat == VK_FORMAT_R8G8B8A8_UNORM);
        int idx = (loadActionIdx << 1) | (framebufferFormat == VK_FORMAT_B8G8R8A8_UNORM ? 1 : 0);
        assert(0 <= idx && idx < kRenderPassVariantCount);
        return idx;
    }

    constexpr static VkFormat FormatFromRenderPassVariant(int idx)
    {
        return (idx & 1) ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R8G8B8A8_UNORM;
    }

    constexpr static VkAttachmentLoadOp LoadOpFromRenderPassVariant(int idx)
    {
        auto loadAction = static_cast<gpu::LoadAction>(idx >> 1);
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

    constexpr static uint32_t PLSAttachmentCount(gpu::InterlockMode interlockMode)
    {
        return interlockMode == gpu::InterlockMode::atomics ? 2 : 4;
    }

    DrawPipelineLayout(PLSRenderContextVulkanImpl* impl,
                       gpu::InterlockMode interlockMode,
                       DrawPipelineLayoutOptions options) :
        m_vk(ref_rcp(impl->vulkanContext())), m_interlockMode(interlockMode), m_options(options)
    {
        assert(interlockMode != gpu::InterlockMode::depthStencil); // TODO: msaa.

        // Most bindings only need to be set once per flush.
        VkDescriptorSetLayoutBinding perFlushLayoutBindings[] = {
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
                .stageFlags = static_cast<VkShaderStageFlags>(
                    m_interlockMode == gpu::InterlockMode::atomics ? VK_SHADER_STAGE_FRAGMENT_BIT
                                                                   : VK_SHADER_STAGE_VERTEX_BIT),
            },
            {
                .binding = PAINT_AUX_BUFFER_IDX,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = static_cast<VkShaderStageFlags>(
                    m_interlockMode == gpu::InterlockMode::atomics ? VK_SHADER_STAGE_FRAGMENT_BIT
                                                                   : VK_SHADER_STAGE_VERTEX_BIT),
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

        VkDescriptorSetLayoutCreateInfo perFlushLayoutInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = std::size(perFlushLayoutBindings),
            .pBindings = perFlushLayoutBindings,
        };

        VK_CHECK(m_vk->CreateDescriptorSetLayout(m_vk->device,
                                                 &perFlushLayoutInfo,
                                                 nullptr,
                                                 &m_descriptorSetLayouts[PER_FLUSH_BINDINGS_SET]));

        // The imageTexture gets updated with every draw that uses it.
        VkDescriptorSetLayoutBinding perDrawLayoutBindings[] = {
            {
                .binding = IMAGE_TEXTURE_IDX,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
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
                                                 &m_descriptorSetLayouts[PER_DRAW_BINDINGS_SET]));

        // Samplers get bound once per lifetime.
        VkDescriptorSetLayoutBinding samplerLayoutBindings[] = {
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

        VkDescriptorSetLayoutCreateInfo samplerLayoutInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = std::size(samplerLayoutBindings),
            .pBindings = samplerLayoutBindings,
        };

        VK_CHECK(m_vk->CreateDescriptorSetLayout(m_vk->device,
                                                 &samplerLayoutInfo,
                                                 nullptr,
                                                 &m_descriptorSetLayouts[SAMPLER_BINDINGS_SET]));

        // PLS planes get bound per flush as input attachments or storage textures.
        VkDescriptorSetLayoutBinding plsLayoutBindings[] = {
            {
                .binding = COLOR_PLANE_IDX,
                .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            {
                .binding = CLIP_PLANE_IDX,
                .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            {
                .binding = SCRATCH_COLOR_PLANE_IDX,
                .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            {
                .binding = COVERAGE_PLANE_IDX,
                .descriptorType = m_interlockMode == gpu::InterlockMode::atomics
                                      ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                                      : VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
        };
        static_assert(COLOR_PLANE_IDX == 0);
        static_assert(CLIP_PLANE_IDX == 1);
        static_assert(SCRATCH_COLOR_PLANE_IDX == 2);
        static_assert(COVERAGE_PLANE_IDX == 3);

        VkDescriptorSetLayoutCreateInfo plsLayoutInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        };
        if (m_options & DrawPipelineLayoutOptions::fixedFunctionColorBlend)
        {
            // Drop the COLOR input attachment when using fixedFunctionColorBlend.
            assert(plsLayoutBindings[0].binding == COLOR_PLANE_IDX);
            plsLayoutInfo.bindingCount = std::size(plsLayoutBindings) - 1;
            plsLayoutInfo.pBindings = plsLayoutBindings + 1;
        }
        else
        {
            plsLayoutInfo.bindingCount = std::size(plsLayoutBindings);
            plsLayoutInfo.pBindings = plsLayoutBindings;
        }

        VK_CHECK(
            m_vk->CreateDescriptorSetLayout(m_vk->device,
                                            &plsLayoutInfo,
                                            nullptr,
                                            &m_descriptorSetLayouts[PLS_TEXTURE_BINDINGS_SET]));

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = BINDINGS_SET_COUNT,
            .pSetLayouts = m_descriptorSetLayouts,
        };

        VK_CHECK(m_vk->CreatePipelineLayout(m_vk->device,
                                            &pipelineLayoutCreateInfo,
                                            nullptr,
                                            &m_pipelineLayout));

        // Create static descriptor sets.
        VkDescriptorPoolSize staticDescriptorPoolSizes[] = {
            {
                .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .descriptorCount = 1, // m_nullImageTexture
            },
            {
                .type = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptorCount = 2, // m_linearSampler, m_mipmapSampler
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

        VkDescriptorSetAllocateInfo nullImageDescriptorSetInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = m_staticDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &m_descriptorSetLayouts[PER_DRAW_BINDINGS_SET],
        };

        VK_CHECK(m_vk->AllocateDescriptorSets(m_vk->device,
                                              &nullImageDescriptorSetInfo,
                                              &m_nullImageDescriptorSet));

        m_vk->updateImageDescriptorSets(m_nullImageDescriptorSet,
                                        {
                                            .dstBinding = IMAGE_TEXTURE_IDX,
                                            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                        },
                                        {{
                                            .imageView = *impl->m_nullImageTexture->m_textureView,
                                            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                        }});

        VkDescriptorSetAllocateInfo samplerDescriptorSetInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = m_staticDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = m_descriptorSetLayouts + SAMPLER_BINDINGS_SET,
        };

        VK_CHECK(m_vk->AllocateDescriptorSets(m_vk->device,
                                              &samplerDescriptorSetInfo,
                                              &m_samplerDescriptorSet));

        m_vk->updateImageDescriptorSets(m_samplerDescriptorSet,
                                        {
                                            .dstBinding = GRAD_TEXTURE_IDX,
                                            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                                        },
                                        {
                                            {.sampler = impl->m_linearSampler},
                                            {.sampler = impl->m_mipmapSampler},
                                        });
        static_assert(IMAGE_TEXTURE_IDX == GRAD_TEXTURE_IDX + 1);
    }

    VkRenderPass renderPassAt(int renderPassVariantIdx)
    {
        if (m_renderPasses[renderPassVariantIdx] == VK_NULL_HANDLE)
        {
            // Create the render pass.
            VkAttachmentDescription attachmentDescriptions[] = {
                {
                    .format = FormatFromRenderPassVariant(renderPassVariantIdx),
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .loadOp = LoadOpFromRenderPassVariant(renderPassVariantIdx),
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
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
            };
            static_assert(COLOR_PLANE_IDX == 0);
            static_assert(CLIP_PLANE_IDX == 1);
            static_assert(SCRATCH_COLOR_PLANE_IDX == 2);
            static_assert(COVERAGE_PLANE_IDX == 3);

            VkAttachmentReference attachmentReferences[] = {
                {
                    .attachment = COLOR_PLANE_IDX,
                    .layout = VK_IMAGE_LAYOUT_GENERAL,
                },
                {
                    .attachment = CLIP_PLANE_IDX,
                    .layout = VK_IMAGE_LAYOUT_GENERAL,
                },
                {
                    .attachment = SCRATCH_COLOR_PLANE_IDX,
                    .layout = VK_IMAGE_LAYOUT_GENERAL,
                },
                {
                    .attachment = COVERAGE_PLANE_IDX,
                    .layout = VK_IMAGE_LAYOUT_GENERAL,
                },
            };
            static_assert(COLOR_PLANE_IDX == 0);
            static_assert(CLIP_PLANE_IDX == 1);
            static_assert(SCRATCH_COLOR_PLANE_IDX == 2);
            static_assert(COVERAGE_PLANE_IDX == 3);

            VkAttachmentReference inputAttachmentReferences[] = {
                // COLOR is not an input attachment if we're using fixed
                // function blending.
                (m_options & DrawPipelineLayoutOptions::fixedFunctionColorBlend)
                    ? VkAttachmentReference{.attachment = VK_ATTACHMENT_UNUSED}
                    : attachmentReferences[0],
                attachmentReferences[1],
                attachmentReferences[2],
                attachmentReferences[3],
            };
            static_assert(sizeof(inputAttachmentReferences) == sizeof(attachmentReferences));

            VkSubpassDescription subpassDescription = {
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .inputAttachmentCount = PLSAttachmentCount(m_interlockMode),
                .pInputAttachments = inputAttachmentReferences,
                .colorAttachmentCount = PLSAttachmentCount(m_interlockMode),
                .pColorAttachments = attachmentReferences,
            };

            if (m_interlockMode == gpu::InterlockMode::rasterOrdering)
            {
                // With EXT_rasterization_order_attachment_access, we just need
                // this flag and all "subpassLoad" dependencies are implicit.
                assert(m_vk->features.rasterizationOrderColorAttachmentAccess);
                subpassDescription.flags |=
                    VK_SUBPASS_DESCRIPTION_RASTERIZATION_ORDER_ATTACHMENT_COLOR_ACCESS_BIT_EXT;
            }

            VkRenderPassCreateInfo renderPassCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                .attachmentCount = PLSAttachmentCount(m_interlockMode),
                .pAttachments = attachmentDescriptions,
                .subpassCount = 1,
                .pSubpasses = &subpassDescription,
            };

            if (m_interlockMode == gpu::InterlockMode::atomics)
            {
                // Without EXT_rasterization_order_attachment_access (aka atomic mode),
                // "subpassLoad" calls require explicit dependencies and barriers.
                constexpr static VkSubpassDependency kSubpassLoadDependency = {
                    .srcSubpass = 0,
                    .dstSubpass = 0,
                    .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    .dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                    .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
                };

                renderPassCreateInfo.dependencyCount = 1;
                renderPassCreateInfo.pDependencies = &kSubpassLoadDependency;
            }

            VK_CHECK(m_vk->CreateRenderPass(m_vk->device,
                                            &renderPassCreateInfo,
                                            nullptr,
                                            &m_renderPasses[renderPassVariantIdx]));
        }
        return m_renderPasses[renderPassVariantIdx];
    }

    ~DrawPipelineLayout()
    {
        for (VkDescriptorSetLayout layout : m_descriptorSetLayouts)
        {
            m_vk->DestroyDescriptorSetLayout(m_vk->device, layout, nullptr);
        }
        m_vk->DestroyPipelineLayout(m_vk->device, m_pipelineLayout, nullptr);
        m_vk->DestroyDescriptorPool(m_vk->device, m_staticDescriptorPool, nullptr);
        for (VkRenderPass renderPass : m_renderPasses)
        {
            m_vk->DestroyRenderPass(m_vk->device, renderPass, nullptr);
        }
    }

    gpu::InterlockMode interlockMode() const { return m_interlockMode; }
    DrawPipelineLayoutOptions options() const { return m_options; }

    VkDescriptorSetLayout perFlushLayout() const
    {
        return m_descriptorSetLayouts[PER_FLUSH_BINDINGS_SET];
    }
    VkDescriptorSetLayout perDrawLayout() const
    {
        return m_descriptorSetLayouts[PER_DRAW_BINDINGS_SET];
    }
    VkDescriptorSetLayout samplerLayout() const
    {
        return m_descriptorSetLayouts[SAMPLER_BINDINGS_SET];
    }
    VkDescriptorSetLayout plsLayout() const
    {
        return m_descriptorSetLayouts[PLS_TEXTURE_BINDINGS_SET];
    }
    VkDescriptorSet nullImageDescriptorSet() const { return m_nullImageDescriptorSet; }
    VkDescriptorSet samplerDescriptorSet() const { return m_samplerDescriptorSet; }

    VkPipelineLayout operator*() const { return m_pipelineLayout; }
    VkPipelineLayout vkPipelineLayout() const { return m_pipelineLayout; }

private:
    const rcp<VulkanContext> m_vk;
    const gpu::InterlockMode m_interlockMode;
    const DrawPipelineLayoutOptions m_options;

    VkDescriptorSetLayout m_descriptorSetLayouts[BINDINGS_SET_COUNT];
    VkPipelineLayout m_pipelineLayout;
    VkDescriptorPool m_staticDescriptorPool; // For descriptorSets that never
                                             // change between frames.
    VkDescriptorSet m_nullImageDescriptorSet;
    VkDescriptorSet m_samplerDescriptorSet;
    std::array<VkRenderPass, kRenderPassVariantCount> m_renderPasses = {};
};

// Wraps vertex and fragment shader modules for a specific combination of DrawType,
// InterlockMode, and ShaderFeatures.
class PLSRenderContextVulkanImpl::DrawShader
{
public:
    DrawShader(VulkanContext* vk,
               gpu::DrawType drawType,
               gpu::InterlockMode interlockMode,
               gpu::ShaderFeatures shaderFeatures,
               gpu::ShaderMiscFlags shaderMiscFlags) :
        m_vk(ref_rcp(vk))
    {
        VkShaderModuleCreateInfo vsInfo = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        VkShaderModuleCreateInfo fsInfo = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};

        if (interlockMode == gpu::InterlockMode::rasterOrdering)
        {
            switch (drawType)
            {
                case DrawType::midpointFanPatches:
                case DrawType::outerCurvePatches:
                    vkutil::set_shader_code(vsInfo, spirv::draw_path_vert);
                    vkutil::set_shader_code(fsInfo, spirv::draw_path_frag);
                    break;

                case DrawType::interiorTriangulation:
                    vkutil::set_shader_code(vsInfo, spirv::draw_interior_triangles_vert);
                    vkutil::set_shader_code(fsInfo, spirv::draw_interior_triangles_frag);
                    break;

                case DrawType::imageMesh:
                    vkutil::set_shader_code(vsInfo, spirv::draw_image_mesh_vert);
                    vkutil::set_shader_code(fsInfo, spirv::draw_image_mesh_frag);
                    break;

                case DrawType::imageRect:
                case DrawType::gpuAtomicResolve:
                case DrawType::gpuAtomicInitialize:
                case DrawType::stencilClipReset:
                    RIVE_UNREACHABLE();
            }
        }
        else
        {
            assert(interlockMode == gpu::InterlockMode::atomics);
            bool fixedFunctionColorBlend =
                shaderMiscFlags & gpu::ShaderMiscFlags::fixedFunctionColorBlend;
            switch (drawType)
            {
                case DrawType::midpointFanPatches:
                case DrawType::outerCurvePatches:
                    vkutil::set_shader_code(vsInfo, spirv::atomic_draw_path_vert);
                    vkutil::set_shader_code_if_then_else(fsInfo,
                                                         fixedFunctionColorBlend,
                                                         spirv::atomic_draw_path_fixedblend_frag,
                                                         spirv::atomic_draw_path_frag);
                    break;

                case DrawType::interiorTriangulation:
                    vkutil::set_shader_code(vsInfo, spirv::atomic_draw_interior_triangles_vert);
                    vkutil::set_shader_code_if_then_else(
                        fsInfo,
                        fixedFunctionColorBlend,
                        spirv::atomic_draw_interior_triangles_fixedblend_frag,
                        spirv::atomic_draw_interior_triangles_frag);
                    break;

                case DrawType::imageRect:
                    vkutil::set_shader_code(vsInfo, spirv::atomic_draw_image_rect_vert);
                    vkutil::set_shader_code_if_then_else(
                        fsInfo,
                        fixedFunctionColorBlend,
                        spirv::atomic_draw_image_rect_fixedblend_frag,
                        spirv::atomic_draw_image_rect_frag);
                    break;

                case DrawType::imageMesh:
                    vkutil::set_shader_code(vsInfo, spirv::atomic_draw_image_mesh_vert);
                    vkutil::set_shader_code_if_then_else(
                        fsInfo,
                        fixedFunctionColorBlend,
                        spirv::atomic_draw_image_mesh_fixedblend_frag,
                        spirv::atomic_draw_image_mesh_frag);
                    break;

                case DrawType::gpuAtomicResolve:
                    vkutil::set_shader_code(vsInfo, spirv::atomic_resolve_pls_vert);
                    vkutil::set_shader_code_if_then_else(fsInfo,
                                                         fixedFunctionColorBlend,
                                                         spirv::atomic_resolve_pls_fixedblend_frag,
                                                         spirv::atomic_resolve_pls_frag);
                    break;

                case DrawType::gpuAtomicInitialize:
                case DrawType::stencilClipReset:
                    RIVE_UNREACHABLE();
            }
        }

        VK_CHECK(m_vk->CreateShaderModule(m_vk->device, &vsInfo, nullptr, &m_vertexModule));
        VK_CHECK(m_vk->CreateShaderModule(m_vk->device, &fsInfo, nullptr, &m_fragmentModule));
    }

    ~DrawShader()
    {
        m_vk->DestroyShaderModule(m_vk->device, m_vertexModule, nullptr);
        m_vk->DestroyShaderModule(m_vk->device, m_fragmentModule, nullptr);
    }

    VkShaderModule vertexModule() const { return m_vertexModule; }
    VkShaderModule fragmentModule() const { return m_fragmentModule; }

private:
    const rcp<VulkanContext> m_vk;
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
    DrawPipeline(PLSRenderContextVulkanImpl* impl,
                 gpu::DrawType drawType,
                 const DrawPipelineLayout& pipelineLayout,
                 gpu::ShaderFeatures shaderFeatures,
                 DrawPipelineOptions drawPipelineOptions,
                 VkRenderPass vkRenderPass) :
        m_vk(ref_rcp(impl->vulkanContext()))
    {
        gpu::InterlockMode interlockMode = pipelineLayout.interlockMode();
        auto shaderMiscFlags = gpu::ShaderMiscFlags::none;
        if (pipelineLayout.options() & DrawPipelineLayoutOptions::fixedFunctionColorBlend)
        {
            shaderMiscFlags |= gpu::ShaderMiscFlags::fixedFunctionColorBlend;
        }
        uint32_t shaderKey =
            gpu::ShaderUniqueKey(drawType, shaderFeatures, interlockMode, shaderMiscFlags);
        const DrawShader& drawShader = impl->m_drawShaders
                                           .try_emplace(shaderKey,
                                                        m_vk.get(),
                                                        drawType,
                                                        interlockMode,
                                                        shaderFeatures,
                                                        shaderMiscFlags)
                                           .first->second;

        VkBool32 shaderPermutationFlags[SPECIALIZATION_COUNT] = {
            shaderFeatures & gpu::ShaderFeatures::ENABLE_CLIPPING,
            shaderFeatures & gpu::ShaderFeatures::ENABLE_CLIP_RECT,
            shaderFeatures & gpu::ShaderFeatures::ENABLE_ADVANCED_BLEND,
            shaderFeatures & gpu::ShaderFeatures::ENABLE_EVEN_ODD,
            shaderFeatures & gpu::ShaderFeatures::ENABLE_NESTED_CLIPPING,
            shaderFeatures & gpu::ShaderFeatures::ENABLE_HSL_BLEND_MODES,
        };
        static_assert(CLIPPING_SPECIALIZATION_IDX == 0);
        static_assert(CLIP_RECT_SPECIALIZATION_IDX == 1);
        static_assert(ADVANCED_BLEND_SPECIALIZATION_IDX == 2);
        static_assert(EVEN_ODD_SPECIALIZATION_IDX == 3);
        static_assert(NESTED_CLIPPING_SPECIALIZATION_IDX == 4);
        static_assert(HSL_BLEND_MODES_SPECIALIZATION_IDX == 5);
        static_assert(SPECIALIZATION_COUNT == 6);

        VkSpecializationMapEntry permutationMapEntries[SPECIALIZATION_COUNT];
        for (uint32_t i = 0; i < SPECIALIZATION_COUNT; ++i)
        {
            permutationMapEntries[i] = {
                .constantID = i,
                .offset = i * static_cast<uint32_t>(sizeof(VkBool32)),
                .size = sizeof(VkBool32),
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
                .module = drawShader.vertexModule(),
                .pName = "main",
                .pSpecializationInfo = &specializationInfo,
            },
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = drawShader.fragmentModule(),
                .pName = "main",
                .pSpecializationInfo = &specializationInfo,
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

        VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        };

        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::outerCurvePatches:
            {
                vertexInputBindingDescriptions = {{{
                    .binding = 0,
                    .stride = sizeof(gpu::PatchVertex),
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
                    .stride = sizeof(gpu::TriangleVertex),
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
            {
                vertexInputBindingDescriptions = {{{
                    .binding = 0,
                    .stride = sizeof(gpu::ImageRectVertex),
                    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
                }}};
                vertexAttributeDescriptions = {{
                    {
                        .location = 0,
                        .binding = 0,
                        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                        .offset = 0,
                    },
                }};
                pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
                pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = 1;
                break;
            }

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

            case DrawType::gpuAtomicResolve:
            {
                pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
                pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
                pipelineInputAssemblyStateCreateInfo.topology =
                    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
                break;
            }

            case DrawType::gpuAtomicInitialize:
            case DrawType::stencilClipReset:
                RIVE_UNREACHABLE();
        }

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
                DrawTypeIsImageDraw(drawType) ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT),
            .frontFace = VK_FRONT_FACE_CLOCKWISE,
            .lineWidth = 1.0,
        };

        VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        };

        VkPipelineColorBlendAttachmentState blendColorAttachments[] = {
            (pipelineLayout.options() & DrawPipelineLayoutOptions::fixedFunctionColorBlend)
                ? VkPipelineColorBlendAttachmentState{
                    .blendEnable = VK_TRUE,
                    .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .colorBlendOp = VK_BLEND_OP_ADD,
                    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .alphaBlendOp = VK_BLEND_OP_ADD,
                    .colorWriteMask = vkutil::kColorWriteMaskRGBA,
                }
                : VkPipelineColorBlendAttachmentState{
                    .colorWriteMask = vkutil::kColorWriteMaskRGBA,
                },
            {.colorWriteMask = vkutil::kColorWriteMaskRGBA},
            {.colorWriteMask = vkutil::kColorWriteMaskRGBA},
            {.colorWriteMask = vkutil::kColorWriteMaskRGBA},
        };
        static_assert(COLOR_PLANE_IDX == 0);
        static_assert(CLIP_PLANE_IDX == 1);
        static_assert(SCRATCH_COLOR_PLANE_IDX == 2);
        static_assert(COVERAGE_PLANE_IDX == 3);

        VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = DrawPipelineLayout::PLSAttachmentCount(interlockMode),
            .pAttachments = blendColorAttachments,
        };

        if (interlockMode == gpu::InterlockMode::rasterOrdering)
        {
            assert(m_vk->features.rasterizationOrderColorAttachmentAccess);
            pipelineColorBlendStateCreateInfo.flags |=
                VK_PIPELINE_COLOR_BLEND_STATE_CREATE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_BIT_EXT;
        }

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
            .layout = *pipelineLayout,
            .renderPass = vkRenderPass,
        };

        VK_CHECK(m_vk->CreateGraphicsPipelines(m_vk->device,
                                               VK_NULL_HANDLE,
                                               1,
                                               &graphicsPipelineCreateInfo,
                                               nullptr,
                                               &m_vkPipeline));
    }

    ~DrawPipeline() { m_vk->DestroyPipeline(m_vk->device, m_vkPipeline, nullptr); }

    const VkPipeline vkPipeline() const { return m_vkPipeline; }

private:
    const rcp<VulkanContext> m_vk;
    VkPipeline m_vkPipeline;
};

PLSRenderContextVulkanImpl::PLSRenderContextVulkanImpl(
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    const VulkanFeatures& features,
    PFN_vkGetInstanceProcAddr fp_vkGetInstanceProcAddr,
    PFN_vkGetDeviceProcAddr fp_vkGetDeviceProcAddr) :
    m_vk(make_rcp<VulkanContext>(instance,
                                 physicalDevice,
                                 device,
                                 features,
                                 fp_vkGetInstanceProcAddr,
                                 fp_vkGetDeviceProcAddr)),
    m_flushUniformBufferRing(m_vk,
                             VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                             vkutil::Mappability::writeOnly),
    m_imageDrawUniformBufferRing(m_vk,
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 vkutil::Mappability::writeOnly),
    m_pathBufferRing(m_vk, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, vkutil::Mappability::writeOnly),
    m_paintBufferRing(m_vk, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, vkutil::Mappability::writeOnly),
    m_paintAuxBufferRing(m_vk, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, vkutil::Mappability::writeOnly),
    m_contourBufferRing(m_vk, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, vkutil::Mappability::writeOnly),
    m_simpleColorRampsBufferRing(m_vk,
                                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                 vkutil::Mappability::writeOnly),
    m_gradSpanBufferRing(m_vk, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vkutil::Mappability::writeOnly),
    m_tessSpanBufferRing(m_vk, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vkutil::Mappability::writeOnly),
    m_triangleBufferRing(m_vk, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vkutil::Mappability::writeOnly),
    m_colorRampPipeline(std::make_unique<ColorRampPipeline>(m_vk)),
    m_tessellatePipeline(std::make_unique<TessellatePipeline>(m_vk))
{
    m_platformFeatures.supportsPixelLocalStorage = features.fragmentStoresAndAtomics;
    m_platformFeatures.supportsRasterOrdering = features.rasterizationOrderColorAttachmentAccess;
    m_platformFeatures.invertOffscreenY = false;
    m_platformFeatures.uninvertOnScreenY = true;
}

void PLSRenderContextVulkanImpl::initGPUObjects()
{
    constexpr static uint8_t black[] = {0, 0, 0, 1};
    m_nullImageTexture = make_rcp<PLSTextureVulkanImpl>(m_vk, 1, 1, 1, black);

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

    VK_CHECK(
        m_vk->CreateSampler(m_vk->device, &linearSamplerCreateInfo, nullptr, &m_linearSampler));

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

    VK_CHECK(
        m_vk->CreateSampler(m_vk->device, &mipmapSamplerCreateInfo, nullptr, &m_mipmapSampler));

    m_tessSpanIndexBuffer = m_vk->makeBuffer(
        {
            .size = sizeof(gpu::kTessSpanIndices),
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        },
        vkutil::Mappability::writeOnly);
    memcpy(vkutil::ScopedBufferFlush(*m_tessSpanIndexBuffer),
           gpu::kTessSpanIndices,
           sizeof(gpu::kTessSpanIndices));

    m_pathPatchVertexBuffer = m_vk->makeBuffer(
        {
            .size = kPatchVertexBufferCount * sizeof(gpu::PatchVertex),
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        },
        vkutil::Mappability::writeOnly);
    m_pathPatchIndexBuffer = m_vk->makeBuffer(
        {
            .size = kPatchIndexBufferCount * sizeof(uint16_t),
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        },
        vkutil::Mappability::writeOnly);
    gpu::GeneratePatchBufferData(
        vkutil::ScopedBufferFlush(*m_pathPatchVertexBuffer).as<PatchVertex*>(),
        vkutil::ScopedBufferFlush(*m_pathPatchIndexBuffer).as<uint16_t*>());

    m_imageRectVertexBuffer = m_vk->makeBuffer(
        {
            .size = sizeof(gpu::kImageRectVertices),
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        },
        vkutil::Mappability::writeOnly);
    memcpy(vkutil::ScopedBufferFlush(*m_imageRectVertexBuffer),
           gpu::kImageRectVertices,
           sizeof(gpu::kImageRectVertices));
    m_imageRectIndexBuffer = m_vk->makeBuffer(
        {
            .size = sizeof(gpu::kImageRectIndices),
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        },
        vkutil::Mappability::writeOnly);
    memcpy(vkutil::ScopedBufferFlush(*m_imageRectIndexBuffer),
           gpu::kImageRectIndices,
           sizeof(gpu::kImageRectIndices));
}

PLSRenderContextVulkanImpl::~PLSRenderContextVulkanImpl()
{
    // Wait for all fences before cleaning up.
    for (const rcp<gpu::CommandBufferCompletionFence>& fence : m_frameCompletionFences)
    {
        if (fence != nullptr)
        {
            fence->wait();
        }
    }

    // Tell the context we are entering our shutdown cycle. After this point, all
    // resources will be deleted immediately upon their refCount reaching zero, as
    // opposed to being kept alive for in-flight command buffers.
    m_vk->shutdown();

    m_vk->DestroySampler(m_vk->device, m_linearSampler, nullptr);
    m_vk->DestroySampler(m_vk->device, m_mipmapSampler, nullptr);
}

void PLSRenderContextVulkanImpl::resizeGradientTexture(uint32_t width, uint32_t height)
{
    width = std::max(width, 1u);
    height = std::max(height, 1u);
    if (m_gradientTexture == nullptr || m_gradientTexture->info().extent.width != width ||
        m_gradientTexture->info().extent.height != height)
    {
        m_gradientTexture = m_vk->makeTexture({
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .extent = {width, height, 1},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        });

        m_gradTextureView = m_vk->makeTextureView(m_gradientTexture);

        m_gradTextureFramebuffer = m_vk->makeFramebuffer({
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
        m_tessVertexTexture = m_vk->makeTexture({
            .format = VK_FORMAT_R32G32B32A32_UINT,
            .extent = {width, height, 1},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        });

        m_tessVertexTextureView = m_vk->makeTextureView(m_tessVertexTexture);

        m_tessTextureFramebuffer = m_vk->makeFramebuffer({
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
    m_bufferRingIdx = (m_bufferRingIdx + 1) % gpu::kBufferRingSize;

    // Wait for the existing resources to finish before we release/recycle them.
    if (rcp<gpu::CommandBufferCompletionFence> fence =
            std::move(m_frameCompletionFences[m_bufferRingIdx]))
    {
        fence->wait();
    }

    // Delete resources that are no longer referenced by in-flight command buffers.
    m_vk->onNewFrameBegun();

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

PLSRenderContextVulkanImpl::DescriptorSetPool::DescriptorSetPool(
    PLSRenderContextVulkanImpl* plsImplVulkan) :
    RenderingResource(plsImplVulkan->m_vk), m_plsImplVulkan(plsImplVulkan)
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
        {
            .type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
            .descriptorCount = 4,
        },
        {
            // For the coverageAtomicTexture in atomic mode.
            .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
        },
    };

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = descriptor_pool_limits::kMaxDescriptorSets,
        .poolSizeCount = std::size(descriptorPoolSizes),
        .pPoolSizes = descriptorPoolSizes,
    };

    VK_CHECK(m_vk->CreateDescriptorPool(m_vk->device,
                                        &descriptorPoolCreateInfo,
                                        nullptr,
                                        &m_vkDescriptorPool));
}

PLSRenderContextVulkanImpl::DescriptorSetPool::~DescriptorSetPool()
{
    freeDescriptorSets();
    m_vk->DestroyDescriptorPool(m_vk->device, m_vkDescriptorPool, nullptr);
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

    VK_CHECK(m_vk->AllocateDescriptorSets(m_vk->device,
                                          &descriptorSetAllocateInfo,
                                          &m_descriptorSets.emplace_back()));

    return m_descriptorSets.back();
}

void PLSRenderContextVulkanImpl::DescriptorSetPool::freeDescriptorSets()
{
    m_vk->ResetDescriptorPool(m_vk->device, m_vkDescriptorPool, 0);
}

void PLSRenderContextVulkanImpl::DescriptorSetPool::onRefCntReachedZero() const
{
    constexpr static uint32_t kMaxDescriptorSetPoolsInPool = 64;

    if (m_plsImplVulkan->m_descriptorSetPoolPool.size() < kMaxDescriptorSetPoolsInPool)
    {
        // Hang out in the plsContext's m_descriptorSetPoolPool until in-flight
        // command buffers have finished using our descriptors.
        m_plsImplVulkan->m_descriptorSetPoolPool.emplace_back(const_cast<DescriptorSetPool*>(this),
                                                              m_vk->currentFrameIdx());
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
        m_descriptorSetPoolPool.front().expirationFrameIdx <= m_vk->currentFrameIdx())
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

VkImageView PLSRenderTargetVulkan::ensureOffscreenColorTextureView(VkCommandBuffer commandBuffer)
{
    if (m_offscreenColorTextureView == nullptr)
    {
        m_offscreenColorTexture = m_vk->makeTexture({
            .format = VK_FORMAT_B8G8R8A8_UNORM,
            .extent = {width(), height(), 1},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        });

        m_vk->insertImageMemoryBarrier(commandBuffer,
                                       *m_offscreenColorTexture,
                                       VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_GENERAL);
        m_offscreenColorTextureView = m_vk->makeTextureView(m_offscreenColorTexture);
    }

    return *m_offscreenColorTextureView;
}

VkImageView PLSRenderTargetVulkan::ensureCoverageTextureView(VkCommandBuffer commandBuffer)
{
    if (m_coverageTextureView == nullptr)
    {
        m_coverageTexture = m_vk->makeTexture({
            .format = VK_FORMAT_R32_UINT,
            .extent = {width(), height(), 1},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        });

        m_vk->insertImageMemoryBarrier(commandBuffer,
                                       *m_coverageTexture,
                                       VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_GENERAL);
        m_coverageTextureView = m_vk->makeTextureView(m_coverageTexture);
    }

    return *m_coverageTextureView;
}

VkImageView PLSRenderTargetVulkan::ensureClipTextureView(VkCommandBuffer commandBuffer)
{
    if (m_clipTextureView == nullptr)
    {
        m_clipTexture = m_vk->makeTexture({
            .format = VK_FORMAT_R32_UINT,
            .extent = {width(), height(), 1},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        });

        m_vk->insertImageMemoryBarrier(commandBuffer,
                                       *m_clipTexture,
                                       VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_GENERAL);
        m_clipTextureView = m_vk->makeTextureView(m_clipTexture);
    }

    return *m_clipTextureView;
}

VkImageView PLSRenderTargetVulkan::ensureScratchColorTextureView(VkCommandBuffer commandBuffer)
{
    if (m_scratchColorTextureView == nullptr)
    {
        m_scratchColorTexture = m_vk->makeTexture({
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .extent = {width(), height(), 1},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        });

        m_vk->insertImageMemoryBarrier(commandBuffer,
                                       *m_scratchColorTexture,
                                       VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_GENERAL);

        m_scratchColorTextureView = m_vk->makeTextureView(m_scratchColorTexture);
    }

    return *m_scratchColorTextureView;
}

VkImageView PLSRenderTargetVulkan::ensureCoverageAtomicTextureView(VkCommandBuffer commandBuffer)
{
    if (m_coverageAtomicTextureView == nullptr)
    {
        m_coverageAtomicTexture = m_vk->makeTexture({
            .format = VK_FORMAT_R32_UINT,
            .extent = {width(), height(), 1},
            .usage = VK_IMAGE_USAGE_STORAGE_BIT |
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT, // For vkCmdClearColorImage
        });

        m_vk->insertImageMemoryBarrier(commandBuffer,
                                       *m_coverageAtomicTexture,
                                       VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_GENERAL);

        m_coverageAtomicTextureView = m_vk->makeTextureView(m_coverageAtomicTexture);
    }

    return *m_coverageAtomicTextureView;
}

void PLSRenderContextVulkanImpl::flush(const FlushDescriptor& desc)
{
    if (desc.interlockMode == gpu::InterlockMode::depthStencil)
    {
        return; // TODO: support MSAA.
    }

    auto commandBuffer = reinterpret_cast<VkCommandBuffer>(desc.externalCommandBuffer);
    rcp<DescriptorSetPool> descriptorSetPool = makeDescriptorSetPool();

    constexpr static VkDeviceSize zeroOffset[1] = {0};
    constexpr static uint32_t zeroOffset32[1] = {0};

    m_vk->insertImageMemoryBarrier(commandBuffer,
                                   *m_gradientTexture,
                                   VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // Render the complex color ramps to the gradient texture.
    if (desc.complexGradSpanCount > 0)
    {
        VkRect2D renderArea = {
            .offset = {0, static_cast<int32_t>(desc.complexGradRowsTop)},
            .extent = {gpu::kGradTextureWidth, desc.complexGradRowsHeight},
        };

        VkRenderPassBeginInfo renderPassBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = m_colorRampPipeline->renderPass(),
            .framebuffer = *m_gradTextureFramebuffer,
            .renderArea = renderArea,
        };

        m_vk->CmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        m_vk->CmdBindPipeline(commandBuffer,
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              m_colorRampPipeline->renderPipeline());

        m_vk->CmdSetViewport(commandBuffer, 0, 1, vkutil::ViewportFromRect2D(renderArea));

        m_vk->CmdSetScissor(commandBuffer, 0, 1, &renderArea);

        VkBuffer gradSpanBuffer = m_gradSpanBufferRing.vkBufferAt(m_bufferRingIdx);
        VkDeviceSize gradSpanOffset = desc.firstComplexGradSpan * sizeof(gpu::GradientSpan);
        m_vk->CmdBindVertexBuffers(commandBuffer, 0, 1, &gradSpanBuffer, &gradSpanOffset);

        VkDescriptorSet descriptorSet =
            descriptorSetPool->allocateDescriptorSet(m_colorRampPipeline->descriptorSetLayout());

        m_vk->updateBufferDescriptorSets(
            descriptorSet,
            {
                .dstBinding = FLUSH_UNIFORM_BUFFER_IDX,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            },
            {{
                .buffer = m_flushUniformBufferRing.vkBufferAt(m_bufferRingIdx),
                .offset = desc.flushUniformDataOffsetInBytes,
                .range = sizeof(gpu::FlushUniforms),
            }});

        m_vk->CmdBindDescriptorSets(commandBuffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_colorRampPipeline->pipelineLayout(),
                                    PER_FLUSH_BINDINGS_SET,
                                    1,
                                    &descriptorSet,
                                    0,
                                    nullptr);

        m_vk->CmdDraw(commandBuffer, 4, desc.complexGradSpanCount, 0, 0);

        m_vk->CmdEndRenderPass(commandBuffer);
    }

    m_vk->insertImageMemoryBarrier(commandBuffer,
                                   *m_gradientTexture,
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy the simple color ramps to the gradient texture.
    if (desc.simpleGradTexelsHeight > 0)
    {
        VkBufferImageCopy bufferImageCopy{
            .bufferOffset = desc.simpleGradDataOffsetInBytes,
            .bufferRowLength = gpu::kGradTextureWidth,
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

        m_vk->CmdCopyBufferToImage(commandBuffer,
                                   m_simpleColorRampsBufferRing.vkBufferAt(m_bufferRingIdx),
                                   *m_gradientTexture,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   1,
                                   &bufferImageCopy);
    }

    m_vk->insertImageMemoryBarrier(commandBuffer,
                                   *m_gradientTexture,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    m_vk->insertImageMemoryBarrier(commandBuffer,
                                   *m_tessVertexTexture,
                                   VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // Tessellate all curves into vertices in the tessellation texture.
    if (desc.tessVertexSpanCount > 0)
    {
        VkRect2D renderArea = {
            .extent = {gpu::kTessTextureWidth, desc.tessDataHeight},
        };

        VkRenderPassBeginInfo renderPassBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = m_tessellatePipeline->renderPass(),
            .framebuffer = *m_tessTextureFramebuffer,
            .renderArea = renderArea,
        };

        m_vk->CmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        m_vk->CmdBindPipeline(commandBuffer,
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              m_tessellatePipeline->renderPipeline());

        m_vk->CmdSetViewport(commandBuffer, 0, 1, vkutil::ViewportFromRect2D(renderArea));

        m_vk->CmdSetScissor(commandBuffer, 0, 1, &renderArea);

        VkBuffer tessBuffer = m_tessSpanBufferRing.vkBufferAt(m_bufferRingIdx);
        VkDeviceSize tessOffset = desc.firstTessVertexSpan * sizeof(gpu::TessVertexSpan);
        m_vk->CmdBindVertexBuffers(commandBuffer, 0, 1, &tessBuffer, &tessOffset);

        m_vk->CmdBindIndexBuffer(commandBuffer, *m_tessSpanIndexBuffer, 0, VK_INDEX_TYPE_UINT16);

        VkDescriptorSet descriptorSet =
            descriptorSetPool->allocateDescriptorSet(m_tessellatePipeline->descriptorSetLayout());

        m_vk->updateBufferDescriptorSets(descriptorSet,
                                         {
                                             .dstBinding = PATH_BUFFER_IDX,
                                             .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                         },
                                         {{
                                             .buffer = m_pathBufferRing.vkBufferAt(m_bufferRingIdx),
                                             .offset = desc.firstPath * sizeof(gpu::PathData),
                                             .range = VK_WHOLE_SIZE,
                                         }});

        m_vk->updateBufferDescriptorSets(
            descriptorSet,
            {
                .dstBinding = CONTOUR_BUFFER_IDX,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            },
            {{
                .buffer = m_contourBufferRing.vkBufferAt(m_bufferRingIdx),
                .offset = desc.firstContour * sizeof(gpu::ContourData),
                .range = VK_WHOLE_SIZE,
            }});

        m_vk->updateBufferDescriptorSets(
            descriptorSet,
            {
                .dstBinding = FLUSH_UNIFORM_BUFFER_IDX,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            },
            {{
                .buffer = m_flushUniformBufferRing.vkBufferAt(m_bufferRingIdx),
                .offset = desc.flushUniformDataOffsetInBytes,
                .range = VK_WHOLE_SIZE,
            }});

        m_vk->CmdBindDescriptorSets(commandBuffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_tessellatePipeline->pipelineLayout(),
                                    PER_FLUSH_BINDINGS_SET,
                                    1,
                                    &descriptorSet,
                                    0,
                                    nullptr);

        m_vk->CmdDrawIndexed(commandBuffer,
                             std::size(gpu::kTessSpanIndices),
                             desc.tessVertexSpanCount,
                             0,
                             0,
                             0);

        m_vk->CmdEndRenderPass(commandBuffer);
    }

    m_vk->insertImageMemoryBarrier(commandBuffer,
                                   *m_tessVertexTexture,
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Apply pending texture updates.
    if (m_nullImageTexture->hasUpdates())
    {
        m_nullImageTexture->synchronize(commandBuffer);
    }
    for (const DrawBatch& batch : *desc.drawList)
    {
        if (auto imageTextureVulkan = static_cast<const PLSTextureVulkanImpl*>(batch.imageTexture))
        {
            if (imageTextureVulkan->hasUpdates())
            {
                imageTextureVulkan->synchronize(commandBuffer);
            }
        }
    }

    auto pipelineLayoutOptions = DrawPipelineLayoutOptions::none;
    if (m_vk->features.independentBlend && desc.interlockMode == gpu::InterlockMode::atomics &&
        !(desc.combinedShaderFeatures & gpu::ShaderFeatures::ENABLE_ADVANCED_BLEND))
    {
        pipelineLayoutOptions |= DrawPipelineLayoutOptions::fixedFunctionColorBlend;
    }

    int pipelineLayoutIdx =
        ((desc.interlockMode == gpu::InterlockMode::atomics) << kDrawPipelineLayoutOptionCount) |
        static_cast<int>(pipelineLayoutOptions);
    assert(pipelineLayoutIdx < m_drawPipelineLayouts.size());
    if (m_drawPipelineLayouts[pipelineLayoutIdx] == nullptr)
    {
        m_drawPipelineLayouts[pipelineLayoutIdx] =
            std::make_unique<DrawPipelineLayout>(this, desc.interlockMode, pipelineLayoutOptions);
    }
    DrawPipelineLayout& pipelineLayout = *m_drawPipelineLayouts[pipelineLayoutIdx];

    auto* renderTarget = static_cast<PLSRenderTargetVulkan*>(desc.renderTarget);

    auto targetView =
        renderTarget->targetViewContainsUsageFlag(VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT) ||
                (pipelineLayout.options() & DrawPipelineLayoutOptions::fixedFunctionColorBlend)
            ? renderTarget->targetTextureView()
            : renderTarget->ensureOffscreenColorTextureView(commandBuffer);
    auto clipView = renderTarget->ensureClipTextureView(commandBuffer);
    auto scratchColorTextureView = desc.interlockMode == gpu::InterlockMode::atomics
                                       ? VK_NULL_HANDLE
                                       : renderTarget->ensureScratchColorTextureView(commandBuffer);
    auto coverageTextureView = desc.interlockMode == gpu::InterlockMode::atomics
                                   ? renderTarget->ensureCoverageAtomicTextureView(commandBuffer)
                                   : renderTarget->ensureCoverageTextureView(commandBuffer);

    if (desc.colorLoadAction == gpu::LoadAction::preserveRenderTarget &&
        targetView == renderTarget->offscreenColorTextureView())
    {
        // Copy the target into our offscreen color texture before rendering.

        auto targetTexture = renderTarget->targetTexture();
        // we know the offscreenColorTexture exists because of the if condition
        auto offScreenTexture = renderTarget->offscreenColorTexture();

        m_vk->insertImageMemoryBarrier(commandBuffer,
                                       targetTexture,
                                       VK_IMAGE_LAYOUT_GENERAL,
                                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        m_vk->insertImageMemoryBarrier(commandBuffer,
                                       offScreenTexture,
                                       VK_IMAGE_LAYOUT_GENERAL,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        m_vk->blitSubRect(commandBuffer,
                          targetTexture,
                          offScreenTexture,
                          desc.renderTargetUpdateBounds);

        m_vk->insertImageMemoryBarrier(commandBuffer,
                                       targetTexture,
                                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                       VK_IMAGE_LAYOUT_GENERAL);

        m_vk->insertImageMemoryBarrier(commandBuffer,
                                       offScreenTexture,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       VK_IMAGE_LAYOUT_GENERAL);
    }

    int renderPassVariantIdx =
        DrawPipelineLayout::RenderPassVariantIdx(renderTarget->framebufferFormat(),
                                                 desc.colorLoadAction);
    VkRenderPass vkRenderPass = pipelineLayout.renderPassAt(renderPassVariantIdx);

    VkImageView imageViews[] = {
        targetView,
        clipView,
        scratchColorTextureView,
        desc.interlockMode == gpu::InterlockMode::atomics ? VK_NULL_HANDLE : coverageTextureView,
    };

    static_assert(COLOR_PLANE_IDX == 0);
    static_assert(CLIP_PLANE_IDX == 1);
    static_assert(SCRATCH_COLOR_PLANE_IDX == 2);
    static_assert(COVERAGE_PLANE_IDX == 3);

    rcp<vkutil::Framebuffer> framebuffer = m_vk->makeFramebuffer({
        .renderPass = vkRenderPass,
        .attachmentCount = DrawPipelineLayout::PLSAttachmentCount(desc.interlockMode),
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
        {.color = vkutil::color_clear_rgba32f(desc.clearColor)},
        {},
        {},
        {.color = vkutil::color_clear_r32ui(desc.coverageClearValue)},
    };
    static_assert(COLOR_PLANE_IDX == 0);
    static_assert(CLIP_PLANE_IDX == 1);
    static_assert(SCRATCH_COLOR_PLANE_IDX == 2);
    static_assert(COVERAGE_PLANE_IDX == 3);

    bool needsBarrierBeforeNextDraw = false;
    if (desc.interlockMode == gpu::InterlockMode::atomics)
    {
        // If the color attachment will be cleared, make sure we get a barrier on
        // it before shaders access it via subpassLoad().
        needsBarrierBeforeNextDraw =
#if 0
            // TODO: If we end up using HW blend when not using advanced blend, we
            // don't need a barrier after the clear.
            desc.combinedShaderFeatures &
                gpu::ShaderFeatures::ENABLE_ADVANCED_BLEND &&
#endif
            desc.colorLoadAction == gpu::LoadAction::clear;

        // Clear the coverage texture, which is not an attachment.
        m_vk->insertImageMemoryBarrier(commandBuffer,
                                       renderTarget->coverageAtomicTexture(),
                                       VK_IMAGE_LAYOUT_GENERAL,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkImageSubresourceRange clearRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
        };

        m_vk->CmdClearColorImage(commandBuffer,
                                 renderTarget->coverageAtomicTexture(),
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 &clearValues[COVERAGE_PLANE_IDX].color,
                                 1,
                                 &clearRange);

        m_vk->insertImageMemoryBarrier(commandBuffer,
                                       renderTarget->coverageAtomicTexture(),
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       VK_IMAGE_LAYOUT_GENERAL);
    }

    VkRenderPassBeginInfo renderPassBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = vkRenderPass,
        .framebuffer = *framebuffer,
        .renderArea = renderArea,
        .clearValueCount = std::size(clearValues),
        .pClearValues = clearValues,
    };

    m_vk->CmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    m_vk->CmdSetViewport(commandBuffer, 0, 1, vkutil::ViewportFromRect2D(renderArea));

    m_vk->CmdSetScissor(commandBuffer, 0, 1, &renderArea);

    // Update the per-flush descriptor sets.
    VkDescriptorSet perFlushDescriptorSet =
        descriptorSetPool->allocateDescriptorSet(pipelineLayout.perFlushLayout());

    m_vk->updateImageDescriptorSets(perFlushDescriptorSet,
                                    {
                                        .dstBinding = TESS_VERTEX_TEXTURE_IDX,
                                        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                    },
                                    {{
                                        .imageView = *m_tessVertexTextureView,
                                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    }});

    m_vk->updateImageDescriptorSets(perFlushDescriptorSet,
                                    {
                                        .dstBinding = GRAD_TEXTURE_IDX,
                                        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                    },
                                    {{
                                        .imageView = *m_gradTextureView,
                                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    }});

    m_vk->updateBufferDescriptorSets(perFlushDescriptorSet,
                                     {
                                         .dstBinding = PATH_BUFFER_IDX,
                                         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                     },
                                     {{
                                         .buffer = m_pathBufferRing.vkBufferAt(m_bufferRingIdx),
                                         .offset = desc.firstPath * sizeof(gpu::PathData),
                                         .range = VK_WHOLE_SIZE,
                                     }});

    m_vk->updateBufferDescriptorSets(
        perFlushDescriptorSet,
        {
            .dstBinding = PAINT_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        },
        {
            {
                .buffer = m_paintBufferRing.vkBufferAt(m_bufferRingIdx),
                .offset = desc.firstPaint * sizeof(gpu::PaintData),
                .range = VK_WHOLE_SIZE,
            },
            {
                .buffer = m_paintAuxBufferRing.vkBufferAt(m_bufferRingIdx),
                .offset = desc.firstPaintAux * sizeof(gpu::PaintAuxData),
                .range = VK_WHOLE_SIZE,
            },
        });
    static_assert(PAINT_AUX_BUFFER_IDX == PAINT_BUFFER_IDX + 1);

    m_vk->updateBufferDescriptorSets(perFlushDescriptorSet,
                                     {
                                         .dstBinding = CONTOUR_BUFFER_IDX,
                                         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                     },
                                     {{
                                         .buffer = m_contourBufferRing.vkBufferAt(m_bufferRingIdx),
                                         .offset = desc.firstContour * sizeof(gpu::ContourData),
                                         .range = VK_WHOLE_SIZE,
                                     }});

    m_vk->updateBufferDescriptorSets(
        perFlushDescriptorSet,
        {
            .dstBinding = FLUSH_UNIFORM_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        },
        {{
            .buffer = m_flushUniformBufferRing.vkBufferAt(m_bufferRingIdx),
            .offset = desc.flushUniformDataOffsetInBytes,
            .range = sizeof(gpu::FlushUniforms),
        }});

    m_vk->updateBufferDescriptorSets(
        perFlushDescriptorSet,
        {
            .dstBinding = IMAGE_DRAW_UNIFORM_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        },
        {{
            .buffer = m_imageDrawUniformBufferRing.vkBufferAt(m_bufferRingIdx),
            .offset = 0,
            .range = sizeof(gpu::ImageDrawUniforms),
        }});

    // Update the PLS input attachment descriptor sets.
    VkDescriptorSet inputAttachmentDescriptorSet =
        descriptorSetPool->allocateDescriptorSet(pipelineLayout.plsLayout());

    if (!(pipelineLayoutOptions & DrawPipelineLayoutOptions::fixedFunctionColorBlend))
    {
        m_vk->updateImageDescriptorSets(inputAttachmentDescriptorSet,
                                        {
                                            .dstBinding = COLOR_PLANE_IDX,
                                            .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                                        },
                                        {{
                                            .imageView = targetView,
                                            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                                        }});
    }

    m_vk->updateImageDescriptorSets(inputAttachmentDescriptorSet,
                                    {
                                        .dstBinding = CLIP_PLANE_IDX,
                                        .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                                    },
                                    {{
                                        .imageView = clipView,
                                        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                                    }});

    if (desc.interlockMode == gpu::InterlockMode::rasterOrdering)
    {
        m_vk->updateImageDescriptorSets(inputAttachmentDescriptorSet,
                                        {
                                            .dstBinding = SCRATCH_COLOR_PLANE_IDX,
                                            .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                                        },
                                        {{
                                            .imageView = scratchColorTextureView,
                                            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                                        }});
    }

    m_vk->updateImageDescriptorSets(
        inputAttachmentDescriptorSet,
        {
            .dstBinding = COVERAGE_PLANE_IDX,
            .descriptorType = desc.interlockMode == gpu::InterlockMode::atomics
                                  ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                                  : VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
        },
        {{
            .imageView = coverageTextureView,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        }});

    // Bind the descriptor sets for this draw pass.
    // (The imageTexture and imageDraw dynamic uniform offsets might have to update
    // between draws, but this is otherwise all we need to bind!)
    VkDescriptorSet drawDescriptorSets[] = {
        perFlushDescriptorSet,
        pipelineLayout.nullImageDescriptorSet(),
        pipelineLayout.samplerDescriptorSet(),
        inputAttachmentDescriptorSet,
    };
    static_assert(PER_FLUSH_BINDINGS_SET == 0);
    static_assert(PER_DRAW_BINDINGS_SET == 1);
    static_assert(SAMPLER_BINDINGS_SET == 2);
    static_assert(PLS_TEXTURE_BINDINGS_SET == 3);
    static_assert(BINDINGS_SET_COUNT == 4);

    m_vk->CmdBindDescriptorSets(commandBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                *pipelineLayout,
                                PER_FLUSH_BINDINGS_SET,
                                std::size(drawDescriptorSets),
                                drawDescriptorSets,
                                1,
                                zeroOffset32);

    // Execute the DrawList.
    uint32_t imageTextureUpdateCount = 0;
    for (const DrawBatch& batch : *desc.drawList)
    {
        if (batch.elementCount == 0)
        {
            continue;
        }

        DrawType drawType = batch.drawType;

        if (batch.imageTexture != nullptr)
        {
            // Update the imageTexture binding and the dynamic offset into the
            // imageDraw uniform buffer.
            auto imageTexture = static_cast<const PLSTextureVulkanImpl*>(batch.imageTexture);
            if (imageTexture->m_descriptorSetFrameIdx != m_vk->currentFrameIdx())
            {
                // Update the image's "texture binding" descriptor set. (These
                // expire every frame, so we need to make a new one each frame.)
                if (imageTextureUpdateCount >= descriptor_pool_limits::kMaxImageTextureUpdates)
                {
                    // We ran out of room for image texture updates. Allocate a new
                    // pool.
                    descriptorSetPool = makeDescriptorSetPool();
                    imageTextureUpdateCount = 0;
                }

                imageTexture->m_imageTextureDescriptorSet =
                    descriptorSetPool->allocateDescriptorSet(pipelineLayout.perDrawLayout());

                m_vk->updateImageDescriptorSets(
                    imageTexture->m_imageTextureDescriptorSet,
                    {
                        .dstBinding = IMAGE_TEXTURE_IDX,
                        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                    },
                    {{
                        .imageView = *imageTexture->m_textureView,
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    }});

                ++imageTextureUpdateCount;
                imageTexture->m_descriptorSetFrameIdx = m_vk->currentFrameIdx();
            }

            VkDescriptorSet imageDescriptorSets[] = {
                perFlushDescriptorSet,                     // Dynamic offset to imageDraw uniforms.
                imageTexture->m_imageTextureDescriptorSet, // imageTexture.
            };
            static_assert(PER_DRAW_BINDINGS_SET == PER_FLUSH_BINDINGS_SET + 1);

            m_vk->CmdBindDescriptorSets(commandBuffer,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        *pipelineLayout,
                                        PER_FLUSH_BINDINGS_SET,
                                        std::size(imageDescriptorSets),
                                        imageDescriptorSets,
                                        1,
                                        &batch.imageDrawDataOffset);
        }

        // Setup the pipeline for this specific drawType and shaderFeatures.
        gpu::ShaderFeatures shaderFeatures = desc.interlockMode == gpu::InterlockMode::atomics
                                                 ? desc.combinedShaderFeatures
                                                 : batch.shaderFeatures;
        uint32_t pipelineKey = gpu::ShaderUniqueKey(drawType,
                                                    shaderFeatures,
                                                    desc.interlockMode,
                                                    gpu::ShaderMiscFlags::none);
        auto drawPipelineOptions = DrawPipelineOptions::none;
        if (desc.wireframe && m_vk->features.fillModeNonSolid)
        {
            drawPipelineOptions |= DrawPipelineOptions::wireframe;
        }
        assert(pipelineKey << kDrawPipelineOptionCount >> kDrawPipelineOptionCount == pipelineKey);
        pipelineKey =
            (pipelineKey << kDrawPipelineOptionCount) | static_cast<uint32_t>(drawPipelineOptions);
        assert(pipelineKey * DrawPipelineLayout::kRenderPassVariantCount /
                   DrawPipelineLayout::kRenderPassVariantCount ==
               pipelineKey);
        pipelineKey =
            (pipelineKey * DrawPipelineLayout::kRenderPassVariantCount) + renderPassVariantIdx;
        const DrawPipeline& drawPipeline = m_drawPipelines
                                               .try_emplace(pipelineKey,
                                                            this,
                                                            drawType,
                                                            pipelineLayout,
                                                            shaderFeatures,
                                                            drawPipelineOptions,
                                                            vkRenderPass)
                                               .first->second;
        m_vk->CmdBindPipeline(commandBuffer,
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              drawPipeline.vkPipeline());

        if (needsBarrierBeforeNextDraw)
        {
            assert(desc.interlockMode == gpu::InterlockMode::atomics);

            VkMemoryBarrier memoryBarrier = {
                .sType = VkStructureType::VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
            };

            m_vk->CmdPipelineBarrier(commandBuffer,
                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                     VK_DEPENDENCY_BY_REGION_BIT,
                                     1,
                                     &memoryBarrier,
                                     0,
                                     nullptr,
                                     0,
                                     nullptr);
        }

        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::outerCurvePatches:
            {
                // Draw PLS patches that connect the tessellation vertices.
                m_vk->CmdBindVertexBuffers(commandBuffer,
                                           0,
                                           1,
                                           m_pathPatchVertexBuffer->vkBufferAddressOf(),
                                           zeroOffset);
                m_vk->CmdBindIndexBuffer(commandBuffer,
                                         *m_pathPatchIndexBuffer,
                                         0,
                                         VK_INDEX_TYPE_UINT16);
                m_vk->CmdDrawIndexed(commandBuffer,
                                     gpu::PatchIndexCount(drawType),
                                     batch.elementCount,
                                     gpu::PatchBaseIndex(drawType),
                                     0,
                                     batch.baseElement);
                break;
            }

            case DrawType::interiorTriangulation:
            {
                VkBuffer buffer = m_triangleBufferRing.vkBufferAt(m_bufferRingIdx);
                m_vk->CmdBindVertexBuffers(commandBuffer, 0, 1, &buffer, zeroOffset);
                m_vk->CmdDraw(commandBuffer, batch.elementCount, 1, batch.baseElement, 0);
                break;
            }

            case DrawType::imageRect:
            {
                assert(desc.interlockMode == gpu::InterlockMode::atomics);
                m_vk->CmdBindVertexBuffers(commandBuffer,
                                           0,
                                           1,
                                           m_imageRectVertexBuffer->vkBufferAddressOf(),
                                           zeroOffset);
                m_vk->CmdBindIndexBuffer(commandBuffer,
                                         *m_imageRectIndexBuffer,
                                         0,
                                         VK_INDEX_TYPE_UINT16);
                m_vk->CmdDrawIndexed(commandBuffer,
                                     std::size(gpu::kImageRectIndices),
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
                m_vk->CmdBindVertexBuffers(commandBuffer,
                                           0,
                                           1,
                                           vertexBuffer->frontVkBufferAddressOf(),
                                           zeroOffset);
                m_vk->CmdBindVertexBuffers(commandBuffer,
                                           1,
                                           1,
                                           uvBuffer->frontVkBufferAddressOf(),
                                           zeroOffset);
                m_vk->CmdBindIndexBuffer(commandBuffer,
                                         indexBuffer->frontVkBuffer(),
                                         0,
                                         VK_INDEX_TYPE_UINT16);
                m_vk->CmdDrawIndexed(commandBuffer, batch.elementCount, 1, batch.baseElement, 0, 0);
                break;
            }

            case DrawType::gpuAtomicResolve:
            {
                assert(desc.interlockMode == gpu::InterlockMode::atomics);
                m_vk->CmdDraw(commandBuffer, 4, 1, 0, 0);
                break;
            }

            case DrawType::gpuAtomicInitialize:
            case DrawType::stencilClipReset:
                RIVE_UNREACHABLE();
        }

        needsBarrierBeforeNextDraw =
            desc.interlockMode == gpu::InterlockMode::atomics && batch.needsBarrier;
    }

    m_vk->CmdEndRenderPass(commandBuffer);

    if (targetView == renderTarget->offscreenColorTextureView())
    {
        // Copy our offscreen color texture back to the render target now that we've finished
        // rendering.
        auto dstImage = renderTarget->targetTexture();
        // we know the offscreenColorTexture exists because of the if condition
        auto offScreenTexture = renderTarget->offscreenColorTexture();

        m_vk->insertImageMemoryBarrier(commandBuffer,
                                       offScreenTexture,
                                       VK_IMAGE_LAYOUT_GENERAL,
                                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        m_vk->insertImageMemoryBarrier(commandBuffer,
                                       dstImage,
                                       VK_IMAGE_LAYOUT_GENERAL,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        m_vk->blitSubRect(commandBuffer, offScreenTexture, dstImage, desc.renderTargetUpdateBounds);

        m_vk->insertImageMemoryBarrier(commandBuffer,
                                       dstImage,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       VK_IMAGE_LAYOUT_GENERAL);

        m_vk->insertImageMemoryBarrier(commandBuffer,
                                       offScreenTexture,
                                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                       VK_IMAGE_LAYOUT_GENERAL);
    }

    if (desc.isFinalFlushOfFrame)
    {
        m_frameCompletionFences[m_bufferRingIdx] = ref_rcp(desc.frameCompletionFence);
    }
}

std::unique_ptr<PLSRenderContext> PLSRenderContextVulkanImpl::MakeContext(
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    const VulkanFeatures& features,
    PFN_vkGetInstanceProcAddr fp_vkGetInstanceProcAddr,
    PFN_vkGetDeviceProcAddr fp_vkGetDeviceProcAddr)
{
    std::unique_ptr<PLSRenderContextVulkanImpl> impl(
        new PLSRenderContextVulkanImpl(instance,
                                       physicalDevice,
                                       device,
                                       features,
                                       fp_vkGetInstanceProcAddr,
                                       fp_vkGetDeviceProcAddr));
    if (!impl->platformFeatures().supportsPixelLocalStorage)
    {
        return nullptr; // TODO: implement MSAA.
    }
    impl->initGPUObjects();
    return std::make_unique<PLSRenderContext>(std::move(impl));
}
} // namespace rive::gpu
