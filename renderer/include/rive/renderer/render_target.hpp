/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/refcnt.hpp"

#include "rive/math/aabb.hpp"
#include "rive/math/simd.hpp"
#include "rive/renderer/gpu.hpp"

namespace rive::gpu
{
// Wraps a backend-specific buffer that RenderContext draws into.
class RenderTarget : public RefCnt<RenderTarget>
{
public:
    virtual ~RenderTarget() {}

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    uint2 size() const { return {m_width, m_height}; }
    IAABB bounds() const
    {
        return IAABB{0,
                     0,
                     static_cast<int>(m_width),
                     static_cast<int>(m_height)};
    }

    // Whether row 0 of the target holds the visual bottom. Targets follow
    // their framebuffer unless a backend keeps some the other way round, and
    // everything that maps Rive pixel space onto the target compensates per
    // flush. Stays inline so a lane linking a subclass never needs gpu.cpp.
    virtual bool bottomUp(const PlatformFeatures& platformFeatures) const
    {
        return platformFeatures.framebufferBottomUp;
    }

protected:
    RenderTarget(uint32_t width, uint32_t height) :
        m_width(width), m_height(height)
    {}

private:
    uint32_t m_width;
    uint32_t m_height;
};
} // namespace rive::gpu
