/*
 * Copyright 2025 Rive
 */

#include "ore_buffer_d3d11.hpp"
#include "rive/rive_types.hpp"

#include <d3d11.h>

namespace rive::ore
{

#if defined(ORE_BACKEND_D3D11)

void BufferD3D11::update(const void* data, uint32_t size, uint32_t offset)
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
    const bool whole = offset == 0 && size == m_size;
    // Every update lands in the shadow so a later partial one can restore
    // what a discard drops.
    if (m_shadow.empty())
        m_shadow.resize(m_size, 0);
    memcpy(m_shadow.data() + offset, data, size);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    [[maybe_unused]] HRESULT hr = m_d3d11Context->Map(m_d3d11Buffer.Get(),
                                                      0,
                                                      D3D11_MAP_WRITE_DISCARD,
                                                      0,
                                                      &mapped);
    assert(SUCCEEDED(hr));
    // A discard leaves the rest of the buffer undefined.
    if (whole)
        memcpy(mapped.pData, data, size);
    else
        memcpy(mapped.pData, m_shadow.data(), m_size);
    m_d3d11Context->Unmap(m_d3d11Buffer.Get(), 0);
}
#endif // ORE_BACKEND_D3D11 && !ORE_BACKEND_D3D12

} // namespace rive::ore
