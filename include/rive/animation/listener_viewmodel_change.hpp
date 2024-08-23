#ifndef _RIVE_LISTENER_VIEW_MODEL_CHANGE_HPP_
#define _RIVE_LISTENER_VIEW_MODEL_CHANGE_HPP_
#include "rive/generated/animation/listener_viewmodel_change_base.hpp"
#include "rive/data_bind/bindable_property.hpp"
#include <stdio.h>
namespace rive
{
class ListenerViewModelChange : public ListenerViewModelChangeBase
{
public:
    void perform(StateMachineInstance* stateMachineInstance,
                 Vec2D position,
                 Vec2D previousPosition) const override;
    StatusCode import(ImportStack& importStack) override;

private:
    BindableProperty* m_bindableProperty;
};
} // namespace rive

#endif