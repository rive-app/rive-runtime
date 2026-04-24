#ifndef _RIVE_LISTENER_INPUT_TYPE_SEMANTIC_BASE_HPP_
#define _RIVE_LISTENER_INPUT_TYPE_SEMANTIC_BASE_HPP_
#include "rive/animation/listener_types/listener_input_type.hpp"
namespace rive
{
class ListenerInputTypeSemanticBase : public ListenerInputType
{
protected:
    typedef ListenerInputType Super;

public:
    static const uint16_t typeKey = 669;

    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ListenerInputTypeSemanticBase::typeKey:
            case ListenerInputTypeBase::typeKey:
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
