/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/cmd/command_stream.hpp"
#include "rive/renderer/cmd/recording_thread.hpp"
#include "rive/renderer/ore/cmd/ore_commands.hpp"
#include "rive/renderer/gpu_resource.hpp"
#include "rive/refcnt.hpp"
#include "rive/span.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>
#include <unordered_map>
#include "rive/renderer/cmd/id_allocator.hpp"
#include <vector>

// Records the ore RenderPass call stream into a flat byte stream (see
// ore_commands.hpp). Upload payloads live in a companion blob arena. Both
// vectors are reused across frames via reset, keeping capacity.
namespace rive::ore::cmd
{

class OreCommandBuffer : public rive::cmd::CommandByteStream
{
public:
    // Claim this stream for the calling thread; every append after it must
    // come from the same one. Deferred producers call it, an inline or
    // backend owned buffer leaves it unbound. Debug only, see RecordingThread.
    void bindRecordingThread() { m_recordingThread.bind(); }

    // When set, capture returns the provider's stable handle instead of a
    // buffer local keep alive index and does not retain the resource here.
    // Only reached for a resource that is not one of the recorder's own
    // deferred objects; those report their handle themselves.
    std::function<ResourceHandle(rive::gpu::GPUResource*)> realHandleProvider;

    // Dedups by pointer so repeated binds cost one ref. Callers must have
    // ruled out a deferred object first: this retains res and hands back an
    // index into the keep alive table, which replay reads as a real resource.
    ResourceHandle capture(rive::gpu::GPUResource* res)
    {
        if (res == nullptr)
        {
            return kInvalidHandle;
        }
        m_recordingThread.check();
        if (realHandleProvider)
        {
            return realHandleProvider(res);
        }
        auto it = m_resourceIds.find(res);
        if (it != m_resourceIds.end())
        {
            return it->second;
        }
        ResourceHandle h = static_cast<ResourceHandle>(m_keepAlive.size());
        m_keepAlive.push_back(rive::ref_rcp(res));
        m_resourceIds.emplace(res, h);
        return h;
    }

    template <typename POD> void append(CommandType type, const POD& pod)
    {
        m_recordingThread.check();
        appendUnchecked(type, pod);
    }

    void appendOpcode(CommandType type)
    {
        m_recordingThread.check();
        writeRaw(&type, sizeof(type));
    }

    // Payload with no opcode, e.g. a make descriptor after its header.
    template <typename POD> void appendPayload(const POD& pod)
    {
        static_assert(std::is_trivially_copyable<POD>::value);
        m_recordingThread.check();
        writeRaw(&pod, sizeof(pod));
    }

    // absent records a null source as distinct from an empty payload.
    BlobRef appendBlobRef(const void* data, uint32_t size, bool absent)
    {
        if (absent)
        {
            return kNoBlob;
        }
        return {appendBlob(data, size), size, 0};
    }
    // A null pointer maps to absent.
    BlobRef appendStringRef(const char* s)
    {
        if (s == nullptr)
        {
            return kNoBlob;
        }
        uint32_t len = static_cast<uint32_t>(std::strlen(s)) + 1; // include NUL
        return appendBlobRef(s, len, false);
    }

    // Dart finalizers destroy on GC threads, so destroys queue and drain on
    // the recording thread. The erase is generation checked.
    struct PendingDestroy
    {
        ResourceHandle handle;
        uint32_t generation;
        rive::IdAllocator<ResourceHandle>* allocator;
    };

private:
    template <typename POD> void appendUnchecked(CommandType type, const POD& p)
    {
        static_assert(std::is_trivially_copyable<POD>::value);
        writeRaw(&type, sizeof(type));
        writeRaw(&p, sizeof(p));
    }

    void applyDestroy(const PendingDestroy& p)
    {
        // Unchecked: the last drain of a session's life runs from wherever
        // the host posted its teardown, which on threaded wasm is the replay
        // worker rather than the recording thread.
        appendUnchecked(CommandType::destroyResource,
                        DestroyResourcePOD{p.handle, p.generation});
        if (p.allocator != nullptr)
        {
            p.allocator->release(p.handle, p.generation);
        }
    }

public:
    void queueDestroy(const PendingDestroy& pending)
    {
        std::lock_guard<std::mutex> lock(m_destroyMutex);
        m_pendingDestroys.push_back(pending);
    }

    // The id goes straight back, so the next create in this same stream may
    // retake it. Safe because a consumer replays whole frames in stream order
    // and every resident slot is generation checked: the retake's create
    // stamps a new generation, so this destroy no-ops when it replays and a
    // snapshot recorded before it still resolves its own generation out of its
    // own byte copy.
    void drainDestroys()
    {
        std::vector<PendingDestroy> pending;
        {
            std::lock_guard<std::mutex> lock(m_destroyMutex);
            pending.swap(m_pendingDestroys);
        }
        for (const auto& p : pending)
        {
            applyDestroy(p);
        }
    }

    // Keeps capacity for reuse across frames.
    void reset()
    {
        m_recordingThread.check();
        clearBytes();
        m_keepAlive.clear();
        m_resourceIds.clear();
    }

    const std::vector<rcp<rive::gpu::GPUResource>>& keepAlive() const
    {
        return m_keepAlive;
    }

private:
    rive::cmd::RecordingThread m_recordingThread;
    std::mutex m_destroyMutex;
    std::vector<PendingDestroy> m_pendingDestroys;
    std::vector<rcp<rive::gpu::GPUResource>> m_keepAlive;
    std::unordered_map<rive::gpu::GPUResource*, ResourceHandle> m_resourceIds;
};

// Sequential reader shared by backend replay, the silver comparator, and a
// viewer.
using OreCommandReader = rive::cmd::CommandReader<CommandType>;

} // namespace rive::ore::cmd
