/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_context.hpp"
#include "rive/rive_types.hpp"

#include <d3d12.h>
#include <cassert>
#include <cstring>

namespace rive::ore
{

// --- Public method definitions (D3D12-only builds) ---
// When both D3D11 and D3D12 are compiled, the combined
// ore_context_d3d11_d3d12.cpp file provides these methods with dispatch.
#if defined(ORE_BACKEND_D3D12) && !defined(ORE_BACKEND_D3D11)

void Buffer::update(const void* data, uint32_t size, uint32_t offset)
{
    assert(offset + size <= m_size);
    assert(m_d3dBuffer != nullptr);
    assert(m_d3dMappedPtr != nullptr);

    // UPLOAD heap buffer is persistently mapped — write directly.
    memcpy(static_cast<uint8_t*>(m_d3dMappedPtr) + offset, data, size);
}

void Buffer::onRefCntReachedZero() const
{
    Context* ctx = m_d3dOreContext;
    auto destroy = [p = const_cast<Buffer*>(this)]() { delete p; };
    if (ctx != nullptr)
        ctx->d3dDeferDestroy(std::move(destroy));
    else
        destroy();
}

#endif // D3D12-only

} // namespace rive::ore
