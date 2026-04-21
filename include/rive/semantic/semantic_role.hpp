#ifndef _RIVE_SEMANTIC_ROLE_HPP_
#define _RIVE_SEMANTIC_ROLE_HPP_

#include <cstdint>

namespace rive
{
enum class SemanticRole : uint8_t
{
    none = 0,

    // Actions
    button = 1,
    link = 2,

    // Controls
    checkbox = 3,
    switchControl = 4,
    slider = 5,
    textField = 6,

    // Content
    text = 7,
    image = 8,

    // Structure
    group = 9,
    list = 10,
    listItem = 11,

    // Navigation
    tab = 12,
    tabList = 13,

    // Phase 2
    dialog = 14,
    alertDialog = 15,
    radioGroup = 16,
    radioButton = 17,
};

/// Returns true for roles that derive their accessible label from child
/// semantic nodes when no explicit label is authored. These roles also
/// flatten (absorb) the children that contributed to the derived label.
/// Note: textField is excluded — it requires an explicit label or external
/// association per accessibility guidelines.
inline bool isInteractiveRole(SemanticRole role)
{
    switch (role)
    {
        case SemanticRole::button:
        case SemanticRole::link:
        case SemanticRole::checkbox:
        case SemanticRole::switchControl:
        case SemanticRole::slider:
        case SemanticRole::tab:
        case SemanticRole::listItem:
        case SemanticRole::radioButton:
            return true;
        default:
            return false;
    }
}

inline bool isInteractiveRole(uint32_t roleValue)
{
    return isInteractiveRole(static_cast<SemanticRole>(roleValue));
}
} // namespace rive

#endif
