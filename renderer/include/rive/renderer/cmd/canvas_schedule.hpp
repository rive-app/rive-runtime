/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/cmd/deferred_session.hpp"

#include <algorithm>
#include <cstring>
#include <unordered_map>
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
  // Ore bytes [begin, end) that replay before order[before], or after every
  // canvas when before is order.size(). Adjacent Ore runs merge into one.
  struct OreRun
  {
    size_t before;
    uint32_t begin;
    uint32_t end;
  };
  std::vector<OreRun> earlyOre;
  // Ore bytes from here on replay inside the screen frame.
  uint32_t lateOreBegin = 0;
  // An Ore run a canvas samples, or one before it, touches the host target, so
  // those canvases see last frame's Ore output.
  bool orePinned = false;
};

// Reads the image handle that leads every draw POD which can sample a canvas.
inline RenderHandle drawnImageHandle(const uint8_t* pod)
{
  RenderHandle h;
  memcpy(&h, pod, sizeof(h));
  return h;
}

// Record order between canvases and runs of Ore passes. A tie means the Ore
// run ended before the canvas began, since a canvas start grows the 2D stream.
struct RecordOrder
{
  const std::vector<uint32_t>& rank;
  size_t canvasCount;
  bool operator()(int a, int b) const
  {
    if (rank[a] != rank[b])
    {
      return rank[a] < rank[b];
    }
    return static_cast<size_t>(a) >= canvasCount &&
           static_cast<size_t>(b) < canvasCount;
  }
};

// Drops each canvas's frames that a later frame on it replaces: readers only
// ever see the last one, so earlier frames are wasted work and phantom writes.
inline std::vector<DeferredSegment> liveSegments(
    const std::vector<DeferredSegment>& segments)
{
  std::unordered_map<uint64_t, size_t> lastFrame;
  for (size_t i = 0; i < segments.size(); i++)
  {
    const DeferredSegment& s = segments[i];
    if (s.target == DeferredSegment::Target::canvas && s.startsFrame)
    {
      lastFrame[s.targetId] = i;
    }
  }
  std::vector<DeferredSegment> live;
  live.reserve(segments.size());
  for (size_t i = 0; i < segments.size(); i++)
  {
    const DeferredSegment& s = segments[i];
    if (s.target == DeferredSegment::Target::canvas)
    {
      auto it = lastFrame.find(s.targetId);
      if (it != lastFrame.end() && i < it->second)
      {
        continue;
      }
    }
    live.push_back(s);
  }
  return live;
}

