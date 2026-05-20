/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/refcnt.hpp"
#include "utils/lite_rtti.hpp"
#include "rive/renderer/ore/ore_types.hpp"

namespace rive::ore
{

class Context;

class Buffer : public RefCnt<Buffer>, public ENABLE_LITE_RTTI(Buffer)
{
public:
    uint32_t size() const { return m_size; }
    BufferUsage usage() const { return m_usage; }

    virtual void update(const void* data,
                        uint32_t size,
                        uint32_t offset = 0) = 0;

    virtual ~Buffer() = default;

    // Default: immediately free the CPU object. Backends that need deferred
    // GPU-resource destruction (D3D12, Vulkan) override this.
    virtual void onRefCntReachedZero() const { delete this; }

protected:
    friend class Context;
    friend class RenderPass;

    Buffer(uint32_t size, BufferUsage usage) : m_size(size), m_usage(usage) {}

    uint32_t m_size;
    BufferUsage m_usage;
};

} // namespace rive::ore
