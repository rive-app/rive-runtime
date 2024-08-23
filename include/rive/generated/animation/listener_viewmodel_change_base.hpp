#ifndef _RIVE_LISTENER_VIEW_MODEL_CHANGE_BASE_HPP_
#define _RIVE_LISTENER_VIEW_MODEL_CHANGE_BASE_HPP_
#include "rive/animation/listener_action.hpp"
namespace rive
{
class ListenerViewModelChangeBase : public ListenerAction
{
protected:
    typedef ListenerAction Super;

public:
    static const uint16_t typeKey = 487;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ListenerViewModelChangeBase::typeKey:
            case ListenerActionBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;

protected:
};
} // namespace rive

#endif