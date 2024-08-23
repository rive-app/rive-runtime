#ifndef _RIVE_ENTRY_STATE_BASE_HPP_
#define _RIVE_ENTRY_STATE_BASE_HPP_
#include "rive/animation/layer_state.hpp"
namespace rive
{
class EntryStateBase : public LayerState
{
protected:
    typedef LayerState Super;

public:
    static const uint16_t typeKey = 63;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case EntryStateBase::typeKey:
            case LayerStateBase::typeKey:
            case StateMachineLayerComponentBase::typeKey:
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