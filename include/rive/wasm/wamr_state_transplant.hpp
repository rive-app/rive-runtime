#ifndef _RIVE_WAMR_STATE_TRANSPLANT_HPP_
#define _RIVE_WAMR_STATE_TRANSPLANT_HPP_

#ifdef WITH_RIVE_SCRIPTING_WASM

// The transplant is the tier ladder's frame-boundary half; it rides the
// same gate.
#include "rive/wasm/module_tier_ladder.hpp"

#include "wasm_export.h"
#include <string>

namespace rive
{

#ifdef RIVE_WASM_TIER_LADDER

// Copies live mutable state (memories, globals, table entries) from one
// instance of a module onto a fresh instance of the same module content in
// a different representation - the frame-boundary half of a tier swap.
// Both instances must come from the same wasm; the destination must not
// have run yet beyond instantiation. On failure the destination is
// abandoned by the caller and the source keeps running - transplant never
// touches the source.
bool wamrTransplantState(wasm_module_inst_t source,
                         wasm_module_inst_t destination,
                         std::string& error);

#else // RIVE_WASM_TIER_LADDER

// Only reachable through the disabled ladder's upgrade path.
inline bool wamrTransplantState(wasm_module_inst_t,
                                wasm_module_inst_t,
                                std::string& error)
{
    error = "tier transplant not built";
    return false;
}

#endif // RIVE_WASM_TIER_LADDER

} // namespace rive

#endif // WITH_RIVE_SCRIPTING_WASM
#endif
