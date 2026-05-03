/*
 * Copyright 2025 Rive
 */

// Combined D3D11 + D3D12 Buffer implementation for Windows.
// Compiled when both ORE_BACKEND_D3D11 and ORE_BACKEND_D3D12 are active.

#if defined(ORE_BACKEND_D3D11) && defined(ORE_BACKEND_D3D12)

#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_context.hpp"
#include "rive/rive_types.hpp"

#include <d3d11.h>
#include <d3d12.h>
#include <cassert>
#include <cstring>

namespace rive::ore
{

void Buffer::update(const void* data, uint32_t size, uint32_t offset)
{
    assert(offset + size <= m_size);
    if (m_d3d11Buffer)
    {
        // D3D11 path — map, copy, unmap.
        assert(m_d3d11Context != nullptr);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        D3D11_MAP mapType = (m_usage == BufferUsage::uniform)
                                ? D3D11_MAP_WRITE_DISCARD
                                : D3D11_MAP_WRITE_NO_OVERWRITE;
        [[maybe_unused]] HRESULT hr =
            m_d3d11Context->Map(m_d3d11Buffer.Get(), 0, mapType, 0, &mapped);
        assert(SUCCEEDED(hr));
        memcpy(static_cast<uint8_t*>(mapped.pData) + offset, data, size);
        m_d3d11Context->Unmap(m_d3d11Buffer.Get(), 0);
    }
    else
    {
        // D3D12 path — UPLOAD heap buffer is persistently mapped.
        assert(m_d3dBuffer != nullptr);
        assert(m_d3dMappedPtr != nullptr);
        memcpy(static_cast<uint8_t*>(m_d3dMappedPtr) + offset, data, size);
    }
}

void Buffer::onRefCntReachedZero() const
{
    // m_d3dOreContext is only set on D3D12-backed buffers (see
    // d3d12MakeBuffer). D3D11-backed buffers keep the immediate-delete path
    // unchanged.
    Context* ctx = m_d3dOreContext;
    auto destroy = [p = const_cast<Buffer*>(this)]() { delete p; };
    if (ctx != nullptr)
        ctx->d3dDeferDestroy(std::move(destroy));
    else
        destroy();
}

} // namespace rive::ore

#endif // ORE_BACKEND_D3D11 && ORE_BACKEND_D3D12
