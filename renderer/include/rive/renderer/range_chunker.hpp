/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/gpu.hpp"
#include "rive/renderer/next_ptr_range.hpp"
#include "rive/renderer/stack_vector.hpp"

#include <stdint.h>
#include <algorithm>
#include <cassert>

namespace rive::gpu
{
// This class breaks large ranges up into bite-size chunks that can be
// accommodated by a single draw call (maxPerDrawCommand). This is needed for
// some Mali and PowerVR devices that crash when issuing draw commands with a
// large instance count.
class RangeChunker
{
public:
  struct Chunk
  {
    uint32_t count;
    uint32_t first;
  };

  struct Iterator
  {
  public:
    uint32_t currentChunkCount() const
    {
      return std::min(end - current, maxPerDrawCommand);
    }

    Chunk operator*() const { return {currentChunkCount(), current}; }

    Iterator& operator++()
    {
      current += currentChunkCount();
      return *this;
    }

    bool operator==(const Iterator& other) const
    {
      assert(end == other.end);
      assert(maxPerDrawCommand == other.maxPerDrawCommand);
      return current == other.current;
    }

    bool operator!=(const Iterator& other) const { return !(*this == other); }

    const uint32_t end;
    const uint32_t maxPerDrawCommand;
    uint32_t current;
  };

  RangeChunker(uint32_t count, uint32_t first, uint32_t maxPerDrawCommand)
      : m_first(first)
      , m_end(first + count)
      , m_maxPerDrawCommand(maxPerDrawCommand)
  {}

  Iterator begin() const { return {m_end, m_maxPerDrawCommand, m_first}; }

  Iterator end() const { return {m_end, m_maxPerDrawCommand, m_end}; }

private:
  const uint32_t m_first;
  const uint32_t m_end;
  const uint32_t m_maxPerDrawCommand;
};

// This class walks the low-level indexed-draw commands needed by a depthStencil
// batch and provides the direct arguments for them.
//
// depthStencil draws use repeating index patterns instead of instancing. Index
// patterns are mapped to real patches via the "baseVertex" parameter. Once the
// GPU adds in the baseVertex, the shader is able to derives all of its
// attributes from gl_VertexID.
//
// Since the index buffer only has a finite number of repetitions built in,
// large draws may also need to be broken up into chunks.
//
// Additionally, depthAAStrokes render in (2 passes) x (N chunks): a depth-only
// pass followed by color. We select the pass by vertex flags rather than
// pipeline state.
//
// A batch may hold a list of ElementRanges (e.g., hairlines merged across draw
// groups). Each range runs all of its subpasses before the next range begins,
// so overlapping ranges still execute their subpasses in order.
class DSIndexRangeChunker
{
public:
  struct Draw
  {
    uint32_t indexCount;
    int32_t baseVertex; // Also contains built-in vertex flags.
  };

  class Iterator
  {
  public:
    Iterator(
        const DSIndexRangeChunker* chunker,
        const DrawBatch::ElementRange* range,
        uint32_t pass,
        uint32_t basePatch)
        : m_chunker(chunker)
        , m_range(range)
        , m_pass(pass)
        , m_basePatch(basePatch)
    {
      skipEmptyRanges();
    }

    Draw operator*() const
    {
      return {
          chunkPatchCount() * m_chunker->m_indexCountPerPatch,
          (static_cast<int32_t>(m_range->base) | m_chunker->m_vertexFlags |
           m_chunker->m_passFlags[m_pass]) +
              (static_cast<int32_t>(m_basePatch)
               << m_chunker->m_patchStrideLog2),
      };
    }

    Iterator& operator++()
    {
      m_basePatch += chunkPatchCount();
      if (m_basePatch == m_range->count)
      {
        m_basePatch = 0;
        ++m_pass;
        if (m_pass == m_chunker->m_passFlags.size())
        {
          m_pass = 0;
          m_range = m_range->next;
          skipEmptyRanges();
        }
      }
      return *this;
    }

    bool operator==(const Iterator& other) const
    {
      assert(m_chunker == other.m_chunker);
      return m_range == other.m_range && m_pass == other.m_pass &&
             m_basePatch == other.m_basePatch;
    }

