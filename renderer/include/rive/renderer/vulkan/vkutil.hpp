/*
 * Copyright 2024 Rive
 */

#pragma once

#include "rive/refcnt.hpp"
#include "rive/renderer/gpu.hpp"
#include <cassert>
#include <deque>
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
class RenderingResource : public RefCnt<RenderingResource>
{
public:
    virtual ~RenderingResource() {}

    const VulkanContext* vulkanContext() const { return m_vk.get(); }

protected:
    RenderingResource(rcp<VulkanContext> vk) : m_vk(std::move(vk)) {}

    const rcp<VulkanContext> m_vk;

private:
    friend class RefCnt<RenderingResource>;

    // Don't delete RenderingResources immediately when their ref count reaches
    // zero; wait until any in-flight command buffers are done referencing their
    // underlying Vulkan objects.
    void onRefCntReachedZero() const;
};

// A RenderingResource that has been fully released, but whose underlying Vulkan
// object may still be referenced by an in-flight command buffer.
template <typename T> struct ZombieResource
{
    ZombieResource(T* resource_, uint64_t lastFrameNumber_) :
        resource(resource_), lastFrameNumber(lastFrameNumber_)
    {
        assert(resource_->debugging_refcnt() == 0);
    }
    std::unique_ptr<T> resource;
    // Frame number at which the underlying Vulkan resource was last used.
    const uint64_t lastFrameNumber;
};

class Buffer : public RenderingResource
{
public:
    ~Buffer() override;

    VkBufferCreateInfo info() const { return m_info; }
    operator VkBuffer() const { return m_vkBuffer; }
    const VkBuffer* vkBufferAddressOf() const { return &m_vkBuffer; }

    // Resize the underlying VkBuffer without waiting for any pipeline
    // synchronization. The caller is responsible to guarantee the underlying
    // VkBuffer is not queued up in any in-flight command buffers.
    void resizeImmediately(size_t sizeInBytes);

    void* contents()
    {
        assert(m_contents != nullptr);
        return m_contents;
    }

    // Calls through to vkFlushMappedMemoryRanges().
    // Called after modifying contents() with the CPU. Makes those modifications
    // available to the GPU.
    void flushContents(size_t sizeInBytes = VK_WHOLE_SIZE);

    // Calls through to vkInvalidateMappedMemoryRanges().
    // Called after modifying the buffer with the GPU. Makes those modifications
    // available to the CPU via contents().
    void invalidateContents(size_t sizeInBytes = VK_WHOLE_SIZE);

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
class BufferPool
{
public:
    BufferPool(rcp<VulkanContext> vk,
               VkBufferUsageFlags usageFlags,
               size_t size = 0) :
        m_vk(std::move(vk)), m_usageFlags(usageFlags), m_targetSize(size)
    {}

    VulkanContext* vulkanContext() const { return m_vk.get(); }

    size_t size() const { return m_targetSize; }
    void setTargetSize(size_t size);

    vkutil::Buffer* currentBuffer();
    uint64_t currentBufferFrameNumber() { return m_currentBufferFrameNumber; }

    void* mapCurrentBuffer(size_t dirtySize = VK_WHOLE_SIZE);
    void unmapCurrentBuffer();

    // Returns the current buffer to the pool.
    void releaseCurrentBuffer();

private:
    const rcp<VulkanContext> m_vk;
    const VkBufferUsageFlags m_usageFlags;
    size_t m_targetSize;
    rcp<vkutil::Buffer> m_currentBuffer;
    uint64_t m_currentBufferFrameNumber = 0;
    size_t m_pendingFlushSize = 0;

    struct PooledBuffer
    {
        PooledBuffer() = default;
        PooledBuffer(rcp<vkutil::Buffer> buffer_, uint64_t lastFrameNumber_) :
            buffer(std::move(buffer_)), lastFrameNumber(lastFrameNumber_)
        {}
        rcp<vkutil::Buffer> buffer;
        uint64_t lastFrameNumber = 0;
    };

    std::deque<PooledBuffer> m_pool;
};

class Texture : public RenderingResource
{
public:
    ~Texture() override;

    const VkImageCreateInfo& info() { return m_info; }
    operator VkImage() const { return m_vkImage; }
    const VkImage* vkImageAddressOf() const { return &m_vkImage; }

private:
    friend class ::rive::gpu::VulkanContext;

    Texture(rcp<VulkanContext>, const VkImageCreateInfo&);

    VkImageCreateInfo m_info;
    VmaAllocation m_vmaAllocation;
    VkImage m_vkImage;
};

class TextureView : public RenderingResource
{
public:
    ~TextureView() override;

    const VkImageViewCreateInfo& info() { return m_info; }
    operator VkImageView() const { return m_vkImageView; }
    VkImageView vkImageView() const { return m_vkImageView; }
    const VkImageView* vkImageViewAddressOf() const { return &m_vkImageView; }

private:
    friend class ::rive::gpu::VulkanContext;

    TextureView(rcp<VulkanContext>,
                rcp<Texture> textureRef,
                const VkImageViewCreateInfo&);

    const rcp<Texture> m_textureRefOrNull;
    VkImageViewCreateInfo m_info;
    VkImageView m_vkImageView;
};

class Framebuffer : public RenderingResource
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
} // namespace rive::gpu::vkutil
