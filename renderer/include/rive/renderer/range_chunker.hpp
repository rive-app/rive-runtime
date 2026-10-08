/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/gpu.hpp"
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

        bool operator!=(const Iterator& other) const
        {
            return !(*this == other);
        }

        const uint32_t end;
        const uint32_t maxPerDrawCommand;
        uint32_t current;
    };

    RangeChunker(uint32_t count, uint32_t first, uint32_t maxPerDrawCommand) :
        m_first(first),
        m_end(first + count),
        m_maxPerDrawCommand(maxPerDrawCommand)
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
class DSIndexRangeChunker
{
public:
    struct Draw
    {
        uint32_t indexCount;
        int32_t baseVertex; // Also contains built-in vertex flags.
    };

    struct Iterator
    {
    public:
        Draw operator*() const
        {
            return {
                chunkPatchCount() * chunker->m_indexCountPerPatch,
                (chunker->m_baseVertex | chunker->m_passFlags[pass]) +
                    (static_cast<int32_t>(basePatch)
                     << chunker->m_patchStrideLog2),
            };
        }

        Iterator& operator++()
        {
            basePatch += chunkPatchCount();
            if (basePatch == chunker->m_patchCount)
            {
                ++pass;
                basePatch = 0;
            }
            return *this;
        }

        bool operator==(const Iterator& other) const
        {
            assert(chunker == other.chunker);
            return pass == other.pass && basePatch == other.basePatch;
        }

        bool operator!=(const Iterator& other) const
        {
            return !(*this == other);
        }

        const DSIndexRangeChunker* chunker;
        uint32_t pass;
        uint32_t basePatch;

    private:
        uint32_t chunkPatchCount() const
        {
            return std::min(chunker->m_patchCount - basePatch,
                            chunker->m_maxPatchesPerDraw);
        }
    };

    DSIndexRangeChunker(const DrawBatch& batch, int32_t vertexFlags = 0) :
        m_patchCount(batch.elementCount),
        m_maxPatchesPerDraw(dsPatchMaxReps(batch.drawType)),
        m_indexCountPerPatch(batch.indexCountPerInstance),
        m_patchStrideLog2(dsPatchStrideLog2(batch.drawType)),
        m_baseVertex(static_cast<int32_t>(batch.baseElement) | vertexFlags)
    {
        if (drawTypeIsDepthAAStroke(batch.drawType))
        {
            // depthAAstrokes (and hairlines) render in two passes: a depth-only
            // pass followed by color. We select the pass by vertex flags rather
            // than pipeline state.
            m_passFlags.push_back(DSVertexFlag_StrokeDepthPass |
                                  DSVertexFlag_DisableColorWrite);
        }
        m_passFlags.push_back(0);
        // Make sure the vertex indices won't stomp on the flags.
        assert((batch.baseElement & ((1 << DSVertexFlagsShift) - 1)) +
                   (batch.elementCount << m_patchStrideLog2) <=
               1 << DSVertexFlagsShift);
    }

    Iterator begin() const
    {
        return m_patchCount == 0 ? end() : Iterator{this, 0, 0};
    }

    Iterator end() const { return {this, m_passFlags.size(), 0}; }

private:
    const uint32_t m_patchCount;
    const uint32_t m_maxPatchesPerDraw;
    const uint32_t m_indexCountPerPatch;
    const uint32_t m_patchStrideLog2;
    const int32_t m_baseVertex;
    StackVector<int32_t, 2> m_passFlags;
};
} // namespace rive::gpu
