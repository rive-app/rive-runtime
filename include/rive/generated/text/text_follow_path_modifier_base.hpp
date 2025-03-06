#ifndef _RIVE_TEXT_FOLLOW_PATH_MODIFIER_BASE_HPP_
#define _RIVE_TEXT_FOLLOW_PATH_MODIFIER_BASE_HPP_
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/text/text_target_modifier.hpp"
namespace rive
{
class TextFollowPathModifierBase : public TextTargetModifier
{
protected:
    typedef TextTargetModifier Super;

public:
    static const uint16_t typeKey = 547;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TextFollowPathModifierBase::typeKey:
            case TextTargetModifierBase::typeKey:
            case TextModifierBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t radialPropertyKey = 779;
    static const uint16_t orientPropertyKey = 782;
    static const uint16_t startPropertyKey = 783;
    static const uint16_t endPropertyKey = 784;
    static const uint16_t strengthPropertyKey = 785;
    static const uint16_t offsetPropertyKey = 786;

protected:
    bool m_Radial = false;
    bool m_Orient = true;
    float m_Start = 0.0f;
    float m_End = 1.0f;
    float m_Strength = 1.0f;
    float m_Offset = 0.0f;

public:
    inline bool radial() const { return m_Radial; }
    void radial(bool value)
    {
        if (m_Radial == value)
        {
            return;
        }
        m_Radial = value;
        radialChanged();
    }

    inline bool orient() const { return m_Orient; }
    void orient(bool value)
    {
        if (m_Orient == value)
        {
            return;
        }
        m_Orient = value;
        orientChanged();
    }

    inline float start() const { return m_Start; }
    void start(float value)
    {
        if (m_Start == value)
        {
            return;
        }
        m_Start = value;
        startChanged();
    }

    inline float end() const { return m_End; }
    void end(float value)
    {
        if (m_End == value)
        {
            return;
        }
        m_End = value;
        endChanged();
    }

    inline float strength() const { return m_Strength; }
    void strength(float value)
    {
        if (m_Strength == value)
        {
            return;
        }
        m_Strength = value;
        strengthChanged();
    }

    inline float offset() const { return m_Offset; }
    void offset(float value)
    {
        if (m_Offset == value)
        {
            return;
        }
        m_Offset = value;
        offsetChanged();
    }

    Core* clone() const override;
    void copy(const TextFollowPathModifierBase& object)
    {
        m_Radial = object.m_Radial;
        m_Orient = object.m_Orient;
        m_Start = object.m_Start;
        m_End = object.m_End;
        m_Strength = object.m_Strength;
        m_Offset = object.m_Offset;
        TextTargetModifier::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case radialPropertyKey:
                m_Radial = CoreBoolType::deserialize(reader);
                return true;
            case orientPropertyKey:
                m_Orient = CoreBoolType::deserialize(reader);
                return true;
            case startPropertyKey:
                m_Start = CoreDoubleType::deserialize(reader);
                return true;
            case endPropertyKey:
                m_End = CoreDoubleType::deserialize(reader);
                return true;
            case strengthPropertyKey:
                m_Strength = CoreDoubleType::deserialize(reader);
                return true;
            case offsetPropertyKey:
                m_Offset = CoreDoubleType::deserialize(reader);
                return true;
        }
        return TextTargetModifier::deserialize(propertyKey, reader);
    }

protected:
    virtual void radialChanged() {}
    virtual void orientChanged() {}
    virtual void startChanged() {}
    virtual void endChanged() {}
    virtual void strengthChanged() {}
    virtual void offsetChanged() {}
};
} // namespace rive

#endif