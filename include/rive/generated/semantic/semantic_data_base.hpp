#ifndef _RIVE_SEMANTIC_DATA_BASE_HPP_
#define _RIVE_SEMANTIC_DATA_BASE_HPP_
#include <string>
#include "rive/component.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class SemanticDataBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 668;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case SemanticDataBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t rolePropertyKey = 982;
    static const uint16_t labelPropertyKey = 983;
    static const uint16_t valuePropertyKey = 984;
    static const uint16_t hintPropertyKey = 985;
    static const uint16_t headingLevelPropertyKey = 986;
    static const uint16_t traitFlagsPropertyKey = 987;
    static const uint16_t stateFlagsPropertyKey = 988;
    static const uint16_t isExpandablePropertyKey = 989;
    static const uint32_t isExpandableBitmask = 1u << 0;
    static const uint16_t isSelectablePropertyKey = 990;
    static const uint32_t isSelectableBitmask = 1u << 1;
    static const uint16_t isCheckablePropertyKey = 991;
    static const uint32_t isCheckableBitmask = 1u << 2;
    static const uint16_t isToggleablePropertyKey = 992;
    static const uint32_t isToggleableBitmask = 1u << 3;
    static const uint16_t isRequirablePropertyKey = 993;
    static const uint32_t isRequirableBitmask = 1u << 4;
    static const uint16_t isEnablablePropertyKey = 994;
    static const uint32_t isEnablableBitmask = 1u << 5;
    static const uint16_t isFocusablePropertyKey = 995;
    static const uint32_t isFocusableBitmask = 1u << 6;
    static const uint16_t isExpandedPropertyKey = 996;
    static const uint32_t isExpandedBitmask = 1u << 0;
    static const uint16_t isSelectedPropertyKey = 997;
    static const uint32_t isSelectedBitmask = 1u << 1;
    static const uint16_t isCheckedPropertyKey = 998;
    static const uint32_t isCheckedBitmask = 1u << 2;
    static const uint16_t isMixedPropertyKey = 999;
    static const uint32_t isMixedBitmask = 1u << 3;
    static const uint16_t isToggledPropertyKey = 1000;
    static const uint32_t isToggledBitmask = 1u << 4;
    static const uint16_t isRequiredPropertyKey = 1001;
    static const uint32_t isRequiredBitmask = 1u << 5;
    static const uint16_t isDisabledPropertyKey = 1002;
    static const uint32_t isDisabledBitmask = 1u << 6;
    static const uint16_t isFocusedPropertyKey = 1003;
    static const uint32_t isFocusedBitmask = 1u << 7;
    static const uint16_t isHiddenPropertyKey = 1004;
    static const uint32_t isHiddenBitmask = 1u << 8;
    static const uint16_t isLiveRegionPropertyKey = 1005;
    static const uint32_t isLiveRegionBitmask = 1u << 9;
    static const uint16_t isReadOnlyPropertyKey = 1006;
    static const uint32_t isReadOnlyBitmask = 1u << 10;
    static const uint16_t isModalPropertyKey = 1007;
    static const uint32_t isModalBitmask = 1u << 11;
    static const uint16_t isObscuredPropertyKey = 1008;
    static const uint32_t isObscuredBitmask = 1u << 12;
    static const uint16_t isMultilinePropertyKey = 1009;
    static const uint32_t isMultilineBitmask = 1u << 13;

protected:
    uint32_t m_Role = 0;
    std::string m_Label = "";
    std::string m_Value = "";
    std::string m_Hint = "";
    uint32_t m_HeadingLevel = 0;
    uint32_t m_TraitFlags = 0;
    uint32_t m_StateFlags = 0;

public:
    inline uint32_t role() const { return m_Role; }
    void role(uint32_t value)
    {
        if (m_Role == value)
        {
            return;
        }
        m_Role = value;
        roleChanged();
    }

    inline const std::string& label() const { return m_Label; }
    void label(std::string value)
    {
        if (m_Label == value)
        {
            return;
        }
        m_Label = value;
        labelChanged();
    }

    inline const std::string& value() const { return m_Value; }
    void value(std::string value)
    {
        if (m_Value == value)
        {
            return;
        }
        m_Value = value;
        valueChanged();
    }

    inline const std::string& hint() const { return m_Hint; }
    void hint(std::string value)
    {
        if (m_Hint == value)
        {
            return;
        }
        m_Hint = value;
        hintChanged();
    }

    inline uint32_t headingLevel() const { return m_HeadingLevel; }
    void headingLevel(uint32_t value)
    {
        if (m_HeadingLevel == value)
        {
            return;
        }
        m_HeadingLevel = value;
        headingLevelChanged();
    }

    inline uint32_t traitFlags() const { return m_TraitFlags; }
    void traitFlags(uint32_t value)
    {
        if (m_TraitFlags == value)
        {
            return;
        }
        m_TraitFlags = value;
        traitFlagsChanged();
    }

    inline uint32_t stateFlags() const { return m_StateFlags; }
    void stateFlags(uint32_t value)
    {
        if (m_StateFlags == value)
        {
            return;
        }
        m_StateFlags = value;
        stateFlagsChanged();
    }

    Core* clone() const override;
    void copy(const SemanticDataBase& object)
    {
        m_Role = object.m_Role;
        m_Label = object.m_Label;
        m_Value = object.m_Value;
        m_Hint = object.m_Hint;
        m_HeadingLevel = object.m_HeadingLevel;
        m_TraitFlags = object.m_TraitFlags;
        m_StateFlags = object.m_StateFlags;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case rolePropertyKey:
                m_Role = CoreUintType::deserialize(reader);
                return true;
            case labelPropertyKey:
                m_Label = CoreStringType::deserialize(reader);
                return true;
            case valuePropertyKey:
                m_Value = CoreStringType::deserialize(reader);
                return true;
            case hintPropertyKey:
                m_Hint = CoreStringType::deserialize(reader);
                return true;
            case headingLevelPropertyKey:
                m_HeadingLevel = CoreUintType::deserialize(reader);
                return true;
            case traitFlagsPropertyKey:
                m_TraitFlags = CoreUintType::deserialize(reader);
                return true;
            case stateFlagsPropertyKey:
                m_StateFlags = CoreUintType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void roleChanged() {}
    virtual void labelChanged() {}
    virtual void valueChanged() {}
    virtual void hintChanged() {}
    virtual void headingLevelChanged() {}
    virtual void traitFlagsChanged() {}
    virtual void stateFlagsChanged() {}
};
} // namespace rive

#endif