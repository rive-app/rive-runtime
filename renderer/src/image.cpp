/*
 * Copyright 2022 Rive
 */

#include "rive/renderer/image.hpp"

namespace rive::gpu
{
PLSTexture::PLSTexture(uint32_t width, uint32_t height) : m_width(width), m_height(height)
{
    static std::atomic_uint32_t textureResourceHashCounter = 0;
    m_textureResourceHash = ++textureResourceHashCounter;
}
} // namespace rive::gpu
