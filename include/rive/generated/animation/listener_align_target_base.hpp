#ifndef _RIVE_LISTENER_ALIGN_TARGET_BASE_HPP_
#define _RIVE_LISTENER_ALIGN_TARGET_BASE_HPP_
#include "rive/animation/listener_action.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class ListenerAlignTargetBase : public ListenerAction
{
protected:
    typedef ListenerAction Super;

public:
    static const uint16_t typeKey = 126;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ListenerAlignTargetBase::typeKey:
            case ListenerActionBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t targetIdPropertyKey = 240;

private:
    uint32_t m_TargetId = 0;

public:
    inline uint32_t targetId() const { return m_TargetId; }
    void targetId(uint32_t value)
    {
        if (m_TargetId == value)
        {
            return;
        }
        m_TargetId = value;
        targetIdChanged();
    }

    Core* clone() const override;
    void copy(const ListenerAlignTargetBase& object)
    {
        m_TargetId = object.m_TargetId;
        ListenerAction::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case targetIdPropertyKey:
                m_TargetId = CoreUintType::deserialize(reader);
                return true;
        }
        return ListenerAction::deserialize(propertyKey, reader);
    }

protected:
    virtual void targetIdChanged() {}
};
} // namespace rive

#endif