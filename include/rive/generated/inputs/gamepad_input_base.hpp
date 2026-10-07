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
        RIVE_EDITOR_CHANGING(kindPropertyKey, &m_Kind, &value);
        m_Kind = value;
        notifyPropertyChanged(kindPropertyKey);
    }

    inline uint32_t mapping() const { return m_Mapping; }
    void mapping(uint32_t value)
    {
        if (m_Mapping == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(mappingPropertyKey, &m_Mapping, &value);
        m_Mapping = value;
        notifyPropertyChanged(mappingPropertyKey);
    }

    inline uint32_t inputIndex() const { return m_InputIndex; }
    void inputIndex(uint32_t value)
    {
        if (m_InputIndex == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(inputIndexPropertyKey, &m_InputIndex, &value);
        m_InputIndex = value;
        notifyPropertyChanged(inputIndexPropertyKey);
    }

    inline uint32_t buttonPhase() const { return m_ButtonPhase; }
    void buttonPhase(uint32_t value)
    {
        if (m_ButtonPhase == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(buttonPhasePropertyKey, &m_ButtonPhase, &value);
        m_ButtonPhase = value;
        notifyPropertyChanged(buttonPhasePropertyKey);
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
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/inputs/gamepad_input_ext.inl"
#endif
};
} // namespace rive

#endif