/*
 * CoreHandle — generational slot handle used by the editor-side object
 * model. Ships in the runtime repo but is inert without editor_native.
 *
 * Stored on runtime Core types inside `#ifdef WITH_RIVE_EDITOR` blocks in
 * place of raw pointers. A handle is just two uint32s (index + generation)
 * and has no behavior here — all resolution goes through the ObjectArena
 * abstract interface, whose concrete implementation lives in the
 * proprietary editor_native package.
 *
 * Design rationale (see dev/editor_native_plan.md, "Object Lifecycle"):
 *   - No shared_ptr / rcp — zero refcount overhead for the runtime
 *   - No dangling pointers — stale handles resolve to nullptr
 *   - No reference cycles — handles are just integers
 *
 * Downstream runtime consumers never need this file; it becomes a no-op
 * header when WITH_RIVE_EDITOR is not defined.
 */

#ifndef _RIVE_EDITOR_CORE_HANDLE_HPP_
#define _RIVE_EDITOR_CORE_HANDLE_HPP_

#include <cstdint>

namespace rive
{

struct CoreHandle
{
    static constexpr uint32_t kNullIndex = UINT32_MAX;

    uint32_t index = kNullIndex;
    uint32_t generation = 0;

    bool isNull() const { return index == kNullIndex; }

    friend bool operator==(const CoreHandle& a, const CoreHandle& b)
    {
        return a.index == b.index && a.generation == b.generation;
    }
    friend bool operator!=(const CoreHandle& a, const CoreHandle& b)
    {
        return !(a == b);
    }
};

} // namespace rive

#endif
