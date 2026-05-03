/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/rive_types.hpp"

#include <d3d11.h>

namespace rive::ore
{

#if defined(ORE_BACKEND_D3D11) && !defined(ORE_BACKEND_D3D12)

void Buffer::update(const void* data, uint32_t size, uint32_t offset)
{
    assert(offset + size <= m_size);
    assert(m_d3d11Context != nullptr);
    assert(m_d3d11Buffer != nullptr);

    // Always WRITE_DISCARD: it allocates fresh GPU memory and is safe
    // for any update pattern, including the common dynamic-VB case
    // where the GPU may still be reading the prior frame's contents.
    // The previous code used `WRITE_NO_OVERWRITE` for vertex/index
    // buffers as a perf optimization, which is undefined behavior per
    // the D3D11 spec when overlapping with in-flight reads — the debug
    // layer flags it. NO_OVERWRITE is only valid for append-style
    // suballocation patterns Ore doesn't currently expose.
    D3D11_MAPPED_SUBRESOURCE mapped{};
    [[maybe_unused]] HRESULT hr = m_d3d11Context->Map(m_d3d11Buffer.Get(),
                                                      0,
                                                      D3D11_MAP_WRITE_DISCARD,
                                                      0,
                                                      &mapped);
    assert(SUCCEEDED(hr));
    memcpy(static_cast<uint8_t*>(mapped.pData) + offset, data, size);
    m_d3d11Context->Unmap(m_d3d11Buffer.Get(), 0);
}

void Buffer::onRefCntReachedZero() const { delete this; }

#endif // ORE_BACKEND_D3D11 && !ORE_BACKEND_D3D12

} // namespace rive::ore
