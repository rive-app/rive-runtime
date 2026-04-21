#ifndef _RIVE_SEMANTIC_TRAIT_HPP_
#define _RIVE_SEMANTIC_TRAIT_HPP_

#include "rive/generated/semantic/semantic_data_base.hpp"
#include <cstdint>

namespace rive
{
/// Bitmask constants for semantic trait flags.
///
/// Traits sit between Role and State: they declare what *capabilities*
/// a semantic node has. A state flag is only meaningful when its
/// corresponding trait is set.
///
/// Example: a button with the Expandable trait can be Expanded or
/// collapsed. Without the trait the Expanded state bit is ignored.
///
/// Stored in SemanticNode::traitFlags and transmitted in
/// SemanticsDiffNode::traitFlags. Platform accessibility layers use
/// traits to distinguish "property not applicable" from "property is
/// false" (the tristate problem).
///
/// Values are pulled from SemanticDataBase's generated `*Bitmask`
/// constants, which are derived from the authoritative JSON definitions.
/// Editing the bit layout means editing dev/defs/semantic/semantic_data.json
/// and regenerating — the enum values will track automatically.
enum class SemanticTrait : uint32_t
{
    None = 0,

    /// Node can be expanded / collapsed (disclosure).
    Expandable = SemanticDataBase::isExpandableBitmask,

    /// Node can be selected / unselected.
    Selectable = SemanticDataBase::isSelectableBitmask,

    /// Node can be checked / unchecked (checkbox, radio).
    Checkable = SemanticDataBase::isCheckableBitmask,

    /// Node can be toggled on / off (switch).
    Toggleable = SemanticDataBase::isToggleableBitmask,

    /// Node can be marked as required / optional (form field).
    Requirable = SemanticDataBase::isRequirableBitmask,

    /// Node has an enabled / disabled concept.
    /// Without this trait, the Disabled state bit is ignored and the
    /// platform sees the node as neither enabled nor disabled.
    Enablable = SemanticDataBase::isEnablableBitmask,

    /// Node can receive focus. Auto-set by the runtime when a sibling
    /// FocusData exists.
    Focusable = SemanticDataBase::isFocusableBitmask,
};

constexpr SemanticTrait operator|(SemanticTrait a, SemanticTrait b)
{
    return static_cast<SemanticTrait>(static_cast<uint32_t>(a) |
                                      static_cast<uint32_t>(b));
}

constexpr SemanticTrait operator&(SemanticTrait a, SemanticTrait b)
{
    return static_cast<SemanticTrait>(static_cast<uint32_t>(a) &
                                      static_cast<uint32_t>(b));
}

constexpr bool hasSemanticTrait(uint32_t flags, SemanticTrait trait)
{
    return (flags & static_cast<uint32_t>(trait)) != 0;
}
} // namespace rive

#endif
