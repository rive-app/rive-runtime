/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/gl/load_gles_extensions.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/rive_types.hpp"

namespace rive::ore
{

// GL internal format for a given ore TextureFormat.
static GLenum oreFormatToGLInternal(TextureFormat fmt)
{
    switch (fmt)
    {
        case TextureFormat::r8unorm:
            return GL_R8;
        case TextureFormat::rg8unorm:
            return GL_RG8;
        case TextureFormat::rgba8unorm:
            return GL_RGBA8;
        case TextureFormat::rgba8snorm:
            return GL_RGBA8_SNORM;
        case TextureFormat::bgra8unorm:
            // GLES3 doesn't have GL_BGRA8; use RGBA8 (swizzle in shader).
            return GL_RGBA8;
        case TextureFormat::rgba16float:
            return GL_RGBA16F;
        case TextureFormat::rg16float:
            return GL_RG16F;
        case TextureFormat::r16float:
            return GL_R16F;
        case TextureFormat::rgba32float:
            return GL_RGBA32F;
        case TextureFormat::rg32float:
            return GL_RG32F;
        case TextureFormat::r32float:
            return GL_R32F;
        case TextureFormat::rgb10a2unorm:
            return GL_RGB10_A2;
        case TextureFormat::r11g11b10float:
            return GL_R11F_G11F_B10F;
        case TextureFormat::depth16unorm:
            return GL_DEPTH_COMPONENT16;
        case TextureFormat::depth24plusStencil8:
            return GL_DEPTH24_STENCIL8;
        case TextureFormat::depth32float:
            return GL_DEPTH_COMPONENT32F;
        case TextureFormat::depth32floatStencil8:
            return GL_DEPTH32F_STENCIL8;
            // Compressed formats — internal format constants.
#ifdef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
        case TextureFormat::bc1unorm:
            return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
        case TextureFormat::bc3unorm:
            return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
#else
        case TextureFormat::bc1unorm:
        case TextureFormat::bc3unorm:
            RIVE_UNREACHABLE();
#endif
#ifdef GL_COMPRESSED_RGBA_BPTC_UNORM
        case TextureFormat::bc7unorm:
            return GL_COMPRESSED_RGBA_BPTC_UNORM;
#else
        case TextureFormat::bc7unorm:
            RIVE_UNREACHABLE();
#endif
        case TextureFormat::etc2rgb8:
            return GL_COMPRESSED_RGB8_ETC2;
        case TextureFormat::etc2rgba8:
            return GL_COMPRESSED_RGBA8_ETC2_EAC;
#ifdef GL_COMPRESSED_RGBA_ASTC_4x4_KHR
        case TextureFormat::astc4x4:
            return GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
        case TextureFormat::astc6x6:
            return GL_COMPRESSED_RGBA_ASTC_6x6_KHR;
        case TextureFormat::astc8x8:
            return GL_COMPRESSED_RGBA_ASTC_8x8_KHR;
#else
        case TextureFormat::astc4x4:
        case TextureFormat::astc6x6:
        case TextureFormat::astc8x8:
            RIVE_UNREACHABLE();
#endif
    }
    RIVE_UNREACHABLE();
}

// GL format + type for upload.
static GLenum oreFormatToGLFormat(TextureFormat fmt)
{
    switch (fmt)
    {
        case TextureFormat::r8unorm:
        case TextureFormat::r16float:
        case TextureFormat::r32float:
            return GL_RED;
        case TextureFormat::rg8unorm:
        case TextureFormat::rg16float:
        case TextureFormat::rg32float:
            return GL_RG;
        case TextureFormat::rgba8unorm:
        case TextureFormat::rgba8snorm:
        case TextureFormat::bgra8unorm:
        case TextureFormat::rgba16float:
        case TextureFormat::rgba32float:
            return GL_RGBA;
        case TextureFormat::rgb10a2unorm:
            return GL_RGBA;
        case TextureFormat::r11g11b10float:
            return GL_RGB;
        case TextureFormat::depth16unorm:
        case TextureFormat::depth32float:
            return GL_DEPTH_COMPONENT;
        case TextureFormat::depth24plusStencil8:
        case TextureFormat::depth32floatStencil8:
            return GL_DEPTH_STENCIL;
        default:
            RIVE_UNREACHABLE();
    }
}

static GLenum oreFormatToGLType(TextureFormat fmt)
{
    switch (fmt)
    {
        case TextureFormat::r8unorm:
        case TextureFormat::rg8unorm:
        case TextureFormat::rgba8unorm:
        case TextureFormat::bgra8unorm:
            return GL_UNSIGNED_BYTE;
        case TextureFormat::rgba8snorm:
            return GL_BYTE;
        case TextureFormat::rgba16float:
        case TextureFormat::rg16float:
        case TextureFormat::r16float:
            return GL_HALF_FLOAT;
        case TextureFormat::rgba32float:
        case TextureFormat::rg32float:
        case TextureFormat::r32float:
        case TextureFormat::depth32float:
            return GL_FLOAT;
        case TextureFormat::rgb10a2unorm:
            return GL_UNSIGNED_INT_2_10_10_10_REV;
        case TextureFormat::r11g11b10float:
            return GL_UNSIGNED_INT_10F_11F_11F_REV;
        case TextureFormat::depth16unorm:
            return GL_UNSIGNED_SHORT;
        case TextureFormat::depth24plusStencil8:
            return GL_UNSIGNED_INT_24_8;
        case TextureFormat::depth32floatStencil8:
            return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
        default:
            RIVE_UNREACHABLE();
    }
}

static bool isDepthFormat(TextureFormat fmt)
{
    return fmt == TextureFormat::depth16unorm ||
           fmt == TextureFormat::depth24plusStencil8 ||
           fmt == TextureFormat::depth32float ||
           fmt == TextureFormat::depth32floatStencil8;
}

static GLenum oreTextureTypeToGLTarget(TextureType type)
{
    switch (type)
    {
        case TextureType::texture2D:
            return GL_TEXTURE_2D;
        case TextureType::cube:
            return GL_TEXTURE_CUBE_MAP;
        case TextureType::texture3D:
            return GL_TEXTURE_3D;
        case TextureType::array2D:
            return GL_TEXTURE_2D_ARRAY;
    }
    RIVE_UNREACHABLE();
}

// Block-compressed format detection. For these formats, `bytesPerRow` is
// the byte size of a row of compressed *blocks* (4×4 for BC1/BC3/BC7/ETC2/
// ASTC4x4, 6×6 / 8×8 for the corresponding ASTC variants), and the upload
// path must go through `glCompressedTexSubImage*` with `imageSize` bytes.
static bool isCompressedFormat(TextureFormat fmt)
{
    switch (fmt)
    {
        case TextureFormat::bc1unorm:
        case TextureFormat::bc3unorm:
        case TextureFormat::bc7unorm:
        case TextureFormat::etc2rgb8:
        case TextureFormat::etc2rgba8:
        case TextureFormat::astc4x4:
        case TextureFormat::astc6x6:
        case TextureFormat::astc8x8:
            return true;
        default:
            return false;
    }
}

#if defined(ORE_BACKEND_GL) && !defined(ORE_BACKEND_METAL) &&                  \
    !defined(ORE_BACKEND_VK)

void Texture::upload(const TextureDataDesc& data)
{
    assert(m_glTexture != 0);
    assert(data.data != nullptr);

    // GLES3 has no `GL_BGRA8` internal format, and uploading BGRA bytes
    // through `GL_RGBA + GL_UNSIGNED_BYTE` silently swizzles channels
    // (red ↔ blue) in the resulting texture. Reject explicitly here
    // rather than letting the corruption render. Callers that need
    // BGRA-source data on GL should pre-swizzle in software or use
    // `rgba8unorm`. (BGRA *render targets* — e.g. canvas wrapping —
    // remain valid since they don't go through this upload path.)
    assert(m_format != TextureFormat::bgra8unorm &&
           "GLES3 cannot upload BGRA pixels — "
           "pre-swizzle to rgba8unorm or use a different format");

    // Force the active unit to GL_TEXTURE0 before a transient bind. See
    // Context::makeTexture() for the full rationale — Rive PLS reserves
    // units 8..14 and a naked glBindTexture() on one of those units would
    // clobber Rive's state.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(m_glTarget, m_glTexture);

    GLenum internalFmt = oreFormatToGLInternal(m_format);

    // Compressed formats route through `glCompressedTexSubImage*` with a
    // byte count, not format/type. `bytesPerRow * rowsPerImage` is the
    // total compressed byte size for one slice; multiply by `depth` for
    // 3D / array uploads. Pre-fix the GL backend always called
    // `glTexSubImage*` regardless of format — uploads of any compressed
    // texture (ETC2/BC/ASTC) failed silently with `GL_INVALID_OPERATION`.
    if (isCompressedFormat(m_format))
    {
        const uint32_t imageSize =
            data.bytesPerRow *
            (data.rowsPerImage > 0 ? data.rowsPerImage : data.height) *
            (data.depth > 0 ? data.depth : 1);
        if (m_glTarget == GL_TEXTURE_3D || m_glTarget == GL_TEXTURE_2D_ARRAY)
        {
            glCompressedTexSubImage3D(m_glTarget,
                                      data.mipLevel,
                                      data.x,
                                      data.y,
                                      data.layer,
                                      data.width,
                                      data.height,
                                      data.depth,
                                      internalFmt,
                                      imageSize,
                                      data.data);
        }
        else if (m_glTarget == GL_TEXTURE_CUBE_MAP)
        {
            glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X +
                                          data.layer,
                                      data.mipLevel,
                                      data.x,
                                      data.y,
                                      data.width,
                                      data.height,
                                      internalFmt,
                                      imageSize,
                                      data.data);
        }
        else
        {
            glCompressedTexSubImage2D(m_glTarget,
                                      data.mipLevel,
                                      data.x,
                                      data.y,
                                      data.width,
                                      data.height,
                                      internalFmt,
                                      imageSize,
                                      data.data);
        }
        glBindTexture(m_glTarget, 0);
        return;
    }