    bool operator!=(const Iterator& other) const { return !(*this == other); }

  private:
    void skipEmptyRanges()
    {
      while (m_range != m_chunker->m_endRange && m_range->count == 0)
        m_range = m_range->next;
    }

    uint32_t chunkPatchCount() const
    {
      return std::min(
          m_range->count - m_basePatch,
          m_chunker->m_maxPatchesPerDraw);
    }

    const DSIndexRangeChunker* m_chunker;
    const DrawBatch::ElementRange* m_range;
    uint32_t m_pass;
    uint32_t m_basePatch;
  };

  // Walks every ElementRange in the batch, including subpasses if >1.
  static DSIndexRangeChunker drawsForBatch(const DrawBatch& batch)
  {
    // Dynamic fills need to call drawsForDynamicFillSubpass() because they
    // update graphics state between subpasses.
    assert(!drawTypeHasPipelineDynamicState(batch.drawType));
    return DSIndexRangeChunker(batch, batch.elementRange, nullptr, 0);
  }

  // Walks one subpass of one of a dynamic fill's ElementRanges.
  static DSIndexRangeChunker drawsForDynamicFillSubpass(
      const DrawBatch& batch,
      const DrawBatch::ElementRange& range,
      const PipelineState& subpassState)
  {
    assert(drawTypeHasPipelineDynamicState(batch.drawType));
    // Always send DSVertexFlag_DisableColorWrite for color-masked subpasses,
    // even when the API also masks color. The flag lets the vertex shader skip
    // paint fetches.
    return DSIndexRangeChunker(
        batch,
        range,
        range.next,
        subpassState.colorWriteEnabled ? 0 : DSVertexFlag_DisableColorWrite);
  }

private:
  DSIndexRangeChunker(
      const DrawBatch& batch,
      const DrawBatch::ElementRange& firstRange,
      const DrawBatch::ElementRange* endRange,
      int32_t vertexFlags)
      : m_firstRange(&firstRange)
      , m_endRange(endRange)
      , m_maxPatchesPerDraw(dsPatchMaxReps(batch.drawType))
      , m_indexCountPerPatch(batch.indexCountPerInstance)
      , m_patchStrideLog2(dsPatchStrideLog2(batch.drawType))
      , m_vertexFlags(vertexFlags)
  {
    // Only strokes, hairlines, and dynamic-state fills merge into lists of
    // ranges.
    assert(
        batch.elementRange.next == nullptr ||
        drawTypeIsDepthStencilPolarStroke(batch.drawType) ||
        batch.drawType == DrawType::depthAAOuterHairline ||
        drawTypeHasPipelineDynamicState(batch.drawType));
    if (drawTypeIsDepthAAStroke(batch.drawType))
    {
      // depthAAstrokes (and hairlines) render in two passes: a depth-only
      // pass followed by color. We select the pass by vertex flags rather
      // than pipeline state.
      m_passFlags.push_back(
          DSVertexFlag_StrokeDepthPass | DSVertexFlag_DisableColorWrite);
    }
    m_passFlags.push_back(0);
#ifndef NDEBUG
    for (const DrawBatch::ElementRange& r :
         NextPtrRange<const DrawBatch::ElementRange>(m_firstRange, m_endRange))
    {
      // Make sure the vertex indices won't stomp on the flags.
      assert(
          (r.base & ((1 << DSVertexFlagsShift) - 1)) +
              (r.count << m_patchStrideLog2) <=
          1 << DSVertexFlagsShift);
    }
#endif
  }

public:
  Iterator begin() const { return {this, m_firstRange, 0, 0}; }

  Iterator end() const { return {this, m_endRange, 0, 0}; }

private:
  const DrawBatch::ElementRange* const m_firstRange;
  const DrawBatch::ElementRange* const m_endRange;
  const uint32_t m_maxPatchesPerDraw;
  const uint32_t m_indexCountPerPatch;
  const uint32_t m_patchStrideLog2;
  const int32_t m_vertexFlags;
  StackVector<int32_t, 2> m_passFlags;
};
} // namespace rive::gpu
