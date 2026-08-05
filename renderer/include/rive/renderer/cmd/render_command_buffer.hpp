/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/span.hpp"
#include "rive/renderer/cmd/command_stream.hpp"
#include "rive/renderer/cmd/id_allocator.hpp"
#include "rive/renderer/cmd/recording_thread.hpp"
#include "rive/renderer/cmd/render_commands.hpp"
#include <cassert>
#include <cstdint>
#include <mutex>
#include <vector>

// RenderCommandBuffer is a flat pointer free byte stream plus a blob arena.
// Everything is referenced by dense resource id, never a pointer, so the bytes
// cross a SharedArrayBuffer to a worker zero copy. It holds no live resources;
// the render side recreates everything from the stream by id.
namespace rive::cmd
{

class RenderCommandBuffer : public CommandByteStream
{
public:
    // A session's producer binds; a backend owned buffer leaves it unbound.
    // Debug only, see RecordingThread.
    void bindRecordingThread() { m_recordingThread.bind(); }

    // Append [type byte][POD]. POD must be trivially copyable.
    template <typename POD> void append(uint8_t type, const POD& pod)
    {
        static_assert(std::is_trivially_copyable<POD>::value,
                      "command POD must be trivially copyable");
        m_recordingThread.check();
        writeRaw(&type, sizeof(type));
        writeRaw(&pod, sizeof(pod));
    }

    // Append just a type byte (commands with no payload).
    void appendType(uint8_t type)
    {
        m_recordingThread.check();
        writeRaw(&type, sizeof(type));
    }

    // GC finalizers destroy on worker threads, so destroys queue and drain on
    // the recording thread.
    struct PendingDestroy
    {
        uint8_t kind;
        RenderHandle id;
        uint32_t generation;
        IdAllocator<RenderHandle>* allocator;
    };

    void queueDestroy(uint8_t kind,
                      RenderHandle id,
                      uint32_t generation,
                      IdAllocator<RenderHandle>* allocator)
    {
        std::lock_guard<std::mutex> lock(m_destroyMutex);
        m_pendingDestroys.push_back({kind, id, generation, allocator});
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
            append(static_cast<uint8_t>(RenderCmd::destroyResource),
                   DestroyResourcePOD{p.kind, p.id, p.generation});
            if (p.allocator != nullptr)
            {
                p.allocator->release(p.id, p.generation);
            }
        }
    }

    void reset()
    {
        clearBytes();
        m_frameId++;
    }

    // Which frame is recording. Resources stamp draws with it so only a
    // mutation of a resource drawn THIS frame bumps a version; the first
    // mutation of a new frame reuses the live replay object in place.
    uint32_t frameId() const { return m_frameId; }

private:
    RecordingThread m_recordingThread;
    std::mutex m_destroyMutex;
    std::vector<PendingDestroy> m_pendingDestroys;
    uint32_t m_frameId = 0;
};

using RenderCommandReader = CommandReader<uint8_t>;

} // namespace rive::cmd
