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
    ~ListenerViewModelChange();
    void perform(StateMachineInstance* stateMachineInstance,
                 const ListenerInvocation& invocation) const override;
    StatusCode import(ImportStack& importStack) override;

private:
    // Writes the value selected by inputValue() from [invocation] into
    // [bindableInstance]. Returns false when the invocation does not carry
    // that value or the bindable's type does not match it.
    bool applyInputValue(BindableProperty* bindableInstance,
                         const ListenerInvocation& invocation) const;

    BindableProperty* m_bindableProperty = nullptr;
};
} // namespace rive

#endif