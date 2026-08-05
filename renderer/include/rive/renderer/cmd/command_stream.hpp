/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/span.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>
#include <vector>

// Wire primitives shared by the 2D and Ore command streams: a flat pointer
// free byte stream plus a blob arena, and its bounds checked reader. The
// vocabularies differ per stream; the byte layout rules live only here.
namespace rive::cmd
{

// Per site stderr throttle so a per frame failure cannot flood the log.
#define RIVE_WARN_THROTTLED(...)                                               \
    do                                                                         \
    {                                                                          \
        static int riveWarnCount = 0;                                          \
        if ((riveWarnCount++ % 120) == 0)                                      \
        {                                                                      \
            fprintf(stderr, __VA_ARGS__);                                      \
        }                                                                      \
    } while (0)

// A stream is written from its start and read from its start: every stream is
// reset each frame, so a blob offset is just an index into the arena.
class CommandByteStream
{
public:
    // Copy bytes into the blob arena and return the offset for the command
    // POD. Null data with a nonzero size would record unappended bytes, so it
    // asserts. Offsets are 64 bit because the wire PODs carrying them are.
    uint64_t appendBlob(const void* data, uint32_t size)
    {
        assert(data != nullptr || size == 0);
        // Blobs start 8 aligned so typed reads of the contents are aligned.
        m_blobs.resize((m_blobs.size() + 7) & ~size_t(7), 0);
        uint64_t offset = m_blobs.size();
        if (size != 0 && data != nullptr)
        {
            appendBytes(m_blobs, data, size);
        }
        return offset;
    }

    bool empty() const { return m_commands.empty(); }

    Span<const uint8_t> commandBytes() const
    {
        return Span<const uint8_t>(m_commands.data(), m_commands.size());
    }
    Span<const uint8_t> blobBytes() const
    {
        return Span<const uint8_t>(m_blobs.data(), m_blobs.size());
    }

protected:
    // Appends are the hottest thing recording does, and vector::insert drags
    // its general mid-range machinery through every one of them.
    static void appendBytes(std::vector<uint8_t>& dst,
                            const void* data,
                            size_t size)
    {
        size_t end = dst.size();
        dst.resize(end + size);
        memcpy(dst.data() + end, data, size);
    }

    void writeRaw(const void* data, size_t size)
    {
        appendBytes(m_commands, data, size);
    }

    void clearBytes() // keeps capacity
    {
        m_commands.clear();
        m_blobs.clear();
    }

    std::vector<uint8_t> m_commands;
    std::vector<uint8_t> m_blobs;
};

// Sequential bounds checked reader: an overrunning read latches and ends the
// walk, an out of range blobAt returns an empty span.
template <typename Opcode> class CommandReader
{
public:
    CommandReader(Span<const uint8_t> commands, Span<const uint8_t> blobs) :
        m_commands(commands), m_blobs(blobs)
    {}

    bool next(Opcode& outType)
    {
        size_t remaining = m_commands.size() - m_pos;
        if (m_overrun || remaining < sizeof(Opcode))
        {
            // Leftover bytes too short for an opcode are a truncated stream,
            // not a clean end.
            if (remaining != 0)
            {
                m_overrun = true;
            }
            return false;
        }
        std::memcpy(&outType, m_commands.data() + m_pos, sizeof(Opcode));
        m_pos += sizeof(Opcode);
        return true;
    }

    template <typename POD> POD read()
    {
        static_assert(std::is_trivially_copyable<POD>::value);
        POD pod{};
        if (m_commands.size() - m_pos < sizeof(POD))
        {
            m_overrun = true;
            return pod;
        }
        std::memcpy(&pod, m_commands.data() + m_pos, sizeof(POD));
        m_pos += sizeof(POD);
        return pod;
    }

    // Advance past a payload without reading it (filtered walks).
    void skip(size_t bytes)
    {
        if (m_commands.size() - m_pos < bytes)
        {
            m_overrun = true;
            return;
        }
        m_pos += bytes;
    }

    Span<const uint8_t> blobAt(uint64_t offset, uint32_t size) const
    {
        // Compare in 64 bit before any narrowing, size_t is 32 bit on wasm.
        if (offset + size > static_cast<uint64_t>(m_blobs.size()))
        {
            return Span<const uint8_t>(nullptr, 0);
        }
        return Span<const uint8_t>(m_blobs.data() + static_cast<size_t>(offset),
                                   size);
    }

    size_t position() const { return m_pos; }
    bool overrun() const { return m_overrun; }

private:
    Span<const uint8_t> m_commands;
    Span<const uint8_t> m_blobs;
    size_t m_pos = 0;
    bool m_overrun = false;
};

} // namespace rive::cmd
