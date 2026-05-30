/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/gpu_resource.hpp"
#include "utils/lite_rtti.hpp"
#include "rive/renderer/ore/ore_types.hpp"

namespace rive::ore
{

class Context;

class Buffer : public rive::gpu::GPUResource, public ENABLE_LITE_RTTI(Buffer)
{
public:
    uint32_t size() const { return m_size; }
    BufferUsage usage() const { return m_usage; }

    virtual void update(const void* data,
                        uint32_t size,
                        uint32_t offset = 0) = 0;

    virtual ~Buffer() = default;

protected:
    friend class Context;
    friend class RenderPass;

    Buffer(uint32_t size, BufferUsage usage) :
        rive::gpu::GPUResource(nullptr), m_size(size), m_usage(usage)
    {}

    Buffer(rcp<rive::gpu::GPUResourceManager> manager,
           uint32_t size,
           BufferUsage usage) :
        rive::gpu::GPUResource(std::move(manager)), m_size(size), m_usage(usage)
    {}

    uint32_t m_size;
    BufferUsage m_usage;
};

} // namespace rive::ore
