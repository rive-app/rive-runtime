/*
 * Copyright 2026 Rive
 */

#pragma once

#include <cassert>
#ifndef NDEBUG
#include <thread>
#endif

// A host may define `check` as a macro -- Unreal does -- which would eat the
// RecordingThread calls here. Keep the name resolving to the method, and hand
// the macro back untouched at the end of the header.
#ifdef check
#pragma push_macro("check")
#undef check
#define RIVE_RECORDING_THREAD_RESTORE_CHECK
#endif

// Debug only thread affinity for the deferred producer streams.
//
// Recording is single threaded by construction: every widget records
// synchronously from the Dart UI isolate. The producer state that assumption
// buys -- the command byte vectors, the id free list -- carries no mutex and
// no atomic, so a second recorder appending concurrently would corrupt them
// with no diagnostic at all. This makes that violation loud.
//
// The off thread work the deferred design does allow never touches producer
// state directly, and must not be checked: cross thread destroys queue under
// OreCommandBuffer's destroy mutex, late destructors serialize on
// recorderRegistryMutex, the producer/consumer handoff has DeferredInFlight's,
// and session teardown drains the destroy queue from whichever thread the host
// posted the delete to.
namespace rive::cmd
{

// Unbound until a recorder claims it, so a command buffer that is not a
// deferred producer -- an inline pass buffer, a backend's pending frame --
// keeps its existing freedom to live wherever its owner does.
class RecordingThread
{
public:
#ifdef NDEBUG
    void bind() {}
    void check() const {}
#else
    void bind() { m_id = std::this_thread::get_id(); }

    void check() const
    {
        assert(
            (m_id == std::thread::id() || std::this_thread::get_id() == m_id) &&
            "deferred recording is single threaded: this stream, its id "
            "allocator, and its keep alive table are all unlocked");
    }

private:
    std::thread::id m_id;
#endif
};

} // namespace rive::cmd
#ifdef RIVE_RECORDING_THREAD_RESTORE_CHECK
#pragma pop_macro("check")
#undef RIVE_RECORDING_THREAD_RESTORE_CHECK
#endif
