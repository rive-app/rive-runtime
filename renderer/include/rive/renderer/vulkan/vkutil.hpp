/*
 * Copyright 2024 Rive
 */

#pragma once

#include "rive/refcnt.hpp"
#include "rive/renderer/gpu.hpp"
#include "rive/renderer/gpu_resource.hpp"
#include "rive/renderer/texture.hpp"
#include <cassert>
#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>

VK_DEFINE_HANDLE(VmaAllocation);

namespace rive::gpu
{
class VulkanContext;
} // namespace rive::gpu

namespace rive::gpu::vkutil
{
inline static void vk_check(VkResult res, const char* file, int line)
{
    if (res != VK_SUCCESS)
    {
        fprintf(stderr,
                "Vulkan error %i at line: %i in file: %s\n",
                res,
                line,
                file);
        abort();
    }
}

#define VK_CHECK(x) ::rive::gpu::vkutil::vk_check(x, __FILE__, __LINE__)

constexpr static VkColorComponentFlags kColorWriteMaskNone = 0;
constexpr static VkColorComponentFlags kColorWriteMaskRGBA =
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

enum class Mappability
{
    none,
    writeOnly,
    readWrite,
};

// Base class for a GPU resource that needs to be kept alive until any in-flight
// command buffers that reference it have completed.
class Resource : public GPUResource
{
public:
    virtual ~Resource() {}

    VulkanContext* vk() const;

protected:
    Resource(rcp<VulkanContext>);
};

class Buffer : public Resource
{
public:
    ~Buffer() override;

    VkBufferCreateInfo info() const { return m_info; }
    operator VkBuffer() const { return m_vkBuffer; }
    const VkBuffer* vkBufferAddressOf() const { return &m_vkBuffer; }

    // Resize the underlying VkBuffer without waiting for any pipeline
    // synchronization. The caller is responsible to guarantee the underlying
    // VkBuffer is not queued up in any in-flight command buffers.
    void resizeImmediately(VkDeviceSize sizeInBytes);

    void* contents()
    {
        assert(m_contents != nullptr);
        return m_contents;
    }

    // Calls through to vkFlushMappedMemoryRanges().
    // Called after modifying contents() with the CPU. Makes those modifications
    // available to the GPU.
    void flushContents(VkDeviceSize sizeInBytes = VK_WHOLE_SIZE);

    // Calls through to vkInvalidateMappedMemoryRanges().
    // Called after modifying the buffer with the GPU. Makes those modifications
    // available to the CPU via contents().
    void invalidateContents(VkDeviceSize sizeInBytes = VK_WHOLE_SIZE);

private:
    friend class ::rive::gpu::VulkanContext;

    Buffer(rcp<VulkanContext>, const VkBufferCreateInfo&, Mappability);

    void init();

    const Mappability m_mappability;
    VkBufferCreateInfo m_info;
    VmaAllocation m_vmaAllocation;
    VkBuffer m_vkBuffer;
    void* m_contents;
};

// Wraps a pool of Buffers so we can map one while other(s) are in-flight.
class BufferPool : public GPUResourcePool
{
public:
    BufferPool(rcp<VulkanContext>, VkBufferUsageFlags, VkDeviceSize size = 0);

    VkDeviceSize size() const { return m_targetSize; }
    void setTargetSize(VkDeviceSize size);

    // Returns a Buffer that is guaranteed to exist and be of size
    // 'm_targetSize'.
    rcp<vkutil::Buffer> acquire();

    void recycle(rcp<vkutil::Buffer> buffer)
    {
        GPUResourcePool::recycle(std::move(buffer));
    }

private:
    VulkanContext* vk() const;

    constexpr static VkDeviceSize MAX_POOL_SIZE = 8;
    const VkBufferUsageFlags m_usageFlags;
    VkDeviceSize m_targetSize;
};

class Image : public Resource
{
public:
    ~Image() override;

    const VkImageCreateInfo& info() { return m_info; }
    operator VkImage() const { return m_vkImage; }
    const VkImage* vkImageAddressOf() const { return &m_vkImage; }

private:
    friend class ::rive::gpu::VulkanContext;

    Image(rcp<VulkanContext>, const VkImageCreateInfo&);

    VkImageCreateInfo m_info;
    VmaAllocation m_vmaAllocation;
    VkImage m_vkImage;
};

class ImageView : public Resource
{
public:
    ~ImageView() override;

    const VkImageViewCreateInfo& info() { return m_info; }
    operator VkImageView() const { return m_vkImageView; }
    VkImageView vkImageView() const { return m_vkImageView; }
    const VkImageView* vkImageViewAddressOf() const { return &m_vkImageView; }

private:
    friend class ::rive::gpu::VulkanContext;

    ImageView(rcp<VulkanContext>,
              rcp<Image> textureRef,
              const VkImageViewCreateInfo&);

    const rcp<Image> m_textureRefOrNull;
    VkImageViewCreateInfo m_info;
    VkImageView m_vkImageView;
};

// Tracks the current layout and access parameters of a VkImage.
struct ImageAccess
{
    VkPipelineStageFlags pipelineStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkAccessFlags accessMask = VK_ACCESS_NONE;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

    bool operator==(const ImageAccess& rhs) const
    {
        return pipelineStages == rhs.pipelineStages &&
               accessMask == rhs.accessMask && layout == rhs.layout;
    }
    bool operator!=(const ImageAccess& rhs) const { return !(*this == rhs); }
};

// Provides a way to communicate that a VkImage may be invalidated (layout
// converted to VK_IMAGE_LAYOUT_UNDEFINED) while performing a barrier.
enum class ImageAccessAction : bool
{
    preserveContents,
    invalidateContents,
};

// Wrapper for a simple 2D VkImage and VkImageView.
class Texture2D : public rive::gpu::Texture
{
public:
    VkImage vkImage() const { return *m_image; }
    VkImageView vkImageView() const { return *m_imageView; }
    const VkImageView* vkImageViewAddressOf() const
    {
        return m_imageView->vkImageViewAddressOf();
    }
    ImageAccess& lastAccess() { return m_lastAccess; }

