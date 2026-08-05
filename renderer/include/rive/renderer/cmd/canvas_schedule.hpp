/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/cmd/deferred_session.hpp"

#include <cstring>
#include <vector>

// Orders canvas replay so samplers follow the canvases they sample,
// regardless of record order.
namespace rive::cmd
{

struct CanvasSchedule
{
    // Canvas ids in replay group order. Record order when no edge disagrees.
    std::vector<uint64_t> order;
    // A dependency cycle (or self sample) was demoted to previous frame
    // sampling; the demoted edge keeps record order.
    bool hadCycle = false;
    // A read landed between two writes of the same canvas; grouped-ranges
    // replay cannot honor the middle state, so the frame keeps record order.
    bool multiWriteFallback = false;
};

// Reads the image handle that leads every draw POD which can sample a canvas.
inline RenderHandle drawnImageHandle(const uint8_t* pod)
{
    RenderHandle h;
    memcpy(&h, pod, sizeof(h));
    return h;
}

inline CanvasSchedule scheduleCanvases(
    Span<const uint8_t> commands,
    const std::vector<DeferredSegment>& segments)
{
    CanvasSchedule result;

    // Written canvases, grouped; node index doubles as record-order rank.
    struct Node
    {
        uint64_t canvasId;
        uint32_t firstBegin; // earliest range start, the record-order key
        uint32_t lastBegin;  // latest range start, for the sandwich test
    };
    std::vector<Node> nodes;
    auto nodeFor = [&](uint64_t id) -> int {
        for (size_t i = 0; i < nodes.size(); i++)
        {
            if (nodes[i].canvasId == id)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    };
    for (const DeferredSegment& s : segments)
    {
        if (s.target != DeferredSegment::Target::canvas)
        {
            continue;
        }
        int n = nodeFor(s.targetId);
        if (n < 0)
        {
            nodes.push_back({s.targetId, s.begin, s.begin});
        }
        else
        {
            nodes[n].lastBegin = s.begin;
        }
    }
    if (nodes.empty())
    {
        return result;
    }

    // reader depends on written canvas: edges[reader] holds node indices.
    std::vector<std::vector<int>> deps(nodes.size());
    for (const DeferredSegment& s : segments)
    {
        if (s.target != DeferredSegment::Target::canvas)
        {
            continue;
        }
        int reader = nodeFor(s.targetId);
        uint32_t pos = s.begin;
        while (pos < s.end && pos < commands.size())
        {
            RenderCmd c = static_cast<RenderCmd>(commands[pos]);
            if (c > RenderCmd::lastRenderCmd)
            {
                break; // corrupt range; the decoder will warn at replay
            }
            uint32_t payload = static_cast<uint32_t>(payloadSizeOf(c));
            if (c == RenderCmd::drawImage || c == RenderCmd::drawImageMesh)
            {
                RenderHandle h = drawnImageHandle(commands.data() + pos + 1);
                if (h != kInvalidRenderHandle && (h & kCanvasHandleFlag))
                {
                    int sampled = nodeFor(h & kCanvasHandleMask);
                    if (sampled == reader && sampled >= 0)
                    {
                        result.hadCycle = true; // self sample: previous frame
                    }
                    else if (sampled >= 0)
                    {
                        // A read between two writes of the sampled canvas
                        // has no honorable grouped schedule.
                        if (nodes[sampled].firstBegin < pos &&
                            nodes[sampled].lastBegin > pos)
                        {
                            result.multiWriteFallback = true;
                        }
                        deps[reader].push_back(sampled);
                    }
                }
            }
            pos += 1 + payload;
        }
    }

    if (result.multiWriteFallback)
    {
        for (const Node& n : nodes)
        {
            result.order.push_back(n.canvasId);
        }
        return result;
    }

    // Kahn with record-order preference; a stuck round emits the earliest
    // recorded remaining node, demoting its unsatisfied edges (cycle case).
    std::vector<bool> done(nodes.size(), false);
    while (result.order.size() < nodes.size())
    {
        int pick = -1;
        for (size_t i = 0; i < nodes.size(); i++)
        {
            if (done[i])
            {
                continue;
            }
            bool ready = true;
            for (int d : deps[i])
            {
                if (!done[d])
                {
                    ready = false;
                    break;
                }
            }
            if (ready)
            {
                pick = static_cast<int>(i);
                break;
            }
        }
        if (pick < 0)
        {
            result.hadCycle = true;
            for (size_t i = 0; i < nodes.size(); i++)
            {
                if (!done[i])
                {
                    pick = static_cast<int>(i);
                    break;
                }
            }
        }
        done[pick] = true;
        result.order.push_back(nodes[pick].canvasId);
    }
    return result;
}

} // namespace rive::cmd
