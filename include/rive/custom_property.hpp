#ifndef _RIVE_CUSTOM_PROPERTY_HPP_
#define _RIVE_CUSTOM_PROPERTY_HPP_
#include "rive/generated/custom_property_base.hpp"
#include <stdio.h>
namespace rive
{
// What scripts see as a property's kind.
enum class CustomPropertyKind : uint32_t
{
    number,
    boolean,
    string,
    color,
    enumeration,
    trigger,
};

class CustomProperty : public CustomPropertyBase
{
public:
    static constexpr uint32_t noNameId = (uint32_t)-1;

    StatusCode onAddedClean(CoreContext* context) override;

    // Script inputs keep their name and no id, so they never tag.
    bool tagsParent() const { return nameId() != noNameId; }
    // The child as a property tagging its parent, null for anything else.
    static CustomProperty* tagging(Component* child)
    {
        return child->is<CustomProperty>() &&
                       child->as<CustomProperty>()->tagsParent()
                   ? child->as<CustomProperty>()
                   : nullptr;
    }
    CustomPropertyKind kind() const;
};
} // namespace rive

#endif
