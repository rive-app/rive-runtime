#ifndef _RIVE_GAMEPAD_INPUT_BASE_HPP_
#define _RIVE_GAMEPAD_INPUT_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/inputs/user_input.hpp"
namespace rive
{
class GamepadInputBase : public UserInput
{
protected:
    typedef UserInput Super;

public:
    static const uint16_t typeKey = 974;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case GamepadInputBase::typeKey:
            case UserInputBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t kindPropertyKey = 1021;
    static const uint16_t mappingPropertyKey = 1018;
    static const uint16_t inputIndexPropertyKey = 1019;
    static const uint16_t buttonPhasePropertyKey = 1020;

protected:
    uint32_t m_Kind = 0;
    uint32_t m_Mapping = 0;
    uint32_t m_InputIndex = 0;
    uint32_t m_ButtonPhase = 1;

public:
    inline uint32_t kind() const { return m_Kind; }
    void kind(uint32_t value)
    {
        if (m_Kind == value)
        {
            return;
        }
        m_Kind = value;
        kindChanged();
    }

    inline uint32_t mapping() const { return m_Mapping; }
    void mapping(uint32_t value)
    {
        if (m_Mapping == value)
        {
            return;
        }
        m_Mapping = value;
        mappingChanged();
    }

    inline uint32_t inputIndex() const { return m_InputIndex; }
    void inputIndex(uint32_t value)
    {
        if (m_InputIndex == value)
        {
            return;
        }
        m_InputIndex = value;
        inputIndexChanged();
    }

    inline uint32_t buttonPhase() const { return m_ButtonPhase; }
    void buttonPhase(uint32_t value)
    {
        if (m_ButtonPhase == value)
        {
            return;
        }
        m_ButtonPhase = value;
        buttonPhaseChanged();
    }

    Core* clone() const override;
    void copy(const GamepadInputBase& object)
    {
        m_Kind = object.m_Kind;
        m_Mapping = object.m_Mapping;
        m_InputIndex = object.m_InputIndex;
        m_ButtonPhase = object.m_ButtonPhase;
        UserInput::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case kindPropertyKey:
                m_Kind = CoreUintType::deserialize(reader);
                return true;
            case mappingPropertyKey:
                m_Mapping = CoreUintType::deserialize(reader);
                return true;
            case inputIndexPropertyKey:
                m_InputIndex = CoreUintType::deserialize(reader);
                return true;
            case buttonPhasePropertyKey:
                m_ButtonPhase = CoreUintType::deserialize(reader);
                return true;
        }
        return UserInput::deserialize(propertyKey, reader);
    }

protected:
    virtual void kindChanged() {}
    virtual void mappingChanged() {}
    virtual void inputIndexChanged() {}
    virtual void buttonPhaseChanged() {}
};
} // namespace rive

#endif