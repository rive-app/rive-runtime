/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/renderer.hpp"
#include "rive/renderer/gpu.hpp"

namespace rive
{
// RenderBuffer with additional indices to track the "front" and "back" buffers,
// assuming a ring of gpu::kBufferRingSize buffers.
class RiveRenderBuffer : public RenderBuffer
{
protected:
    RiveRenderBuffer(RenderBufferType type,
                     RenderBufferFlags flags,
                     size_t sizeInBytes) :
        RenderBuffer(type, flags, sizeInBytes)
    {}

    // Returns the index of the buffer to map and update, prior to rendering.
    int backBufferIdx() const { return m_backBufferIdx; }

    // Returns the index of the buffer to submit with rendering commands.
    // Automatically advances the buffer ring if the RenderBuffer is dirty.
    int frontBufferIdx()
    {
        if (checkAndResetDirty())
        {
            // The update buffer is dirty. Advance the buffer ring.
            m_frontBufferIdx = m_backBufferIdx;
            m_backBufferIdx = (m_backBufferIdx + 1) % gpu::kBufferRingSize;
        }
        return m_frontBufferIdx;
    }

private:
    int m_backBufferIdx = 0;
    int m_frontBufferIdx = -1;
};
} // namespace rive
