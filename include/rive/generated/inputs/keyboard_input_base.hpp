#ifndef _RIVE_KEYBOARD_INPUT_BASE_HPP_
#define _RIVE_KEYBOARD_INPUT_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/inputs/user_input.hpp"
namespace rive
{
class KeyboardInputBase : public UserInput
{
protected:
    typedef UserInput Super;

public:
    static const uint16_t typeKey = 664;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case KeyboardInputBase::typeKey:
            case UserInputBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t keyTypePropertyKey = 971;
    static const uint16_t keyPhasePropertyKey = 972;
    static const uint16_t modifiersPropertyKey = 973;

protected:
    uint32_t m_KeyType = -1;
    uint32_t m_KeyPhase = 0;
    uint32_t m_Modifiers = 0;

public:
    inline uint32_t keyType() const { return m_KeyType; }
    void keyType(uint32_t value)
    {
        if (m_KeyType == value)
        {
            return;
        }
        m_KeyType = value;
        keyTypeChanged();
    }

    inline uint32_t keyPhase() const { return m_KeyPhase; }
    void keyPhase(uint32_t value)
    {
        if (m_KeyPhase == value)
        {
            return;
        }
        m_KeyPhase = value;
        keyPhaseChanged();
    }

    inline uint32_t modifiers() const { return m_Modifiers; }
    void modifiers(uint32_t value)
    {
        if (m_Modifiers == value)
        {
            return;
        }
        m_Modifiers = value;
        modifiersChanged();
    }

    Core* clone() const override;
    void copy(const KeyboardInputBase& object)
    {
        m_KeyType = object.m_KeyType;
        m_KeyPhase = object.m_KeyPhase;
        m_Modifiers = object.m_Modifiers;
        UserInput::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case keyTypePropertyKey:
                m_KeyType = CoreUintType::deserialize(reader);
                return true;
            case keyPhasePropertyKey:
                m_KeyPhase = CoreUintType::deserialize(reader);
                return true;
            case modifiersPropertyKey:
                m_Modifiers = CoreUintType::deserialize(reader);
                return true;
        }
        return UserInput::deserialize(propertyKey, reader);
    }

protected:
    virtual void keyTypeChanged() {}
    virtual void keyPhaseChanged() {}
    virtual void modifiersChanged() {}
};
} // namespace rive

#endif