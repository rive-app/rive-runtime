#ifndef _RIVE_SEMANTIC_STATE_HPP_
#define _RIVE_SEMANTIC_STATE_HPP_

#include "rive/generated/semantic/semantic_data_base.hpp"
#include <cstdint>

namespace rive
{
/// Bitmask constants for semantic state flags.
/// These are stored in SemanticNode::stateFlags and transmitted in
/// SemanticsDiffNode::stateFlags. The platform accessibility layer
/// interprets them per-role.
///
/// Bits 0-7 are trait-gated: they are only meaningful when the
/// corresponding SemanticTrait is set on the node.
/// Bits 8-13 are non-trait states (binary, role-implied).
///
/// Values are pulled from SemanticDataBase's generated `*Bitmask`
/// constants, which are derived from the authoritative JSON definitions.
/// Editing the bit layout means editing dev/defs/semantic/semantic_data.json
/// and regenerating — the enum values will track automatically.
///
/// Checked/Mixed precedence: Checked and Mixed occupy two independent bits
/// on stateFlags. When both bits are set, Mixed wins — the node is reported
/// as indeterminate, not checked.  Platform adapters must
/// surface checked state as `!Mixed && Checked`.
enum class SemanticState : uint32_t
{
    None = 0,

    // Trait-gated states (only meaningful when trait is active)
    Expanded = SemanticDataBase::isExpandedBitmask, // requires Expandable
    Selected = SemanticDataBase::isSelectedBitmask, // requires Selectable
    Checked = SemanticDataBase::isCheckedBitmask,   // requires Checkable
    Mixed = SemanticDataBase::isMixedBitmask, // requires Checkable; wins over
                                              // Checked
    Toggled = SemanticDataBase::isToggledBitmask,   // requires Toggleable
    Required = SemanticDataBase::isRequiredBitmask, // requires Requirable
    Disabled = SemanticDataBase::isDisabledBitmask, // requires Enablable
    Focused = SemanticDataBase::isFocusedBitmask,   // requires Focusable

    // Non-trait states (binary, always applicable or role-implied)
    Hidden = SemanticDataBase::isHiddenBitmask,
    LiveRegion = SemanticDataBase::isLiveRegionBitmask,
    ReadOnly = SemanticDataBase::isReadOnlyBitmask,
    Modal = SemanticDataBase::isModalBitmask,
    Obscured = SemanticDataBase::isObscuredBitmask,
    Multiline = SemanticDataBase::isMultilineBitmask,
};

constexpr SemanticState operator|(SemanticState a, SemanticState b)
{
    return static_cast<SemanticState>(static_cast<uint32_t>(a) |
                                      static_cast<uint32_t>(b));
}

constexpr SemanticState operator&(SemanticState a, SemanticState b)
{
    return static_cast<SemanticState>(static_cast<uint32_t>(a) &
                                      static_cast<uint32_t>(b));
}

constexpr bool hasSemanticState(uint32_t flags, SemanticState flag)
{
    return (flags & static_cast<uint32_t>(flag)) != 0;
}
} // namespace rive

#endif