    // Deferred mechanism for uploading image data without a command buffer.
    void stageContentsForUpload(const void* imageData,
                                size_t imageDataSizeInBytes);
    bool hasUpdates() const { return m_imageUploadBuffer != nullptr; }
    void synchronize(VkCommandBuffer);

    void barrier(VkCommandBuffer,
                 const ImageAccess& dstAccess,
                 ImageAccessAction = ImageAccessAction::preserveContents,
                 VkDependencyFlags = 0);

    // Downscales the top level into sub-levels.
    // NOTE: Does not wrap the edges when filtering down. This is not an ideal
    // situation for non-power-of-two textures that are intended to be used with
    // a wrap mode of "repeat". We may want to add a "wrap" argument at some
    // point.
    void generateMipmaps(VkCommandBuffer, const ImageAccess& dstAccess);

    // Simple mechanism for caching and reusing a descriptor set for this
    // texture within a frame.
    VkDescriptorSet getCachedDescriptorSet(uint64_t frameNumber,
                                           ImageSampler sampler) const
    {
        return frameNumber == m_cachedDescriptorSetFrameNumber &&
                       sampler == m_cachedDescriptorSetSampler
                   ? m_cachedDescriptorSet
                   : VK_NULL_HANDLE;
    }

    void updateCachedDescriptorSet(VkDescriptorSet descriptorSet,
                                   uint64_t frameNumber,
                                   ImageSampler sampler)
    {
        m_cachedDescriptorSet = descriptorSet;
        m_cachedDescriptorSetFrameNumber = frameNumber;
        m_cachedDescriptorSetSampler = sampler;
    }

protected:
    friend class ::rive::gpu::VulkanContext;

    Texture2D(rcp<VulkanContext> vk, VkImageCreateInfo);

    rcp<Image> m_image;
    rcp<ImageView> m_imageView;
    ImageAccess m_lastAccess;

    rcp<vkutil::Buffer> m_imageUploadBuffer;

    // Simple mechanism for caching and reusing a descriptor set for this
    // texture within a frame.
    VkDescriptorSet m_cachedDescriptorSet = VK_NULL_HANDLE;
    uint64_t m_cachedDescriptorSetFrameNumber;
    ImageSampler m_cachedDescriptorSetSampler;
};

class Framebuffer : public Resource
{
public:
    ~Framebuffer() override;

    const VkFramebufferCreateInfo& info() const { return m_info; }
    operator VkFramebuffer() const { return m_vkFramebuffer; }

private:
    friend class ::rive::gpu::VulkanContext;

    Framebuffer(rcp<VulkanContext>, const VkFramebufferCreateInfo&);

    VkFramebufferCreateInfo m_info;
    VkFramebuffer m_vkFramebuffer;
};

// Utility to generate a simple 2D VkViewport from a VkRect2D.
class ViewportFromRect2D
{
public:
    ViewportFromRect2D(const VkRect2D rect) :
        m_viewport{
            .x = static_cast<float>(rect.offset.x),
            .y = static_cast<float>(rect.offset.y),
            .width = static_cast<float>(rect.extent.width),
            .height = static_cast<float>(rect.extent.height),
            .minDepth = DEPTH_MIN,
            .maxDepth = DEPTH_MAX,
        }
    {}

    operator const VkViewport*() const { return &m_viewport; }

private:
    VkViewport m_viewport;
};

inline void set_shader_code(VkShaderModuleCreateInfo& info,
                            const uint32_t* code,
                            size_t codeSize)
{
    info.codeSize = codeSize;
    info.pCode = code;
}

inline void set_shader_code_if_then_else(VkShaderModuleCreateInfo& info,
                                         bool _if,
                                         const uint32_t* codeIf,
                                         size_t codeSizeIf,
                                         const uint32_t* codeElse,
                                         size_t codeSizeElse)
{
    if (_if)
    {
        set_shader_code(info, codeIf, codeSizeIf);
    }
    else
    {
        set_shader_code(info, codeElse, codeSizeElse);
    }
}

inline void set_shader_code(VkShaderModuleCreateInfo& info,
                            rive::Span<const uint32_t> code)
{
    info.codeSize = code.size_bytes();
    info.pCode = code.data();
}

inline void set_shader_code_if_then_else(VkShaderModuleCreateInfo& info,
                                         bool _if,
                                         rive::Span<const uint32_t> codeIf,
                                         rive::Span<const uint32_t> codeElse)
{
    if (_if)
    {
        set_shader_code(info, codeIf);
    }
    else
    {
        set_shader_code(info, codeElse);
    }
}

inline VkClearColorValue color_clear_rgba32f(ColorInt riveColor)
{
    VkClearColorValue ret;
    UnpackColorToRGBA32FPremul(riveColor, ret.float32);
    return ret;
}

inline VkClearColorValue color_clear_r32ui(uint32_t value)
{
    VkClearColorValue ret;
    ret.uint32[0] = value;
    return ret;
}

inline VkFormat get_preferred_depth_stencil_format(bool isD24S8Supported)
{
    return isD24S8Supported ? VK_FORMAT_D24_UNORM_S8_UINT
                            : VK_FORMAT_D32_SFLOAT_S8_UINT;
}
} // namespace rive::gpu::vkutil