inline CanvasSchedule scheduleCanvases(
    Span<const uint8_t> commands,
    const std::vector<DeferredSegment>& segments,
    const std::vector<DeferredOreSegment>& oreSegments = {})
{
  CanvasSchedule result;

  // Written canvases, grouped; node index doubles as record-order rank.
  struct Node
  {
    uint64_t canvasId;
    uint32_t firstBegin; // earliest range start, the record-order key
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
    if (nodeFor(s.targetId) < 0)
    {
      nodes.push_back({s.targetId, s.begin});
    }
  }
  if (nodes.empty())
  {
    return result;
  }

  // reader depends on written canvas: edges[reader] holds node indices.
  std::vector<std::vector<int>> deps(nodes.size());
  // Every canvas each reader samples, written by 2D or not.
  std::vector<std::vector<uint64_t>> sampled(nodes.size());
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
      // applyLayerMask belongs here as much as the draws do: layer
      // masking samples a coverage canvas from inside the content canvas
      // it masks, so leaving it out means that edge never registers and
      // the schedule only works by accident of record order. So does
      // canvasSample, which names the canvas a drawPath's paint samples.
      if (c == RenderCmd::drawImage || c == RenderCmd::drawImageMesh ||
          c == RenderCmd::drawImageMeshInstanced ||
          c == RenderCmd::applyLayerMask || c == RenderCmd::canvasSample)
      {
        RenderHandle h = drawnImageHandle(commands.data() + pos + 1);
        if (h != kInvalidRenderHandle && (h & kCanvasHandleFlag))
        {
          sampled[reader].push_back(h & kCanvasHandleMask);
          int sampledNode = nodeFor(h & kCanvasHandleMask);
          if (sampledNode == reader && sampledNode >= 0)
          {
            result.hadCycle = true; // self sample: previous frame
          }
          else if (sampledNode >= 0)
          {
            deps[reader].push_back(sampledNode);
          }
        }
      }
      pos += 1 + payload;
    }
  }
  for (std::vector<uint64_t>& ids : sampled)
  {
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
  }

  // Ore runs stay in record order, so a canvas sampling what one wrote pulls
  // it and every earlier run ahead. Readers see a canvas's last write.
  std::unordered_map<uint64_t, size_t> lastOreWriter;
  for (size_t i = 0; i < oreSegments.size(); i++)
  {
    for (uint64_t id : oreSegments[i].writes)
    {
      lastOreWriter[id] = i;
    }
  }
  size_t earlyOre = 0;
  for (const std::vector<uint64_t>& reads : sampled)
  {
    for (uint64_t id : reads)
    {
      auto it = lastOreWriter.find(id);
      if (it != lastOreWriter.end())
      {
        earlyOre = std::max(earlyOre, it->second + 1);
      }
    }
  }
  for (size_t i = 0; i < earlyOre; i++)
  {
    if (oreSegments[i].pinned)
    {
      result.orePinned = true;
      earlyOre = 0;
      break;
    }
  }

  // Canvas nodes first, then the early Ore runs.
  const size_t total = nodes.size() + earlyOre;
  std::vector<uint32_t> rank(total);
  for (size_t i = 0; i < nodes.size(); i++)
  {
    rank[i] = nodes[i].firstBegin;
  }
  deps.resize(total);
  for (size_t e = 0; e < earlyOre; e++)
  {
    const size_t node = nodes.size() + e;
    const DeferredOreSegment& ore = oreSegments[e];
    rank[node] = ore.recordPos;
    if (e > 0)
    {
      deps[node].push_back(static_cast<int>(node - 1));
    }
    // After the canvases it samples, and after any 2D content in a canvas
    // it draws over.
    for (const std::vector<uint64_t>* ids : {&ore.reads, &ore.writes})
    {
      for (uint64_t id : *ids)
      {
        int n = nodeFor(id);
        if (n >= 0)
        {
          deps[node].push_back(n);
        }
      }
    }
  }
  for (size_t reader = 0; reader < nodes.size(); reader++)
  {
    for (uint64_t id : sampled[reader])
    {
      auto it = lastOreWriter.find(id);
      if (it != lastOreWriter.end() && it->second < earlyOre)
      {
        deps[reader].push_back(static_cast<int>(nodes.size() + it->second));
      }
    }
  }

  const RecordOrder before{rank, nodes.size()};
  std::vector<int> picks;
  picks.reserve(total);
  // Kahn with record-order preference; a stuck round emits the earliest
  // recorded remaining node, demoting its unsatisfied edges (cycle case).
  std::vector<bool> done(total, false);
  while (picks.size() < total)
  {
    int pick = -1;
    bool stuck = true;
    for (size_t i = 0; i < total; i++)
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
      if (ready && (stuck || before(static_cast<int>(i), pick)))
      {
        pick = static_cast<int>(i);
        stuck = false;
      }
    }
    if (stuck)
    {
      result.hadCycle = true;
      for (size_t i = 0; i < total; i++)
      {
        if (!done[i] && (pick < 0 || before(static_cast<int>(i), pick)))
        {
          pick = static_cast<int>(i);
        }
      }
    }
    done[pick] = true;
    picks.push_back(pick);
  }

  for (int pick : picks)
  {
    if (static_cast<size_t>(pick) < nodes.size())
    {
      result.order.push_back(nodes[pick].canvasId);
      continue;
    }
    const DeferredOreSegment& ore = oreSegments[pick - nodes.size()];
    if (!result.earlyOre.empty() &&
        result.earlyOre.back().before == result.order.size())
    {
      result.earlyOre.back().end = ore.end;
    }
    else
    {
      result.earlyOre.push_back({result.order.size(), ore.begin, ore.end});
    }
  }
  if (earlyOre > 0)
  {
    result.lateOreBegin = oreSegments[earlyOre - 1].end;
  }
  return result;
}

} // namespace rive::cmd
