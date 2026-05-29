/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/refcnt.hpp"

namespace rive::gpu
{
class Texture : public RefCnt<Texture>
{
public:
    Texture(uint32_t width, uint32_t height);
    virtual ~Texture() {}

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }

    // Quazi-unique identifier of the underlying GPU texture resource managed by
    // this class.
    uint32_t textureResourceHash() const { return m_textureResourceHash; }

    // Returns the backend-native texture handle (id<MTLTexture>, GLuint,
    // VkImage, etc.) as a void pointer.  Used by
    // ore::Context::wrapRiveTexture() to bridge Rive renderer images into the
    // Ore GPU abstraction. Default returns nullptr (backend must override).
    virtual void* nativeHandle() const { return nullptr; }

protected:
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_textureResourceHash;
};

} // namespace rive::gpu
