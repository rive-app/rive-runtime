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
/// Check state is the exception: it is a two-bit *field* rather than a flag,
/// because a checkbox is tri-state. It is not a member of this enum — read it
/// with checkStateOf() and switch on SemanticCheckState.
enum class SemanticState : uint32_t
{
    None = 0,

    // Trait-gated states (only meaningful when trait is active)
    Expanded = SemanticDataBase::isExpandedBitmask, // requires Expandable
    Selected = SemanticDataBase::isSelectedBitmask, // requires Selectable
    // Checked is deliberately absent: it is a field, not a flag. See
    // SemanticCheckState / checkStateOf() below.
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

/// The tri-state of a checkable node. Occupies two bits of stateFlags at
/// SemanticDataBase::isCheckedBitOffset, so it is read as a value rather than
/// tested as a flag. Only meaningful when the Checkable trait is set.
enum class SemanticCheckState : uint8_t
{
    Unchecked = 0,
    Checked = 1,
    Mixed = 2,
};

/// Decodes the check field out of [flags]. The field is two bits so it can
/// physically hold a fourth value; it has no meaning and is read as Mixed
/// rather than falling through to an unnamed state.
constexpr SemanticCheckState checkStateOf(uint32_t flags)
{
    const uint32_t value = (flags & SemanticDataBase::isCheckedFieldMask) >>
                           SemanticDataBase::isCheckedBitOffset;
    return value >= 2 ? SemanticCheckState::Mixed
                      : static_cast<SemanticCheckState>(value);
}
} // namespace rive

#endif
