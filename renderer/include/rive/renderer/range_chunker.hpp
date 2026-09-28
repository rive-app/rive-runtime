/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/renderer/gpu.hpp"

#include <stdint.h>
#include <algorithm>
#include <cassert>

namespace rive::gpu
{
// This class breaks large ranges up into bite-size chunks that can be
// accommodated by a single draw call (maxPerDrawCommand). This is needed for
// depthStencil draws, which use repeating index patterns instead of instancing,
// and for some Mali and PowerVR devices that crash when issuing draw commands
// with a large instance count.
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

// depthStencil draws use repeating index patterns instead of instancing. Index
// patterns are mapped to real patches via the "baseVertex" parameter. Once the
// GPU adds in the baseVertex, the shader is able to derives all of its
// attributes from gl_VertexID.
//
// Since the index buffer only has a finite number of repetitions built in,
// large draws may also need to be broken up into chunks.
//
// This class walks the draws needed by a depthStencil batch and provides the
// direct arguments for the raw indexed-draw commands.
class DSIndexRangeChunker
{
public:
    struct Draw
    {
        uint32_t indexCount;
        int32_t baseVertex;
    };

    struct Iterator
    {
    public:
        Draw operator*() const
        {
            auto [patchCount, firstPatch] = *chunk;
            return {patchCount * indexCountPerPatch,
                    (static_cast<int32_t>(firstPatch) << patchStrideLog2) |
                        vertexFlags};
        }

        Iterator& operator++()
        {
            ++chunk;
            return *this;
        }

        bool operator==(const Iterator& other) const
        {
            assert(indexCountPerPatch == other.indexCountPerPatch);
            assert(patchStrideLog2 == other.patchStrideLog2);
            assert(vertexFlags == other.vertexFlags);
            return chunk == other.chunk;
        }

        bool operator!=(const Iterator& other) const
        {
            return !(*this == other);
        }

        const uint32_t indexCountPerPatch;
        const uint32_t patchStrideLog2;
        const int32_t vertexFlags;
        RangeChunker::Iterator chunk;
    };

    DSIndexRangeChunker(DrawType drawType,
                        uint32_t patchCount,
                        uint32_t firstPatch,
                        int32_t vertexFlags = 0) :
        DSIndexRangeChunker(drawTypeSubmitsOuterCubicPatches(drawType),
                            patchCount,
                            firstPatch,
                            vertexFlags)
    {}

    Iterator begin() const
    {
        return {m_indexCountPerPatch,
                m_patchStrideLog2,
                m_vertexFlags,
                m_chunker.begin()};
    }

    Iterator end() const
    {
        return {m_indexCountPerPatch,
                m_patchStrideLog2,
                m_vertexFlags,
                m_chunker.end()};
    }

private:
    DSIndexRangeChunker(bool outerCubic,
                        uint32_t patchCount,
                        uint32_t firstPatch,
                        int32_t vertexFlags) :
        m_chunker(patchCount, firstPatch, dsFillPatchMaxReps(outerCubic)),
        m_indexCountPerPatch(dsFillPatchIndexCount(outerCubic)),
        m_patchStrideLog2(outerCubic ? DSOuterCubicFillPatchStrideLog2
                                     : DSMidpointFanFillPatchStrideLog2),
        m_vertexFlags(outerCubic ? vertexFlags | DSFillVertexFlagOuterCubic
                                 : vertexFlags)
    {
        // Make sure the vertex indices won't stomp on the flags.
        assert((firstPatch + patchCount) << m_patchStrideLog2 <=
               1 << DSFillVertexFlagsShift);
    }

    const RangeChunker m_chunker;
    const uint32_t m_indexCountPerPatch;
    const uint32_t m_patchStrideLog2;
    const int32_t m_vertexFlags;
};
} // namespace rive::gpu
