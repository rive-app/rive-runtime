#ifndef _RIVE_BACKBOARD_HPP_
#define _RIVE_BACKBOARD_HPP_
#include "rive/generated/backboard_base.hpp"
namespace rive
{
class Backboard : public BackboardBase
{
#ifdef WITH_RIVE_EDITOR
public:
    // Editor-only. `hydrogenPanes` is `"runtime": false` + `"coop":
    // false` — it's pure in-memory editor state for the script editor
    // panel layout, never wired through coop or the `.riv` file. The
    // generator still emits it as an `encoded` pure virtual so the
    // class signature stays consistent; we satisfy the vtable with
    // no-op stubs.
    void decodeHydrogenPanes(Span<const uint8_t>) override {}
    void copyHydrogenPanes(const BackboardBase&) override {}
    // `stackedPanels` mirrors `hydrogenPanes` — `"runtime": false` +
    // `"coop": false`, pure in-memory editor state for the left-hand
    // stacked panel layout. Never reaches `.riv` or coop. Empty stubs
    // satisfy the vtable.
    void decodeStackedPanels(Span<const uint8_t>) override {}
    void copyStackedPanels(const BackboardBase&) override {}
#endif
};
} // namespace rive

#endif