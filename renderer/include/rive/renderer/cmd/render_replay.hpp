/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/factory.hpp"
#include <cstdio>
#include "rive/renderer.hpp"
#include "rive/shapes/paint/image_sampler.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/renderer/cmd/render_command_buffer.hpp"
#include "rive/renderer/cmd/render_commands.hpp"
#include <cassert>
#include <cstring>
#include <functional>
#include <unordered_map>
#include <vector>

// Replays a recorded 2D command stream against a real Factory and Renderer.
// Creation commands rebuild real resources into dense per type vectors,
// mutations replay in order, and draws drive the real renderer.
namespace rive::cmd
{

// Bulk copy is safe since the bytes came from a valid RawPath at record time.
inline RawPath rebuildRawPath(Span<const uint8_t> verbBytes,
                              Span<const uint8_t> pointBytes)
{
    return RawPath(
        Span<const PathVerb>(
            reinterpret_cast<const PathVerb*>(verbBytes.data()),
            verbBytes.size() / sizeof(PathVerb)),
        Span<const Vec2D>(reinterpret_cast<const Vec2D*>(pointBytes.data()),
                          pointBytes.size() / sizeof(Vec2D)));
}

// Resolves a flagged canvas id to the real canvas's RenderImage at replay.
// Null when the stream draws no canvases.
using CanvasImageResolver = std::function<RenderImage*(RenderHandle canvasId)>;

// Draw drop accounting for verification.
struct ReplayStats
{
    uint32_t droppedDraws = 0;
};

namespace replay_detail
{
inline void logDroppedDraw(uint8_t type, uint32_t h0, uint32_t h1)
{
    RIVE_WARN_THROTTLED("rive replay: dropped draw opcode=%u handles=%u,%u "
                        "(unresolved at replay)\n",
                        type,
                        h0,
                        h1);
}
} // namespace replay_detail

// Which command classes a walk executes. The filter only gates side effects;
// reads always consume payloads so the walk stays in sync.
enum class ReplayFilter : uint8_t
{
    all,
    resources, // creates, mutations, version bumps, in record order
    draws,     // draws, brackets, markers, in scheduled segment order
    destroys,  // destroyResource
};

// Render thread hooks for a deferred runtime frame. A bare replay leaves them
// null and everything draws into renderer.
struct ReplayHooks
{
    ReplayFilter filter = ReplayFilter::all;
    CanvasImageResolver canvasImage = nullptr;
    // Open the real canvas's frame and return the renderer that draws into
    // it; a null return safely drops that canvas's content.
    std::function<Renderer*(RenderHandle canvasId, uint32_t clearColor)>
        beginCanvasContent = nullptr;
    // Nonzero dropped draws mean mixed factory recording or a replay bug.
    ReplayStats* stats = nullptr;
};

// One resident resource type: a dense vector indexed by id, each slot carrying
// the real object and its creation generation. Never compacts.
template <typename T> struct Resident
{
    std::vector<rcp<T>> objects;
    std::vector<uint32_t> generations;
    // Draws pin the exact version they recorded against; older versions of a
    // slot live in per frame aliases keyed (id << 32) | version.
    std::vector<uint32_t> versions;
    std::unordered_map<uint64_t, rcp<T>> versionAliases;

    void set(RenderHandle id, rcp<T> obj, uint32_t generation)
    {
        if (id > objects.size())
        {
            // The producer mints ids sequentially, so a fresh id may only
            // append; anything further ahead is a corrupt stream.
            assert(false);
            return;
        }
        if (id == objects.size())
        {
            objects.push_back(std::move(obj));
            generations.push_back(generation);
            versions.push_back(0);
            return;
        }
        // A retaken id must not resolve to the old take's versions.
        dropVersionAliases(id);
        objects[id] = std::move(obj);
        generations[id] = generation;
        versions[id] = 0;
    }
    void dropVersionAliases(RenderHandle id)
    {
        if (versionAliases.empty())
        {
            return;
        }
        for (auto it = versionAliases.begin(); it != versionAliases.end();)
        {
            if (static_cast<RenderHandle>(it->first >> 32) == id)
            {
                it = versionAliases.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
    // Move the live object under its old version and make obj the live one.
    void newVersion(RenderHandle id, uint32_t version, rcp<T> obj)
    {
        if (id >= objects.size())
        {
            return;
        }
        versionAliases[(uint64_t(id) << 32) | versions[id]] =
            std::move(objects[id]);
        objects[id] = std::move(obj);
        versions[id] = version;
    }
    void destroy(RenderHandle id, uint32_t generation)
    {
        if (id < objects.size() && generations[id] == generation)
        {
            objects[id] = nullptr;
            dropVersionAliases(id);
        }
    }
    T* get(RenderHandle id) const
    {
        return id < objects.size() ? objects[id].get() : nullptr;
    }
    T* get(RenderHandle id, uint32_t version) const
    {
        if (id >= objects.size())
        {
            return nullptr;
        }
        if (version == versions[id])
        {
            return objects[id].get();
        }
        auto it = versionAliases.find((uint64_t(id) << 32) | version);
        return it == versionAliases.end() ? nullptr : it->second.get();
    }
    // Owning rcp for APIs that take one.
    rcp<T> shared(RenderHandle id) const
    {
        return id < objects.size() ? objects[id] : nullptr;
    }
    rcp<T> shared(RenderHandle id, uint32_t version) const
    {
        if (id >= objects.size())
        {
            return nullptr;
        }
        if (version == versions[id])
        {
            return objects[id];
        }
        auto it = versionAliases.find((uint64_t(id) << 32) | version);
        return it == versionAliases.end() ? nullptr : it->second;
    }
};

// The materialized 2D resources, persisted across frames so resources stay
// resident on the render thread.
// CPU shadows of mutable state, so a version bump can materialize a fresh
// object carrying the outgoing state without cloning backend objects.
struct PaintShadow
{
    // Must be the backend's defaults: a property the producer never sends is
    // one it knows the fresh object already carries.
    uint8_t style = 1; // fill; a fresh paint is unstroked until told
    ColorInt color = 0xFF000000;
    float thickness = 1;
    uint8_t join = 0;
    uint8_t cap = 0;
    float feather = 0;
    uint8_t blendMode = 3; // srcOver
    RenderHandle shader = kInvalidRenderHandle;
};
struct BufferShadow
{
    uint8_t type = 0;
    uint16_t flags = 0;
    uint32_t size = 0;
};

struct ResourceTable
{
    Resident<RenderPath> paths;
    Resident<RenderPaint> paints;
    Resident<RenderShader> shaders;
    Resident<RenderImage> images;
    Resident<RenderBuffer> buffers;
    std::vector<PaintShadow> paintShadows;
    std::vector<uint8_t> pathFillRules;
    std::vector<BufferShadow> bufferShadows;

    // Generation checked so a stale destroy after slot reuse is a no-op.
    void destroy(ResourceKind kind, RenderHandle id, uint32_t generation)
    {
        switch (kind)
        {
            case ResourceKind::path:
                paths.destroy(id, generation);
                break;
            case ResourceKind::paint:
                paints.destroy(id, generation);
                break;
            case ResourceKind::shader:
                shaders.destroy(id, generation);
                break;
            case ResourceKind::image:
                images.destroy(id, generation);
                break;
            case ResourceKind::buffer:
                buffers.destroy(id, generation);
                break;
        }
    }

    // Old versions only live within one frame's replay.
    void clearVersionAliases()
    {
        paths.versionAliases.clear();
        paints.versionAliases.clear();
        shaders.versionAliases.clear();
        images.versionAliases.clear();
        buffers.versionAliases.clear();
    }
};

// Span form: replays raw stream bytes so a snapshot copy can replay without a
// RenderCommandBuffer. Defined in src/deferred_cmd.cpp.
void replayRenderCommands(Factory* factory,
                          Renderer* renderer,
                          Span<const uint8_t> commands,
                          Span<const uint8_t> blobs,
                          ResourceTable& table,
                          const ReplayHooks& hooks = {});

// Buffer form.
inline void replayRenderCommands(Factory* factory,
                                 Renderer* renderer,
                                 const RenderCommandBuffer& cmd,
                                 ResourceTable& table,
                                 const ReplayHooks& hooks = {})
{
    replayRenderCommands(factory,
                         renderer,
                         cmd.commandBytes(),
                         cmd.blobBytes(),
                         table,
                         hooks);
}

// Convenience form with a throwaway table.
inline void replayRenderCommands(Factory* factory,
                                 Renderer* renderer,
                                 const RenderCommandBuffer& cmd,
                                 const ReplayHooks& hooks = {})
{
    ResourceTable table;
    replayRenderCommands(factory, renderer, cmd, table, hooks);
}

} // namespace rive::cmd
