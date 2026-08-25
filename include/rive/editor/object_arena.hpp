/*
 * ObjectArena — abstract interface for the editor's slot-based allocator
 * with generational handles.
 *
 * Ships in the runtime repo so runtime headers (component.hpp, etc.) can
 * reference `ObjectArena*` members inside `#ifdef WITH_RIVE_EDITOR`
 * blocks. The concrete implementation (ObjectArenaImpl) lives in the
 * proprietary editor_native package — downstream runtime consumers never
 * instantiate or call it.
 *
 * Implementations must be thread-compatible with the Rive Thread model:
 * mutations from a single consumer thread, reads may be serialized
 * through that same thread. See dev/editor_native_plan.md ("Rive Thread:
 * Decoupled Rendering") for the full threading contract.
 */

#ifndef _RIVE_EDITOR_OBJECT_ARENA_HPP_
#define _RIVE_EDITOR_OBJECT_ARENA_HPP_

#include "rive/editor/core_handle.hpp"

namespace rive
{

class Core;

class ObjectArena
{
public:
    virtual ~ObjectArena() = default;

    // Resolve a handle to its live Core object, or nullptr if the handle
    // is stale (slot freed, generation bumped), out of range, or never
    // allocated.
    virtual Core* resolve(CoreHandle handle) const = 0;

    // Find the current handle for an object that this arena owns.
    // Returns a null handle if the object isn't in this arena.
    virtual CoreHandle handleOf(const Core* object) const = 0;
};

} // namespace rive

#endif
