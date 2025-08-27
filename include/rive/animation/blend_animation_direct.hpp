#ifndef _RIVE_BLEND_ANIMATION_DIRECT_HPP_
#define _RIVE_BLEND_ANIMATION_DIRECT_HPP_
#include "rive/generated/animation/blend_animation_direct_base.hpp"
#include <stdio.h>
namespace rive
{
class BindableProperty;
enum class DirectBlendSource : unsigned int
{
    inputId = 0,
    mixValue = 1,
    dataBindId = 2,
};

class BlendAnimationDirect : public BlendAnimationDirectBase
{
public:
    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode onAddedClean(CoreContext* context) override;
    StatusCode import(ImportStack& importStack) override;
    void bindableProperty(BindableProperty* value)
    {
        m_bindableProperty = value;
    };
    BindableProperty* bindableProperty() const { return m_bindableProperty; };

private:
    BindableProperty* m_bindableProperty;
};

} // namespace rive

#endif