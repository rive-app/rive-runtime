#ifndef _RIVE_TRANSITION_COMPARATOR_BASE_HPP_
#define _RIVE_TRANSITION_COMPARATOR_BASE_HPP_
#include "rive/core.hpp"
namespace rive
{
class TransitionComparatorBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 477;

    uint16_t coreType() const override { return typeKey; }

    void copy(const TransitionComparatorBase& object)
    {
        RIVE_EDITOR_COPY_VALIDATED(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        return false;
    }
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/transition_comparator_ext.inl"
#endif
};
} // namespace rive

#endif