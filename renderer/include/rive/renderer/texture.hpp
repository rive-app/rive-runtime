/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/refcnt.hpp"
#include "rive/renderer/render_context_impl.hpp"

namespace rive::gpu
{
class Texture : public RefCnt<Texture>
{
public:
    Texture(uint32_t width, uint32_t height);
    virtual ~Texture() {}

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }

    // Quazi-unique identifier of the underlying GPU texture resource managed by
    // this class.
    uint32_t textureResourceHash() const { return m_textureResourceHash; }

protected:
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_textureResourceHash;
};

} // namespace rive::gpu
