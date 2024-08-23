#ifndef _RIVE_NESTED_INPUT_BASE_HPP_
#define _RIVE_NESTED_INPUT_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class NestedInputBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 121;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case NestedInputBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t inputIdPropertyKey = 237;

private:
    uint32_t m_InputId = -1;

public:
    inline uint32_t inputId() const { return m_InputId; }
    void inputId(uint32_t value)
    {
        if (m_InputId == value)
        {
            return;
        }
        m_InputId = value;
        inputIdChanged();
    }

    void copy(const NestedInputBase& object)
    {
        m_InputId = object.m_InputId;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case inputIdPropertyKey:
                m_InputId = CoreUintType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void inputIdChanged() {}
};
} // namespace rive

#endif