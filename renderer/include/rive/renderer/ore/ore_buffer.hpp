/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/refcnt.hpp"
#include "rive/renderer/ore/ore_types.hpp"

#if defined(ORE_BACKEND_METAL)
#import <Metal/Metal.h>
#endif
// Note: load_gles_extensions.hpp (glad) is intentionally NOT included here.
// GL object handles are GLuint = unsigned int; no glad needed in the header.
#if defined(ORE_BACKEND_D3D11)
#include <d3d11.h>
#include <wrl/client.h>
#endif
#if defined(ORE_BACKEND_D3D12)
#include <d3d12.h>
#include <wrl/client.h>
#endif
#if defined(ORE_BACKEND_WGPU)
#include <webgpu/webgpu_cpp.h>
#endif
#if defined(ORE_BACKEND_VK)
#include <vulkan/vulkan.h>
VK_DEFINE_HANDLE(VmaAllocator);
VK_DEFINE_HANDLE(VmaAllocation);
#endif

namespace rive::ore
{

class Context;

class Buffer : public RefCnt<Buffer>
{
public:
    uint32_t size() const { return m_size; }
    BufferUsage usage() const { return m_usage; }

    void update(const void* data, uint32_t size, uint32_t offset = 0);

private:
    friend class Context;
    friend class RenderPass;

    Buffer(uint32_t size, BufferUsage usage) : m_size(size), m_usage(usage) {}

    uint32_t m_size;
    BufferUsage m_usage;

#if defined(ORE_BACKEND_METAL)
    id<MTLBuffer> m_mtlBuffer = nil;
#endif
#if defined(ORE_BACKEND_GL)
    unsigned int m_glBuffer = 0;
    unsigned int m_glTarget = 0;
#endif
#if defined(ORE_BACKEND_D3D11)
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_d3d11Buffer;
    ID3D11DeviceContext* m_d3d11Context =
        nullptr; // Weak ref, set by Context::makeBuffer.
#endif
#if defined(ORE_BACKEND_WGPU)
    wgpu::Buffer m_wgpuBuffer;
    wgpu::Queue
        m_wgpuQueue; // Weak ref (addref'd copy), set by Context::makeBuffer.
#endif
#if defined(ORE_BACKEND_VK)
    VkBuffer m_vkBuffer = VK_NULL_HANDLE;
    VmaAllocation m_vmaAllocation = VK_NULL_HANDLE;
    void* m_vkMappedPtr = nullptr;
    VkDevice m_vkDevice = VK_NULL_HANDLE;         // Weak ref.
    VmaAllocator m_vmaAllocator = VK_NULL_HANDLE; // Weak ref.
    // Back-ref so onRefCntReachedZero() can route the vmaDestroyBuffer call
    // through Context::vkDeferDestroy() in external-CB mode. Weak ref.
    Context* m_vkOreContext = nullptr;
#endif
#if defined(ORE_BACKEND_D3D12)
    // UPLOAD heap — persistently mapped; GPU reads directly (acceptable for
    // tests).
    Microsoft::WRL::ComPtr<ID3D12Resource> m_d3dBuffer;
    void* m_d3dMappedPtr = nullptr;
    // Back-ref so onRefCntReachedZero() can route the ComPtr release through
    // Context::d3dDeferDestroy() in external-CL mode. Weak ref.
    Context* m_d3dOreContext = nullptr;
#endif

public:
    void onRefCntReachedZero() const;
};

} // namespace rive::ore
