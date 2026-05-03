/*
 * Copyright 2025 Rive
 */

// Combined Metal + GL Texture implementation for macOS.
// Compiled when both ORE_BACKEND_METAL and ORE_BACKEND_GL are active.

#if defined(ORE_BACKEND_METAL) && defined(ORE_BACKEND_GL)

#include "rive/renderer/gl/load_gles_extensions.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/rive_types.hpp"

#import <Metal/Metal.h>

// Pull in GL static helpers (format conversion functions).
// The #ifndef ORE_BACKEND_METAL guard in ore_texture_gl.cpp excludes
// class method bodies, so we get only the helper functions.
#include "../gl/ore_texture_gl.cpp"

namespace rive::ore
{

void Texture::upload(const TextureDataDesc& data)
{
    assert(data.data != nullptr);

    if (m_glTexture != 0)
    {
        glBindTexture(m_glTarget, m_glTexture);

        GLenum internalFmt = oreFormatToGLInternal(m_format);
        GLenum format = oreFormatToGLFormat(m_format);
        GLenum type = oreFormatToGLType(m_format);
        (void)internalFmt;

        if (m_glTarget == GL_TEXTURE_3D || m_glTarget == GL_TEXTURE_2D_ARRAY)
        {
            glTexSubImage3D(m_glTarget,
                            data.mipLevel,
                            data.x,
                            data.y,
                            data.layer,
                            data.width,
                            data.height,
                            data.depth,
                            format,
                            type,
                            data.data);
        }
        else if (m_glTarget == GL_TEXTURE_CUBE_MAP)
        {
            glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + data.layer,
                            data.mipLevel,
                            data.x,
                            data.y,
                            data.width,
                            data.height,
                            format,
                            type,
                            data.data);
        }
        else
        {
            glTexSubImage2D(m_glTarget,
                            data.mipLevel,
                            data.x,
                            data.y,
                            data.width,
                            data.height,
                            format,
                            type,
                            data.data);
        }

        glBindTexture(m_glTarget, 0);
        return;
    }

    assert(m_mtlTexture != nil);
    MTLRegion region = MTLRegionMake3D(
        data.x, data.y, data.z, data.width, data.height, data.depth);
    [m_mtlTexture replaceRegion:region
                    mipmapLevel:data.mipLevel
                          slice:data.layer
                      withBytes:data.data
                    bytesPerRow:data.bytesPerRow
                  bytesPerImage:data.bytesPerRow * data.rowsPerImage];
}

void Texture::onRefCntReachedZero() const
{
    if (m_glTexture != 0 && m_glOwnsTexture)
    {
        GLuint tex = m_glTexture;
        glDeleteTextures(1, &tex);
    }
    delete this;
}

void TextureView::onRefCntReachedZero() const
{
    if (m_glTextureView != 0)
    {
        GLuint tex = m_glTextureView;
        glDeleteTextures(1, &tex);
    }
    delete this;
}

} // namespace rive::ore

#endif // ORE_BACKEND_METAL && ORE_BACKEND_GL
