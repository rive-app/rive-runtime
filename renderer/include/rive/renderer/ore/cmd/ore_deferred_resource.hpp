/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/ore/cmd/ore_handle.hpp"
#include "rive/renderer/ore/cmd/ore_replay.hpp"
#include "rive/renderer/ore/cmd/ore_make_recording.hpp"
#include "rive/renderer/cmd/id_allocator.hpp"
#include "rive/renderer/cmd/live_recorder_registry.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include <cassert>
#include <functional>
#include <unordered_map>
#include <vector>

// Client handle resource objects. A make* returns one immediately with no GPU
// object, carrying its handle, generation, and the descriptor facts validation
// needs. Replay creates the real object at the same handle; destruction
// records a destroy and returns the id for reuse with a bumped generation.
namespace rive::ore::cmd
{

// Pointer keyed handles for resources this recorder did not create: real
// backend objects a frame binds, deduped so repeated binds cost one ref.
// Deferred objects are never in here, they answer for themselves.
using PtrHandleMap =
    std::unordered_map<rive::gpu::GPUResource*, ResourceHandle>;

// Common mixin. The DeferredOreContext owns the stream and allocator and
// outlives every resource, so the back pointers stay valid.
class DeferredResource
{
public:
    DeferredResource(ResourceHandle handle,
                     uint32_t generation,
                     OreCommandBuffer* stream,
                     rive::IdAllocator<ResourceHandle>* allocator) :
        m_clientHandle(handle),
        m_generation(generation),
        m_stream(stream),
        m_allocator(allocator)
    {}
    // Asking the live object is what makes a lookup immune to address
    // recycling: an address only names this handle for as long as this object
    // occupies it.
    ResourceHandle clientHandle() const { return m_clientHandle; }

    // A handle resolves against the table this resource's stream feeds, so a
    // reader that writes a different stream must not use it.
    bool recordsInto(const OreCommandBuffer* stream) const
    {
        return m_stream != nullptr && m_stream == stream;
    }

protected:
    ~DeferredResource()
    {
        // Destructors run on any thread, possibly after the owning context
        // died, so stragglers no-op and live destroys queue for the drain.
        std::lock_guard<std::mutex> lock(rive::cmd::recorderRegistryMutex());
        if (m_stream != nullptr)
        {
            if (rive::cmd::liveRecorders().count(m_stream) == 0)
            {
                return; // the context died first, nothing to record into
            }
            m_stream->queueDestroy({m_clientHandle, m_generation, m_allocator});
            return;
        }
        if (m_allocator != nullptr &&
            rive::cmd::liveRecorders().count(m_allocator) == 0)
        {
            return;
        }
        if (m_allocator != nullptr)
        {
            m_allocator->release(m_clientHandle, m_generation);
        }
    }
    OreCommandBuffer* stream() const { return m_stream; }

private:
    ResourceHandle m_clientHandle;
    uint32_t m_generation;
    OreCommandBuffer* m_stream;
    rive::IdAllocator<ResourceHandle>* m_allocator;
};

class DeferredBuffer : public LITE_RTTI_OVERRIDE(Buffer, DeferredBuffer),
                       public DeferredResource
{
public:
    DeferredBuffer(ResourceHandle handle,
                   uint32_t generation,
                   OreCommandBuffer* stream,
                   rive::IdAllocator<ResourceHandle>* allocator,
                   uint32_t size,
                   BufferUsage usage) :
        LITE_RTTI_OVERRIDE(Buffer, DeferredBuffer)(size, usage),
        DeferredResource(handle, generation, stream, allocator)
    {}

    // Recorded, replayed on the real buffer before the passes.
    void update(const void* data, uint32_t size, uint32_t offset) override
    {
        if (stream() != nullptr)
        {
            recordBufferUpdate(*stream(), clientHandle(), data, size, offset);
        }
    }
};

// The remaining types carry the descriptor so record time validation works
// with no GPU object.

class DeferredTexture : public LITE_RTTI_OVERRIDE(Texture, DeferredTexture),
                        public DeferredResource
{
public:
    DeferredTexture(ResourceHandle handle,
                    uint32_t generation,
                    OreCommandBuffer* stream,
                    rive::IdAllocator<ResourceHandle>* allocator,
                    const TextureDesc& desc) :
        LITE_RTTI_OVERRIDE(Texture, DeferredTexture)(desc),
        DeferredResource(handle, generation, stream, allocator)
    {}

    // Recorded, replayed on the real texture before the passes.
    void upload(const TextureDataDesc& data) override
    {
        if (stream() != nullptr)
        {
            recordTextureUpload(*stream(), clientHandle(), data);
        }
    }
};

class DeferredTextureView
    : public LITE_RTTI_OVERRIDE(TextureView, DeferredTextureView),
      public DeferredResource
{
public:
    DeferredTextureView(ResourceHandle handle,
                        uint32_t generation,
                        OreCommandBuffer* stream,
                        rive::IdAllocator<ResourceHandle>* allocator,
                        rcp<Texture> texture,
                        const TextureViewDesc& desc) :
        LITE_RTTI_OVERRIDE(TextureView, DeferredTextureView)(std::move(texture),
                                                             desc),
        DeferredResource(handle, generation, stream, allocator)
    {}
};

class DeferredSampler : public LITE_RTTI_OVERRIDE(Sampler, DeferredSampler),
                        public DeferredResource
{
public:
    DeferredSampler(ResourceHandle handle,
                    uint32_t generation,
                    OreCommandBuffer* stream,
                    rive::IdAllocator<ResourceHandle>* allocator) :
        DeferredResource(handle, generation, stream, allocator)
    {}
};

class DeferredShaderModule
    : public LITE_RTTI_OVERRIDE(ShaderModule, DeferredShaderModule),
      public DeferredResource
{
public:
    DeferredShaderModule(ResourceHandle handle,
                         uint32_t generation,
                         OreCommandBuffer* stream,
                         rive::IdAllocator<ResourceHandle>* allocator) :
        DeferredResource(handle, generation, stream, allocator)
    {}
};

class DeferredBindGroupLayout
    : public LITE_RTTI_OVERRIDE(BindGroupLayout, DeferredBindGroupLayout),
      public DeferredResource
{
public:
    DeferredBindGroupLayout(ResourceHandle handle,
                            uint32_t generation,
                            OreCommandBuffer* stream,
                            rive::IdAllocator<ResourceHandle>* allocator) :
        DeferredResource(handle, generation, stream, allocator)
    {}
};

class DeferredPipeline : public LITE_RTTI_OVERRIDE(Pipeline, DeferredPipeline),
                         public DeferredResource
{
public:
    DeferredPipeline(ResourceHandle handle,
                     uint32_t generation,
                     OreCommandBuffer* stream,
                     rive::IdAllocator<ResourceHandle>* allocator,
                     const PipelineDesc& desc) :
        LITE_RTTI_OVERRIDE(Pipeline, DeferredPipeline)(desc),
        DeferredResource(handle, generation, stream, allocator)
    {}
};

class DeferredBindGroup
    : public LITE_RTTI_OVERRIDE(BindGroup, DeferredBindGroup),
      public DeferredResource
{
public:
    DeferredBindGroup(ResourceHandle handle,
                      uint32_t generation,
                      OreCommandBuffer* stream,
                      rive::IdAllocator<ResourceHandle>* allocator) :
        DeferredResource(handle, generation, stream, allocator)
    {}
};

} // namespace rive::ore::cmd
