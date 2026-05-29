/*
 * Copyright 2025 Rive
 */

#include "ore_texture_metal.hpp"
#include "rive/rive_types.hpp"

#import <Metal/Metal.h>

namespace rive::ore
{

void TextureMetal::upload(const TextureDataDesc& data)
{
    assert(m_mtlTexture != nil);
    assert(data.data != nullptr);

    MTLRegion region = MTLRegionMake3D(
        data.x, data.y, data.z, data.width, data.height, data.depth);

    // Apple's `replaceRegion:` docs require `bytesPerImage = 0` for
    // non-array 2D textures (Metal API Validation aborts on any other
    // value). For texture3D / array2D the value is the per-slice stride.
    const NSUInteger mtlBytesPerImage =
        (m_type == TextureType::texture3D || m_type == TextureType::array2D)
            ? static_cast<NSUInteger>(data.bytesPerRow) * data.rowsPerImage
            : 0;

    [m_mtlTexture replaceRegion:region
                    mipmapLevel:data.mipLevel
                          slice:data.layer
                      withBytes:data.data
                    bytesPerRow:data.bytesPerRow
                  bytesPerImage:mtlBytesPerImage];
}

} // namespace rive::ore
