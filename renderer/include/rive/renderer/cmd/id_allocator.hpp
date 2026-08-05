/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/cmd/handle_flags.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

// Dense reusable id allocation for the deferred resource tables. A free list
// carries each returned id's next generation, so stale commands are caught by
// generation mismatch on the consumer. An id retires when its generation would
// overflow. Lives in rive, not cmd, shared by the 2D and Ore layers.
namespace rive
{

template <typename Id> class IdAllocator
{
public:
    struct Allocation
    {
        Id id;
        uint32_t generation;
    };

    // Reuse a returned id (generation already bumped) or mint a fresh one at 0.
    Allocation alloc()
    {
        if (!m_free.empty())
        {
            Allocation a = m_free.back();
            m_free.pop_back();
            return a;
        }
        // The top bit is reserved for flagged foreign ids, so a minted id
        // must never reach it.
        assert(m_next < cmd::kHandleForeignFlag);
        return {static_cast<Id>(m_next++), 0u};
    }

    // The caller passes the generation it held, so the allocator needs no per
    // id state. Retire the id if the next generation would overflow.
    void release(Id id, uint32_t generation)
    {
        if (generation != 0xffffffffu)
        {
            m_free.push_back({id, generation + 1u});
        }
    }

private:
    std::vector<Allocation> m_free;
    uint32_t m_next = 0; // high-water for fresh ids
};

} // namespace rive
