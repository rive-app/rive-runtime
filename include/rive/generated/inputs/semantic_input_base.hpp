#ifndef _RIVE_SEMANTIC_INPUT_BASE_HPP_
#define _RIVE_SEMANTIC_INPUT_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/inputs/user_input.hpp"
namespace rive
{
class SemanticInputBase : public UserInput
{
protected:
    typedef UserInput Super;

public:
    static const uint16_t typeKey = 670;

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t actionTypePropertyKey = 1010;

protected:
    uint32_t m_ActionType = 0;

public:
    inline uint32_t actionType() const { return m_ActionType; }
    void actionType(uint32_t value)
    {
        if (m_ActionType == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(actionTypePropertyKey, &m_ActionType, &value);
        m_ActionType = value;
        notifyPropertyChanged(actionTypePropertyKey);
    }

    Core* clone() const override;
    void copy(const SemanticInputBase& object)
    {
        m_ActionType = object.m_ActionType;
        UserInput::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case actionTypePropertyKey:
                m_ActionType = CoreUintType::deserialize(reader);
                return true;
        }
        return UserInput::deserialize(propertyKey, reader);
    }

protected:
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/inputs/semantic_input_ext.inl"
#endif
};
} // namespace rive

#endif