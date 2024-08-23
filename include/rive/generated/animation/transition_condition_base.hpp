#ifndef _RIVE_TRANSITION_CONDITION_BASE_HPP_
#define _RIVE_TRANSITION_CONDITION_BASE_HPP_
#include "rive/core.hpp"
namespace rive
{
class TransitionConditionBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 476;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TransitionConditionBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    void copy(const TransitionConditionBase& object) {}

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override { return false; }

protected:
};
} // namespace rive

#endif