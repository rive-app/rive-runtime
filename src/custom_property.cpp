#include "rive/custom_property.hpp"
#include "rive/drawable.hpp"
#include "rive/generated/custom_property_boolean_base.hpp"
#include "rive/generated/custom_property_color_base.hpp"
#include "rive/generated/custom_property_enum_base.hpp"
#include "rive/generated/custom_property_string_base.hpp"
#include "rive/generated/custom_property_trigger_base.hpp"

using namespace rive;

StatusCode CustomProperty::onAddedClean(CoreContext* context)
{
    if (tagsParent() && parent() != nullptr && parent()->is<Drawable>())
    {
        parent()->as<Drawable>()->markHasCustomProperties();
    }
    return Super::onAddedClean(context);
}

CustomPropertyKind CustomProperty::kind() const
{
    switch (coreType())
    {
        case CustomPropertyBooleanBase::typeKey:
            return CustomPropertyKind::boolean;
        case CustomPropertyStringBase::typeKey:
            return CustomPropertyKind::string;
        case CustomPropertyColorBase::typeKey:
            return CustomPropertyKind::color;
        case CustomPropertyEnumBase::typeKey:
            return CustomPropertyKind::enumeration;
        case CustomPropertyTriggerBase::typeKey:
            return CustomPropertyKind::trigger;
        default:
            return CustomPropertyKind::number;
    }
}
