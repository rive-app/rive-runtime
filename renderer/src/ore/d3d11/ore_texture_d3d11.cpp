/*
 * Copyright 2025 Rive
 */

#include "ore_texture_d3d11.hpp"
#include "rive/rive_types.hpp"

#include <d3d11.h>

namespace rive::ore
{

#if defined(ORE_BACKEND_D3D11)

void TextureD3D11::upload(const TextureDataDesc& data)
{
    assert(m_d3d11Context != nullptr);
    assert(m_d3d11Texture != nullptr);
    assert(data.data != nullptr);

    UINT subresource =
        D3D11CalcSubresource(data.mipLevel, data.layer, m_numMipmaps);

    D3D11_BOX box{};
    box.left = data.x;
    box.top = data.y;
    box.front = data.z;
    box.right = data.x + data.width;
    box.bottom = data.y + data.height;
    box.back = data.z + data.depth;

    m_d3d11Context->UpdateSubresource(m_d3d11Texture.Get(),
                                      subresource,
                                      &box,
                                      data.data,
                                      data.bytesPerRow,
                                      data.bytesPerRow * data.rowsPerImage);
}

#endif // ORE_BACKEND_D3D11

} // namespace rive::ore