    GLenum format = oreFormatToGLFormat(m_format);
    GLenum type = oreFormatToGLType(m_format);

    // Honor `bytesPerRow` / `rowsPerImage` for non-tightly-packed source
    // data via `GL_UNPACK_ROW_LENGTH` / `GL_UNPACK_IMAGE_HEIGHT`. Save +
    // restore so we don't trash the host's pixel-store state. Pre-fix the
    // GL backend ignored these fields — uploads from a sub-rect of a
    // larger source buffer (or from a layout that pads rows) read garbage
    // bytes between rows.
    GLint savedRowLength = 0, savedImageHeight = 0;
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &savedRowLength);
    glGetIntegerv(GL_UNPACK_IMAGE_HEIGHT, &savedImageHeight);
    const uint32_t bpt = textureFormatBytesPerTexel(m_format);
    if (data.bytesPerRow != 0 && bpt != 0 && (data.bytesPerRow % bpt) == 0)
    {
        glPixelStorei(GL_UNPACK_ROW_LENGTH,
                      static_cast<GLint>(data.bytesPerRow / bpt));
    }
    if (data.rowsPerImage != 0)
    {
        glPixelStorei(GL_UNPACK_IMAGE_HEIGHT,
                      static_cast<GLint>(data.rowsPerImage));
    }

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

    // Restore host pixel-store state.
    glPixelStorei(GL_UNPACK_ROW_LENGTH, savedRowLength);
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, savedImageHeight);

    glBindTexture(m_glTarget, 0);
}

void Texture::onRefCntReachedZero() const
{
    if (m_glRenderbuffer != 0 && m_glOwnsTexture)
    {
        GLuint rb = m_glRenderbuffer;
        glDeleteRenderbuffers(1, &rb);
    }
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

#endif // ORE_BACKEND_GL && !ORE_BACKEND_METAL

} // namespace rive::ore
