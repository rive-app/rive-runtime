/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/refcnt.hpp"

#include "rive/math/aabb.hpp"
#include "rive/math/simd.hpp"

namespace rive::gpu
{
// Wraps a backend-specific buffer that PLSRenderContext draws into.
class PLSRenderTarget : public RefCnt<PLSRenderTarget>
{
public:
    virtual ~PLSRenderTarget() {}

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    uint2 size() const { return {m_width, m_height}; }
    IAABB bounds() const
    {
        return IAABB{0, 0, static_cast<int>(m_width), static_cast<int>(m_height)};
    }

protected:
    PLSRenderTarget(uint32_t width, uint32_t height) : m_width(width), m_height(height) {}

private:
    uint32_t m_width;
    uint32_t m_height;
};
} // namespace rive::gpu
