#ifndef _RIVE_FOCUS_ACTION_TRAVERSAL_BASE_HPP_
#define _RIVE_FOCUS_ACTION_TRAVERSAL_BASE_HPP_
#include "rive/animation/focus_action.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class FocusActionTraversalBase : public FocusAction
{
protected:
    typedef FocusAction Super;

public:
    static const uint16_t typeKey = 672;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case FocusActionTraversalBase::typeKey:
            case FocusActionBase::typeKey:
            case ListenerActionBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t traversalKindPropertyKey = 1011;

protected:
    uint32_t m_TraversalKind = 0;

public:
    inline uint32_t traversalKind() const { return m_TraversalKind; }
    void traversalKind(uint32_t value)
    {
        if (m_TraversalKind == value)
        {
            return;
        }
        m_TraversalKind = value;
        traversalKindChanged();
    }

    Core* clone() const override;
    void copy(const FocusActionTraversalBase& object)
    {
        m_TraversalKind = object.m_TraversalKind;
        FocusAction::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case traversalKindPropertyKey:
                m_TraversalKind = CoreUintType::deserialize(reader);
                return true;
        }
        return FocusAction::deserialize(propertyKey, reader);
    }

protected:
    virtual void traversalKindChanged() {}
};
} // namespace rive

#endif